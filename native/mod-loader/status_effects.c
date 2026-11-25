/* SDK 29 (hellforge_status.h): status effects of a mod's own, for game build 25480438. One ApplyStatus hook for every
 * mod only queues a note; the meters, levels and looks are worked out on the game thread. A texture is set only when an
 * entity's shown level changes: setting the same texture again shows the material's own for a frame or two, which is
 * a flicker. An entity that loses its unit gets its own textures back while that unit is still alive and still the
 * same resource, so a reused handle is never touched. A look row with texture slot 0 swaps the whole material slot to
 * another material resource (SDK 33): instanced batches (bugs) draw every member with one member's material, so only
 * a resource of its own keeps a look on one unit. Included by world_calls.c after joint_poses.c. */

#define SE_EFFECTS 64
#define SE_TARGETS 256
#define SE_SLOTS 4      /* effects one entity carries at once */
#define SE_SHOWN 32     /* textures one entity shows */
#define SE_QUEUE 2048
#define SE_CHANNEL "@hellforge.status" /* no mod can have it: mod ids start with a letter or digit */
#define SE_WIRE_VERSION 1u

typedef struct SeEffect
{
    HfMod* mod;
    HfStatusEffect def;
    char name[64];
    uint32_t key;        /* the other machines find the effect by this hash of "<mod id>/<name>" */
    HfEffectLook* looks;
    uint32_t lookCount;
    uint64_t package;
} SeEffect;
typedef struct SeSlot { uint32_t effect, level; float meter; double lastGain, levelSince; int remote; } SeSlot;
typedef struct SeShown { uint32_t material, texture, effect; uint64_t own, shown; } SeShown;
typedef struct SeTarget
{
    uint32_t entity, unit, shownCount;
    uint64_t name;
    int dirty;
    SeSlot slots[SE_SLOTS];
    SeShown shown[SE_SHOWN];
} SeTarget;
typedef struct SeNote { uint32_t entity, status, effect; float amount; uint64_t update; } SeNote; /* effect 0: a vanilla status */
#pragma pack(push, 1)
typedef struct SeWire { uint8_t version, level; uint16_t reserved; uint32_t network, key; } SeWire;
#pragma pack(pop)

typedef void (*SeApplyStatusFn)(void* receivers, uint32_t entity, uint32_t type, uint32_t r9, float buildup, uint32_t s6, uint64_t s7);
typedef uint8_t* (*SeDescriptorFn)(uint32_t entity);
typedef uint8_t (*SeAliveFn)(uint32_t unit);
typedef uint32_t (*SeNumMeshesFn)(uint32_t unit);
typedef void* (*SeMeshFn)(uint32_t unit, int32_t index);
typedef uint64_t (*SeUnitNameFn)(uint32_t unit);
typedef void (*SeSetTextureFn)(void* material, uint32_t slot, uint64_t texture);
typedef void* (*SeSetMaterialFn)(uint32_t unit, uint32_t slot, uint64_t material);

static SeEffect g_se[SE_EFFECTS];
static volatile LONG g_seCount;
static SeTarget g_seTargets[SE_TARGETS];
static SeNote g_seQueue[SE_QUEUE], g_seTaken[SE_QUEUE];
static uint32_t g_seQueued;
static SeWire g_seInbox[64];
static uint32_t g_seInboxCount, g_seTokens = CHANNEL_RATE;
static ULONGLONG g_seRefilled;
static volatile uint64_t g_seUpdate = 1;
static double g_seClock;
static SRWLOCK g_seLock = SRWLOCK_INIT;       /* the queue and the inbox */
static SRWLOCK g_seTargetLock = SRWLOCK_INIT; /* the targets and the looks, held by the game thread's whole update */
static volatile LONG g_seInTick;
static volatile LONG g_seTicking, g_seDropped;
static int g_seState; /* 0 waiting for the game, 1 ready, -1 off (logged) */
static int g_seHooked;
static HfMod g_seMod; /* the loader's own tick */
static SeApplyStatusFn g_seApplyStatus;
static SeDescriptorFn g_seDescriptor;
static SeAliveFn g_seAlive;
static SeNumMeshesFn g_seNumMeshes;
static SeMeshFn g_seMesh;
static SeUnitNameFn g_seUnitName;
static SeSetTextureFn g_seSetTexture;
static SeSetMaterialFn g_seSetMaterial;     /* NULL: material looks are off (logged once) */
/* the damage each entity takes now, for the damage consumer (rebuilt every update) */
typedef struct SeDamageRow { uint32_t entity; float scale; } SeDamageRow;
static SeDamageRow g_seDamage[SE_TARGETS];
static volatile LONG g_seDamageCount;

static int SeFeedsAny(void)
{
    for (LONG i = 0; i < g_seCount; i++) if (g_se[i].def.feed_count && !ModGone(g_se[i].mod)) return 1;
    return 0;
}

static void SeQueue(SeNote note)
{
    AcquireSRWLockExclusive(&g_seLock);
    if (g_seQueued < SE_QUEUE) g_seQueue[g_seQueued++] = note;
    else InterlockedIncrement(&g_seDropped);
    ReleaseSRWLockExclusive(&g_seLock);
}

static int SeFeedsStatus(uint32_t status)
{
    for (LONG i = 0; i < g_seCount; i++)
        for (uint32_t f = 0; f < g_se[i].def.feed_count; f++)
            if (g_se[i].def.feeds[f].status == status || g_se[i].def.feeds[f].with_status == status) return 1;
    return 0;
}

/* Passes every call straight on; a status some effect counts is noted for the game thread. */
static void SeApplyStatusDetour(void* receivers, uint32_t entity, uint32_t type, uint32_t r9, float buildup, uint32_t s6, uint64_t s7)
{
    if (entity != 0 && buildup > 0.0f && SeFeedsStatus(type)) SeQueue((SeNote){entity, type, 0, buildup, g_seUpdate});
    g_seApplyStatus(receivers, entity, type, r9, buildup, s6, s7);
}

/* ---- targets ---- */

static uint32_t SeUnitOf(uint32_t entity)
{
    uint32_t unit = 0;
    uint8_t* d = g_seDescriptor(entity);
    if (d == NULL || SafeRead((uintptr_t)d + 0xC, &unit, 4) || unit == 0 || !(g_seAlive(unit) & 1)) return 0;
    return unit;
}

static SeTarget* SeTargetOf(uint32_t entity, int create)
{
    SeTarget* free_at = NULL;
    for (int i = 0; i < SE_TARGETS; i++)
    {
        if (g_seTargets[i].entity == entity) return &g_seTargets[i];
        if (free_at == NULL && g_seTargets[i].entity == 0) free_at = &g_seTargets[i];
    }
    if (!create || free_at == NULL) return NULL;
    uint32_t unit = SeUnitOf(entity);
    if (unit == 0) return NULL;
    memset(free_at, 0, sizeof *free_at);
    *free_at = (SeTarget){entity, unit, 0, g_seUnitName(unit), 0, {{0}}, {{0}}};
    return free_at;
}

static SeSlot* SeSlotOf(SeTarget* t, uint32_t effect, int create)
{
    for (int i = 0; i < SE_SLOTS; i++) if (t->slots[i].effect == effect) return &t->slots[i];
    if (!create) return NULL;
    for (int i = 0; i < SE_SLOTS; i++)
        if (t->slots[i].effect == 0) { t->slots[i] = (SeSlot){effect, 0, 0, g_seClock, g_seClock, 0}; return &t->slots[i]; }
    return NULL;
}

/* ---- looks ---- */

static const HfEffectLook* SeLookRow(const SeEffect* e, uint64_t unit, uint32_t material, uint32_t texture)
{
    for (uint32_t i = 0; i < e->lookCount; i++)
        if (e->looks[i].unit == unit && e->looks[i].material_slot == material && e->looks[i].texture_slot == texture) return &e->looks[i];
    return NULL;
}

static int SeResident(const SeEffect* e) { return e->package == 0 || ApiLoadPackage(&g_seMod, e->package) == HF_PACKAGE_RESIDENT; }

/* The texture a target wants on one material's texture slot: the look of its highest level there, 0 its own. */
static uint64_t SeWanted(const SeTarget* t, uint32_t material, uint32_t texture, uint32_t* effect)
{
    uint64_t best = 0;
    uint32_t bestLevel = 0;
    for (int i = 0; i < SE_SLOTS; i++)
    {
        const SeSlot* s = &t->slots[i];
        if (s->effect == 0 || s->level <= bestLevel || ModGone(g_se[s->effect - 1].mod)) continue;
        const HfEffectLook* row = SeLookRow(&g_se[s->effect - 1], t->name, material, texture);
        if (row == NULL || row->level[s->level - 1] == 0 || !SeResident(&g_se[s->effect - 1])) continue;
        best = row->level[s->level - 1];
        bestLevel = s->level;
        *effect = s->effect;
    }
    return best;
}

static SeShown* SeShownOf(SeTarget* t, uint32_t material, uint32_t texture, int create)
{
    for (uint32_t i = 0; i < t->shownCount; i++) if (t->shown[i].material == material && t->shown[i].texture == texture) return &t->shown[i];
    if (!create || t->shownCount >= SE_SHOWN) return NULL;
    t->shown[t->shownCount] = (SeShown){material, texture, 0, 0, 0};
    return &t->shown[t->shownCount++];
}

/* The k-th material of a mesh when its slot is `material` and it is drawn (has a render id), else 0. */
static uintptr_t SeMaterialAt(uintptr_t mats, uintptr_t slots, uint32_t k, uint32_t material)
{
    uint32_t slot = 0, word = 0;
    uintptr_t m = 0;
    if (SafeRead(slots + 4 * k, &slot, 4) || slot != material || SafeRead(mats + 8 * k, &m, 8) || !m) return 0;
    return SafeRead(m + 4, &word, 4) || (word & 0xFFFFFFu) == 0xFFFFFFu ? 0 : m;
}

/* The drawn materials of a unit with this material slot, on every mesh, up to max; their count. */
static uint32_t SeSlotMaterials(uint32_t unit, uint32_t material, uintptr_t* out, uint32_t max)
{
    uint32_t meshes = g_seNumMeshes(unit), n = 0;
    for (uint32_t i = 0; i < meshes && i < 64; i++)
    {
        void* mesh = g_seMesh(unit, (int32_t)i);
        uint32_t count = 0;
        uintptr_t mats = 0, slots = 0, m;
        if (mesh == NULL || SafeRead((uintptr_t)mesh + 0x18, &count, 4) || count == 0 || count > 64) continue;
        if (SafeRead((uintptr_t)mesh + 0x20, &mats, 8) || SafeRead((uintptr_t)mesh + 0x60, &slots, 8) || !mats || !slots) continue;
        for (uint32_t k = 0; k < count && n < max; k++)
            if ((m = SeMaterialAt(mats, slots, k, material)) != 0) out[n++] = m;
    }
    return n;
}

/* The material resource a unit's material slot draws with now (a material instance's +0x38), 0 if none. */
static uint64_t SeMaterialResourceOf(uint32_t unit, uint32_t material)
{
    uintptr_t m = 0;
    uint64_t resource = 0;
    if (SeSlotMaterials(unit, material, &m, 1) == 0 || SafeRead(m + 0x38, &resource, 8)) return 0;
    return resource;
}

/* A material look: the unit's material slot on another material resource. Back to its own is a set of its own
 * resource too: the engine's reset (set_material with no resource, vtable +0x200 with 0) gives the slot an empty
 * material, which draws as the purple see-through debug look. */
static void SeSetMaterialOnUnit(uint32_t unit, uint32_t material, uint64_t value)
{
    if (g_seSetMaterial != NULL && value) g_seSetMaterial(unit, material, value);
}

/* Every drawn material of the unit with this material slot gets the texture; texture slot 0 swaps the material. */
static void SeSetOnUnit(uint32_t unit, uint32_t material, uint32_t texture, uint64_t value)
{
    uintptr_t mats[256];
    if (texture == 0) { SeSetMaterialOnUnit(unit, material, value); return; }
    uint32_t n = SeSlotMaterials(unit, material, mats, 256);
    for (uint32_t k = 0; k < n; k++) g_seSetTexture((void*)mats[k], texture, value);
}

/* One texture slot brought to what the target wants; a texture already shown is never set again. */
static void SeShowOne(SeTarget* t, const HfEffectLook* row)
{
    uint32_t effect = 0;
    uint64_t want = SeWanted(t, row->material_slot, row->texture_slot, &effect);
    SeShown* s = SeShownOf(t, row->material_slot, row->texture_slot, want != 0);
    if (s == NULL || s->shown == want) return;
    if (s->shown == 0) s->own = row->texture_slot ? row->own : SeMaterialResourceOf(t->unit, row->material_slot);
    if (row->texture_slot == 0 && s->own == 0) return; /* a material look it could not undo is never shown */
    SeSetOnUnit(t->unit, row->material_slot, row->texture_slot, want ? want : s->own);
    s->shown = want;
    s->effect = effect;
}

/* Every look row of the target's unit, in every effect it carries, and every texture it shows now. An effect whose
 * textures are not resident yet leaves the target to be shown again next update. */
static void SeShow(SeTarget* t)
{
    int waiting = 0;
    for (int i = 0; i < SE_SLOTS; i++)
    {
        const SeEffect* e = t->slots[i].effect ? &g_se[t->slots[i].effect - 1] : NULL;
        if (e != NULL && t->slots[i].level && e->lookCount && !SeResident(e)) waiting = 1;
        for (uint32_t r = 0; e != NULL && r < e->lookCount; r++) if (e->looks[r].unit == t->name) SeShowOne(t, &e->looks[r]);
    }
    for (uint32_t i = 0; i < t->shownCount; i++)
    {
        SeShown* s = &t->shown[i];
        if (s->shown == 0) continue;
        HfEffectLook own = {t->name, s->material, s->texture, s->own, {0}};
        SeShowOne(t, &own);
    }
    t->dirty = waiting;
}

/* Its own textures back, when the unit is still alive and still the same resource, then forgotten. */
static void SeForget(SeTarget* t)
{
    int same = t->unit && (g_seAlive(t->unit) & 1) && g_seUnitName(t->unit) == t->name;
    for (uint32_t i = 0; same && i < t->shownCount; i++)
        if (t->shown[i].shown) SeSetOnUnit(t->unit, t->shown[i].material, t->shown[i].texture, t->shown[i].own);
    memset(t, 0, sizeof *t);
}

/* ---- meters and levels ---- */

static void SeShare(const SeEffect* e, const SeTarget* t, uint32_t level)
{
    uint32_t unit = 0, network = WC_NO_NETWORK;
    if (!(e->def.flags & HF_EFFECT_SHARED_LOOK) || !WcEntity(t->entity, &unit, &network) || network == WC_NO_NETWORK) return;
    ULONGLONG now = GetTickCount64();
    if (now - g_seRefilled >= 1000) { g_seTokens = CHANNEL_RATE; g_seRefilled = now; }
    if (g_seTokens == 0) return;
    g_seTokens--;
    SeWire wire = {SE_WIRE_VERSION, (uint8_t)level, 0, network, e->key};
    char hex[sizeof wire * 2 + 1], line[64 + sizeof hex];
    ChannelHex((const uint8_t*)&wire, sizeof wire, hex);
    snprintf(line, sizeof line, "send\t%s\tall\t%s", SE_CHANNEL, hex);
    LiveQueue(line);
}

static void SeLevelChanged(SeTarget* t, SeSlot* s, uint32_t level)
{
    const SeEffect* e = &g_se[s->effect - 1];
    uint32_t from = s->level;
    s->level = level;
    s->levelSince = g_seClock;
    t->dirty = 1;
    if (level && (e->def.levels[level - 1].flags & HF_EFFECT_SPEND)) s->meter = 0;
    if (!s->remote) SeShare(e, t, level);
    if (e->def.on_level && !ModGone(e->mod)) e->def.on_level(e->mod, s->effect, t->entity, from, level, s->remote, e->def.user);
}

/* The level the meter makes now: up as soon as an enter is reached; down one level an update, only below the level's
 * leave and after its hold, so each level on the way down shows for its own hold (a thawing freeze keeps its rime). */
static uint32_t SeLevelFor(const SeEffect* e, const SeSlot* s)
{
    uint32_t level = s->level;
    while (level < e->def.level_count && s->meter >= e->def.levels[level].enter) level++;
    if (level > s->level) return level;
    if (level > 0 && g_seClock - s->levelSince >= e->def.levels[level - 1].hold_s && s->meter < e->def.levels[level - 1].leave) level--;
    return level;
}

static int SeSpending(const SeEffect* e, const SeSlot* s)
{
    return s->level && (e->def.levels[s->level - 1].flags & HF_EFFECT_SPEND) && g_seClock - s->levelSince < e->def.levels[s->level - 1].hold_s;
}

static void SeGain(uint32_t entity, uint32_t effect, float amount)
{
    const SeEffect* e = &g_se[effect - 1];
    SeTarget* t = SeTargetOf(entity, amount > 0);
    SeSlot* s = t ? SeSlotOf(t, effect, amount > 0) : NULL;
    if (s == NULL || s->remote || ModGone(e->mod) || SeSpending(e, s)) return;
    s->meter = s->meter + amount > e->def.full ? e->def.full : s->meter + amount < 0 ? 0 : s->meter + amount;
    if (amount > 0) s->lastGain = g_seClock;
}

/* One entity's notes of one update: each feed counts the buildup of its status, if its with_status came too. */
static float SeFeedSum(const HfEffectFeed* feed, const SeNote* notes, uint32_t count)
{
    float sum = 0;
    int with = feed->with_status == 0;
    for (uint32_t n = 0; n < count; n++)
    {
        if (notes[n].status == feed->status) sum += notes[n].amount;
        if (notes[n].status == feed->with_status) with = 1;
    }
    return with ? sum * feed->scale : 0;
}

static void SeFeedGroup(const SeNote* notes, uint32_t count)
{
    for (LONG i = 0; i < g_seCount; i++)
        for (uint32_t f = 0; f < g_se[i].def.feed_count; f++)
        {
            float gain = SeFeedSum(&g_se[i].def.feeds[f], notes, count);
            if (gain > 0) SeGain(notes[0].entity, (uint32_t)i + 1, gain);
        }
}

/* The queue, in groups of one entity and one update (the detour notes them in order, so a group is contiguous
 * unless another entity's status lands in between; those are rare and only split a group). */
static void SeTakeNotes(void)
{
    AcquireSRWLockExclusive(&g_seLock);
    uint32_t n = g_seQueued;
    memcpy(g_seTaken, g_seQueue, sizeof g_seQueue[0] * n);
    g_seQueued = 0;
    g_seUpdate++;
    ReleaseSRWLockExclusive(&g_seLock);
    for (uint32_t i = 0; i < n;)
    {
        if (g_seTaken[i].effect) { SeGain(g_seTaken[i].entity, g_seTaken[i].effect, g_seTaken[i].amount); i++; continue; }
        uint32_t end = i + 1;
        while (end < n && !g_seTaken[end].effect && g_seTaken[end].entity == g_seTaken[i].entity && g_seTaken[end].update == g_seTaken[i].update) end++;
        SeFeedGroup(&g_seTaken[i], end - i);
        i = end;
    }
    LONG dropped = InterlockedExchange(&g_seDropped, 0);
    if (dropped) Note("status effects: %ld note(s) past %d in an update were dropped", dropped, SE_QUEUE);
}

static void SeDecay(const SeEffect* e, SeSlot* s, float dt)
{
    if (s->remote || g_seClock - s->lastGain < e->def.hold_s) return;
    s->meter -= e->def.decay_per_s * dt;
    if (e->def.half_life_s > 0) s->meter *= (float)pow(0.5, dt / e->def.half_life_s);
    if (s->meter < 1e-4f) s->meter = 0;
}

/* One entity's effects for this update; 0 when nothing is left of them. */
static int SeTargetTick(SeTarget* t, float dt)
{
    int left = 0;
    for (int i = 0; i < SE_SLOTS; i++)
    {
        SeSlot* s = &t->slots[i];
        if (s->effect == 0) continue;
        const SeEffect* e = &g_se[s->effect - 1];
        if (ModGone(e->mod)) { s->effect = 0; t->dirty = 1; continue; }
        SeDecay(e, s, dt);
        uint32_t level = s->remote ? s->level : SeLevelFor(e, s);
        if (level != s->level) SeLevelChanged(t, s, level);
        if (s->level == 0 && s->meter <= 0) { s->effect = 0; t->dirty = 1; continue; }
        left = 1;
    }
    for (uint32_t i = 0; i < t->shownCount && !left; i++) left = t->shown[i].shown != 0;
    return left;
}

static float SeDamageScale(const SeTarget* t)
{
    float scale = 1;
    for (int i = 0; i < SE_SLOTS; i++)
    {
        const SeSlot* s = &t->slots[i];
        float d = s->effect && s->level && !s->remote ? g_se[s->effect - 1].def.levels[s->level - 1].damage_taken : 0;
        if (d > 0) scale *= d;
    }
    return scale;
}

/* ---- the other machines' levels ---- */

static uint32_t SeEffectOfKey(uint32_t key)
{
    for (LONG k = 0; k < g_seCount; k++) if (g_se[k].key == key && !ModGone(g_se[k].mod)) return (uint32_t)k + 1;
    return 0;
}

static void SeRemoteLevel(const SeWire* wire)
{
    uint32_t effect = SeEffectOfKey(wire->key), entity = ApiEntityOfNetwork(NULL, wire->network);
    if (!effect || !entity || wire->level > g_se[effect - 1].def.level_count) return;
    SeTarget* t = SeTargetOf(entity, wire->level > 0);
    SeSlot* s = t ? SeSlotOf(t, effect, wire->level > 0) : NULL;
    if (s == NULL) return;
    s->remote = 1;
    if (s->level != wire->level) SeLevelChanged(t, s, wire->level);
}

static void SeTakeInbox(void)
{
    SeWire inbox[64];
    AcquireSRWLockExclusive(&g_seLock);
    uint32_t n = g_seInboxCount;
    memcpy(inbox, g_seInbox, sizeof inbox[0] * n);
    g_seInboxCount = 0;
    ReleaseSRWLockExclusive(&g_seLock);
    for (uint32_t i = 0; i < n; i++) SeRemoteLevel(&inbox[i]);
}

/* The other machines' level changes, as the live pipe delivers them (its thread): queued for the game thread. */
static void SeReceive(uint64_t from, const uint8_t* data, uint32_t bytes)
{
    (void)from;
    SeWire wire;
    if (bytes != sizeof wire) return;
    memcpy(&wire, data, sizeof wire);
    if (wire.version != SE_WIRE_VERSION) return;
    AcquireSRWLockExclusive(&g_seLock);
    if (g_seInboxCount < 64) g_seInbox[g_seInboxCount++] = wire;
    ReleaseSRWLockExclusive(&g_seLock);
}

/* ---- the game thread ---- */

static int SeReady(void)
{
    if (g_seState != 0) return g_seState > 0;
    if (!g_fired[HF_EVENT_GAME_LOADED]) return 0;
    g_seDescriptor = (SeDescriptorFn)ApiFunction(&g_seMod, "EntityDescriptor");
    g_seAlive = (SeAliveFn)ApiFunction(&g_seMod, "UnitAlive");
    g_seNumMeshes = (SeNumMeshesFn)ApiFunction(&g_seMod, "UnitNumMeshes");
    g_seMesh = (SeMeshFn)ApiFunction(&g_seMod, "UnitMesh");
    g_seUnitName = (SeUnitNameFn)ApiFunction(&g_seMod, "UnitResourceName");
    g_seSetTexture = (SeSetTextureFn)ApiFunction(&g_seMod, "MaterialSetTexture");
    g_seSetMaterial = (SeSetMaterialFn)ApiFunction(&g_seMod, "UnitSetMaterial");
    if (!g_seSetMaterial) Note("status effects: material looks are off; UnitSetMaterial is not the reviewed one");
    if (!g_seDescriptor || !g_seAlive || !g_seNumMeshes || !g_seMesh || !g_seUnitName || !g_seSetTexture)
    { g_seState = -1; Note("status effects: off; a unit or material call is not the reviewed one"); return 0; }
    g_seState = 1;
    return 1;
}

static void SeHook(void)
{
    if (g_seHooked || !SeFeedsAny()) return;
    g_seHooked = 1;
    if (ApiHookFunction(&g_seMod, "ApplyStatus", (void*)SeApplyStatusDetour, (void**)&g_seApplyStatus, 0))
        Note("status effects: ApplyStatus could not be hooked; only add_effect_buildup fills the meters");
}

static void SeTick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!SeReady()) return;
    SeHook();
    AcquireSRWLockExclusive(&g_seTargetLock);
    InterlockedExchange(&g_seInTick, 1);
    g_seClock += dt;
    SeTakeNotes();
    SeTakeInbox();
    LONG damage = 0;
    for (int i = 0; i < SE_TARGETS; i++)
    {
        SeTarget* t = &g_seTargets[i];
        if (t->entity == 0) continue;
        if (SeUnitOf(t->entity) != t->unit) { SeForget(t); continue; }
        if (!SeTargetTick(t, dt)) { if (t->dirty) SeShow(t); memset(t, 0, sizeof *t); continue; }
        if (t->dirty) SeShow(t);
        float scale = SeDamageScale(t);
        if (scale != 1) g_seDamage[damage++] = (SeDamageRow){t->entity, scale};
    }
    InterlockedExchange(&g_seDamageCount, damage);
    InterlockedExchange(&g_seInTick, 0);
    ReleaseSRWLockExclusive(&g_seTargetLock);
}

/* The update's own thread already holds the targets (a callback asking); any other waits for the update to end. */
static int SeOnTickThread(void) { return g_seInTick && g_gameThread != 0 && GetCurrentThreadId() == g_gameThread; }

/* For the damage consumer (event_sources.c): an entity's damage scaled by its effects' levels, on this machine. */
static int SeWantsDamage(void)
{
    for (LONG i = 0; i < g_seCount; i++)
        for (uint32_t l = 0; l < g_se[i].def.level_count; l++) if (g_se[i].def.levels[l].damage_taken > 0) return 1;
    return 0;
}

static void SeDamage(uint8_t* event)
{
    uint32_t entity, kind;
    int32_t amount;
    memcpy(&entity, event, 4); memcpy(&kind, event + 4, 4); memcpy(&amount, event + 0x2C, 4);
    if (amount <= 0 || kind > 7) return; /* -1 kills; drowning and bleeding out are not hits */
    for (LONG i = 0; i < g_seDamageCount; i++)
        if (g_seDamage[i].entity == entity)
        {
            float scaled = (float)amount * g_seDamage[i].scale + 0.5f;
            int32_t after = scaled >= 2147483520.0f ? INT32_MAX : (int32_t)scaled;
            memcpy(event + 0x2C, &after, 4);
            return;
        }
}

/* ---- the API ---- */

static int SeIn(float v, float low, float high) { return v >= low && v <= high; } /* false for NaN too */

static const char* SeLevelRefusal(const HfStatusEffect* d, uint32_t i)
{
    const HfEffectLevel* l = &d->levels[i];
    if (!(l->enter > 0) || !SeIn(l->leave, -1e30f, l->enter) || l->enter > d->full || (i && !(l->enter > d->levels[i - 1].enter)))
        return "each level's enter above 0 and the level before, at most full, and leave at most enter";
    if (!SeIn(l->hold_s, 0, 600) || (l->damage_taken != 0 && !SeIn(l->damage_taken, 0.1f, 10))) return "a level's hold_s 0 .. 600 and damage_taken 0 or 0.1 .. 10";
    return NULL;
}

static const char* SeFeedRefusal(const HfEffectFeed* f)
{
    return f->status && SeIn(f->scale, 0.001f, 1000) ? NULL : "a feed names a status and a scale of 0.001 .. 1000";
}

/* Why a definition is refused, or NULL. */
static const char* SeRefusal(const HfStatusEffect* d)
{
    const char* why = NULL;
    if (d == NULL || d->size < sizeof *d) return "no HfStatusEffect, or its size is not set";
    if (d->name == NULL || !d->name[0] || strlen(d->name) >= sizeof g_se[0].name) return "a name of 1 to 63 bytes";
    if (!SeIn(d->full, 0.01f, 100000)) return "full is 0.01 .. 100000";
    if (!SeIn(d->hold_s, 0, 600) || !SeIn(d->decay_per_s, 0, 1e30f) || !SeIn(d->half_life_s, 0, 1e30f)) return "hold_s 0 .. 600, decay_per_s and half_life_s 0 or more";
    if (d->level_count < 1 || d->level_count > HF_EFFECT_LEVELS || d->feed_count > HF_EFFECT_FEEDS) return "1 .. 3 levels and at most 8 feeds";
    for (uint32_t i = 0; i < d->level_count && !why; i++) why = SeLevelRefusal(d, i);
    for (uint32_t i = 0; i < d->feed_count && !why; i++) why = SeFeedRefusal(&d->feeds[i]);
    return why;
}

static void SeStartTick(void)
{
    if (InterlockedExchange(&g_seTicking, 1) != 0) return;
    g_seMod.api = &g_api;
    snprintf(g_seMod.id, sizeof g_seMod.id, "hellforge.status-effects");
    g_seMod.log = g_log;
    g_seMod.loaded = 1;
    ApiOnGameThread(&g_seMod, SeTick, NULL);
}

static uint32_t ApiAddStatusEffect(HfMod* mod, const HfStatusEffect* effect)
{
    const char* why = SeRefusal(effect);
    if (why) { ApiLog(mod, "add_status_effect: refused: %s", why); return 0; }
    char key[160];
    snprintf(key, sizeof key, "%s/%s", mod->id, effect->name);
    uint32_t handle = 0, hash = (uint32_t)(Murmur64A(key, strlen(key)) >> 32);
    AcquireSRWLockExclusive(&g_seLock);
    for (LONG i = 0; i < g_seCount && !why; i++) if (g_se[i].key == hash && !ModGone(g_se[i].mod)) why = "the mod has an effect of that name";
    if (!why && g_seCount >= SE_EFFECTS) why = "64 effects in all";
    if (!why)
    {
        SeEffect* e = &g_se[g_seCount];
        *e = (SeEffect){mod, *effect, {0}, hash, NULL, 0, 0};
        snprintf(e->name, sizeof e->name, "%s", effect->name);
        e->def.name = e->name;
        handle = (uint32_t)InterlockedIncrement(&g_seCount);
    }
    ReleaseSRWLockExclusive(&g_seLock);
    if (why) { ApiLog(mod, "add_status_effect(%s): refused: %s", effect->name, why); return 0; }
    SeStartTick();
    ApiLog(mod, "status effect %s: handle %u, %u level(s), %u feed(s)", effect->name, handle, effect->level_count, effect->feed_count);
    return handle;
}

static SeEffect* SeOwn(HfMod* mod, uint32_t effect, const char* call)
{
    if (effect >= 1 && effect <= (uint32_t)g_seCount && g_se[effect - 1].mod == mod) return &g_se[effect - 1];
    ApiLog(mod, "%s: refused: %u is not one of the mod's status effects", call, effect);
    return NULL;
}

static int ApiAddEffectLooks(HfMod* mod, uint32_t effect, const HfEffectLook* rows, uint32_t count, uint64_t package)
{
    SeEffect* e = SeOwn(mod, effect, "add_effect_looks");
    if (e == NULL) return 1;
    if (rows == NULL || count == 0 || count > 65536) { ApiLog(mod, "add_effect_looks: refused: 1 .. 65536 rows"); return 1; }
    if (SeOnTickThread()) { ApiLog(mod, "add_effect_looks: refused: not from an on_level callback (call it from hellforge_mod_init)"); return 1; }
    AcquireSRWLockExclusive(&g_seTargetLock); /* lookCount read under it: two calls at once would size from a stale count */
    HfEffectLook* all = malloc(sizeof *all * (e->lookCount + count));
    if (all == NULL) { ReleaseSRWLockExclusive(&g_seTargetLock); ApiLog(mod, "add_effect_looks: out of memory"); return 1; }
    if (e->lookCount) memcpy(all, e->looks, sizeof *all * e->lookCount);
    memcpy(all + e->lookCount, rows, sizeof *all * count);
    HfEffectLook* old = e->looks;
    e->looks = all;
    e->lookCount += count;
    if (package) e->package = package;
    ReleaseSRWLockExclusive(&g_seTargetLock);
    free(old);
    ApiLog(mod, "status effect %s: %u look row(s)", e->name, e->lookCount);
    return 0;
}

static int ApiAddEffectBuildup(HfMod* mod, uint32_t effect, uint32_t entity, float amount)
{
    if (SeOwn(mod, effect, "add_effect_buildup") == NULL) return 1;
    if (entity == 0 || !(amount == amount) || amount == 0) { ApiLog(mod, "add_effect_buildup: refused: an entity and an amount other than 0"); return 1; }
    SeQueue((SeNote){entity, 0, effect, amount, g_seUpdate});
    return 0;
}

static uint32_t ApiEffectState(HfMod* mod, uint32_t effect, uint32_t entity, HfEffectState* state)
{
    (void)mod;
    HfEffectState found = {sizeof found, 0, 0, 0};
    int lock = !SeOnTickThread();
    if (lock) AcquireSRWLockShared(&g_seTargetLock);
    for (int i = 0; i < SE_TARGETS && effect; i++)
    {
        if (g_seTargets[i].entity != entity) continue;
        for (int k = 0; k < SE_SLOTS; k++)
            if (g_seTargets[i].slots[k].effect == effect)
                found = (HfEffectState){sizeof found, g_seTargets[i].slots[k].level, g_seTargets[i].slots[k].meter, (float)(g_seClock - g_seTargets[i].slots[k].lastGain)};
    }
    if (lock) ReleaseSRWLockShared(&g_seTargetLock);
    if (state != NULL && state->size >= sizeof *state) *state = found;
    return found.level;
}
