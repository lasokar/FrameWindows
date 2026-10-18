#pragma once

#include "Types.hpp"

namespace frame_windows {

class Controller {
public:
    static Controller& get() {
        static Controller instance;
        return instance;
    }
    void startLevel(PlayLayer* layer);

    bool showRings() const;
    void clearRings();
    bool countersOn() const;
    void buildHUD();
    bool soundsOn() const;

    bool batching() const;
    bool makingSandbox() const;
    void setSandbox(PlayLayer* layer);
    bool isSandbox(PlayLayer* layer) const;
    bool skipFx(GJBaseGameLayer* layer) const;
    bool skipEffects() const;
    void stopTrail(PlayerObject* player);
    bool isClone(PlayerObject* player) const;
    bool isProbe(PlayerObject* player) const;
    bool probing() const;
    void clearProbeHit();
    bool hideGround(GJGroundLayer* ground) const;
    GJGroundLayer* probeG1() const;
    GJGroundLayer* probeG2() const;
    bool usedPortal(GameObject* object) const;
    void addPortal(GameObject* object);
    void probeDeath(PlayerObject* player, GameObject* object);
    void probeDone();
    bool skipGhosts(PlayerObject* player) const;
    bool skipPlayerFx(PlayerObject* player) const;
    bool disableRings() const;
    bool skipAttempts(PlayLayer* layer) const;


    std::int64_t snapLimit(std::int64_t frames) const;
    int windowMax() const;
    bool inRange(std::uint64_t frames) const;
    bool pastLimit() const;
    bool runningBatch(PlayLayer* layer) const;
    bool batchDead(PlayLayer* layer) const;
    void markDeath(PlayLayer* layer, GameObject* deathObject);
    void confirmJump(
        ClickJob& job,
        std::int64_t activationFrame,
        cocos2d::CCPoint const& activationPos,
        bool fromRing,
        bool wasBuffered,
        RingObject* ring,
        bool try_holding
    );
    void jumpActive(PlayerObject* player, RingObject* ring);
    void applyJump(ClickJob& job);
    bool preButton(GJBaseGameLayer* base, bool, int, bool);
    bool beforeJump(PlayerObject* player, PlayerButton button, bool down);
    void afterJump(
        PlayerObject* player,
        PlayerButton button,
        bool down,
        bool accepted
    );
    void preCommands(GJBaseGameLayer* base);
    void postCommands(GJBaseGameLayer* base, float);
    bool blockDeath(PlayLayer* layer, PlayerObject* player);
    bool claimDeath(PlayLayer* layer, PlayerObject* player);
    bool confirmDeath(PlayLayer* layer, PlayerObject* player);
    void handleDeath(PlayLayer* layer, GameObject* deathObject);
    void finishLevel(PlayLayer* layer);
    bool completeLevel(PlayLayer* layer);
    void beforeReset(PlayLayer* layer);
    void onReset(PlayLayer* layer, bool internal);
    void afterTick(PlayLayer* layer);
    void onQuit(PlayLayer* layer);
    void onExit(PlayLayer* layer);
    bool internalReset() const;
    void pauseStart(PlayLayer* layer);
    void pauseEnd(PlayLayer* layer);

    void saveHist(PlayLayer* layer, PlayerObject* player);
    void afterUpdate(GJBaseGameLayer* base);

private:
    bool activeLayer(GJBaseGameLayer* layer) const;
    void startClock(PlayLayer* layer);
    std::int64_t rawFrame(PlayLayer* layer) const;
    std::int64_t frame(PlayLayer* layer) const;
    CheckpointObject* makeSnap(PlayLayer* layer);
    void dropSnap(Snapshot& entry);
    void clearSnaps();

    Snapshot const* findSnap(int offset) const;
    bool pressEvent(InputEvent const& event) const;
    bool releaseEvent(InputEvent const& event) const;
    bool noReleaseWin(ClickJob const& job) const;
    bool ringBuffered(ClickJob const& job) const;
    std::int64_t pressFrameOf(ClickJob const& job) const;
    std::int64_t releaseFrameOf(ClickJob const& job) const;
    bool skipRelease() const;
    bool findJumps();
    void pickJump(std::size_t index);
    std::int64_t jumpFrame() const;
    std::int64_t compareAt() const;
    std::int64_t shiftAt() const;
    bool canShiftRelease(int offset) const;
    bool canMoveBack(int offset) const;
    bool hasPrevFrame() const;
    void resetBase(PlayLayer* layer);
    bool prepReplay(int offset, Snapshot const* snapshot);
    void resetClick(PlayLayer* layer);

    bool beginRelease();
    void endAxis();
    bool nextJump();

    bool passes(std::int64_t deathFrame, GameObject* deathObject, bool survived) const;
    int savedMode(PlayerCheckpoint const* state) const;
    int playerMode(PlayerObject const* player) const;
    bool canHoldCube(PlayerObject const* player) const;

    void syncMode(PlayerObject* player, PlayerCheckpoint* state);
    void syncPlayers(PlayLayer* layer, CheckpointObject* checkpoint);
    PlayerObject* newClone(PlayLayer* layer, PlayerObject* source, bool secondPlayer);
    bool addClones(PlayLayer* layer);
    void undoClones(PlayLayer* layer);
    void hideClones();
    void hideWorld(PlayLayer* layer);
    void showWorld(PlayLayer* layer);
    bool loadSource(PlayLayer* layer, Snapshot const* source);
    void startRun(PlayLayer* layer);
    bool setupNoJump(PlayLayer* layer);
    bool checkNoJump(PlayLayer* layer);

    bool prepOffset(PlayLayer* layer, Phase phase, int offset);
    Result runOffset(
        PlayLayer* layer,
        Phase phase,
        int offset
    );
    void muteAudio();
    void clearFx();
    void unmuteAudio();
    void showDisplay(PlayLayer* layer);

    bool batchRelease();
    bool nextBatch();
    void endBatch(PlayLayer* layer);
    static bool isFlight(PlayerObject const* player);
    static bool isUfo(PlayerObject const* player);
    bool playersReady(PlayLayer* layer) const;
    bool setupBase(PlayLayer* layer);
    cocos2d::CCPoint ringPos(PlayLayer* layer, cocos2d::CCPoint objectPosition) const;
    void moveRings(PlayLayer* layer);
    std::filesystem::path sfxPath(int index) const;
    void loadSfx();
    int soundId();
    void playSound(std::uint64_t frames);
    void clearHUD();
    void resetHUD();
    void pulseHUD(std::uint64_t windowFrames);
    cocos2d::ccColor3B ringColor(std::uint64_t windowFrames) const;
    cocos2d::CCNode* newRing(std::uint64_t windowFrames);
    void attachRing(
        PlayLayer* layer,
        cocos2d::CCPoint const& position,
        std::uint64_t windowFrames,
        bool withSound = true
    );
    void addRing(
        PlayLayer* layer,
        ClickJob const& job,
        std::uint64_t pressWindow,
        std::uint64_t releaseWindow,
        bool drawPress,
        bool drawRelease
    );
    PlayerObject* newProbe(PlayLayer* layer);
    void clearHist();
    void dropProbe();
    void setupProbe(PlayLayer* layer);
    History const* findHist(bool player1, std::int64_t desiredFrame) const;
    bool probeReady(PlayerObject* player) const;
    void probeInput(PlayerObject* probe, InputEvent const& event, bool pressEvent);
    PathState runPath(
        PlayLayer* layer,
        ClickJob const& job,
        ScanAxis axis,
        int offset,
        bool removeTarget,
        std::int64_t stopAt = -1,
        bool isBaseline = false
    );
    bool sameJump(
        ClickJob const& job,
        PathState const& result
    ) const;
    float releasePct(
        ClickJob const& job,
        PathState const& result
    ) const;
    bool pickCubeHold(PlayLayer* layer, ClickJob& job);
    bool pathValid(
        ClickJob const& job,
        PathState const& baseline,
        PathState const& candidate,
        ScanAxis axis
    ) const;
    bool checkRange(
        PlayLayer* layer,
        ClickJob const& job,
        PathState const& baseline,
        ScanAxis axis,
        bool earlier,
        int depth,
        std::uint64_t& checks
    );
    int scanDepth(
        PlayLayer* layer,
        ClickJob const& job,
        PathState const& baseline,
        ScanAxis axis,
        bool earlier,
        int maxDepth,
        std::uint64_t& checks
    );
    void scanRoute(
        PlayLayer* layer,
        ClickJob const& job,
        PathState const& baseline,
        ScanAxis axis,
        std::uint64_t& windowFrames,
        std::uint64_t& checks,
        bool& hitLimit
    );
    bool scanClick(PlayLayer* layer, ClickJob& job);
    void bindLayer(PlayLayer* layer);
    PlayLayer* newBox(PlayLayer* liveLayer);

    void dropSource();
    void parkSandbox(PlayLayer* liveLayer, PlayLayer* sandbox);
    void dropSandbox(PlayLayer* liveLayer);
    bool replayEvents(PlayLayer* sandbox, std::int64_t from, std::int64_t to, std::uint64_t cutoff);
    bool saveSource(PlayLayer* sandbox, std::uint64_t cutoff);
    bool setupBox(
        PlayLayer* sandbox,
        std::int64_t sourceFrame,
        std::uint64_t cutoffOrder
    );
    bool probeOffset(PlayLayer* layer, Phase phase, int offset);
    int passDepth(
        PlayLayer* layer,
        Phase phase,
        int maxDepth,
        bool releaseAxis
    );
    void scanAxis(
        PlayLayer* sandbox,
        ScanAxis axis,
        std::uint64_t& windowFrames,
        std::uint64_t& checkCount,
        bool& foundEdge,
        bool& hitLimit
    );

    void scanClicks(PlayLayer* layer);
    void startBatch(PlayLayer* layer, Phase startPhase, int startOffset);
    void stepOffset(PlayLayer* layer);
    void stepBatch(PlayLayer* layer);
    void queueRun(Phase nextPhase, int offset);
    void restart(PlayLayer* layer);
    void loadCheckpoint(PlayLayer* layer);
    void finish();
    void stop(PlayLayer* layer);
    void idle();
    void resetAll();

private:
    CCLayer* m_probeLayer = nullptr;
    GJGroundLayer* m_probeG1 = nullptr;
    GJGroundLayer* m_probeG2 = nullptr;
    GJGroundLayer* m_ground1 = nullptr;
    GJGroundLayer* m_ground2 = nullptr;
    PlayerObject* m_probeP1 = nullptr;
    PlayerObject* m_probeP2 = nullptr;
    std::array<std::array<History, kHistSize>, 2> m_history{};
    std::vector<InputEvent> m_probeEvents;
    std::array<std::int64_t, 2> m_lastHistFrame{-1, -1};
    bool m_probing = false;
    bool m_probeDead = false;
    bool m_probeDone = false;
    GameObject* m_probeHit = nullptr;
    std::int64_t m_probeDeath = -1;
    std::int64_t m_probeAt = -1;
    PlayerObject* m_probePlayer = nullptr;
    bool m_probeDown = false;
    bool m_probeJumped = false;
    std::int64_t m_probeJumpAt = -1;
    bool m_probeRingJump = false;
    RingObject* m_ring = nullptr;
    std::vector<GameObject*> m_portals;
    bool m_paused = false;
    PlayLayer* m_level = nullptr;
    PlayLayer* m_sandbox = nullptr;
    GJGameLevel* m_boxLevel = nullptr;
    bool m_makingBox = false;
    bool m_boxStarted = false;
    Snapshot m_srcSnapshot{-1, nullptr};
    std::uint64_t m_sourceOrder = 0;
    bool m_forced = false;
    Phase m_phase = Phase::Idle;
    Phase m_queuedPhase = Phase::Idle;
    std::vector<InputEvent> m_events;
    std::vector<InputEvent> m_replay;
    std::vector<JumpTarget> m_targets;

    std::vector<ClickJob> m_clicks;
    std::deque<JumpEvent> m_jumps;
    std::vector<RingMarker> m_markers;
    cocos2d::CCNode* m_hud = nullptr;
    std::vector<CountRow> m_counters;
    std::deque<Snapshot> m_recentSnaps;
    std::vector<Snapshot> m_sparseSnaps;
    Snapshot m_startSnap{-1, nullptr};
    std::int64_t m_lastFrame = 0;
    std::int64_t m_lastSnap = -2;
    std::int64_t m_lastBand = -1;
    std::int64_t m_replayStart = 0;
    std::int64_t m_pressTarget = -1;
    std::int64_t m_releaseTarget = -1;
    std::int64_t m_baseDeath = -1;
    std::int64_t m_baseEnd = -1;
    GameObject* m_baseObject = nullptr;
    std::int64_t m_firstDown = -1;
    std::int64_t m_firstUp = -1;
    std::int64_t m_pressAt = -1;
    std::int64_t m_queuedFrame = -1;
    int m_offset = 0;
    int m_queuedOffset = 0;
    std::uint64_t m_checks = 0;
    std::uint64_t m_passedFrames = 0;
    std::uint64_t m_orderCounter = 0;
    std::uint64_t m_earlyDeaths = 0;
    std::uint64_t m_earlyWins = 0;
    std::uint64_t m_measured = 0;
    std::size_t m_jumpCount = 0;
    std::uint64_t m_cutoffOrder = kNoOrder;
    std::size_t m_playAt = 0;
    std::size_t m_jumpIndex = 0;

    std::uint64_t m_pressOrder = kNoOrder;
    std::uint64_t m_releaseOrder = kNoOrder;
    ScanAxis m_scanAxis = ScanAxis::Press;

    bool m_overLimit = false;
    bool m_checkEdge = false;
    bool m_internalReset = false;
    bool m_resetting = false;
    int m_resetDepth = 0;
    bool m_levelDone = false;
    bool m_blockRestarts = false;
    bool m_platformer = false;
    bool m_soundsReady = false;
    std::uint32_t m_soundSerial = 0;
    bool m_started = false;
    bool m_attemptDone = false;
    bool m_alrDied = false;
    bool m_clockReady = false;
    bool m_firstPressed = false;
    bool m_replayStarted = false;
    bool m_loadQueued = false;
    int m_restartDelay = 0;
    CheckpointObject* m_pendingCP = nullptr;
    bool m_replayingInput = false;
    bool m_sendingJump = false;
    bool m_jumpP1 = true;
    int m_firstBtn = static_cast<int>(PlayerButton::Jump);
    bool m_firstP1 = true;
    bool m_finishing = false;


    bool m_batching = false;
    bool m_scanning = false;
    bool m_keepHold = false;
    PlayerObject* m_realP1 = nullptr;
    PlayerObject* m_realP2 = nullptr;
    cocos2d::CCNode* m_p1Parent = nullptr;
    cocos2d::CCNode* m_p2Parent = nullptr;
    int m_p1Z = 0;
    int m_p2Z = 0;
    bool m_p1Detached = false;
    bool m_p2Detached = false;
    PlayerObject* m_cloneP1 = nullptr;
    PlayerObject* m_cloneP2 = nullptr;
    bool m_hasClones = false;
    bool m_runningBatch = false;
    bool m_resolved = false;
    bool m_completed = false;
    bool m_running = false;
    bool m_steppedExtra = false;
    std::int64_t m_stopFrame = -1;
    std::int64_t m_tickLimit = 0;
    std::int64_t m_ticks = 0;
    std::int64_t m_deathAt = -1;
    GameObject* m_hit = nullptr;
    bool m_jumped = false;
    std::int64_t m_jumpAt = -1;
    cocos2d::CCPoint m_jumpPos = cocos2d::CCPoint{0.f, 0.f};
    bool m_ringJump = false;
    Phase m_nextPhase = Phase::Idle;
    int m_nextOffset = 0;
    bool m_precheckDone = false;
    bool m_audioMuted = false;
    float m_musicVol = 1.f;
    float m_sfxVol = 1.f;
    bool m_hiddenWorld = false;
    bool m_objectsVisible = true;
};

}
