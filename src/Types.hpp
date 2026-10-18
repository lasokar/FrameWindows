#pragma once

#include <Geode/Geode.hpp>
#include <Geode/fmod/fmod.hpp>
#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGroundLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/cocos/draw_nodes/CCDrawNode.h>
#include <Geode/utils/NodeIDs.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/loader/SettingV3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

struct StateBackup {
    float middleGroundOffsetY;
    float portalY;
    bool gravityRelated;
    bool freeMode;
    bool dualMode;
    float levelFlipping;
    float cameraZoom;
    float targetCameraZoom;
    cocos2d::CCPoint cameraOffset;
    cocos2d::CCPoint cameraPosition;
    cocos2d::CCPoint cameraStepDiff;
    float cameraAngle;
    float targetCameraAngle;
    GameObject* lastPortal1;
    GameObject* lastPortal2;

    static StateBackup capture(GJGameState const& state) {
        return {
            state.m_middleGroundOffsetY,
            state.m_portalY,
            state.m_gravityRelated,
            state.m_isFreeMode,
            state.m_isDualMode,
            state.m_levelFlipping,
            state.m_cameraZoom,
            state.m_targetCameraZoom,
            state.m_cameraOffset,
            state.m_cameraPosition,
            state.m_cameraStepDiff,
            state.m_cameraAngle,
            state.m_targetCameraAngle,
            state.m_lastActivatedPortal1,
            state.m_lastActivatedPortal2,
        };
    }

    void restore(GJGameState& state) const {
        state.m_middleGroundOffsetY = middleGroundOffsetY;
        state.m_portalY = portalY;
        state.m_gravityRelated = gravityRelated;
        state.m_isFreeMode = freeMode;
        state.m_isDualMode = dualMode;
        state.m_levelFlipping = levelFlipping;
        state.m_cameraZoom = cameraZoom;
        state.m_targetCameraZoom = targetCameraZoom;
        state.m_cameraOffset = cameraOffset;
        state.m_cameraPosition = cameraPosition;
        state.m_cameraStepDiff = cameraStepDiff;
        state.m_cameraAngle = cameraAngle;
        state.m_targetCameraAngle = targetCameraAngle;
        state.m_lastActivatedPortal1 = lastPortal1;
        state.m_lastActivatedPortal2 = lastPortal2;
    }
};

static constexpr std::uint64_t kNoOrder = std::numeric_limits<std::uint64_t>::max();
static constexpr std::int64_t kFinishFrame = 999999;
static constexpr double kPhysicsHz = 240.0;
static constexpr std::int64_t kLead = 120; 
static constexpr double kDeathTol = 0.10;
static constexpr std::int64_t kDeathGap = 8;
static constexpr float kStep = 1.f / 240.f;
static constexpr std::int64_t kPadding = 240;
static constexpr std::int64_t kReleaseDelay = 8;
static constexpr std::int64_t kFlightMax = 120;
static constexpr std::int64_t kGroundMax = 720;

static constexpr std::int64_t kCubeLimit = 1440;
static constexpr int kGroundSteps = 6;
static constexpr std::int64_t kDefaultMin = 0;
static constexpr std::int64_t kDefaultMax = 17;
static constexpr std::int64_t kMaxLimit = 240;

static constexpr std::size_t kHistSize = 4096;
static constexpr int kProbeLimit = 720;
static constexpr int kHoldLimit = 1440;
static constexpr std::int64_t kPresets[] = {12, 17, 30, 60, 120, 240};
static constexpr int kHoldSteps[] = {1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 160, 240};
static constexpr int kHoldCount = sizeof(kHoldSteps) / sizeof(kHoldSteps[0]);
static constexpr int kHoldRefine = 3;
static constexpr float kReleaseTol = 0.10f;

enum class Phase {
    Idle,
    Recording,
    Restarting,
    Earlier,
    Later,
    Finished,
};

enum class Result {
    Failed,
    Passed,
    OverLimit,
};

enum class ScanAxis {
    Press,
    Release,
};

struct Band {
    std::int64_t minFrames = 0;
    std::int64_t maxFrames = 0;
    std::string label;
};

struct CountRow {
    Band bucket;
    std::uint64_t count = 0;
    cocos2d::CCLabelBMFont* valueLabel = nullptr;
    cocos2d::ccColor3B baseColor{255, 255, 255};
};

inline std::vector<Band> bandsForPreset(std::int64_t preset) {

    if (preset <= 12) {
        return {
            {9, 12, "9-12"},
            {7, 8, "7-8"},
            {5, 6, "5-6"},
            {4, 4, "4"},
            {3, 3, "3"},
            {2, 2, "2"},
            {1, 1, "1"},
        };
    }
    if (preset <= 17) {
        return {
            {17, 17, "17+"},
            {13, 16, "13+"},
            {9, 12, "9+"},
            {7, 8, "7-8"},
            {5, 6, "5-6"},
            {4, 4, "4"},
            {3, 3, "3"},
            {2, 2, "2"},
            {1, 1, "1"},
        };
    }

    const auto width = std::max<std::int64_t>(1, preset / 6);
    std::vector<Band> result;
    result.reserve(6);
    for (int band = 5; band >= 0; --band) {
        const auto lo = band * width + 1;
        const auto hi = band == 5 ? preset : (band + 1) * width;
        result.push_back({lo, hi, fmt::format("{}-{}", lo, hi)});
    }
    return result;
}

struct Snapshot {
    std::int64_t frame = -1; 
    CheckpointObject* checkpoint = nullptr;
};

struct InputEvent {
    std::int64_t frame = 0;
    PlayerButton button = PlayerButton::Jump;
    bool down = false;
    bool player2 = false;
    bool firstClick = false;
    bool firstRelease = false;
    std::uint64_t order = 0;
};

inline bool inputBefore(InputEvent const& a, InputEvent const& b) {
    if (a.frame != b.frame) return a.frame < b.frame;
    return a.order < b.order;
}

inline void sortInputs(std::vector<InputEvent>& events) {
    std::stable_sort(events.begin(), events.end(), inputBefore);
}

struct JumpTarget {
    std::int64_t frame = -1;
    std::int64_t releaseFrame = -1;
    std::uint64_t pressOrder = kNoOrder;
    std::uint64_t releaseOrder = kNoOrder;
    int button = static_cast<int>(PlayerButton::Jump);
    bool player1 = true;
    bool releaseIgnored = false;
    bool keepHold = true;
};



struct JumpEvent {
    std::int64_t frame = -1;
    bool player1 = true;
    cocos2d::CCPoint position = {0.f, 0.f};
    bool fromRing = false;
    bool wasBuffered = false;
    RingObject* ring = nullptr;
    bool try_holding = false;
};

struct ClickJob {
    std::size_t clickIndex = 0;
    std::int64_t pressFrame = -1;
    std::int64_t releaseFrame = -1;
    std::int64_t activationFrame = -1;
    std::uint64_t pressOrder = kNoOrder;
    std::uint64_t releaseOrder = kNoOrder;
    int button = static_cast<int>(PlayerButton::Jump);
    bool player1 = true;
    cocos2d::CCPoint pressPos = {0.f, 0.f};
    cocos2d::CCPoint activationPos = {0.f, 0.f};
    cocos2d::CCPoint releasePos = {0.f, 0.f};
    bool activated = false;
    bool fromRing = false;
    bool wasBuffered = false;
    RingObject* activationRing = nullptr;

    bool try_holding = false;
    std::int64_t heldFrames = -1;
    bool holdReady = false;
    bool holdSafe = false;
    bool needsRelease = false;
    int baseFailures = 0;
    bool pressDone = false;
    bool releaseDone = false;
    bool canScan = false;

    bool keepHold = true;

    bool ignoreHit = false;
};

struct History {
    PlayerCheckpoint* state = nullptr;
    std::int64_t frame = -1;
    std::array<bool, 4> held{};
    double totalTime = 0.0;

    std::vector<RingObject*> touchingRings;
    gd::unordered_set<int> touchedRings{};
    gd::unordered_set<int> ringRelatedSet{};
    bool touchedRing = false;
    bool touchedCustomRing = false;
    bool stateRingJump = false;
    bool stateRingJump2 = false;
    bool ringJumpRelated = false;

    bool valid = false;
};

struct PathState {
    bool started = false;
    bool activated = false;
    bool died = false;
    bool survivedHorizon = false;
    bool fromRing = false;
    RingObject* activationRing = nullptr;
    std::int64_t targetPressAt = -1;
    std::int64_t targetReleaseAt = -1;
    std::int64_t activationFrame = -1;
    std::int64_t deathFrame = -1;
    std::int64_t resolveFrame = -1;
    GameObject* deathObject = nullptr;
    cocos2d::CCPoint activationPos = {0.f, 0.f};
    cocos2d::CCPoint finalPosition = {0.f, 0.f};
};

struct RingMarker {
    cocos2d::CCNode* node = nullptr;
    cocos2d::CCPoint worldPosition = {0.f, 0.f};
};

struct ActiveState {
    EffectGameObject* object = nullptr;
    bool activated = false;
    bool p1 = false;
    bool p2 = false;
    bool portal = false;
};

struct SpriteBackup {
    cocos2d::CCSprite* sprite = nullptr;
    bool visible = false;
    unsigned char opacity = 255;
    cocos2d::CCPoint position = {0.f, 0.f};
    float scaleX = 1.f;
    float scaleY = 1.f;

    static SpriteBackup capture(cocos2d::CCSprite* sprite);
    void restore() const;
};

struct GroundState {
    GJGroundLayer* layer = nullptr;
    bool visible = false;
    cocos2d::CCPoint position = {0.f, 0.f};
    float scaleX = 1.f;
    float scaleY = 1.f;
    bool showGround = false;
    bool showGround1 = false;
    bool showGround2 = false;
    SpriteBackup ground1;
    SpriteBackup ground2;
    SpriteBackup line;

    static GroundState capture(GJGroundLayer* layer);
    void restore() const;
};

}
