#include "Plugin.h"
#include "GameContext.h"
#include "GameMode.h"
#include "WorldState.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

BAKKESMOD_PLUGIN(
    BakkesTasPlugin,
    "TAS",
    "1.0.0",
    PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING
)

void BakkesTasPlugin::onLoad() {
    const auto dataFolder = gameWrapper->GetDataFolder();
    const auto root = dataFolder / "TAS";
    const auto legacyRoot = dataFolder / "BakkesTAS";
    std::error_code migrationError;
    if (!std::filesystem::exists(root) && std::filesystem::exists(legacyRoot)) {
        std::filesystem::copy(
            legacyRoot,
            root,
            std::filesystem::copy_options::recursive,
            migrationError
        );
    }

    std::string error;
    store_.setRoot(root);
    if (!store_.initialize(error)) {
        notify("Storage initialization failed: " + error, true);
    }

    if (auto savedSettings = store_.loadSettings(error)) {
        settings_ = *savedSettings;
    } else if (!error.empty()) {
        notify("Settings could not be loaded: " + error, true);
    }

    refreshFiles();
    registerCommands();
    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.SetVehicleInput",
        [this](CarWrapper car, void* params, std::string eventName) {
            handleInput(car, params, eventName);
        }
    );
    const auto stopOnDestroy = [this](const std::string&) {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        if (session_.isRunning()) {
            stopTas();
        }
    };
    gameWrapper->HookEvent("Function TAGame.GameEvent_Tutorial_TA.Destroyed", stopOnDestroy);
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", stopOnDestroy);
    notify("TAS loaded");
}

void BakkesTasPlugin::onUnload() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    session_.stop();
    setGameSpeed(1.0f);
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Tutorial_TA.Destroyed");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed");
}

void BakkesTasPlugin::registerCommands() {
    cvarManager->registerNotifier("tas_start", [this](std::vector<std::string> args) {
        enqueue([this, args = std::move(args)] {
            if (!args.empty() && args.back() != "tas_start") {
                try {
                    startFrame_ = std::max(0, std::stoi(args.back()));
                } catch (...) {
                    notify("Start frame must be a whole number", true);
                    return;
                }
            }
            startTas();
        });
    }, "Start or restart TAS: tas_start [frame]", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_play", [this](std::vector<std::string> args) {
        enqueue([this, args = std::move(args)] {
            if (!args.empty() && args.back() != "tas_play") {
                try {
                    startFrame_ = std::max(0, std::stoi(args.back()));
                } catch (...) {
                    notify("Start frame must be a whole number", true);
                    return;
                }
            }
            startTas();
        });
    }, "Play or restart TAS: tas_play [frame]", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_stop", [this](std::vector<std::string>) {
        enqueue([this] { stopTas(); });
    }, "Stop TAS replay or recording", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_update", [this](std::vector<std::string>) {
        enqueue([this] { updateTas(); });
    }, "Commit the recorded take", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_stopandupdate", [this](std::vector<std::string>) {
        enqueue([this] { stopAndUpdate(); });
    }, "Stop and commit the recorded take", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_undo", [this](std::vector<std::string>) {
        enqueue([this] { undoLastTake(); });
    }, "Undo the last committed take", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_redo", [this](std::vector<std::string>) {
        enqueue([this] { redoLastTake(); });
    }, "Redo the last reverted take", PERMISSION_ALL);
    cvarManager->registerNotifier("tas_changespeed", [this](std::vector<std::string>) {
        enqueue([this] { toggleSpeed(); });
    }, "Toggle replay and record speed", PERMISSION_ALL);
}

void BakkesTasPlugin::handleInput(
    CarWrapper car,
    void* params,
    const std::string&
) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (!params) {
        return;
    }
    const auto localCar = GameContext::localCar(*gameWrapper);
    if (!localCar || localCar.memory_address != car.memory_address) {
        return;
    }

    const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool z = (GetAsyncKeyState('Z') & 0x8000) != 0;
    const bool y = (GetAsyncKeyState('Y') & 0x8000) != 0;
    const bool undoChord = control && z && !shift;
    const bool redoChord = control && (y || (shift && z));

    if (undoChord && !undoChordDown_) {
        undoChordDown_ = true;
        undoLastTake();
    } else if (!undoChord) {
        undoChordDown_ = false;
    }
    if (redoChord && !redoChordDown_) {
        redoChordDown_ = true;
        redoLastTake();
    } else if (!redoChord) {
        redoChordDown_ = false;
    }

    if (session_.isRunning() && !GameMode::isSupported(*gameWrapper)) {
        const bool discardTake = session_.hasPendingTake();
        session_.stop();
        if (discardTake) {
            session_.discardTake();
        }
        notify("TAS stopped because the current mode is not supported", true);
        return;
    }
    if (!session_.isRunning()) {
        return;
    }

    auto& input = *static_cast<ControllerInput*>(params);
    const auto decision = session_.advance(input, settings_.triggers);
    if (decision.replayFrame &&
        !WorldState::applyFrame(*gameWrapper, car, *decision.replayFrame, input)) {
        session_.stop();
        setGameSpeed(1.0f);
        notify("Replay stopped because frame state was unavailable", true);
        return;
    }

    if (decision.recordFrame) {
        TasFrame frame;
        if (!WorldState::captureFrame(*gameWrapper, car, input, frame)) {
            session_.stop();
            setGameSpeed(1.0f);
            notify("Recording stopped because frame state was unavailable", true);
            return;
        }
        session_.appendRecordedFrame(std::move(frame));
    }

    if (decision.beganRecording) {
        alternateSpeed_ = false;
        applyModeSpeed();
        notify("Replay complete. Recording started");
    }
}

void BakkesTasPlugin::enqueue(std::function<void()> action) {
    gameWrapper->Execute([this, action = std::move(action)](GameWrapper*) mutable {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        action();
    });
}
