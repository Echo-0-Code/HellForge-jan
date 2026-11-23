-- HD2-Addon: mods/hellforge/mod_options
-- HellForge Mod Options: code mods' settings (SDK 30 add_option) as rows of the Vanilla Plus Megapack's OPTIONS > MODS
-- menu (_G.ModOptionsMenu, api 1). HellForge keeps the values: this reads them through the Lua bridge (SDK 30 exports,
-- native/mod-sdk/hellforge_lua.h), registers each one, reports the player's applied changes back and sets a row again
-- when a mod changes its value from code. The menu's own saved value never wins over HellForge's.
-- Started by Bingus Shared Loader (v15+). Does nothing without HellForge's loader or the menu.

local ffi = require('ffi')
ffi.cdef [[
    void* hfopt1_GetModuleHandleA(const char* name) __asm__("GetModuleHandleA");
    struct hfopt1_option {
        uint32_t size, kind;
        const char* mod_id; const char* mod_name; const char* key; const char* label; const char* description;
        double value, min, max, step;
        uint32_t gap, choice_count;
        const char* choices[16];
        uint32_t serial;
    };
    uint32_t hfopt1_options(uint32_t* revision) __asm__("hellforge_lua_options");
    int hfopt1_option(uint32_t index, struct hfopt1_option* out) __asm__("hellforge_lua_option");
    int hfopt1_applied(uint32_t index, double value) __asm__("hellforge_lua_option_applied");
    void hfopt1_note(const char* text) __asm__("hellforge_lua_options_note");
]]
local kernel32 = ffi.load('kernel32')
local DLL = 'hellforge_modloader.dll'
local KINDS = {[1] = 'toggle', [2] = 'choice', [3] = 'slider'}

local lib = nil          -- the loader with the options exports, false when it has none
local wait = 0           -- frames until the loader is looked for again
local menu_seen = nil
local rows = {}          -- index (0 ..) -> {key, kind, serial} of a registered row, or false when the menu refused it
local revision_seen = -1
local revision = ffi.new('uint32_t[1]')
local option = ffi.new('struct hfopt1_option')
option.size = ffi.sizeof(option)
local after = ffi.new('struct hfopt1_option') -- read in on_change, which may run while option is in use
after.size = ffi.sizeof(after)
local notes = {}

local function note(text)
    if notes[text] then return end
    notes[text] = true
    if lib then pcall(lib.hfopt1_note, text) else print('[hellforge mod options] ' .. text) end
end

local function find_loader()
    if lib ~= nil then return lib end
    if wait > 0 then wait = wait - 1; return nil end
    wait = 60
    if kernel32.hfopt1_GetModuleHandleA(DLL) == nil then return nil end -- no code mods: HellForge's loader is not here
    local ok, found = pcall(ffi.load, DLL)
    if ok and pcall(function() return found.hfopt1_options end) then lib = found
    else lib = false; print('[hellforge mod options] this HellForge has no mod options (SDK 30)') end
    return lib
end

local function text(pointer) return pointer ~= nil and ffi.string(pointer) or nil end

-- HellForge's value (toggle 0/1, choice 0-based) as the menu's (true/false, 1-based), and back
local function to_menu(kind, value)
    if kind == 1 then return value ~= 0 end
    if kind == 2 then return math.floor(value + 0.5) + 1 end
    return value
end
local function from_menu(kind, value)
    if kind == 1 then return value and 1 or 0 end
    if kind == 2 then return (tonumber(value) or 1) - 1 end
    return tonumber(value) or 0
end
local function same(row, a, b)
    if row.kind == 3 then return type(b) == 'number' and math.abs(a - b) <= row.step * 1e-3 end
    return a == b
end

-- puts the menu's row to HellForge's value (registration may have given it the menu's saved one)
local function show(menu, row, value)
    if type(menu.get) ~= 'function' or type(menu.set) ~= 'function' then return end
    local ok, shown = pcall(menu.get, row.key)
    if ok and not same(row, value, shown) then
        local done, why = pcall(menu.set, row.key, value)
        if not done then note(row.key .. ': the menu did not take the value (' .. tostring(why) .. ')') end
    end
end

local function register(menu, index)
    local kind = option.kind
    local value = to_menu(kind, option.value)
    local spec = {type = KINDS[kind], label = text(option.label), mod = text(option.mod_name), mod_id = text(option.mod_id),
                  description = text(option.description), default = value, gap = option.gap ~= 0 or nil}
    if kind == 2 then
        spec.choices = {}
        for i = 0, option.choice_count - 1 do spec.choices[#spec.choices + 1] = text(option.choices[i]) end
    elseif kind == 3 then
        spec.min, spec.max, spec.step = option.min, option.max, option.step
    end
    local key = text(option.key)
    local ok, added, why = pcall(menu.register_option, key, spec)
    if not (ok and added) then
        rows[index] = false
        note(key .. ' refused by the menu: ' .. tostring(ok and why or added))
        return
    end
    local row = {key = key, kind = kind, serial = option.serial, step = option.step}
    rows[index] = row
    pcall(menu.on_change, key, function(applied)
        local fine, err = pcall(lib.hfopt1_applied, index, from_menu(kind, applied))
        if not fine then note(key .. ': ' .. tostring(err)) end
        if lib.hfopt1_option(index, after) == 1 then row.serial = after.serial end
    end)
    show(menu, row, value)
end

local function step()
    if not find_loader() then return end
    local menu = rawget(_G, 'ModOptionsMenu')
    if type(menu) ~= 'table' or menu.api ~= 1 or type(menu.register_option) ~= 'function' then return end
    if menu ~= menu_seen then menu_seen, rows, revision_seen = menu, {}, -1 end
    local count = lib.hfopt1_options(revision)
    if revision[0] == revision_seen then return end
    revision_seen = revision[0]
    local added = 0
    for index = 0, count - 1 do
        if lib.hfopt1_option(index, option) == 1 then
            local row = rows[index]
            if row == nil then register(menu, index); added = added + (rows[index] and 1 or 0)
            elseif row and row.serial ~= option.serial then
                row.serial = option.serial
                show(menu, row, to_menu(row.kind, option.value))
            end
        end
    end
    if added > 0 then lib.hfopt1_note(added .. ' option(s) added to OPTIONS > MODS') end
end

local previous_update = rawget(_G, 'update')
update = function(dt)
    local ok, err = pcall(step)
    if not ok then note('update failed: ' .. tostring(err)) end
    if type(previous_update) == 'function' then return previous_update(dt) end
end
