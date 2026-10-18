#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

void Controller::queueRun(Phase nextPhase, int offset) {
    if (nextPhase != Phase::Earlier && nextPhase != Phase::Later) return;

    m_queuedPhase = nextPhase;
    m_queuedOffset = offset;
    m_phase = Phase::Restarting;
    m_attemptDone = true;
    m_alrDied = true;
    m_clockReady = false;

    auto* expectedLayer = m_level;
    if (!m_batching) {
        geode::queueInMainThread([expectedLayer, nextPhase, offset] {
            Controller::get().startBatch(expectedLayer, nextPhase, offset);
        });
        return;
    }

    m_firstPressed = false;
    m_pressAt = -1;
    m_playAt = 0;
    m_replayStarted = false;
    geode::queueInMainThread([expectedLayer] {
        Controller::get().restart(expectedLayer);
    });
}

void Controller::restart(PlayLayer* layer) {
    if (!layer || layer != m_level || m_phase != Phase::Restarting) return;

    if (!m_batching) {
        startBatch(layer, m_queuedPhase, m_queuedOffset);
        return;
    }

    const int nextOffset = m_queuedOffset;
    auto const* snapshot = findSnap(nextOffset);
    if (!snapshot) {
        finish();
        return;
    }

    m_pendingCP = snapshot->checkpoint;
    m_queuedFrame = snapshot->frame;
    m_loadQueued = false;
    m_restartDelay = 0;

    resetClick(layer);

    m_internalReset = true;
    layer->resetLevelFromStart();
    m_internalReset = false;
    resetClick(layer);

    m_clockReady = false;
    m_restartDelay = 1;
}

void Controller::loadCheckpoint(PlayLayer* layer) {
    if (!layer || layer != m_level || m_phase != Phase::Restarting) return;
    m_loadQueued = false;

    if (!m_batching) {
        startBatch(layer, m_queuedPhase, m_queuedOffset);
        return;
    }

    Snapshot source{m_queuedFrame, m_pendingCP};

    if (source.checkpoint) {
        layer->loadFromCheckpoint(source.checkpoint);
        syncPlayers(layer, source.checkpoint);
        resetClick(layer);
    }

    if (!prepReplay(m_queuedOffset, &source)) {
        finish();
        return;
    }

    m_playAt = 0;
    m_offset = m_queuedOffset;
    m_phase = m_queuedPhase;
    m_attemptDone = false;
    m_alrDied = false;
    m_firstPressed = false;
    m_pressAt = -1;
    m_replayStarted = true;
    startClock(layer);
}

void Controller::finish() {
    m_phase = Phase::Finished;
    m_attemptDone = true;
    m_clockReady = false;
    m_finishing = true;
}

void Controller::stop(PlayLayer* layer) {
    undoClones(layer);
    showWorld(layer);
    unmuteAudio();

    idle();
}

void Controller::idle() {
    undoClones(m_level);
    showWorld(m_level);
    clearRings();
    m_clicks.clear();
    m_jumps.clear();
    m_sendingJump = false;
    m_jumpP1 = true;
    m_measured = 0;
    m_jumpCount = 0;
    m_scanning = false;
    m_cutoffOrder = kNoOrder;
    dropSandbox(m_level);
    clearSnaps();
    m_phase = Phase::Idle;
    m_batching = false;
    m_runningBatch = false;
    m_running = false;
    m_steppedExtra = false;
    m_stopFrame = -1;
    m_tickLimit = 0;
    m_ticks = 0;
    m_attemptDone = false;
    m_alrDied = false;
    m_clockReady = false;
    m_loadQueued = false;
    m_restartDelay = 0;
    m_pendingCP = nullptr;
    m_queuedFrame = -1;
    m_finishing = false;
}

void Controller::resetAll() {
    undoClones(m_level);
    showWorld(m_level);
    clearRings();
    clearHUD();
    clearSnaps();
    m_clicks.clear();
    m_jumps.clear();
    m_measured = 0;
    m_scanning = false;
    m_cutoffOrder = kNoOrder;
    dropSandbox(m_level);
    dropProbe();
    m_level = nullptr;
    m_phase = Phase::Idle;
    m_queuedPhase = Phase::Idle;
    m_events.clear();
    m_replay.clear();
    m_targets.clear();
    m_jumpIndex = 0;
    m_pressOrder = kNoOrder;
    m_releaseOrder = kNoOrder;
    m_keepHold = false;
    m_lastFrame = 0;
    m_replayStart = 0;
    m_pressTarget = -1;
    m_releaseTarget = -1;
    m_baseDeath = -1;
    m_baseEnd = -1;
    m_baseObject = nullptr;
    m_firstDown = -1;
    m_firstUp = -1;
    m_firstBtn = static_cast<int>(PlayerButton::Jump);
    m_firstP1 = true;
    m_pressAt = -1;
    m_offset = 0;
    m_queuedOffset = 0;
    m_checks = 0;
    m_passedFrames = 0;
    m_orderCounter = 0;
    m_playAt = 0;
    m_internalReset = false;
    m_resetting = false;
    m_resetDepth = 0;
    m_blockRestarts = false;
    m_platformer = false;
    m_started = false;
    m_attemptDone = false;
    m_alrDied = false;
    m_clockReady = false;
    m_firstPressed = false;
    m_replayStarted = false;
    m_loadQueued = false;
    m_restartDelay = 0;
    m_pendingCP = nullptr;
    m_queuedFrame = -1;
    m_replayingInput = false;
    m_batching = false;
    m_realP1 = nullptr;
    m_realP2 = nullptr;
    m_p1Parent = nullptr;
    m_p2Parent = nullptr;
    m_p1Z = 0;
    m_p2Z = 0;
    m_p1Detached = false;
    m_p2Detached = false;
    m_cloneP1 = nullptr;
    m_cloneP2 = nullptr;
    m_hasClones = false;
    m_runningBatch = false;
    m_resolved = false;
    m_completed = false;
    m_running = false;
    m_steppedExtra = false;
    m_stopFrame = -1;
    m_tickLimit = 0;
    m_ticks = 0;
    m_deathAt = -1;
    m_hit = nullptr;
    m_nextPhase = Phase::Idle;
    m_nextOffset = 0;
    m_precheckDone = false;
    m_scanAxis = ScanAxis::Press;
    m_overLimit = false;
    m_checkEdge = false;
    m_audioMuted = false;
    m_musicVol = 1.f;
    m_sfxVol = 1.f;
    m_hiddenWorld = false;
    m_objectsVisible = true;
    m_finishing = false;
    m_earlyDeaths = 0;
    m_earlyWins = 0;
    m_paused = false;
    m_levelDone = false;
}

}
