#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

static void detachPlayer(
    PlayerObject* player,
    cocos2d::CCNode*& parentOut,
    int& zOut,
    bool& detachedOut
) {
    parentOut = nullptr;
    zOut = 0;
    detachedOut = false;
    if (!player) return;
    auto* parent = player->getParent();
    if (!parent) return;
    parentOut = parent;
    zOut = player->getZOrder();
    player->retain();
    player->removeFromParentAndCleanup(false);
    detachedOut = true;
}

static void reattachPlayer(
    PlayerObject* player,
    cocos2d::CCNode*& parent,
    int zOrder,
    bool& detached
) {
    if (!player || !detached) {
        parent = nullptr;
        detached = false;
        return;
    }
    if (parent && !player->getParent()) {
        parent->addChild(player, zOrder);
    }
    
    player->release();
    parent = nullptr;
    detached = false;
}

static void dropClone(PlayerObject*& clone) {
    if (!clone) return;
    Controller::get().stopTrail(clone);
    if (clone->getParent()) {
        clone->removeFromParentAndCleanup(true);
    }
    clone->release();
    clone = nullptr;
}

static void hideClone(PlayerObject* clone) {
    if (!clone) return;
    clone->setVisible(false);
    clone->setOpacity(0);
}

PlayerObject* Controller::newClone(PlayLayer* layer, PlayerObject* source, bool secondPlayer) {
    if (!layer || !source || !layer->m_objectLayer) return nullptr;

    auto* gameManager = GameManager::sharedState();
    const int cube = gameManager ? gameManager->getPlayerFrame() : 1;
    const int ship = gameManager ? gameManager->getPlayerShip() : 1;

    auto* clone = PlayerObject::create(cube, ship, layer, layer->m_objectLayer, true);
    if (!clone) return nullptr;
    clone->retain();

    clone->copyAttributes(source);
    auto* playerState = PlayerCheckpoint::create();
    if (!playerState) {
        clone->release();
        return nullptr;
    }
    source->saveToCheckpoint(playerState);
    clone->loadFromCheckpoint(playerState);
    syncMode(clone, playerState);

    clone->m_isSecondPlayer = secondPlayer;
    clone->setVisible(false);
    clone->setOpacity(0);
    stopTrail(clone);

    if (auto* parent = source->getParent()) {
        parent->addChild(clone, source->getZOrder());
    }

    return clone;
}

bool Controller::addClones(PlayLayer* layer) {
    if (!layer || layer != m_level) return false;
    if (m_scanning && layer != m_sandbox) {
        return false;
    }
    if (m_hasClones) return true;
    if (!layer->m_player1) return false;

    m_realP1 = layer->m_player1;
    m_realP2 = layer->m_player2;

    m_cloneP1 = newClone(layer, m_realP1, false);
    if (!m_cloneP1) {
        m_realP1 = nullptr;
        m_realP2 = nullptr;
        return false;
    }

    if (m_realP2) {
        m_cloneP2 = newClone(layer, m_realP2, true);
        if (!m_cloneP2) {
            if (m_cloneP1->getParent()) {
                m_cloneP1->removeFromParentAndCleanup(true);
            }
            m_cloneP1->release();
            m_cloneP1 = nullptr;
            m_realP1 = nullptr;
            m_realP2 = nullptr;
            return false;
        }
    }

    detachPlayer(
        m_realP1,
        m_p1Parent,
        m_p1Z,
        m_p1Detached
    );
    detachPlayer(
        m_realP2,
        m_p2Parent,
        m_p2Z,
        m_p2Detached
    );

    layer->m_player1 = m_cloneP1;
    if (m_cloneP2) {
        layer->m_player2 = m_cloneP2;
    }
    m_hasClones = true;

    return true;
}

void Controller::undoClones(PlayLayer* layer) {
    if (!m_hasClones) return;

    reattachPlayer(
        m_realP1,
        m_p1Parent,
        m_p1Z,
        m_p1Detached
    );
    reattachPlayer(
        m_realP2,
        m_p2Parent,
        m_p2Z,
        m_p2Detached
    );

    if (layer) {
        if (m_realP1) layer->m_player1 = m_realP1;
        if (m_realP2) layer->m_player2 = m_realP2;
    }

    dropClone(m_cloneP1);
    dropClone(m_cloneP2);

    m_realP1 = nullptr;
    m_realP2 = nullptr;
    m_p1Parent = nullptr;
    m_p2Parent = nullptr;
    m_p1Z = 0;
    m_p2Z = 0;
    m_p1Detached = false;
    m_p2Detached = false;
    m_hasClones = false;
}

void Controller::hideClones() {
    hideClone(m_cloneP1);
    hideClone(m_cloneP2);
}

void Controller::hideWorld(PlayLayer* layer) {
    if (!layer || layer != m_level || m_hiddenWorld || !layer->m_objectLayer) return;
    m_objectsVisible = layer->m_objectLayer->isVisible();
    layer->m_objectLayer->setVisible(false);
    m_hiddenWorld = true;
}

void Controller::showWorld(PlayLayer* layer) {
    if (!m_hiddenWorld) return;
    if (layer && layer->m_objectLayer) {
        layer->m_objectLayer->setVisible(m_objectsVisible);
    }
    m_hiddenWorld = false;
    m_objectsVisible = true;
}

bool Controller::loadSource(PlayLayer* layer, Snapshot const* source) {
    if (!source || !addClones(layer)) return false;

    m_phase = Phase::Restarting;
    m_attemptDone = true;
    m_clockReady = false;
    resetClick(layer);

    m_internalReset = true;
    layer->resetLevelFromStart();
    m_internalReset = false;
    resetClick(layer);

    if (source->checkpoint) {
        layer->loadFromCheckpoint(source->checkpoint);
        syncPlayers(layer, source->checkpoint);
        resetClick(layer);
    }
    hideClones();
    return true;
}

void Controller::startRun(PlayLayer* layer) {
    m_playAt = 0;
    m_attemptDone = false;
    m_alrDied = false;
    m_firstPressed = false;
    m_pressAt = -1;
    m_replayStarted = true;
    m_resolved = false;
    m_completed = false;
    m_running = false;
    m_steppedExtra = false;
    m_stopFrame = -1;
    m_tickLimit = 0;
    m_ticks = 0;
    m_deathAt = -1;
    m_hit = nullptr;
    m_jumped = false;
    m_jumpAt = -1;
    m_jumpPos = cocos2d::CCPoint{0.f, 0.f};
    m_ringJump = false;
    startClock(layer);
}

bool Controller::setupNoJump(PlayLayer* layer) {
    auto const* source = findSnap(0);
    if (!source) return false;

    if (!loadSource(layer, source)) return false;

    m_pressTarget = m_firstDown;
    m_releaseTarget = -1;
    m_replayStart = source->frame;
    m_replay.clear();
    m_replay.reserve(m_events.size());
    for (auto const& event : m_events) {
        if (m_scanning && event.order > m_cutoffOrder) continue;
        if (pressEvent(event) || releaseEvent(event)) continue;
        if (event.frame > source->frame) {
            m_replay.push_back(event);
        }
    }
    sortInputs(m_replay);

    m_phase = Phase::Later;
    m_offset = 0;
    startRun(layer);
    return true;
}

bool Controller::checkNoJump(PlayLayer* layer) {

    if (!layer || layer != m_level ||
        m_baseDeath < 0 || m_baseDeath == kFinishFrame ||
        m_firstDown < 0 || m_baseDeath <= m_firstDown) {
        return false;
    }
    if (!setupNoJump(layer)) {
        return false;
    }

    const auto compareFrames = compareAt();
    const double toleranceFrames = compareFrames * kDeathTol;

    const auto extraFrames = std::max(kPadding, compareFrames);
    const auto stopFrame = m_baseDeath + extraFrames;
    const auto sourceFrame = m_replayStart;
    auto maxTicks = stopFrame - sourceFrame + kPadding;
    if (maxTicks < 1) maxTicks = 1;

    std::int64_t ticks = 0;
    GJBaseGameLayer* game = layer;
    m_runningBatch = true;
    while (!m_resolved &&
           rawFrame(layer) < stopFrame &&
           ticks < maxTicks) {
        game->update(kStep);
        ++ticks;
    }
    if (!m_resolved && ticks < maxTicks) {
        game->update(kStep);
        ++ticks;
    }
    m_runningBatch = false;
    hideClones();

    if (!m_resolved || m_completed || m_deathAt < 0) {
        return false;
    }

    const bool sameHit =
        m_baseObject && m_hit &&
        m_baseObject == m_hit;

    const auto deathGap = std::llabs(m_deathAt - m_baseDeath);
    const bool withinTime =
        deathGap < toleranceFrames;
    const bool nearDeath =
        deathGap < kDeathGap;

    return sameHit || withinTime || nearDeath;
}

bool Controller::prepOffset(PlayLayer* layer, Phase phase, int offset) {
    auto const* source = findSnap(offset);
    if (!source) return false;

    if (!loadSource(layer, source)) return false;

    if (!prepReplay(offset, source)) return false;

    m_phase = phase;
    m_offset = offset;
    startRun(layer);
    return true;
}

Result Controller::runOffset(
    PlayLayer* layer,
    Phase phase,
    int offset
) {
    if (!m_running) {
        if (!prepOffset(layer, phase, offset)) {
            return Result::Failed;
        }

        const auto sourceFrame = m_replayStart;
        m_stopFrame = std::max<std::int64_t>(
            m_baseEnd,
            shiftAt()
        );
        m_tickLimit = m_stopFrame - sourceFrame + kPadding;
        if (m_tickLimit < 1) m_tickLimit = 1;
        m_ticks = 0;
        m_steppedExtra = false;
        m_running = true;
    } else if (m_phase != phase || m_offset != offset) {
        m_running = false;
        return Result::Failed;
    }

    GJBaseGameLayer* game = layer;
    m_runningBatch = true;
    while (!m_resolved &&
           rawFrame(layer) < m_stopFrame &&
           m_ticks < m_tickLimit) {
        game->update(kStep);
        ++m_ticks;

        if (m_resolved) break;
    }

    if (!m_resolved &&
        m_ticks < m_tickLimit &&
        !m_steppedExtra) {
        game->update(kStep);
        ++m_ticks;
        m_steppedExtra = true;
    }
    m_runningBatch = false;

    hideClones();

    const bool survivedBaseline =
        !m_resolved && rawFrame(layer) >= m_baseEnd;
    const bool missedWindow =
        m_scanning && phase == Phase::Later &&
        shiftAt() > m_baseEnd;
    const bool targetActive = !m_scanning || m_jumped;
    const bool passed = targetActive && !missedWindow && passes(
        m_deathAt,
        m_hit,
        m_completed || survivedBaseline
    );

    ++m_checks;
    if (passed) ++m_passedFrames;
    const bool overLimit =
        !m_checkEdge && passed && pastLimit();

    m_running = false;
    m_ticks = 0;
    m_tickLimit = 0;
    m_stopFrame = -1;
    m_steppedExtra = false;
    if (overLimit) {
        return Result::OverLimit;
    }
    return passed ? Result::Passed : Result::Failed;
}

void Controller::muteAudio() {
    if (m_audioMuted) return;
    auto* audio = FMODAudioEngine::get();
    if (!audio) return;

    m_musicVol = audio->getBackgroundMusicVolume();
    m_sfxVol = audio->getEffectsVolume();
    audio->setBackgroundMusicVolume(0.f);
    audio->setEffectsVolume(0.f);

    if (!m_scanning) {
        audio->stopAllEffects();
    }
    audio->m_queuedEffects.clear();
    m_audioMuted = true;
}

void Controller::clearFx() {
    auto* audio = FMODAudioEngine::get();
    if (!audio) return;
    audio->m_queuedEffects.clear();
    if (!m_scanning) {
        audio->stopAllEffects();
    }
}

void Controller::unmuteAudio() {
    if (!m_audioMuted) return;
    auto* audio = FMODAudioEngine::get();
    if (audio) {
        audio->m_queuedEffects.clear();
        if (!m_scanning) {
            audio->stopAllEffects();
        }
        audio->setBackgroundMusicVolume(m_musicVol);
        audio->setEffectsVolume(m_sfxVol);
    }
    m_audioMuted = false;
}

void Controller::showDisplay(PlayLayer* layer) {
    if (!layer || layer != m_level) return;

    undoClones(layer);

    m_phase = Phase::Restarting;
    m_clockReady = false;
    resetClick(layer);

    m_internalReset = true;
    layer->resetLevelFromStart();
    m_internalReset = false;
    resetClick(layer);

    if (auto const* displaySource = findSnap(0); displaySource && displaySource->checkpoint) {
        layer->loadFromCheckpoint(displaySource->checkpoint);
        syncPlayers(layer, displaySource->checkpoint);
        resetClick(layer);
    }

    clearFx();
}

bool Controller::batchRelease() {
    if (skipRelease() ||
        m_firstUp < m_firstDown ||
        m_releaseOrder == kNoOrder) {
        return false;
    }
    m_scanAxis = ScanAxis::Release;
    m_checks = 1;
    m_passedFrames = 1;
    m_overLimit = false;
    m_nextPhase = hasPrevFrame() ? Phase::Earlier : Phase::Later;
    m_nextOffset = hasPrevFrame() ? -1 : 1;
    return true;
}

bool Controller::nextBatch() {
    if (m_jumpIndex + 1 >= m_targets.size()) return false;
    ++m_jumpIndex;
    pickJump(m_jumpIndex);
    m_scanAxis = ScanAxis::Press;
    m_checks = 1;
    m_passedFrames = 1;
    m_overLimit = false;
    m_nextPhase = m_firstDown > 0 ? Phase::Earlier : Phase::Later;
    m_nextOffset = m_firstDown > 0 ? -1 : 1;
    m_precheckDone = false;
    return true;
}

void Controller::endBatch(PlayLayer* layer) {
    if (!layer || layer != m_level) return;

    m_runningBatch = false;
    m_running = false;
    showDisplay(layer);
    showWorld(layer);
    unmuteAudio();
    m_batching = false;
    idle();
}

bool Controller::isFlight(PlayerObject const* player) {
    return player && (player->m_isShip || player->m_isDart || player->m_isSwing);
}

bool Controller::isUfo(PlayerObject const* player) {
    return player && player->m_isBird;
}

bool Controller::playersReady(PlayLayer* layer) const {
    if (!layer) return false;
    auto* player = m_firstP1 ? layer->m_player1 : layer->m_player2;
    if (!player) return false;

    if (isFlight(player)) return false;

    const bool grounded =
        player->m_stateOnGround != 0 ||
        player->m_isOnGround2 ||
        player->m_isOnGround3 ||
        player->m_isOnGround4 ||
        player->m_lastGroundObject != nullptr;
    return grounded && std::abs(player->getYVelocity()) < 0.05;
}

bool Controller::setupBase(PlayLayer* layer) {
    if (!prepOffset(layer, Phase::Later, 0)) {
        return false;
    }

    const auto releaseFrame = m_releaseTarget >= 0
        ? m_releaseTarget
        : m_pressTarget;
    const auto sourceFrame = m_replayStart;

    const auto jumpDeadline = releaseFrame + kGroundMax;
    const auto hardCutoff = jumpDeadline + kGroundMax;
    auto maxTicks = hardCutoff - sourceFrame + kPadding;
    if (maxTicks < 1) maxTicks = 1;

    int settledFrames = 0;
    std::int64_t ticks = 0;
    std::int64_t effectAt = -1;
    std::int64_t minEndFrame = -1;
    std::int64_t hardStopFrame = hardCutoff;
    bool flying = false;

    GJBaseGameLayer* game = layer;
    m_runningBatch = true;
    while (!m_resolved && ticks < maxTicks) {
        game->update(kStep);
        ++ticks;

        const auto now = rawFrame(layer);

        if (m_jumped && effectAt < 0) {
            effectAt = std::max<std::int64_t>(
                releaseFrame,
                m_jumpAt
            );
            minEndFrame = effectAt + kReleaseDelay;

            auto* targetPlayer = m_firstP1 ? layer->m_player1 : layer->m_player2;
            flying = targetPlayer &&
                isFlight(targetPlayer);
            hardStopFrame = effectAt + (
                flying
                    ? kFlightMax
                    : kGroundMax
            );
        }

        if (!m_jumped) {
            if (now >= jumpDeadline) break;
            continue;
        }

        if (now < minEndFrame) continue;

        if (flying) {
            if (now >= hardStopFrame) break;
            continue;
        }

        if (playersReady(layer)) {
            ++settledFrames;
            if (settledFrames >= kGroundSteps) break;
        } else {
            settledFrames = 0;
        }

        if (now >= hardStopFrame) break;
    }
    m_runningBatch = false;
    hideClones();

    if (!m_jumped) {
        m_running = false;
        return false;
    }

    if (m_resolved) {
        m_baseEnd = m_deathAt >= 0
            ? m_deathAt
            : rawFrame(layer);
        if (m_completed) {
            m_baseDeath = kFinishFrame;
            m_baseObject = nullptr;
        } else {
            m_baseDeath = m_deathAt;
            m_baseObject = m_hit;
        }
    } else {
        m_baseEnd = rawFrame(layer);
        m_baseDeath = kFinishFrame;
        m_baseObject = nullptr;
    }

    m_running = false;
    m_ticks = 0;
    m_tickLimit = 0;
    m_stopFrame = -1;
    m_steppedExtra = false;
    return m_baseEnd > m_firstDown;
}

}
