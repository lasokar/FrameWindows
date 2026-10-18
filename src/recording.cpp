#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

static bool clickBefore(ClickJob const* a, ClickJob const* b) {
    return a->clickIndex < b->clickIndex;
}

void Controller::startClock(PlayLayer* layer) {
    if (!layer) return;
    m_clockReady = true;
    m_lastFrame = rawFrame(layer);
}

std::int64_t Controller::rawFrame(PlayLayer* layer) const {
    if (!layer) return m_lastFrame;
    const double levelTime = std::max(
        0.0,
        static_cast<double>(layer->m_gameState.m_levelTime)
    );

    return std::max<std::int64_t>(
        0,
        static_cast<std::int64_t>(std::floor(levelTime * kPhysicsHz + 1e-7))
    );
}

std::int64_t Controller::frame(PlayLayer* layer) const {
    if (!layer || !m_clockReady) return m_lastFrame;
    return rawFrame(layer);
}

CheckpointObject* Controller::makeSnap(PlayLayer* layer) {
    if (!layer || !m_started || !layer->m_player1 || !layer->isGameplayActive()) {
        return nullptr;
    }

    auto* checkpoint = layer->createCheckpoint();
    if (!checkpoint) return nullptr;
    checkpoint->retain();
    return checkpoint;
}

void Controller::dropSnap(Snapshot& entry) {
    if (entry.checkpoint) {
        entry.checkpoint->release();
        entry.checkpoint = nullptr;
    }
}

void Controller::clearSnaps() {
    for (auto& entry : m_recentSnaps) {
        dropSnap(entry);
    }
    m_recentSnaps.clear();

    for (auto& entry : m_sparseSnaps) {
        dropSnap(entry);
    }
    m_sparseSnaps.clear();

    m_lastSnap = -2;
    m_lastBand = -1;
}

Snapshot const* Controller::findSnap(int offset) const {
    if (m_forced) {
        return &m_srcSnapshot;
    }
    if (m_firstDown < 0) return nullptr;

    const std::int64_t target = m_scanAxis == ScanAxis::Release
        ? m_firstDown
        : std::max<std::int64_t>(0, m_firstDown + offset);
    const std::int64_t normalStart = std::max<std::int64_t>(0, m_firstDown - kLead);

    std::int64_t desiredStart = normalStart;
    if (target <= normalStart) {
        desiredStart = std::max<std::int64_t>(0, target - kLead);
    }

    Snapshot const* best = nullptr;
    for (auto const& entry : m_sparseSnaps) {
        if (entry.checkpoint && entry.frame <= desiredStart &&
            (!best || entry.frame > best->frame)) {
            best = &entry;
        }
    }
    for (auto const& entry : m_recentSnaps) {
        if (entry.checkpoint && entry.frame <= desiredStart &&
            (!best || entry.frame > best->frame)) {
            best = &entry;
        }
    }

    if (!best) {
        return &m_startSnap;
    }

    return best;
}

bool Controller::pressEvent(InputEvent const& event) const {
    return event.order == m_pressOrder;
}

bool Controller::releaseEvent(InputEvent const& event) const {
    return m_releaseOrder != kNoOrder &&
           event.order == m_releaseOrder;
}

bool Controller::noReleaseWin(ClickJob const& job) const {
    return job.releaseFrame < job.pressFrame ||
        job.releaseOrder == kNoOrder;
}

bool Controller::ringBuffered(ClickJob const& job) const {
    return job.activated && job.fromRing &&
        job.activationFrame >= 0 && job.activationFrame > job.pressFrame;
}

std::int64_t Controller::pressFrameOf(ClickJob const& job) const {
    return ringBuffered(job) ? job.activationFrame : job.pressFrame;
}

std::int64_t Controller::releaseFrameOf(ClickJob const& job) const {
    if (job.releaseFrame < 0) return -1;

    if (ringBuffered(job)) {
        return std::max(job.releaseFrame, job.activationFrame);
    }
    return job.releaseFrame;
}

bool Controller::skipRelease() const {
    return m_jumpIndex < m_targets.size() &&
           m_targets[m_jumpIndex].releaseIgnored;
}

bool Controller::findJumps() {
    m_targets.clear();

    std::vector<ClickJob const*> confirmed;
    confirmed.reserve(m_clicks.size());
    for (auto const& job : m_clicks) {
        if (job.activated && job.clickIndex != 0) {
            confirmed.push_back(&job);
        }
    }
    std::sort(confirmed.begin(), confirmed.end(), clickBefore);

    for (auto const* job : confirmed) {
        JumpTarget target;
        target.frame = job->pressFrame;
        target.releaseFrame = job->releaseFrame;
        target.pressOrder = job->pressOrder;
        target.releaseOrder = job->releaseOrder;
        target.button = job->button;
        target.player1 = job->player1;
        target.releaseIgnored = noReleaseWin(*job);
        target.keepHold = job->keepHold;
        m_targets.push_back(target);
    }

    return !m_targets.empty();
}

void Controller::pickJump(std::size_t index) {
    if (index >= m_targets.size()) return;
    auto const& target = m_targets[index];
    m_firstDown = target.frame;
    m_firstUp = target.releaseFrame;
    m_firstBtn = target.button;
    m_firstP1 = target.player1;
    m_pressOrder = target.pressOrder;
    m_releaseOrder = target.releaseOrder;
    m_keepHold = target.keepHold;
    m_firstPressed = false;
    m_pressAt = -1;

}

std::int64_t Controller::jumpFrame() const {
    const auto next = m_jumpIndex + 1;
    if (next < m_targets.size()) return m_targets[next].frame;
    return -1;
}

std::int64_t Controller::compareAt() const {
    auto endpoint = jumpFrame();
    if (endpoint < 0) endpoint = m_baseEnd;
    return std::max<std::int64_t>(1, endpoint - m_firstDown);
}

std::int64_t Controller::shiftAt() const {
    if (m_scanAxis == ScanAxis::Release && m_releaseTarget >= 0) {
        return m_releaseTarget;
    }
    return m_pressTarget;
}

bool Controller::canShiftRelease(int offset) const {
    if (m_firstUp < 0) return false;
    return m_firstUp + offset >= m_firstDown;
}

bool Controller::canMoveBack(int offset) const {
    if (m_scanAxis == ScanAxis::Release) {
        return canShiftRelease(offset);
    }
    return m_firstDown + offset >= 0;
}

bool Controller::hasPrevFrame() const {
    return m_scanAxis == ScanAxis::Release
        ? m_firstUp > m_firstDown
        : m_firstDown > 0;
}

void Controller::resetBase(PlayLayer* layer) {
    dropSandbox(layer);
    dropProbe();
    clearRings();
    resetHUD();
    clearSnaps();
    m_events.clear();
    m_clicks.clear();
    m_jumps.clear();
    m_sendingJump = false;
    m_jumpP1 = true;
    m_measured = 0;
    m_jumpCount = 0;
    m_scanning = false;
    m_cutoffOrder = kNoOrder;
    m_replay.clear();
    m_targets.clear();
    m_jumpIndex = 0;
    m_pressOrder = kNoOrder;
    m_releaseOrder = kNoOrder;
    m_keepHold = false;
    m_scanAxis = ScanAxis::Press;
    m_overLimit = false;
    m_baseDeath = -1;
    m_baseEnd = -1;
    m_baseObject = nullptr;
    m_firstDown = -1;
    m_firstUp = -1;
    m_firstBtn = static_cast<int>(PlayerButton::Jump);
    m_firstP1 = true;
    m_offset = 0;
    m_checks = 0;
    m_passedFrames = 0;
    m_orderCounter = 0;
    m_attemptDone = false;
    m_alrDied = false;
    m_playAt = 0;
    m_firstPressed = false;
    m_pressAt = -1;
    m_replayStart = 0;
    m_pressTarget = -1;
    m_releaseTarget = -1;
    m_replayStarted = false;
    m_earlyDeaths = 0;
    m_earlyWins = 0;
    m_started = !m_platformer;
    if (!m_platformer) {
        startClock(layer);
    } else {
        m_clockReady = false;
    }
}

bool Controller::prepReplay(int offset, Snapshot const* snapshot) {
    if (!snapshot || (snapshot->frame >= 0 && !snapshot->checkpoint)) {
        return false;
    }

    if (m_scanAxis == ScanAxis::Release) {
        if (m_firstUp < 0) {
            return false;
        }
        m_pressTarget = m_firstDown;
        m_releaseTarget = m_firstUp + offset;
        if (m_releaseTarget < m_pressTarget) {
            return false;
        }
    } else {
        m_pressTarget = std::max<std::int64_t>(0, m_firstDown + offset);
        if (m_scanAxis == ScanAxis::Press && !m_keepHold) {
            m_releaseTarget = -1;
        } else {
            const std::int64_t effectiveShift = m_pressTarget - m_firstDown;
            m_releaseTarget = m_firstUp >= 0
                ? std::max<std::int64_t>(m_pressTarget, m_firstUp + effectiveShift)
                : -1;
        }
    }

    m_replayStart = snapshot->frame;
    m_replay.clear();
    m_replay.reserve(m_events.size());

    for (auto event : m_events) {
        if (m_scanning && event.order > m_cutoffOrder) {
            continue;
        }
        const bool press = pressEvent(event);
        const bool release = !press && releaseEvent(event) && m_firstUp >= 0;
        if (release && m_scanAxis == ScanAxis::Press && !m_keepHold) continue;
        if (press) event.frame = m_pressTarget;
        if (release) event.frame = m_releaseTarget;
        if (event.frame > snapshot->frame) m_replay.push_back(event);
    }

    sortInputs(m_replay);

    bool haveFirst = false;
    bool haveRelease = m_scanAxis != ScanAxis::Release;
    for (auto const& event : m_replay) {
        if (pressEvent(event)) haveFirst = true;
        if (releaseEvent(event)) haveRelease = true;
    }
    if (!haveFirst || !haveRelease) return false;

    return true;
}

void Controller::resetClick(PlayLayer* layer) {
    if (!layer) return;

    if (m_firstDown >= 0) {
        m_replayingInput = true;
        GJBaseGameLayer* game = layer;
        game->handleButton(false, m_firstBtn, m_firstP1);
        m_replayingInput = false;
    }
    layer->m_queuedButtons.clear();
    layer->m_queuedRecordedButtons.clear();
    layer->m_queuedReplayButtons.clear();
}

bool Controller::beginRelease() {
    if (skipRelease() ||
        m_firstUp < m_firstDown ||
        m_releaseOrder == kNoOrder) {
        return false;
    }
    m_scanAxis = ScanAxis::Release;
    m_checks = 1;
    m_passedFrames = 1;
    m_overLimit = false;

    const bool earlier = hasPrevFrame();
    queueRun(earlier ? Phase::Earlier : Phase::Later, earlier ? -1 : 1);
    return true;
}

void Controller::endAxis() {
    if (m_scanAxis == ScanAxis::Press && beginRelease()) {
        return;
    }

    if (!nextJump()) {
        finish();
    }
}

bool Controller::nextJump() {
    if (m_jumpIndex + 1 >= m_targets.size()) return false;
    ++m_jumpIndex;
    pickJump(m_jumpIndex);
    m_scanAxis = ScanAxis::Press;
    m_overLimit = false;
    m_checks = 1;
    m_passedFrames = 1;
    queueRun(
        m_firstDown > 0 ? Phase::Earlier : Phase::Later,
        m_firstDown > 0 ? -1 : 1
    );
    return true;
}

bool Controller::passes(std::int64_t deathFrame, GameObject* deathObject, bool survived) const {
    if (survived) return true;
    if (m_baseDeath == kFinishFrame) return false;
    if (deathFrame >= m_baseDeath) return true;

    const bool sameHit =
        deathObject && m_baseObject && deathObject == m_baseObject;
    if (!sameHit || m_baseDeath <= m_firstDown) return false;

    const auto earlyByFrames = m_baseDeath - deathFrame;
    const auto compareFrames = compareAt();
    const double tolerance = compareFrames * kDeathTol;
    return earlyByFrames < tolerance;
}

int Controller::savedMode(PlayerCheckpoint const* state) const {
    if (!state) return 0;
    if (state->m_isShip) return 1;
    if (state->m_isBall) return 2;
    if (state->m_isBird) return 3;
    if (state->m_isSwing) return 4;
    if (state->m_isDart) return 5;
    if (state->m_isRobot) return 6;
    if (state->m_isSpider) return 7;
    return 0;
}

int Controller::playerMode(PlayerObject const* player) const {
    if (!player) return 0;
    if (player->m_isShip) return 1;
    if (player->m_isBall) return 2;
    if (player->m_isBird) return 3;
    if (player->m_isSwing) return 4;
    if (player->m_isDart) return 5;
    if (player->m_isRobot) return 6;
    if (player->m_isSpider) return 7;
    return 0;
}

bool Controller::canHoldCube(PlayerObject const* player) const {
    if (!player || !m_level || m_platformer) return false;

    if (m_level->m_gameState.m_isDualMode) return false;
    return playerMode(player) == 0;
}

void Controller::syncMode(PlayerObject* player, PlayerCheckpoint* state) {
    if (!player || !state) return;

    const int expectedMode = savedMode(state);
    const int actualMode = playerMode(player);
    const float actualSpeed = player->m_playerSpeed;
    const bool modeMismatch = expectedMode != actualMode;
    const bool speedMismatch = std::abs(actualSpeed - state->m_playerSpeed) > 0.0001f;
    if (!modeMismatch && !speedMismatch) return;

    if (modeMismatch) {
        if (player->m_isShip) player->toggleFlyMode(false, true);
        if (player->m_isBall) player->toggleRollMode(false, true);
        if (player->m_isBird) player->toggleBirdMode(false, true);
        if (player->m_isSwing) player->toggleSwingMode(false, true);
        if (player->m_isDart) player->toggleDartMode(false, true);
        if (player->m_isRobot) player->toggleRobotMode(false, true);
        if (player->m_isSpider) player->toggleSpiderMode(false, true);

        switch (expectedMode) {
            case 1: player->toggleFlyMode(true, true); break;
            case 2: player->toggleRollMode(true, true); break;
            case 3: player->toggleBirdMode(true, true); break;
            case 4: player->toggleSwingMode(true, true); break;
            case 5: player->toggleDartMode(true, true); break;
            case 6: player->toggleRobotMode(true, true); break;
            case 7: player->toggleSpiderMode(true, true); break;
            default: break;
        }
        player->loadFromCheckpoint(state);
    }

    player->m_playerSpeed = state->m_playerSpeed;

}

void Controller::syncPlayers(PlayLayer* layer, CheckpointObject* checkpoint) {
    if (!layer || !checkpoint) return;
    syncMode(layer->m_player1, checkpoint->m_player1Checkpoint);
    syncMode(layer->m_player2, checkpoint->m_player2Checkpoint);
}

}
