/* SDK 28 (hellforge_world.h): on_render, pose_joint and joint_pose, for helldivers2.exe build 25480438. A joint that
 * follows another (a turret head, a suit's limb on its wearer's bone) must be set after the game has animated every
 * unit and before the frame renders, or it trails a frame behind: that is the engine's script render step, which the
 * terminal camera hooks for the same reason (terminal_camera.c, whose pins this file shares). The poses are set with
 * the engine's set_local_pose, and each unit touched is updated once, after every callback. Included by world_calls.c. */

#define JP_HAS_NODE 0x1FE590u
#define JP_NODE 0x1FE5E0u
#define JP_ALIVE 0x1FEEB0u
#define JP_WORLD_POSE 0x1FD220u
#define JP_LOCAL_POSE 0x1FC520u
#define JP_CALLBACKS 32
#define JP_QUEUE 1024
#define JP_TOUCHED 512

static const ActPin kJpPins[] = {
    {1, JP_HAS_NODE, "40534883ec208bdae823f3e9ff488bc84c8b0041ff90e800000033c9448b4010"},
    {1, JP_NODE, "40534883ec208bdae8d3f2e9ff488bc84c8b0041ff90e8000000488bd033c044"},
    {1, JP_ALIVE, "4883ec28e807eae9ff4885c00f95c04883c428c3"},
    {1, JP_WORLD_POSE, "40534883ec204863dae89206eaff488bc84c8b0041ff90e8000000488bcb48c1"},
    {1, JP_LOCAL_POSE, "40534883ec204863dae89213eaff488bc84c8b0041ff90e8000000488bcb48c1"},
};
typedef uint8_t (*JpHasNodeFn)(uint32_t unit, uint32_t name);
typedef uint32_t (*JpNodeFn)(uint32_t unit, uint32_t name);
typedef uint8_t (*JpAliveFn)(uint32_t unit);
typedef const float* (*JpPoseFn)(uint32_t unit, int32_t node);

typedef struct JpCallback { HfMod* mod; HfRenderCallback callback; void* user; } JpCallback;
typedef struct JpQueued { uint32_t unit, node; HfJointPose pose; } JpQueued;
static JpCallback g_jpCallbacks[JP_CALLBACKS];
static volatile LONG g_jpCallbackCount, g_jpWanted, g_jpDropped;
static JpQueued g_jpQueue[JP_QUEUE], g_jpTaken[JP_QUEUE];
static uint32_t g_jpQueued;
static uint32_t g_jpTouched[JP_TOUCHED], g_jpTouchedCount;
static SRWLOCK g_jpLock = SRWLOCK_INIT;
static TcamStepFn g_jpStep;
static int g_jpState;           /* 0 not installed, 1 hooked, -1 off (logged) */
static int g_jpCodeState;       /* the unit calls: 0 not checked, 1 as reviewed, -1 not */
static DWORD g_jpRenderThread;  /* set while the callbacks run, so pose_joint applies at once there */

/* Checked once the game has loaded: before that the exe's code may still be packed. */
static int JpCode(void)
{
    if (g_jpCodeState == 0 && g_fired[HF_EVENT_GAME_LOADED]) g_jpCodeState = ActPins(0, kJpPins, sizeof kJpPins / sizeof kJpPins[0]) ? 1 : -1;
    return g_jpCodeState > 0;
}

/* The node's index on a live unit, or -1. */
static int32_t JpNodeOf(uint32_t unit, uint32_t node)
{
    uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    if (!unit || unit == 0xFFFFFFFFu || !JpCode() || !(((JpAliveFn)(exe + JP_ALIVE))(unit) & 1)) return -1;
    if (node == 0) return 0;
    if (!(((JpHasNodeFn)(exe + JP_HAS_NODE))(unit, node) & 1)) return -1;
    return (int32_t)((JpNodeFn)(exe + JP_NODE))(unit, node);
}

static void JpTouch(uint32_t unit)
{
    for (uint32_t i = 0; i < g_jpTouchedCount; i++) if (g_jpTouched[i] == unit) return;
    if (g_jpTouchedCount < JP_TOUCHED) g_jpTouched[g_jpTouchedCount++] = unit;
}

/* On the render thread: one pose set, its unit marked for the update after the callbacks. */
static int JpApply(uint32_t unit, uint32_t node, const HfJointPose* pose)
{
    int32_t index = JpNodeOf(unit, node);
    if (index < 0) return 0;
    g_tcamSetPose(unit, index, (const float*)pose);
    JpTouch(unit);
    return 1;
}

/* The frame's poses: the queued ones, then the callbacks', then one update of each unit they touched. */
static void JpFrame(void)
{
    AcquireSRWLockExclusive(&g_jpLock);
    uint32_t n = g_jpQueued;
    memcpy(g_jpTaken, g_jpQueue, sizeof g_jpQueue[0] * n);
    g_jpQueued = 0;
    ReleaseSRWLockExclusive(&g_jpLock);
    g_jpTouchedCount = 0;
    for (uint32_t i = 0; i < n; i++) JpApply(g_jpTaken[i].unit, g_jpTaken[i].node, &g_jpTaken[i].pose);
    g_jpRenderThread = GetCurrentThreadId();
    LONG count = g_jpCallbackCount;
    for (LONG i = 0; i < count && i < JP_CALLBACKS; i++)
        if (!ModGone(g_jpCallbacks[i].mod)) g_jpCallbacks[i].callback(g_jpCallbacks[i].mod, g_jpCallbacks[i].user);
    g_jpRenderThread = 0;
    uintptr_t world = g_jpTouchedCount ? g_tcamWorld() : 0;
    for (uint32_t i = 0; i < g_jpTouchedCount && world; i++)
    {
        uintptr_t u = g_tcamUnit(g_jpTouched[i]);
        if (u) g_tcamUpdate(world, u);
    }
    LONG dropped = InterlockedExchange(&g_jpDropped, 0);
    if (dropped) Note("joint poses: %ld pose(s) queued past %d in a frame were dropped", dropped, JP_QUEUE);
}

static void JpStepDetour(void* app)
{
    JpFrame();
    g_jpStep(app);
}

/* The render step hooked once a mod needs it and the game has loaded (its code is only unpacked then). */
static void JointPosesInstall(void)
{
    if (g_jpState != 0 || !g_jpWanted || !g_fired[HF_EVENT_GAME_LOADED]) return;
    g_jpState = -1;
    uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    if (strcmp(g_build, "25480438") || !exe || !JpCode() || !PinMatches(exe + TCAM_RENDER_STEP, kTcamStep, sizeof kTcamStep) ||
        !PinMatches(exe + TCAM_SET_LOCAL_POSE, kTcamSetPose, sizeof kTcamSetPose) ||
        !PinMatches(exe + TCAM_UPDATE_UNIT, kTcamUpdate, sizeof kTcamUpdate) || !PinMatches(exe + TCAM_UNIT, kTcamUnit, sizeof kTcamUnit) ||
        !PinMatches(exe + TCAM_MAIN_WORLD, kTcamWorld, sizeof kTcamWorld))
    { Note("joint poses: off; the engine's unit calls are not the reviewed ones"); return; }
    g_tcamSetPose = (TcamSetPoseFn)(exe + TCAM_SET_LOCAL_POSE);
    g_tcamUpdate = (TcamUpdateFn)(exe + TCAM_UPDATE_UNIT);
    g_tcamUnit = (TcamUnitFn)(exe + TCAM_UNIT);
    g_tcamWorld = (TcamWorldFn)(exe + TCAM_MAIN_WORLD);
    if (LoaderHook("joint poses", exe + TCAM_RENDER_STEP, (void*)JpStepDetour, (void**)&g_jpStep)) { Note("joint poses: off; could not hook the render step"); return; }
    g_jpState = 1;
    Note("joint poses: on; mods' poses are set just before each frame renders");
}

/* SDK 28, on_render: 1 added, 0 refused (logged). */
/* SDK 48, view_pose: where this machine's camera is and looks (terminal_camera.c reads the world's render pose). The
 * fov is not read yet: 0. */
static int ApiViewPose(HfMod* mod, HfView* out)
{
    float p[16];
    if (mod == NULL || out == NULL || out->size < sizeof *out || !TcamView(p)) return 0;
    memcpy(out->right, p, 12); memcpy(out->forward, p + 4, 12); memcpy(out->up, p + 8, 12); memcpy(out->position, p + 12, 12);
    out->fov = 0;
    return 1;
}

static int ApiOnRender(HfMod* mod, HfRenderCallback callback, void* user)
{
    if (callback == NULL || ModGone(mod)) { ApiLog(mod, "on_render: refused: no callback"); return 0; }
    if (g_jpState < 0) { ApiLog(mod, "on_render: refused: joint poses are off (modloader.log says why)"); return 0; }
    AcquireSRWLockExclusive(&g_jpLock);
    LONG at = g_jpCallbackCount;
    if (at < JP_CALLBACKS) { g_jpCallbacks[at] = (JpCallback){mod, callback, user}; InterlockedIncrement(&g_jpCallbackCount); }
    ReleaseSRWLockExclusive(&g_jpLock);
    if (at >= JP_CALLBACKS) { ApiLog(mod, "on_render: refused: %d render callbacks are taken", JP_CALLBACKS); return 0; }
    InterlockedExchange(&g_jpWanted, 1);
    return 1;
}

/* Why a pose can't be set, or NULL. */
static const char* JpWhyNot(uint32_t unit, uint32_t node, const HfJointPose* pose)
{
    if (pose == NULL) return "no pose";
    if (g_jpState < 0) return "joint poses are off (modloader.log says why)";
    if (JpNodeOf(unit, node) < 0) return "no such unit, or it has no such node";
    for (int k = 0; k < 16; k++) if (!isfinite(((const float*)pose)[k])) return "a pose value is not finite";
    return NULL;
}

/* SDK 28, pose_joint: 1 set (in a render callback) or queued for the next frame, 0 refused (logged at most 8 times). */
static int ApiPoseJoint(HfMod* mod, uint32_t unit, uint32_t node, const HfJointPose* pose)
{
    static volatile LONG logged;
    const char* why = JpWhyNot(unit, node, pose);
    if (why) { if (InterlockedIncrement(&logged) <= 8) ApiLog(mod, "pose_joint: refused: %s", why); return 0; }
    InterlockedExchange(&g_jpWanted, 1);
    if (g_jpRenderThread && GetCurrentThreadId() == g_jpRenderThread) return JpApply(unit, node, pose);
    AcquireSRWLockExclusive(&g_jpLock);
    int queued = g_jpQueued < JP_QUEUE;
    if (queued) g_jpQueue[g_jpQueued++] = (JpQueued){unit, node, *pose};
    ReleaseSRWLockExclusive(&g_jpLock);
    if (!queued) InterlockedIncrement(&g_jpDropped);
    return queued;
}

/* SDK 28, joint_pose: 1 with out filled, 0 when the unit or node does not read. */
static int ApiJointPose(HfMod* mod, uint32_t unit, uint32_t node, uint32_t space, float out[16])
{
    (void)mod;
    uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    int32_t index = out && space <= HF_POSE_LOCAL ? JpNodeOf(unit, node) : -1;
    if (index < 0) return 0;
    const float* m = ((JpPoseFn)(exe + (space == HF_POSE_WORLD ? JP_WORLD_POSE : JP_LOCAL_POSE)))(unit, index);
    return m != NULL && !SafeRead((uintptr_t)m, out, 16 * sizeof out[0]);
}
