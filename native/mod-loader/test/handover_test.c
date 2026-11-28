/* What follows the machine that simulates an entity, which the game changes with no new spawn (a host leaves): a
 * walker's mover (movers.c MvWhoMoves), a damage zone's count (movers_kit.c ZnCount), a status effect's level another
 * machine sent (status_effects.c SeAdopt). And a turret behaviour's slot that outlived its turret (behaviour_turret.c
 * MindOf). On channel_test.c's synthetic game.dll: entity 42 exists, 7 does not. Included by stats_test.c. */

static void HandoverSet(uint32_t flags) { memcpy(g_channelAuthority + 0x14, &flags, 4); }

static void HandoverMoverFixture(void)
{
    MvDef walker = {.mod = &g_mods[0], .def = {.mode = HF_MOVE_WALK}}, hover = {.mod = &g_mods[0], .def = {.mode = HF_MOVE_HOVER}};
    MvUnit m = {.entity = 42, .remote = 1, .synced = 1, .peerEntity = 9, .peerFrom = 5, .throttle = 1};
    /* handed to this machine: no longer a copy, its sender forgotten, nobody's throttle carried over */
    HandoverSet(1);
    MvWhoMoves(&m, &walker);
    assert(!m.remote && !m.synced && !m.peerEntity && !m.peerFrom && m.throttle == 0);
    m.throttle = 1;
    MvWhoMoves(&m, &walker);
    assert(!m.remote && m.throttle == 1); /* nothing changed: left alone */
    /* handed away: a copy that waits for its new sender */
    HandoverSet(0);
    MvWhoMoves(&m, &walker);
    assert(m.remote && !m.synced && !m.peerEntity);
    /* an entity the game does not know (or calls not reviewed): as it was */
    m.entity = 7;
    m.remote = 0;
    MvWhoMoves(&m, &walker);
    assert(!m.remote);
    /* a physics body is the engine's to copy: never remote */
    m.entity = 42;
    MvWhoMoves(&m, &hover);
    assert(!m.remote);
}

static void HandoverZoneFixture(void)
{
    KitZone zone = {.mod = &g_mods[0], .def = {.health = 100}, .slot = 2};
    MvUnit m = {.entity = 42};
    m.znHealth[2] = 100;
    HandoverSet(0); /* the hit was computed here, the craft is flown elsewhere: no count, no break */
    ZnCount(&m, &zone, 150);
    assert(m.znHealth[2] == 100);
    HandoverSet(1);
    ZnCount(&m, &zone, 30);
    assert(m.znHealth[2] == 70);
    ZnCount(&m, &zone, 150);
    assert(m.znHealth[2] == 0);
}

static void HandoverEffectFixture(void)
{
    SeEffect effect = {.def = {.full = 10, .level_count = 2, .levels = {{.enter = 3, .leave = 2}, {.enter = 6, .leave = 5, .flags = HF_EFFECT_SPEND}}}};
    SeTarget target = {.entity = 42};
    SeSlot* s = &target.slots[0];
    *s = (SeSlot){.effect = 1, .level = 1, .remote = 1};
    const double clock = g_seClock;
    g_seClock = 50;
    HandoverSet(0); /* simulated elsewhere: the sender's level stays the sender's */
    SeAdopt(&effect, &target, s);
    assert(s->remote && s->meter == 0);
    /* simulated here: the slot is this machine's, its meter where the level starts so that it holds, then decays */
    HandoverSet(1);
    SeAdopt(&effect, &target, s);
    assert(!s->remote && s->meter == 3 && s->lastGain == 50 && SeLevelFor(&effect, s) == 1);
    /* a level that spends the meter keeps it empty: it ends with its hold */
    *s = (SeSlot){.effect = 1, .level = 2, .remote = 1};
    SeAdopt(&effect, &target, s);
    assert(!s->remote && s->meter == 0);
    /* its own slot is left as it is */
    *s = (SeSlot){.effect = 1, .level = 1, .meter = 4, .lastGain = 7};
    SeAdopt(&effect, &target, s);
    assert(s->meter == 4 && s->lastGain == 7);
    g_seClock = clock;
}

static void HandoverMindFixture(void)
{
    TurretMind kept[T_MINDS];
    memcpy(kept, g_minds, sizeof kept);
    memset(g_minds, 0, sizeof g_minds);
    const uint64_t t0 = 1000000000ull;
    TurretMind* m = MindOf(77, t0);
    m->reserve = 5; m->reserveSet = 1;
    /* the same turret an update later, and after a pause shorter than the limit: its reserve as it spent it */
    assert(MindOf(77, t0 + 16000) == m && m->reserveSet && m->reserve == 5);
    assert(MindOf(77, t0 + 16000 + T_MIND_STALE) == m && m->reserveSet);
    /* the id again long after: another turret, which starts with a full reserve */
    m = MindOf(77, t0 + 16000 + 2 * T_MIND_STALE + 1);
    assert(m->turret == 77 && !m->reserveSet && m->reserve == 0);
    /* the clock behind the slot's last use (it started again): the same */
    m->reserve = 3; m->reserveSet = 1;
    m = MindOf(77, 5);
    assert(m->turret == 77 && !m->reserveSet && m->used == 5);
    memcpy(g_minds, kept, sizeof kept);
}

static void HandoverFixture(void)
{
    ChannelFakeGame fake = ChannelFakeGameStart();
    HandoverMoverFixture();
    HandoverZoneFixture();
    HandoverEffectFixture();
    ChannelFakeGameEnd(&fake);
    HandoverSet(0);
    HandoverMindFixture();
}
