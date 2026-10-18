#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

void Controller::bindLayer(PlayLayer* layer) {
    auto* manager = GameManager::sharedState();
    if (!manager) return;
    manager->m_playLayer = layer;
    manager->m_gameLayer = layer;
}

PlayLayer* Controller::newBox(PlayLayer* liveLayer) {
    if (!liveLayer || !liveLayer->m_level) return nullptr;

    if (m_sandbox && m_boxLevel) {
        m_sandbox->setVisible(false);
        if (m_sandbox->m_objectLayer) {
            m_sandbox->m_objectLayer->setVisible(false);
        }
        bindLayer(liveLayer);
        return m_sandbox;
    }

    auto* sandboxLvl = GJGameLevel::create();
    if (!sandboxLvl) return nullptr;
    sandboxLvl->retain();
    sandboxLvl->copyLevelInfo(liveLayer->m_level);
    sandboxLvl->m_levelString = liveLayer->m_level->m_levelString;
    sandboxLvl->m_dontSave = true;
    m_boxLevel = sandboxLvl;

    m_makingBox = true;
    m_sandbox = nullptr;
    auto* sandbox = PlayLayer::create(sandboxLvl, false, false);
    m_makingBox = false;
    bindLayer(liveLayer);

    if (!sandbox) {
        m_sandbox = nullptr;
        m_boxLevel->release();
        m_boxLevel = nullptr;
        return nullptr;
    }

    sandbox->retain();
    m_sandbox = sandbox;
    m_boxStarted = false;
    sandbox->setVisible(false);
    if (sandbox->m_objectLayer) sandbox->m_objectLayer->setVisible(false);

    return sandbox;
}

void Controller::dropSource() {
    if (m_srcSnapshot.checkpoint) {
        m_srcSnapshot.checkpoint->release();
        m_srcSnapshot.checkpoint = nullptr;
    }
    m_srcSnapshot.frame = -1;
    m_sourceOrder = 0;
    m_forced = false;
}

void Controller::parkSandbox(PlayLayer* liveLayer, PlayLayer* sandbox) {
    if (!sandbox) {
        bindLayer(liveLayer);
        return;
    }

    undoClones(sandbox);
    m_level = liveLayer;
    bindLayer(liveLayer);
    sandbox->setVisible(false);
    if (sandbox->m_objectLayer) sandbox->m_objectLayer->setVisible(false);
}

void Controller::dropSandbox(PlayLayer* liveLayer) {
    auto* sandbox = m_sandbox;
    dropSource();

    if (sandbox) {
        undoClones(sandbox);
        if (liveLayer) {
            m_level = liveLayer;
            bindLayer(liveLayer);
        }
        m_sandbox = nullptr;
        sandbox->release();
    }

    if (m_boxLevel) {
        m_boxLevel->release();
        m_boxLevel = nullptr;
    }
    m_boxStarted = false;
    m_makingBox = false;
}

bool Controller::replayEvents(
    PlayLayer* sandbox,
    std::int64_t from,
    std::int64_t to,
    std::uint64_t cutoff
) {
    m_replay.clear();
    m_replay.reserve(m_events.size());
    for (auto const& event : m_events) {
        if (event.order <= cutoff && (from < 0 || event.frame > from)) {
            m_replay.push_back(event);
        }
    }
    sortInputs(m_replay);

    const auto pressOrder = m_pressOrder;
    const auto releaseOrder = m_releaseOrder;
    m_pressOrder = kNoOrder;
    m_releaseOrder = kNoOrder;
    m_replayStart = from;
    m_playAt = 0;
    m_phase = Phase::Later;
    m_attemptDone = false;
    m_alrDied = false;
    m_firstPressed = false;
    m_resolved = false;
    m_completed = false;
    m_deathAt = -1;
    m_hit = nullptr;
    startClock(sandbox);

    std::int64_t ticks = 0;
    auto maxTicks = to + kPadding;
    if (from >= 0) {
        maxTicks -= from;
        if (maxTicks < 1) maxTicks = 1;
    }
    GJBaseGameLayer* game = sandbox;
    m_runningBatch = true;
    while (!m_resolved && rawFrame(sandbox) < to && ticks < maxTicks) {
        game->update(kStep);
        ++ticks;
    }
    m_runningBatch = false;

    m_pressOrder = pressOrder;
    m_releaseOrder = releaseOrder;
    return !m_resolved && rawFrame(sandbox) >= to;
}

bool Controller::saveSource(PlayLayer* sandbox, std::uint64_t cutoff) {
    auto* checkpoint = sandbox->createCheckpoint();
    if (!checkpoint) return false;
    checkpoint->retain();
    if (m_srcSnapshot.checkpoint) m_srcSnapshot.checkpoint->release();
    m_srcSnapshot = {rawFrame(sandbox), checkpoint};
    m_sourceOrder = cutoff;
    m_forced = true;
    return true;
}

bool Controller::setupBox(
    PlayLayer* sandbox,
    std::int64_t sourceFrame,
    std::uint64_t cutoffOrder
) {
    if (!sandbox) return false;

    const bool canReuse =
        m_forced && m_srcSnapshot.checkpoint &&
        m_srcSnapshot.frame >= -1 &&
        sourceFrame >= m_srcSnapshot.frame &&
        cutoffOrder >= m_sourceOrder;

    if (canReuse &&
        (sourceFrame == m_srcSnapshot.frame ||
         (m_srcSnapshot.frame == -1 && sourceFrame <= 0))) {
        m_sourceOrder = cutoffOrder;
        return true;
    }

    if (canReuse && sourceFrame > m_srcSnapshot.frame) {
        const auto cachedFrame = m_srcSnapshot.frame;
        auto* checkpoint = m_srcSnapshot.checkpoint;
        m_phase = Phase::Restarting;
        m_attemptDone = true;
        m_alrDied = true;
        m_clockReady = false;
        resetClick(sandbox);

        sandbox->loadFromCheckpoint(checkpoint);
        syncPlayers(sandbox, checkpoint);
        resetClick(sandbox);

        if (replayEvents(sandbox, cachedFrame, sourceFrame, cutoffOrder) &&
            saveSource(sandbox, cutoffOrder)) {
            return true;
        }
    }

    dropSource();
    if (sourceFrame <= 0) {
        m_srcSnapshot = {-1, nullptr};
        m_sourceOrder = cutoffOrder;
        m_forced = true;
        return true;
    }

    m_phase = Phase::Restarting;
    m_attemptDone = true;
    m_alrDied = true;
    m_clockReady = false;
    m_internalReset = true;
    sandbox->resetLevelFromStart();
    m_internalReset = false;

    if (!replayEvents(sandbox, -1, sourceFrame, cutoffOrder)) return false;
    return saveSource(sandbox, cutoffOrder);
}

bool Controller::probeOffset(PlayLayer* layer, Phase phase, int offset) {
    const bool oldProbe = m_checkEdge;
    m_checkEdge = true;
    const auto result = runOffset(layer, phase, offset);
    m_checkEdge = oldProbe;
    return result == Result::Passed ||
        result == Result::OverLimit;
}

int Controller::passDepth(
    PlayLayer* layer,
    Phase phase,
    int maxDepth,
    bool releaseAxis
) {
    if (!layer || maxDepth <= 0) return 0;

    if (phase == Phase::Earlier) {
        int lo = 0;
        int hi = maxDepth;
        while (lo < hi) {
            const int mid = lo + (hi - lo + 1) / 2;
            const int offset = -mid;
            const bool allowed = releaseAxis
                ? canShiftRelease(offset) : canMoveBack(offset);
            if (allowed) lo = mid;
            else hi = mid - 1;
        }
        maxDepth = lo;
        if (maxDepth <= 0) return 0;
    }

    int lo = 0; 
    int probe = 1;
    const int dir = phase == Phase::Earlier ? -1 : 1;
    while (probe <= maxDepth) {
        if (probeOffset(layer, phase, dir * probe)) {
            lo = probe;
            if (probe == maxDepth) return maxDepth;
            probe = std::max(probe + 1, probe > maxDepth / 2 ? maxDepth : probe * 2);
            continue;
        }

        int hi = probe;
        while (hi - lo > 1) {
            const int mid = lo + (hi - lo) / 2;
            if (probeOffset(layer, phase, dir * mid)) lo = mid;
            else hi = mid;
        }
        return lo;
    }
    return lo;
}

void Controller::scanAxis(
    PlayLayer* sandbox,
    ScanAxis axis,
    std::uint64_t& windowFrames,
    std::uint64_t& checkCount,
    bool& foundEdge,
    bool& hitLimit
) {
    windowFrames = 0;
    checkCount = 0;
    foundEdge = false;
    hitLimit = false;
    if (!sandbox) return;

    m_scanAxis = axis;
    m_checks = 1;
    m_passedFrames = 1;
    m_overLimit = false;

    const bool releaseAxis = axis == ScanAxis::Release;
    const int maxWindow = windowMax();

    int earlyPass = 0;
    const bool hasEarlier = releaseAxis
        ? (m_firstUp > m_firstDown)
        : (m_firstDown > 0);
    if (hasEarlier) {
        earlyPass = passDepth(
            sandbox,
            Phase::Earlier,
            maxWindow,
            releaseAxis
        );
    }

    int passed = 1 + earlyPass;
    if (passed > maxWindow) {
        hitLimit = true;
        m_passedFrames = passed;
        checkCount = m_checks;
        return;
    }

    const int lateNeed = maxWindow - passed + 1;
    const int latePass = passDepth(sandbox, Phase::Later, lateNeed, releaseAxis);

    passed += latePass;
    if (latePass < lateNeed) {
        foundEdge = true;
    } else if (passed > maxWindow) {
        hitLimit = true;
    }

    m_passedFrames = passed;
    m_overLimit = hitLimit;
    windowFrames = hitLimit ? 0u : passed;
    checkCount = m_checks;
}

void Controller::scanClicks(PlayLayer* layer) {
    if (m_scanning || m_platformer || m_resetting ||
        m_phase != Phase::Recording) return;

    for (auto& job : m_clicks) {
        if (!job.canScan || !job.activated) continue;

        const bool canRelease =
            releaseFrameOf(job) >= pressFrameOf(job) &&
            job.releaseOrder != kNoOrder;
        const bool cubeReady = job.try_holding &&
            !job.needsRelease;
        const bool pressPending = !job.pressDone &&
            (cubeReady || !job.keepHold || canRelease);
        const bool pendingRelease = job.pressDone &&
            !job.releaseDone && canRelease;
        if (!pressPending && !pendingRelease) continue;
        if (pendingRelease && noReleaseWin(job)) {
            job.releaseDone = true;
            continue;
        }

        scanClick(layer, job);
        break;
    }
}

void Controller::startBatch(PlayLayer* layer, Phase startPhase, int startOffset) {
    if (!layer || layer != m_level) return;
    if (m_baseEnd < 0 || m_firstDown < 0) {
        finish();
        return;
    }
    if (startPhase != Phase::Earlier && startPhase != Phase::Later) {
        finish();
        return;
    }

    m_batching = true;
    m_runningBatch = false;
    m_running = false;
    m_phase = Phase::Restarting;
    m_clockReady = false;
    m_nextPhase = startPhase;
    m_nextOffset = startOffset;
    m_precheckDone = false;

    muteAudio();
    hideWorld(layer);

    stepBatch(layer);
}

void Controller::stepOffset(PlayLayer* layer) {
    const auto phase = m_nextPhase;
    const int offset = m_nextOffset;
    const auto result = runOffset(layer, phase, offset);
    if (result == Result::OverLimit) {
        m_overLimit = true;
        m_nextPhase = Phase::Finished;
        return;
    }

    if (phase == Phase::Earlier) {
        if (result == Result::Passed && canMoveBack(offset - 1)) {
            m_nextOffset = offset - 1;
        } else {
            m_nextPhase = Phase::Later;
            m_nextOffset = 1;
        }
        return;
    }

    if (result != Result::Passed || offset >= 100000) {
        m_nextPhase = Phase::Finished;
    } else {
        m_nextOffset = offset + 1;
    }
}

void Controller::stepBatch(PlayLayer* layer) {
    if (!m_batching || !layer || layer != m_level || m_runningBatch) return;

    while (m_batching && m_jumpIndex < m_targets.size()) {
        if (!m_precheckDone) {
            m_precheckDone = true;
            if (checkNoJump(layer)) {
                if (!nextBatch()) break;
                continue;
            }
        }

        while (m_nextPhase == Phase::Earlier || m_nextPhase == Phase::Later) {
            stepOffset(layer);
        }

        if (m_scanAxis == ScanAxis::Press && batchRelease()) continue;
        if (!nextBatch()) break;
    }

    endBatch(layer);
}

}
