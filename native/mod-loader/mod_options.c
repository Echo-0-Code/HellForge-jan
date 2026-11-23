/* SDK 29 add_option, get_option and set_option, and the Lua bridge exports for the bundled Lua mod that shows them in the
 * Megapack's OPTIONS > MODS menu (docs/modding/options.md). HellForge's saved value is the truth: the Megapack's menu only
 * serves the game's Lua, and the Lua mod sets each row to HellForge's value after registering it, so a menu without
 * HellForge (or HellForge without the menu) never loses a value. Included at the end of lua_bridge.c. */

#define OPTIONS_MAX 256
#define OPTION_ID_MAX 48
#define OPTION_LABEL_CHARS 64
#define OPTION_DESCRIPTION_CHARS 400
#define OPTION_CHOICE_CHARS 48
#define OPTION_MOD_NAME_CHARS 40
#define OPTION_FILE_LINES 256

typedef struct ModOption
{
    HfMod* mod;                 /* the mod that added it last (a live mod set up again is a new entry: same id) */
    char modId[80];
    char modName[OPTION_MOD_NAME_CHARS * 4 + 1];
    char id[OPTION_ID_MAX + 1];
    char key[160];              /* the menu's id: hellforge/<mod id>/<id> */
    char label[OPTION_LABEL_CHARS * 4 + 1];
    char description[OPTION_DESCRIPTION_CHARS * 4 + 1];
    char choiceText[HF_OPTION_MAX_CHOICES][OPTION_CHOICE_CHARS * 4 + 1];
    const char* choices[HF_OPTION_MAX_CHOICES];
    uint32_t kind, choiceCount, gap;
    double value, min, max, step;
    uint32_t serial;            /* bumped by every change of value */
    HfOptionCallback onChange;
    void* user;
} ModOption;

static ModOption g_options[OPTIONS_MAX];
static volatile LONG g_optionCount;  /* entries are whole before the count shows them */
static volatile LONG g_optionRevision;
static SRWLOCK g_optionsLock = SRWLOCK_INIT;

/* UTF-8 characters in text (bytes that are not continuation bytes), or -1 when it holds a control character. */
static int OptionChars(const char* text)
{
    int n = 0;
    for (const unsigned char* c = (const unsigned char*)text; *c; c++)
    {
        if (*c < 0x20 || *c == 0x7F) return -1;
        if ((*c & 0xC0) != 0x80) n++;
    }
    return n;
}

static int OptionIdValid(const char* id)
{
    size_t n = id != NULL ? strlen(id) : 0;
    if (n == 0 || n > OPTION_ID_MAX) return 0;
    for (size_t i = 0; i < n; i++)
    {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return 0;
    }
    return 1;
}

/* A text within limit characters copied to out (size bytes); 0 when it is NULL-but-required, too long or not text. */
static int OptionText(const char* text, int limit, int required, char* out, size_t size)
{
    out[0] = 0;
    if (text == NULL || text[0] == 0) return !required;
    int n = OptionChars(text);
    if (n < 0 || n > limit || strlen(text) >= size) return 0;
    memcpy(out, text, strlen(text) + 1);
    return 1;
}

/* value as the option holds it (a toggle 0/1, a choice's index, a slider clamped and snapped), or NaN when not a number. */
static double OptionFit(const ModOption* o, double value)
{
    if (value != value || value > 1e300 || value < -1e300) return NAN;
    if (o->kind == HF_OPTION_TOGGLE) return value != 0 ? 1.0 : 0.0;
    if (o->kind == HF_OPTION_CHOICE)
    {
        double i = floor(value + 0.5);
        return i < 0 ? 0 : i > (double)(o->choiceCount - 1) ? (double)(o->choiceCount - 1) : i;
    }
    double v = value < o->min ? o->min : value > o->max ? o->max : value;
    v = o->min + floor((v - o->min) / o->step + 0.5) * o->step;
    return v > o->max ? o->max : v;
}

/* The mod's values file: <the folder above the session's logs>\mod-options\<mod id>.txt (folder made when making). */
static int OptionFilePath(const char* modId, wchar_t* out, size_t count, int make)
{
    if (!g_logs[0]) return 0;
    wchar_t base[MAX_PATH];
    wcsncpy(base, g_logs, MAX_PATH - 1);
    base[MAX_PATH - 1] = 0;
    size_t n = wcslen(base);
    while (n > 0 && (base[n - 1] == L'\\' || base[n - 1] == L'/')) base[--n] = 0;
    while (n > 0 && base[n - 1] != L'\\' && base[n - 1] != L'/') n--;
    base[n] = 0;
    if (n == 0) return 0;
    wchar_t folder[MAX_PATH];
    if (swprintf(folder, MAX_PATH, L"%lsmod-options", base) < 0) return 0;
    if (make) CreateDirectoryW(folder, NULL);
    return swprintf(out, count, L"%ls\\%hs.txt", folder, modId) > 0;
}

/* The saved value of id in the mod's file: 1 with *value, else 0. */
static int OptionSaved(const char* modId, const char* id, double* value)
{
    wchar_t path[MAX_PATH];
    if (!OptionFilePath(modId, path, MAX_PATH, 0)) return 0;
    FILE* f = _wfopen(path, L"r");
    if (f == NULL) return 0;
    char line[160], name[OPTION_ID_MAX + 2];
    double v;
    int found = 0;
    while (!found && fgets(line, sizeof line, f))
        if (sscanf(line, " %49s %lf", name, &v) == 2 && !strcmp(name, id) && v == v) { *value = v; found = 1; }
    fclose(f);
    return found;
}

static ModOption* OptionFind(const char* modId, const char* id)
{
    for (LONG i = 0; i < g_optionCount; i++)
        if (!strcmp(g_options[i].modId, modId) && !strcmp(g_options[i].id, id)) return &g_options[i];
    return NULL;
}

/* The lines of the mod's file whose ids it did not add this session, kept when the file is written. Under the lock. */
static int OptionKeptLines(const wchar_t* path, const char* modId, char (*kept)[160])
{
    FILE* old = _wfopen(path, L"r");
    if (old == NULL) return 0;
    int count = 0;
    char line[160], name[OPTION_ID_MAX + 2];
    double v;
    while (count < OPTION_FILE_LINES && fgets(line, sizeof line, old))
        if (sscanf(line, " %49s %lf", name, &v) == 2 && OptionFind(modId, name) == NULL)
            snprintf(kept[count++], sizeof kept[0], "%s %.17g", name, v);
    fclose(old);
    return count;
}

/* Writes the mod's file (to a new file, then moved over the old: a crash never leaves half a file). Under the lock. */
static void OptionSave(const char* modId)
{
    wchar_t path[MAX_PATH], temp[MAX_PATH];
    if (!OptionFilePath(modId, path, MAX_PATH, 1) || swprintf(temp, MAX_PATH, L"%ls.new", path) < 0) return;
    static char kept[OPTION_FILE_LINES][160];
    int keptCount = OptionKeptLines(path, modId, kept);
    FILE* f = _wfopen(temp, L"w");
    if (f == NULL) return;
    for (LONG i = 0; i < g_optionCount; i++)
        if (!strcmp(g_options[i].modId, modId)) fprintf(f, "%s %.17g\n", g_options[i].id, g_options[i].value);
    for (int i = 0; i < keptCount; i++) fprintf(f, "%s\n", kept[i]);
    if (fclose(f) == 0) MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING);
    else DeleteFileW(temp);
}

/* The mod's name from its manifest (hellforge-mod.json "name"), within the menu's 40 characters; else its id. */
static void OptionModName(const HfMod* mod, char* out, size_t size)
{
    snprintf(out, size, "%s", mod->id);
    if (mod->name[0] && OptionChars(mod->name) > 0 && OptionChars(mod->name) <= OPTION_MOD_NAME_CHARS)
    { snprintf(out, size, "%s", mod->name); return; }
    char path[MAX_PATH * 3 + 32];
    snprintf(path, sizeof path, "%s\\hellforge-mod.json", mod->folder);
    wchar_t wide[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, MAX_PATH * 2)) return;
    FILE* f = _wfopen(wide, L"rb");
    if (f == NULL) return;
    static char json[65536];
    size_t n = fread(json, 1, sizeof json - 1, f);
    fclose(f);
    json[n] = 0;
    const char* at = strstr(json, "\"name\"");
    if (at == NULL || (at = strchr(at + 6, ':')) == NULL || (at = strchr(at, '"')) == NULL) return;
    at++;
    char name[OPTION_MOD_NAME_CHARS * 4 + 1];
    size_t k = 0;
    while (*at && *at != '"' && k < sizeof name - 1)
    {
        if (*at == '\\' && at[1]) at++; /* a simple escape: the next character as it is */
        name[k++] = *at++;
    }
    name[k] = 0;
    if (*at == '"' && OptionChars(name) > 0 && OptionChars(name) <= OPTION_MOD_NAME_CHARS) snprintf(out, size, "%s", name);
}

static const char* OptionCheckChoices(const HfOption* def, ModOption* o)
{
    if (def->choices == NULL || def->choice_count < 2 || def->choice_count > HF_OPTION_MAX_CHOICES) return "a choice needs 2 to 16 choices";
    o->choiceCount = def->choice_count;
    for (uint32_t i = 0; i < o->choiceCount; i++)
        if (!OptionText(def->choices[i], OPTION_CHOICE_CHARS, 1, o->choiceText[i], sizeof o->choiceText[i]))
            return "each choice must be 1 to 48 characters";
    return NULL;
}

/* The menu keeps a slider's numbers as floats. */
static const char* OptionCheckSlider(const HfOption* def, ModOption* o)
{
    o->min = def->min, o->max = def->max, o->step = def->step > 0 ? def->step : 1.0;
    int finite = o->min == o->min && o->max == o->max && fabs(o->min) <= 3.0e38 && fabs(o->max) <= 3.0e38 && o->step <= 3.0e38;
    if (!finite || !(o->min < o->max) || o->step > o->max - o->min || o->step < 1.2e-38)
        return "a slider needs finite min < max and 0 < step <= max - min";
    return NULL;
}

/* def checked and copied into o: NULL, or why it is refused. */
static const char* OptionCheck(const HfOption* def, ModOption* o)
{
    o->kind = def->kind;
    if (o->kind != HF_OPTION_TOGGLE && o->kind != HF_OPTION_CHOICE && o->kind != HF_OPTION_SLIDER)
        return "kind must be HF_OPTION_TOGGLE, _CHOICE or _SLIDER";
    if (!OptionText(def->label, OPTION_LABEL_CHARS, 1, o->label, sizeof o->label))
        return "the label must be 1 to 64 characters, no control characters";
    if (!OptionText(def->description, OPTION_DESCRIPTION_CHARS, 0, o->description, sizeof o->description))
        return "the description must be up to 400 characters, no control characters";
    const char* why = o->kind == HF_OPTION_CHOICE ? OptionCheckChoices(def, o) : o->kind == HF_OPTION_SLIDER ? OptionCheckSlider(def, o) : NULL;
    if (why != NULL) return why;
    o->gap = def->gap != 0;
    o->onChange = def->on_change, o->user = def->user;
    return OptionFit(o, def->value) == OptionFit(o, def->value) ? NULL : "its value is not a number";
}

/* Adds o (checked) under the lock: 1 added, 2 already there (the callback is the new one: a live mod set up again),
 * 0 no room. */
static int OptionInsert(ModOption* o, double fallback)
{
    ModOption* there = OptionFind(o->modId, o->id);
    if (there != NULL) { there->mod = o->mod, there->onChange = o->onChange, there->user = o->user; return 2; }
    int mine = 0;
    for (LONG i = 0; i < g_optionCount; i++) mine += !strcmp(g_options[i].modId, o->modId);
    if (mine >= (int)HF_OPTION_MAX_PER_MOD || g_optionCount >= OPTIONS_MAX) return 0;
    double saved = NAN;
    if (OptionSaved(o->modId, o->id, &saved)) saved = OptionFit(o, saved);
    o->value = saved == saved ? saved : fallback;
    o->serial = 1;
    ModOption* slot = &g_options[g_optionCount];
    *slot = *o;
    for (uint32_t i = 0; i < slot->choiceCount; i++) slot->choices[i] = slot->choiceText[i];
    MemoryBarrier(); /* the entry is whole before the count shows it to the bridge */
    InterlockedIncrement(&g_optionCount);
    InterlockedIncrement(&g_optionRevision);
    return 1;
}

/* SDK 29, add_option: 1 added, 2 already there, 0 refused (logged). */
static int ApiAddOption(HfMod* mod, const HfOption* def)
{
    if (mod == NULL) return 0;
    if (ModGone(mod)) { ApiLog(mod, "add_option: refused: the mod is being taken out"); return 0; }
    if (def == NULL || def->size < sizeof(HfOption)) { ApiLog(mod, "add_option: refused: it needs an HfOption with size set"); return 0; }
    if (!OptionIdValid(def->id)) { ApiLog(mod, "add_option: refused: the id must be 1 to %d of a-z 0-9 _ . -", OPTION_ID_MAX); return 0; }
    static ModOption o; /* large: built here, under g_optionsAddLock */
    static SRWLOCK g_optionsAddLock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&g_optionsAddLock);
    memset(&o, 0, sizeof o);
    const char* why = OptionCheck(def, &o);
    int result = 0;
    if (why == NULL)
    {
        o.mod = mod;
        snprintf(o.modId, sizeof o.modId, "%s", mod->id);
        snprintf(o.id, sizeof o.id, "%s", def->id);
        snprintf(o.key, sizeof o.key, "hellforge/%s/%s", mod->id, def->id);
        OptionModName(mod, o.modName, sizeof o.modName);
        AcquireSRWLockExclusive(&g_optionsLock);
        result = OptionInsert(&o, OptionFit(&o, def->value));
        ReleaseSRWLockExclusive(&g_optionsLock);
        if (result == 0) why = "no room (32 options a mod, 256 in all)";
    }
    if (why != NULL) ApiLog(mod, "add_option: %s refused: %s", def->id, why);
    else if (result == 1) ApiLog(mod, "add_option: %s added, value %g", def->id, o.value);
    ReleaseSRWLockExclusive(&g_optionsAddLock);
    return result;
}

/* SDK 29, get_option: 1 with *value, 0 when the mod has no such option. */
static int ApiGetOption(HfMod* mod, const char* id, double* value)
{
    if (mod == NULL || id == NULL || value == NULL) return 0;
    AcquireSRWLockShared(&g_optionsLock);
    const ModOption* o = OptionFind(mod->id, id);
    if (o != NULL) *value = o->value;
    ReleaseSRWLockShared(&g_optionsLock);
    return o != NULL;
}

/* SDK 29, set_option: 1 done (saved, the menu follows), 0 refused (logged). */
static int ApiSetOption(HfMod* mod, const char* id, double value)
{
    if (mod == NULL || id == NULL) return 0;
    AcquireSRWLockExclusive(&g_optionsLock);
    ModOption* o = OptionFind(mod->id, id);
    double v = o != NULL ? OptionFit(o, value) : NAN;
    if (o != NULL && v == v && v != o->value)
    {
        o->value = v;
        o->serial++;
        InterlockedIncrement(&g_optionRevision);
        OptionSave(o->modId);
    }
    ReleaseSRWLockExclusive(&g_optionsLock);
    if (o == NULL) { ApiLog(mod, "set_option: %s refused: the mod added no such option", id); return 0; }
    if (v != v) { ApiLog(mod, "set_option: %s refused: not a number", id); return 0; }
    return 1;
}

/* ---- the Lua bridge's side (hellforge_lua.h): what the bundled Lua mod reads and reports ----------------- */

/* How many options there are, and the revision (bumped by every option added and every value changed). */
HF_LUA_API uint32_t hellforge_lua_options(uint32_t* revision)
{
    if (revision != NULL) *revision = (uint32_t)g_optionRevision;
    return (uint32_t)g_optionCount;
}

/* Option index (0 ..) in out: 1, or 0 past the last. Its texts stay where they are for the session. */
HF_LUA_API int hellforge_lua_option(uint32_t index, HfLuaOption* out)
{
    if (out == NULL || out->size < sizeof *out || index >= (uint32_t)g_optionCount) return 0;
    AcquireSRWLockShared(&g_optionsLock);
    const ModOption* o = &g_options[index];
    *out = (HfLuaOption){sizeof *out, o->kind, o->modId, o->modName, o->key, o->label, o->description[0] ? o->description : NULL,
        o->value, o->min, o->max, o->step, o->gap, o->choiceCount, {0}, o->serial};
    for (uint32_t i = 0; i < o->choiceCount; i++) out->choices[i] = o->choices[i];
    ReleaseSRWLockShared(&g_optionsLock);
    return 1;
}

/* The player applied value to option index in the menu (the Lua's on_change, on the game thread): saved, and the mod's
 * on_change called. 1 taken, 0 refused (no such option, not a number). */
HF_LUA_API int hellforge_lua_option_applied(uint32_t index, double value)
{
    if (index >= (uint32_t)g_optionCount) return 0;
    AcquireSRWLockExclusive(&g_optionsLock);
    ModOption* o = &g_options[index];
    double v = OptionFit(o, value);
    int changed = v == v && v != o->value;
    if (changed)
    {
        o->value = v;
        o->serial++;
        InterlockedIncrement(&g_optionRevision);
        OptionSave(o->modId);
    }
    HfMod* mod = o->mod;
    HfOptionCallback onChange = o->onChange;
    void* user = o->user;
    char id[OPTION_ID_MAX + 1];
    memcpy(id, o->id, sizeof id);
    ReleaseSRWLockExclusive(&g_optionsLock);
    if (v != v) return 0;
    if (!changed) return 1;
    ApiLog(mod, "option %s set to %g in the MODS menu", id, v);
    if (onChange != NULL && !mod->removing && !mod->removed)
    {
        HfMod* previous = g_callingMod;
        g_callingMod = mod;
        onChange(mod, id, v, user);
        g_callingMod = previous;
    }
    return 1;
}

/* A line in modloader.log from the bundled Lua mod (what it registered, what the menu refused). */
HF_LUA_API void hellforge_lua_options_note(const char* text)
{
    if (text == NULL) return;
    char line[400];
    snprintf(line, sizeof line, "%s", text);
    for (char* c = line; *c; c++) if ((unsigned char)*c < 0x20) *c = ' ';
    Note("MODS menu: %s", line);
}
