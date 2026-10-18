#include "Controller.hpp"

using namespace geode::prelude;
using namespace cocos2d;

$on_mod(Loaded) {
    auto* mod = Mod::get();
    if (!mod->getSavedValue<bool>("native-settings-migrated", false)) {
        mod->setSettingValue<bool>("show-rings", mod->getSavedValue<bool>("show-rings", true));
        mod->setSettingValue<bool>("show-counters", mod->getSavedValue<bool>("show-counters", true));
        mod->setSettingValue<bool>("ding-sounds", mod->getSavedValue<bool>("ding-sounds", true));

        const auto previousMax = mod->getSavedValue<std::int64_t>(
            "maximum-window-frames", frame_windows::kDefaultMax
        );
        mod->setSettingValue<std::string>(
            "maximum-window-frames",
            std::to_string(frame_windows::Controller::get().snapLimit(previousMax))
        );
        mod->setSavedValue<bool>("native-settings-migrated", true);
    }

    listenForSettingChanges<bool>("show-rings", [](bool enabled) {
        if (!enabled) frame_windows::Controller::get().clearRings();
    });
    listenForSettingChanges<bool>("show-counters", [](bool) {
        frame_windows::Controller::get().buildHUD();
    });
    listenForSettingChanges<std::string>("maximum-window-frames", [](std::string) {
        frame_windows::Controller::get().buildHUD();
    });
}
