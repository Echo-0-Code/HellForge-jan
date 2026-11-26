/* Co-op for walkers, drivers, rail and tracked movers (docs/modding/movers.md "Read how it moves"). Their root is teleported on the
 * machine that moves them, and the game sends none of it on, so that machine sends each one's world pose about 10 times
 * a second under "@hellforge.movers"; the others write it to their copy's root, smoothed, in their own parent frame.
 * A copy is matched by network id, or (none yet) by the nearest unclaimed unit of the same mover to where it started.
 * Hovers, fliers, vessels and units driven through their body are physics bodies the engine replicates. Included by movers.c after MvSetRoot. */

#define MV_SYNC_CHANNEL "@hellforge.movers" /* no mod can have it: mod ids start with a letter or digit */
#define MV_SYNC_S 0.1          /* how often the poses are sent */
#define MV_SYNC_VERSION 1u
#define MV_SYNC_PER 13u        /* records in one message (HF_MESSAGE_MAX) */
#define MV_SYNC_MATCH 60.0f    /* metres: the farthest a copy's start may be from the sender's to be matched by it */
#define MV_SYNC_SNAP 15.0f     /* metres: a copy further off than this jumps instead of gliding */
#define MV_SYNC_AHEAD 0.3f     /* seconds: the most a pose is carried forward along its speed */

typedef struct MvWireHead { uint8_t version, count; uint16_t pad; uint32_t pad2; } MvWireHead;
typedef struct MvInbox { uint64_t from; MvWire w; } MvInbox;
_Static_assert(sizeof(MvWireHead) + MV_SYNC_PER * sizeof(MvWire) <= HF_MESSAGE_MAX, "one message holds MV_SYNC_PER poses");

static MvInbox g_mvInbox[MV_UNITS];
static uint32_t g_mvInboxCount;
static SRWLOCK g_mvInboxLock = SRWLOCK_INIT;
static double g_mvSyncWait;

static uint32_t MvRole(void)
{
    AcquireSRWLockShared(&g_sessionLock);
    const uint32_t role = g_session.role;
    ReleaseSRWLockShared(&g_sessionLock);
    return role;
}

static void MvSyncFlush(const uint8_t* data, uint32_t bytes)
{
    char hex[HF_MESSAGE_MAX * 2 + 1], line[64 + sizeof hex];
    ChannelHex(data, bytes, hex);
    snprintf(line, sizeof line, "send\t%s\tall\t%s", MV_SYNC_CHANNEL, hex);
    LiveQueue(line);
}

/* The poses of the movers this machine moves, for the others: one message an interval (the lobby takes 64 a second
 * from a machine, mods' and effects' among them), taking turns past MV_SYNC_PER. Game thread, g_mvLock held. */
static void MvSyncSend(double dt)
{
    static int next;
    if ((g_mvSyncWait += dt) < MV_SYNC_S || MvRole() == HF_SESSION_SOLO) return;
    g_mvSyncWait = 0;
    uint64_t buf[(sizeof(MvWireHead) + MV_SYNC_PER * sizeof(MvWire)) / 8]; /* aligned for MvWire */
    MvWireHead* head = (MvWireHead*)(void*)buf;
    MvWire* rec = (MvWire*)(void*)(head + 1);
    memset(head, 0, sizeof *head);
    head->version = MV_SYNC_VERSION;
    int k = 0;
    for (; k < MV_UNITS && head->count < MV_SYNC_PER; k++)
    {
        const MvUnit* m = &g_mvUnits[(next + k) % MV_UNITS];
        if (!m->entity || m->remote || !m->started || m->physics || MvPhysical(g_mvDefs[m->def - 1].def.mode)) continue;
        uint32_t unit = 0, network = WC_NO_NETWORK;
        WcEntity(m->entity, &unit, &network);
        rec[head->count++] = (MvWire){g_mvDefs[m->def - 1].def.entity, m->entity, network, (uint8_t)m->status, (uint8_t)m->driver,
            (uint8_t)m->driven, (uint8_t)m->boosting, {m->pos[0], m->pos[1], m->pos[2]}, {m->start[0], m->start[1], m->start[2]}, m->yaw,
            m->pitch, m->roll, m->speed, m->turning, m->grade, 0};
    }
    next = (next + k) % MV_UNITS;
    if (head->count) MvSyncFlush((const uint8_t*)buf, (uint32_t)(sizeof *head + head->count * sizeof(MvWire)));
}

/* The other machines' poses, as the live pipe delivers them (its thread): the newest per sender's unit is kept. */
static int MvWireFinite(const MvWire* w)
{
    float f[12]; /* pos, start, yaw, pitch, roll, speed, turning, grade: floats in a row */
    _Static_assert(offsetof(MvWire, grade) - offsetof(MvWire, pos) == 11 * sizeof(float), "MvWire floats in a row");
    memcpy(f, w->pos, sizeof f);
    for (int k = 0; k < 12; k++)
        if (!isfinite(f[k])) return 0;
    return 1;
}

static void MvSyncReceive(uint64_t from, const uint8_t* data, uint32_t bytes)
{
    MvWireHead head;
    if (bytes < sizeof head) return;
    memcpy(&head, data, sizeof head);
    if (head.version != MV_SYNC_VERSION || bytes != sizeof head + head.count * sizeof(MvWire)) return;
    AcquireSRWLockExclusive(&g_mvInboxLock);
    for (uint32_t k = 0; k < head.count; k++)
    {
        MvWire w;
        memcpy(&w, data + sizeof head + k * sizeof w, sizeof w);
        if (!MvWireFinite(&w)) continue; /* a peer's NaN pose would spread through MvSetRoot */
        uint32_t i = 0;
        while (i < g_mvInboxCount && (g_mvInbox[i].from != from || g_mvInbox[i].w.entity != w.entity)) i++;
        if (i == g_mvInboxCount && g_mvInboxCount < MV_UNITS) g_mvInboxCount++;
        if (i < g_mvInboxCount) g_mvInbox[i] = (MvInbox){from, w};
    }
    ReleaseSRWLockExclusive(&g_mvInboxLock);
}

/* The unclaimed copy of the same mover that started nearest to where the sender's did, within MV_SYNC_MATCH. */
static MvUnit* MvSyncNearest(const MvWire* w)
{
    MvUnit* best = NULL;
    float bestD = MV_SYNC_MATCH * MV_SYNC_MATCH;
    for (int i = 0; i < MV_UNITS; i++)
    {
        MvUnit* m = &g_mvUnits[i];
        if (!m->entity || !m->remote || m->peerEntity || !m->started || g_mvDefs[m->def - 1].def.entity != w->resource) continue;
        const float dx = m->start[0] - w->start[0], dy = m->start[1] - w->start[1], dz = m->start[2] - w->start[2], d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) { bestD = d; best = m; }
    }
    return best;
}

/* This machine's copy of the sender's unit: claimed before, its network id's, or the nearest start's. */
static MvUnit* MvSyncCopy(const MvInbox* in)
{
    const uint32_t local = in->w.network != WC_NO_NETWORK ? ApiEntityOfNetwork(NULL, in->w.network) : 0xFFFFFFFFu;
    MvUnit* byNetwork = NULL;
    for (int i = 0; i < MV_UNITS; i++)
    {
        MvUnit* m = &g_mvUnits[i];
        if (!m->entity || !m->remote) continue;
        if (m->peerFrom == in->from && m->peerEntity == in->w.entity) return m;
        if (m->entity == local) byNetwork = m;
    }
    MvUnit* m = byNetwork != NULL ? byNetwork : MvSyncNearest(&in->w);
    if (m == NULL) return NULL;
    m->peerFrom = in->from; m->peerEntity = in->w.entity;
    ApiLog(g_mvDefs[m->def - 1].mod, "mover: unit %u follows unit %u of machine %016llx (%s)", m->entity, in->w.entity,
        (unsigned long long)in->from, byNetwork != NULL ? "its network id" : "the nearest start");
    return m;
}

/* The poses received since the last update, onto their copies. Game thread, g_mvLock held. */
static void MvSyncTake(void)
{
    MvInbox in[MV_UNITS];
    AcquireSRWLockExclusive(&g_mvInboxLock);
    const uint32_t n = g_mvInboxCount;
    memcpy(in, g_mvInbox, sizeof in[0] * n);
    g_mvInboxCount = 0;
    ReleaseSRWLockExclusive(&g_mvInboxLock);
    for (uint32_t k = 0; k < n; k++)
    {
        MvUnit* m = MvSyncCopy(&in[k]);
        if (m == NULL) continue;
        m->sync = in[k].w; m->syncAge = 0; m->synced = 1;
    }
}

/* One update of another machine's mover: its last pose carried forward along its speed, glided toward. */
static void MvSyncTick(MvUnit* m, float dt)
{
    m->status = HF_MOVER_REMOTE;
    if (!m->started) { if (!MvParent(m)) return; m->started = 1; memcpy(m->start, m->pos, 12); }
    if ((m->sinceSolid += dt) >= MV_SOLID_S) { m->sinceSolid = 0; float w[16]; if (!MvFrame(m, w)) { m->started = 0; return; } }
    if (!m->synced) return;
    const MvWire* s = &m->sync;
    const float ahead = (float)fmin(m->syncAge += dt, MV_SYNC_AHEAD) * s->speed;
    const float goal[3] = {s->pos[0] - sinf(s->yaw) * ahead, s->pos[1] + cosf(s->yaw) * ahead, s->pos[2]};
    const float d[3] = {goal[0] - m->pos[0], goal[1] - m->pos[1], goal[2] - m->pos[2]};
    const float k = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > MV_SYNC_SNAP * MV_SYNC_SNAP ? 1.0f : 1.0f - expf(-8.0f * dt);
    for (int i = 0; i < 3; i++) m->pos[i] += d[i] * k;
    m->yaw = MvWrap(m->yaw + MvWrap(s->yaw + s->turning * (float)fmin(m->syncAge, MV_SYNC_AHEAD) - m->yaw) * k);
    m->pitch += (s->pitch - m->pitch) * k; m->roll += (s->roll - m->roll) * k;
    m->speed = s->speed; m->turning = s->turning; m->grade = s->grade;
    m->driver = s->driver; m->driven = s->driven; m->boosting = s->boosting;
    MvSetRoot(m);
}
