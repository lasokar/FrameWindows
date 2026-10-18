#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

void Controller::startLevel(PlayLayer* layer) {
    if (!layer) return;

    if (m_level && m_level != layer) {
        undoClones(m_level);
        showWorld(m_level);
        unmuteAudio();
    }

    resetAll();
    m_level = layer;
    loadSfx();
    m_phase = Phase::Recording;

    m_platformer = layer->m_level && layer->m_level->isPlatformer();

    m_started = false;
    m_clockReady = false;

    buildHUD();

}

bool Controller::showRings() const {
    return Mod::get()->getSettingValue<bool>("show-rings");
}

bool Controller::countersOn() const {
    return Mod::get()->getSettingValue<bool>("show-counters");
}

bool Controller::soundsOn() const {
    return Mod::get()->getSettingValue<bool>("ding-sounds");
}

bool Controller::batching() const {
    return m_batching;
}

bool Controller::makingSandbox() const {
    return m_makingBox;
}

void Controller::setSandbox(PlayLayer* layer) {
    if (m_makingBox) {
        m_sandbox = layer;
    }
}

bool Controller::isSandbox(PlayLayer* layer) const {
    return layer && layer == m_sandbox;
}

bool Controller::skipFx(GJBaseGameLayer* layer) const {
    if (m_probing && activeLayer(layer)) return true;
    return m_batching && m_runningBatch && activeLayer(layer);
}

bool Controller::skipEffects() const {
    return m_probing ||
        (m_batching && m_runningBatch && m_boxStarted);
}

void Controller::stopTrail(PlayerObject* player) {
    if (!player) return;
    auto* trail = player->m_ghostTrail;
    if (!trail) return;
    trail->stopTrail();
    trail->unscheduleAllSelectors();
}

bool Controller::isClone(PlayerObject* player) const {
    return player && m_hasClones &&
        (player == m_cloneP1 || player == m_cloneP2);
}

bool Controller::isProbe(PlayerObject* player) const {
    return player && (player == m_probeP1 || player == m_probeP2);
}

bool Controller::probing() const {
    return m_probing;
}

void Controller::clearProbeHit() {
    if (!m_probing || !m_level) return;

    m_level->m_isDebugDrawEnabled = false;
    m_level->m_hitboxesOnDeath = false;
    m_level->m_playerDied = false;
    if (m_level->m_debugDrawNode) m_level->m_debugDrawNode->setVisible(false);
}

bool Controller::hideGround(GJGroundLayer* ground) const {
    if (!m_probing || !m_level || !ground) return false;

    return ground == m_ground1 || ground == m_ground2 ||
        ground == m_level->m_groundLayer || ground == m_level->m_groundLayer2;
}

GJGroundLayer* Controller::probeG1() const { return m_probeG1; }

GJGroundLayer* Controller::probeG2() const { return m_probeG2; }

bool Controller::usedPortal(GameObject* object) const {
    return object && std::find(
        m_portals.begin(),
        m_portals.end(),
        object
    ) != m_portals.end();
}

void Controller::addPortal(GameObject* object) {
    if (!object || usedPortal(object)) return;
    m_portals.push_back(object);
}

void Controller::probeDeath(PlayerObject* player, GameObject* object) {
    if (!m_probing || !isProbe(player)) return;

    clearProbeHit();
    m_probeDead = true;
    m_probeHit = object;
    m_probeDeath = m_probeAt;
    if (player) player->m_isDead = false;
    clearProbeHit();
}

void Controller::probeDone() {
    if (m_probing) m_probeDone = true;
}

bool Controller::skipGhosts(PlayerObject* player) const {
    if (!player || !m_level) return false;
    if (!m_levelDone && !m_blockRestarts) return false;

    return player == m_level->m_player1 || player == m_level->m_player2;
}

bool Controller::skipPlayerFx(PlayerObject* player) const {
    if (!player) return false;
    if (m_probing && isProbe(player)) return true;
    if (!m_batching || !m_runningBatch) return false;
    if (isClone(player)) return true;

    return m_sandbox &&
        (player == m_sandbox->m_player1 || player == m_sandbox->m_player2);
}

bool Controller::disableRings() const {
    return m_probing || (m_batching && m_runningBatch &&
        (m_hasClones || m_sandbox != nullptr));
}

bool Controller::skipAttempts(PlayLayer* layer) const {
    const bool scannerLayer = layer && (layer == m_level || layer == m_sandbox);
    return scannerLayer && (m_batching || m_internalReset || m_makingBox);
}

std::int64_t Controller::snapLimit(std::int64_t frames) const {
    auto best = kPresets[0];
    auto bestDistance = std::llabs(frames - best);
    for (auto preset : kPresets) {
        const auto distance = std::llabs(frames - preset);
        if (distance < bestDistance) {
            best = preset;
            bestDistance = distance;
        }
    }
    return best;
}

int Controller::windowMax() const {
    
    const auto selected = Mod::get()->getSettingValue<std::string>("maximum-window-frames");
    for (auto preset : kPresets) {
        if (selected == std::to_string(preset)) return preset;
    }
    return kDefaultMax;
}

bool Controller::inRange(std::uint64_t frames) const {
    return static_cast<std::int64_t>(frames) <= windowMax();
}

bool Controller::pastLimit() const {
    return static_cast<std::int64_t>(m_passedFrames) > windowMax();
}

bool Controller::runningBatch(PlayLayer* layer) const {
    return m_batching && m_runningBatch && layer && layer == m_level;
}

bool Controller::batchDead(PlayLayer* layer) const {
    return runningBatch(layer) && (m_alrDied || m_resolved);
}

void Controller::markDeath(PlayLayer* layer, GameObject* deathObject) {
    if (!runningBatch(layer) || m_resolved) return;

    m_resolved = true;
    m_completed = false;
    m_deathAt = rawFrame(layer);
    m_hit = deathObject;
}

void Controller::confirmJump(
    ClickJob& job,
    std::int64_t activationFrame,
    cocos2d::CCPoint const& activationPos,
    bool fromRing,
    bool wasBuffered,
    RingObject* ring,
    bool try_holding
) {
    if (job.activated) {
        if (fromRing) {
            job.fromRing = true;
            job.activationFrame = activationFrame;
            job.activationPos = activationPos;
            if (ring) job.activationRing = ring;
        }
        if (wasBuffered) job.wasBuffered = true;
        job.try_holding = job.try_holding || try_holding;
        return;
    }

    job.activationFrame = activationFrame;
    job.activationPos = activationPos;
    job.activated = true;
    job.fromRing = fromRing;
    job.wasBuffered = wasBuffered || activationFrame > job.pressFrame;
    job.activationRing = ring;
    job.try_holding = try_holding;
    job.clickIndex = ++m_jumpCount;

    if (m_firstDown < 0) {
        m_firstDown = job.pressFrame;
        m_firstBtn = job.button;
        m_firstP1 = job.player1;
        if (job.releaseFrame >= 0) m_firstUp = job.releaseFrame;

    }

}

void Controller::jumpActive(PlayerObject* player, RingObject* ring) {
    const bool fromRing = ring != nullptr;
    if (player && m_probing && player == m_probePlayer) {
        if (!m_probeDown) return;
        if (!m_probeJumped) {
            m_probeJumped = true;
            m_probeJumpAt = m_probeAt;
            m_probeRingJump = fromRing;
            m_ring = ring;
        } else if (fromRing && m_probeJumpAt == m_probeAt) {
            m_probeRingJump = true;
            m_ring = ring;
        }
        return;
    }
    if (!player || !m_level || !m_started || m_platformer) return;

    bool isPlayer1 = false;
    if (player == m_level->m_player1) {
        isPlayer1 = true;
    } else if (player == m_level->m_player2) {
        isPlayer1 = false;
    } else {
        return;
    }

    const auto activationFrame = rawFrame(m_level);
    const auto activationPos = player->getPosition();
    const bool wasBuffered = player->m_jumpBuffered || player->m_wasJumpBuffered;
    const bool try_holding = canHoldCube(player);

    if (m_scanning && m_batching && m_runningBatch) {
        if (isPlayer1 == m_firstP1 &&
            m_firstPressed &&
            !m_jumped) {
            m_jumped = true;
            m_jumpAt = activationFrame;
            m_jumpPos = activationPos;
            m_ringJump = fromRing;
        }
        return;
    }

    if (m_batching || m_replayingInput || m_phase != Phase::Recording || m_attemptDone) {
        return;
    }

    if (fromRing) {
        for (auto it = m_clicks.rbegin(); it != m_clicks.rend(); ++it) {
            if (!it->activated || it->player1 != isPlayer1) continue;
            if (it->activationFrame < activationFrame) break;
            if (it->activationFrame != activationFrame) continue;
            confirmJump(
                *it, activationFrame, activationPos, true, wasBuffered, ring, try_holding
            );
            return;
        }
    }

    for (auto it = m_clicks.rbegin(); it != m_clicks.rend(); ++it) {
        if (it->activated || it->player1 != isPlayer1) continue;
        if (it->pressFrame > activationFrame) continue;
        confirmJump(*it, activationFrame, activationPos, fromRing, wasBuffered, ring, try_holding);
        return;
    }

    if (!m_sendingJump || m_jumpP1 != isPlayer1) {
        return;
    }

    for (auto& pending : m_jumps) {
        if (pending.player1 == isPlayer1 && pending.frame == activationFrame) {
            if (fromRing) {
                pending.fromRing = true;
                pending.ring = ring;
                pending.position = activationPos;
            }
            if (wasBuffered) pending.wasBuffered = true;
            pending.try_holding = pending.try_holding || try_holding;
            return;
        }
    }

    JumpEvent pending;
    pending.frame = activationFrame;
    pending.player1 = isPlayer1;
    pending.position = activationPos;
    pending.fromRing = fromRing;
    pending.wasBuffered = wasBuffered;
    pending.ring = ring;
    pending.try_holding = try_holding;
    m_jumps.push_back(pending);
    while (m_jumps.size() > 8) {
        m_jumps.pop_front();
    }
}

void Controller::applyJump(ClickJob& job) {
    for (auto it = m_jumps.begin(); it != m_jumps.end(); ++it) {
        if (it->player1 != job.player1) continue;
        if (it->frame < job.pressFrame || it->frame > job.pressFrame + 1) continue;
        const auto frame = it->frame;
        const auto position = it->position;
        const bool fromRing = it->fromRing;
        const bool wasBuffered = it->wasBuffered;
        auto* ring = it->ring;
        const bool try_holding = it->try_holding;
        m_jumps.erase(it);
        confirmJump(job, frame, position, fromRing, wasBuffered, ring, try_holding);
        return;
    }
}

bool Controller::preButton(GJBaseGameLayer* base, bool, int, bool) {
    if (!activeLayer(base)) return true;
    if (m_replayingInput) return true;

    if (m_phase == Phase::Restarting || m_phase == Phase::Earlier || m_phase == Phase::Later) {
        return false;
    }
    return true;
}

bool Controller::beforeJump(PlayerObject* player, PlayerButton button, bool down) {
    if (!player || !m_level) return true;

    bool isPlayer1 = false;
    if (player == m_level->m_player1) {
        isPlayer1 = true;
    } else if (player == m_level->m_player2) {
        isPlayer1 = false;
    } else {
        return true;
    }

    if (m_replayingInput) return true;
    if (m_platformer) return true;

    if (m_phase == Phase::Restarting || m_phase == Phase::Earlier || m_phase == Phase::Later) {
        return false;
    }

    if (m_phase != Phase::Recording || m_attemptDone || !m_started) {
        return true;
    }

    if (down && button == PlayerButton::Jump) {
        m_sendingJump = true;
        m_jumpP1 = isPlayer1;
    }
    return true;
}

void Controller::afterJump(
    PlayerObject* player,
    PlayerButton button,
    bool down,
    bool accepted
) {
    m_sendingJump = false;

    if (!player || !m_level || m_replayingInput || m_platformer) return;
    if (m_phase != Phase::Recording || m_attemptDone || !m_started) return;

    bool isPlayer1 = false;
    if (player == m_level->m_player1) {
        isPlayer1 = true;
    } else if (player == m_level->m_player2) {
        isPlayer1 = false;
    } else {
        return;
    }

    const auto atFrame = frame(m_level);

    InputEvent event;
    event.frame = atFrame;
    event.button = button;
    event.down = down;
    event.player2 = !isPlayer1;
    event.order = m_orderCounter++;
    m_events.push_back(event);

    if (button == PlayerButton::Jump && down) {
        ClickJob job;

        job.pressFrame = atFrame;
        job.pressOrder = event.order;
        job.button = static_cast<int>(button);
        job.player1 = isPlayer1;
        job.canScan = true;
        job.pressPos = player->getPosition();

        job.keepHold = true;
        job.ignoreHit = player->m_isShip || player->m_isDart;
        applyJump(job);

        if (!job.activated && accepted &&
            (isFlight(player) || isUfo(player))) {
            confirmJump(job, atFrame, player->getPosition(), false, false, nullptr, false);
        }

        m_clicks.push_back(job);
    } else if (button == PlayerButton::Jump && !down) {
        for (auto it = m_clicks.rbegin(); it != m_clicks.rend(); ++it) {
            if (it->releaseFrame >= 0) continue;
            if (it->button != static_cast<int>(button) || it->player1 != isPlayer1) continue;
            it->releaseFrame = atFrame;
            it->releaseOrder = event.order;
            it->releasePos = player->getPosition();

            it->ignoreHit =
                it->ignoreHit || player->m_isShip || player->m_isDart;
            if (it->clickIndex == 1) {
                m_firstUp = atFrame;
            }
            break;
        }
    }
}

void Controller::preCommands(GJBaseGameLayer* base) {
    if (!activeLayer(base)) return;
    if (m_phase != Phase::Earlier && m_phase != Phase::Later) return;
    if (m_attemptDone || !m_clockReady) return;

    auto* layer = static_cast<PlayLayer*>(base);
    m_lastFrame = frame(layer);

    if (m_lastFrame < m_replayStart) {
        return;
    }
    m_replayStarted = true;

    while (m_playAt < m_replay.size() &&
           m_replay[m_playAt].frame <= m_lastFrame) {
        auto const& event = m_replay[m_playAt];


        const bool targetPress = pressEvent(event);
        if (targetPress) {
            m_firstPressed = true;
            m_pressAt = m_lastFrame;
        }

        m_replayingInput = true;
        base->handleButton(
            event.down,
            static_cast<int>(event.button),
            !event.player2
        );
        m_replayingInput = false;

        if (targetPress && m_scanning && !m_jumped) {
            auto* targetPlayer = m_firstP1 ? layer->m_player1 : layer->m_player2;
            if (targetPlayer &&
                (isFlight(targetPlayer) || isUfo(targetPlayer))) {
                m_jumped = true;
                m_jumpAt = m_lastFrame;
                m_jumpPos = targetPlayer->getPosition();
                m_ringJump = false;
            }
        }


        ++m_playAt;
    }
}

void Controller::postCommands(GJBaseGameLayer* base, float) {
    if (!activeLayer(base) || !m_clockReady) return;
    auto* layer = static_cast<PlayLayer*>(base);
    m_lastFrame = frame(layer);

}

bool Controller::blockDeath(PlayLayer* layer, PlayerObject* player) {
    if (!layer || layer != m_level || !player) return false;
    if (m_resetting || m_phase != Phase::Earlier) return false;
    if (m_attemptDone || m_firstPressed) return false;
    if (player != layer->m_player1 && player != layer->m_player2) return false;

    ++m_earlyDeaths;

    return true;
}

bool Controller::claimDeath(PlayLayer* layer, PlayerObject* player) {
    if (!layer || layer != m_level || !player) return false;
    if (m_alrDied || m_attemptDone || m_resetting) return false;
    if (player != layer->m_player1 && player != layer->m_player2) return false;

    if (m_phase == Phase::Recording && m_firstDown < 0) return false;
    if (m_phase != Phase::Recording &&
        m_phase != Phase::Earlier && m_phase != Phase::Later) return false;

    m_alrDied = true;
    return true;
}

bool Controller::confirmDeath(PlayLayer* layer, PlayerObject* player) {
    if (!layer || layer != m_level || !player) return false;
    const bool confirmed = player->m_isDead || layer->m_playerDied;
    if (!confirmed) {
        m_alrDied = false;

    }
    return confirmed;
}

void Controller::handleDeath(PlayLayer* layer, GameObject* deathObject) {
    if (!layer || layer != m_level || m_attemptDone) return;

    const auto deathFrame = frame(layer);
    m_lastFrame = deathFrame;
    m_attemptDone = true;

    if (m_phase == Phase::Recording) {
        m_baseDeath = deathFrame;
        m_baseEnd = deathFrame;
        m_baseObject = deathObject;

        for (auto& job : m_clicks) {
            if (!job.canScan) continue;
            if (!job.pressDone) job.pressDone = true;
            if (!job.releaseDone) job.releaseDone = true;
            job.canScan = false;
        }

        return;
    }

    if (m_phase == Phase::Earlier && !m_firstPressed) {
        queueRun(Phase::Earlier, m_offset);
        return;
    }

    ++m_checks;
    const auto resolvedPhase = m_phase;
    const auto offsetAt = m_offset;
    const bool passed = passes(deathFrame, deathObject, false);
    if (passed) {
        ++m_passedFrames;
    }

    if (passed && pastLimit()) {
        m_overLimit = true;

        endAxis();
        return;
    }

    if (resolvedPhase == Phase::Earlier) {
        if (passed && canMoveBack(offsetAt - 1)) {
            queueRun(Phase::Earlier, offsetAt - 1);
        } else {
            queueRun(Phase::Later, 1);
        }
        return;
    }

    if (resolvedPhase != Phase::Later) return;
    if (!passed) {
        endAxis();
    } else {
        queueRun(Phase::Later, offsetAt + 1);
    }
}

void Controller::finishLevel(PlayLayer* layer) {
    if (!layer || layer != m_level || m_levelDone) return;
    m_levelDone = true;
    m_blockRestarts = true;

    m_scanning = false;
    m_probing = false;
    m_probePlayer = nullptr;
    m_probeDown = false;
    m_probeJumped = false;
    m_ring = nullptr;
    m_jumps.clear();
    m_sendingJump = false;
    m_clockReady = false;
    m_started = false;

    dropProbe();
    dropSandbox(layer);
    clearRings();

    clearSnaps();
}

bool Controller::completeLevel(PlayLayer* layer) {
    if (!layer || layer != m_level) return true;

    if (m_batching && m_runningBatch) {
        if (m_resolved) {
            return false;
        }
        if (m_phase == Phase::Earlier && !m_firstPressed) {
            ++m_earlyWins;
            return false;
        }
        m_resolved = true;
        m_completed = true;
        m_deathAt = rawFrame(layer);
        m_hit = nullptr;
        return false;
    }
    if (m_phase == Phase::Idle || m_phase == Phase::Finished) {
        finishLevel(layer);
        return true;
    }
    if (m_phase == Phase::Restarting) return false;
    if (m_attemptDone) return false;

    if (m_phase == Phase::Recording) {
        if (m_firstDown < 0) {
            finishLevel(layer);
            return true;
        }

        m_attemptDone = true;
        m_baseEnd = frame(layer);
        m_baseDeath = kFinishFrame;
        m_baseObject = nullptr;

        finishLevel(layer);
        return true;
    }

    if (m_phase != Phase::Earlier && m_phase != Phase::Later) return false;
    if (m_phase == Phase::Earlier && !m_firstPressed) {
        ++m_earlyWins;
        return false;
    }

    m_attemptDone = true;
    ++m_checks;
    ++m_passedFrames;

    if (pastLimit()) {
        m_overLimit = true;
        endAxis();
        return false;
    }

    if (m_phase == Phase::Later) {
        queueRun(Phase::Later, m_offset + 1);
    } else if (canMoveBack(m_offset - 1)) {
        queueRun(Phase::Earlier, m_offset - 1);
    } else {
        queueRun(Phase::Later, 1);
    }
    return false;
}

void Controller::beforeReset(PlayLayer* layer) {
    if (layer != m_level) return;

    if (m_resetDepth == 0) {
        m_blockRestarts = m_levelDone;
    }
    ++m_resetDepth;
    m_resetting = true;

    if (m_internalReset) return;
    m_scanning = false;
    m_probing = false;
    m_probePlayer = nullptr;
    m_probeDown = false;
    m_probeJumped = false;
    m_ring = nullptr;
    m_jumps.clear();
    m_sendingJump = false;
    if (!m_blockRestarts) dropProbe();
}

void Controller::onReset(PlayLayer* layer, bool internal) {
    if (layer != m_level) return;

    if (m_resetDepth > 0) --m_resetDepth;
    m_resetting = m_resetDepth > 0;

    if (!m_resetting) {
        m_levelDone = false;
        m_blockRestarts = false;
    }

    if (internal) {
        m_alrDied = false;
        m_firstPressed = false;
        m_pressAt = -1;
        m_playAt = 0;
        m_clockReady = false;
        m_replayStarted = false;
        m_lastFrame = 0;
        return;
    }

    if (m_phase == Phase::Recording) {
        resetBase(layer);

        return;
    }

    if (m_phase == Phase::Restarting || m_phase == Phase::Earlier || m_phase == Phase::Later) {
        stop(layer);
    }
}

void Controller::afterTick(PlayLayer* layer) {
    if (layer != m_level) return;

    if (m_levelDone) return;
    if (m_batching) {
        if (m_scanning) return;
        stepBatch(layer);
        return;
    }

    if (m_platformer) return;

    moveRings(layer);
    if (!m_hud && countersOn()) buildHUD();

    if (m_phase == Phase::Recording && !m_started &&
        layer->m_player1 && layer->isGameplayActive()) {
        m_started = true;
        startClock(layer);
    }

    if (m_phase == Phase::Recording && m_started && !m_attemptDone && !m_probeLayer) {
        setupProbe(layer);
    }

    if (m_phase == Phase::Restarting && m_restartDelay > 0) {
        --m_restartDelay;
        if (m_restartDelay == 0 && !m_loadQueued) {
            m_loadQueued = true;
            auto* expectedLayer = m_level;
            geode::queueInMainThread([expectedLayer] {
                Controller::get().loadCheckpoint(expectedLayer);
            });
        }
    }

    if (m_finishing) {
        m_finishing = false;
        idle();
    }
}

void Controller::onQuit(PlayLayer* layer) {
    if (layer != m_level) return;
    undoClones(layer);
    showWorld(layer);
    unmuteAudio();
    resetAll();
}

void Controller::onExit(PlayLayer* layer) {
    if (layer != m_level) return;
    if (m_paused) {
        return;
    }
    undoClones(layer);
    showWorld(layer);
    unmuteAudio();
    resetAll();
}

bool Controller::internalReset() const {
    return m_internalReset;
}

void Controller::pauseStart(PlayLayer* layer) {
    if (layer && layer == m_level) {
        m_paused = true;
    }
}

void Controller::pauseEnd(PlayLayer* layer) {
    if (!layer || layer == m_level) m_paused = false;
}

void Controller::afterUpdate(GJBaseGameLayer* base) {
    if (!base || m_probing || m_batching || !m_level) return;
    if (static_cast<GJBaseGameLayer*>(m_level) != base) return;
    if (m_platformer || m_resetting ||
        m_phase != Phase::Recording || !m_started || m_attemptDone) return;
    scanClicks(m_level);
}

bool Controller::activeLayer(GJBaseGameLayer* layer) const {
    return m_level && static_cast<GJBaseGameLayer*>(m_level) == layer;
}

}
