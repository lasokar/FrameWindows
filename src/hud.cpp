#include "Controller.hpp"
#include <Geode/utils/string.hpp>
#include <system_error>

using namespace geode::prelude;
using namespace cocos2d;

namespace frame_windows {

void Controller::clearRings() {
    for (auto const& entry : m_markers) {
        auto* marker = entry.node;
        if (!marker) continue;
        if (marker->getParent()) marker->removeFromParentAndCleanup(true);
        marker->release();
    }
    m_markers.clear();
}

cocos2d::CCPoint Controller::ringPos(PlayLayer* layer, cocos2d::CCPoint position) const {
    if (!layer || !layer->m_objectLayer) return position;
    const auto world = layer->m_objectLayer->convertToWorldSpace(position);
    return layer->convertToNodeSpace(world);
}

void Controller::moveRings(PlayLayer* layer) {
    if (!layer || layer != m_level || !layer->m_objectLayer) return;
    for (auto const& entry : m_markers) {
        if (!entry.node) continue;
        entry.node->setPosition(ringPos(layer, entry.worldPosition));
        entry.node->setRotation(0.f);
        entry.node->setScaleX(1.f);
        entry.node->setScaleY(1.f);
    }
}

std::filesystem::path Controller::sfxPath(int index) const {
    const auto path = Mod::get()->getResourcesDir() / fmt::format("frame{}.mp3", index);
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error ? path : std::filesystem::path{};
}

void Controller::loadSfx() {
    if (m_soundsReady) return;
    auto* audio = FMODAudioEngine::get();
    if (!audio) return;
    bool found = false;
    for (int index = 1; index <= 12; ++index) {
        const auto path = sfxPath(index);
        if (path.empty()) continue;
        const auto filename = geode::utils::string::pathToString(path);
        audio->preloadEffect(filename.c_str());
        found = true;
    }
    m_soundsReady = found;
}

int Controller::soundId() {
    constexpr std::uint32_t kBase = 0x51000000u;
    constexpr std::uint32_t kMask = 0x00ffffffu;
    m_soundSerial = (m_soundSerial + 1u) & kMask;
    if (m_soundSerial == 0u) m_soundSerial = 1u;
    return static_cast<int>(kBase | m_soundSerial);
}

void Controller::playSound(std::uint64_t frames) {
    if (frames == 0 || !soundsOn()) return;
    const int index = static_cast<int>(std::min<std::uint64_t>(frames, 12));

    auto* audio = FMODAudioEngine::get();
    if (!audio) return;
    if (!m_soundsReady) loadSfx();
    const auto path = sfxPath(index);
    if (path.empty()) return;
    const auto filename = geode::utils::string::pathToString(path);

    const int channelID = audio->getNextChannelID();
    const int uniqueID = soundId();

    audio->playEffectAdvanced(
        filename.c_str(),
        1.f,   
        0.f,
        0.5f,  
        1.f,   
        false,
        false,
        0,
        0,
        0,
        0,
        false,
        uniqueID,
        false, 
        false,
        channelID,
        uniqueID,
        0.f,   
        0
    );
}

void Controller::clearHUD() {
    m_counters.clear();
    if (m_hud) {
        if (m_hud->getParent()) {
            m_hud->removeFromParentAndCleanup(true);
        }
        m_hud = nullptr;
    }
}

void Controller::resetHUD() {
    for (auto& row : m_counters) {
        row.count = 0;
        if (row.valueLabel) {
            row.valueLabel->stopAllActions();
            row.valueLabel->setString("0");
            row.valueLabel->setScale(.50f);
            row.valueLabel->setColor(row.baseColor);
        }
    }
}

void Controller::buildHUD() {
    clearHUD();
    if (!m_level || !countersOn() || m_platformer) return;

    auto* root = cocos2d::CCNode::create();
    if (!root) return;

    const auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
    root->setPosition({8.f, winSize.height - 13.f});
    root->setID("frame-windows-counter-hud"_spr);
    m_level->addChild(root, 1000002);
    m_hud = root;

    const auto buckets = bandsForPreset(windowMax());
    m_counters.reserve(buckets.size());
    constexpr float kRowSpacing = 15.f;

    for (std::size_t i = 0; i < buckets.size(); ++i) {
        auto const& bucket = buckets[i];
        const float y = -kRowSpacing * i;
        const auto color = windowMax() > 17
            ? cocos2d::ccColor3B{255, 255, 255}
            : ringColor(static_cast<std::uint64_t>(bucket.minFrames));

        if (auto* rangeLabel = cocos2d::CCLabelBMFont::create(
            fmt::format("{}:", bucket.label).c_str(), "bigFont.fnt"
        )) {
            rangeLabel->setAnchorPoint({0.f, .5f});
            rangeLabel->setPosition({0.f, y});
            rangeLabel->setScale(.44f);
            rangeLabel->setColor(color);
            rangeLabel->setOpacity(245);
            root->addChild(rangeLabel, 2);
        }

        auto* value = cocos2d::CCLabelBMFont::create("0", "bigFont.fnt");
        if (value) {
            value->setAnchorPoint({.5f, .5f});
            value->setPosition({76.f, y});
            value->setScale(.50f);
            value->setColor(color);
            value->setOpacity(255);
            root->addChild(value, 3);
        }

        m_counters.push_back({bucket, 0, value, color});
    }
}

void Controller::pulseHUD(std::uint64_t windowFrames) {
    if (!m_hud || windowFrames == 0) return;

    for (auto& row : m_counters) {
        if (windowFrames < static_cast<std::uint64_t>(row.bucket.minFrames) ||
            windowFrames > static_cast<std::uint64_t>(row.bucket.maxFrames)) {
            continue;
        }

        ++row.count;
        auto* value = row.valueLabel;
        if (!value) return;

        value->stopAllActions();
        const auto text = fmt::format("{}", row.count);
        value->setString(text.c_str());
        value->setColor({255, 255, 255});
        value->setScale(.62f);
        value->runAction(cocos2d::CCSpawn::create(
            cocos2d::CCScaleTo::create(.20f, .50f),
            cocos2d::CCTintTo::create(.32f, row.baseColor.r, row.baseColor.g, row.baseColor.b),
            nullptr
        ));

        const auto glowPath = geode::utils::string::pathToString(Mod::get()->getResourcesDir() / "glow.png");
        auto* glow = cocos2d::CCSprite::create(glowPath.c_str());
        if (!glow) return;

        constexpr float kGlowDiameter = 26.f;
        const auto size = glow->getContentSize();
        if (size.width > 0.f) glow->setScale(kGlowDiameter / size.width);
        glow->setPosition(value->getPosition());
        glow->setOpacity(150);
        m_hud->addChild(glow, 1);
        glow->runAction(cocos2d::CCSequence::create(
            cocos2d::CCFadeOut::create(.5f),
            cocos2d::CCRemoveSelf::create(),
            nullptr
        ));
        return;
    }
}

cocos2d::ccColor3B Controller::ringColor(std::uint64_t windowFrames) const {
    static const cocos2d::ccColor3B kPalette[] = {
        {0x80, 0x80, 0x80}, 
        {0xFF, 0x00, 0x00}, 
        {0xFF, 0x7F, 0x00}, 
        {0xF6, 0xFF, 0x00}, 
        {0xFF, 0xFF, 0xFF}, 
        {0x00, 0xFF, 0x00}, 
        {0x00, 0xFD, 0x81}, 
        {0x00, 0xFF, 0xF9}, 
        {0x00, 0xC4, 0xFF}, 
        {0x08, 0x7B, 0xFF}, 
        {0x06, 0x3A, 0xFF}, 
        {0x0A, 0x00, 0xFF}, 
        {0x3F, 0x01, 0xFF}, 
        {0x7F, 0x08, 0xFF}, 
        {0xBF, 0x10, 0xFF}, 
        {0xFF, 0x18, 0xFF}, 
        {0xFF, 0x1B, 0xFF}, 
        {0xFF, 0x3C, 0xFF}, 
        {0xFF, 0x5D, 0xFF}, 
        {0xFF, 0x7C, 0xFF}, 
    };
    return kPalette[std::min<std::uint64_t>(windowFrames, 19)];
}

cocos2d::CCNode* Controller::newRing(std::uint64_t windowFrames) {
    auto* marker = cocos2d::CCNode::create();
    auto* ring = cocos2d::CCDrawNode::create();
    if (!marker || !ring) return nullptr;

    const auto color = ringColor(windowFrames);
    const cocos2d::ccColor4F clear{0.f, 0.f, 0.f, 0.f};
    const cocos2d::ccColor4F black{0.f, 0.f, 0.f, 1.f};
    const cocos2d::ccColor4F colored{
        color.r / 255.f,
        color.g / 255.f,
        color.b / 255.f,
        1.f
    };

    ring->drawCircle({0.f, 0.f}, 9.0f, clear, 4.0f, black, 96);
    ring->drawCircle({0.f, 0.f}, 9.0f, clear, 2.0f, colored, 96);
    marker->addChild(ring);

    const auto text = windowFrames == 0
        ? std::string("X")
        : fmt::format("{}", windowFrames);
    if (auto* label = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt")) {
        label->setAnchorPoint({1.f, .5f});
        label->setPosition({-13.6f, 0.f});
        label->setScale(.42f);
        label->setColor(color);
        label->setOpacity(255);
        marker->addChild(label, 1);
    }
    return marker;
}

void Controller::attachRing(
    PlayLayer* layer,
    cocos2d::CCPoint const& position,
    std::uint64_t windowFrames,
    bool withSound
) {
    if (!layer || !layer->m_objectLayer) return;

    pulseHUD(windowFrames);
    if (withSound) playSound(windowFrames);
    if (windowFrames == 0) return;
    if (!showRings()) return;

    auto* marker = newRing(windowFrames);
    if (!marker) return;
    marker->retain();
    layer->addChild(marker, 1000000);
    m_markers.push_back({marker, position});
    moveRings(layer);
}

void Controller::addRing(
    PlayLayer* layer,
    ClickJob const& job,
    std::uint64_t pressWindow,
    std::uint64_t releaseWindow,
    bool drawPress,
    bool drawRelease
) {
    if (!layer || !layer->m_objectLayer) return;

    const bool mergeSound = drawPress && drawRelease;
    std::uint64_t soundFrames = 0;
    if (mergeSound) {
        if (pressWindow > 0) soundFrames = pressWindow;
        if (releaseWindow > 0 && (soundFrames == 0 || releaseWindow < soundFrames)) {
            soundFrames = releaseWindow;
        }
    }

    if (drawPress) {
        attachRing(
            layer,
            job.activationPos,
            pressWindow,
            !mergeSound
        );
    }

    if (drawRelease) {
        const auto releasePos =
            releaseFrameOf(job) != job.releaseFrame
                ? job.activationPos
                : job.releasePos;
        attachRing(
            layer,
            releasePos,
            releaseWindow,
            !mergeSound
        );
    }

    if (mergeSound && soundFrames > 0) {
        playSound(soundFrames);
    }
}

}
