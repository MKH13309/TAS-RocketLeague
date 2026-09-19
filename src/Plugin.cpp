#include "Plugin.h"
#include "GameContext.h"
#include "GameMode.h"
#include "WorldState.h"

#include "bakkesmod/wrappers/GameObject/BallWrapper.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <utility>

namespace {
struct BallCarTouchParams {
    std::uintptr_t hitCar{};
    unsigned char hitType{};
};
}

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
    gameWrapper->HookEventWithCallerPost<BallWrapper>(
        "Function TAGame.Ball_TA.OnCarTouch",
        [this](BallWrapper ball, void* params, std::string) {
            handleBallTouch(ball, params);
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
    WorldState::setSessionProtection(*gameWrapper, false);
    setGameSpeed(1.0f);
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
    gameWrapper->UnhookEventPost("Function TAGame.Ball_TA.OnCarTouch");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Tutorial_TA.Destroyed");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed");
}

void BakkesTasPlugin::registerCommands() {
    const auto start = [this](std::vector<std::string> args) {
        enqueue([this, args = std::move(args)] {
            if (args.size() > 1) {
                try {
                    startFrame_ = std::max(0, std::stoi(args.back()));
                } catch (...) {
                    notify("Start frame must be a whole number", true);
                    return;
                }
            }
            startTas();
        });
    };
    cvarManager->registerNotifier(
        "tas_start", start, "Start or restart TAS: tas_start [frame]", PERMISSION_ALL
    );
    cvarManager->registerNotifier(
        "tas_play", start, "Play or restart TAS: tas_play [frame]", PERMISSION_ALL
    );
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
    cvarManager->registerNotifier("tas_player", [this](std::vector<std::string> args) {
        if (args.size() < 2) {
            notify("Usage: tas_player 1 or tas_player 2", true);
            return;
        }
        int player = 0;
        try {
            player = std::stoi(args.back());
        } catch (...) {
            notify("Player must be 1 or 2", true);
            return;
        }
        enqueue([this, player] {
            const auto* tas = session_.loaded();
            if (!tas || player < 1 || player > tas->playerCount) {
                notify("That player track is not available", true);
                return;
            }
            if (session_.isRunning()) {
                notify("Stop the TAS before switching players", true);
                return;
            }
            activePlayer_ = static_cast<std::size_t>(player - 1);
            notify("Active track: Player " + std::to_string(player));
        });
    }, "Select active track: tas_player 1|2", PERMISSION_ALL);
}

void BakkesTasPlugin::handleInput(
    CarWrapper car,
    void* params,
    const std::string&
) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (!params || !car) {
        return;
    }
    const auto localCar = GameContext::localCar(*gameWrapper);
    if (!localCar) {
        return;
    }

    const auto* tas = session_.loaded();
    if (car.memory_address != localCar.memory_address) {
        if (tas && tas->playerCount > 1 && session_.isRunning() &&
            !injectingDummyInput_) {
            *static_cast<ControllerInput*>(params) = ControllerInput{};
        }
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
        WorldState::setSessionProtection(*gameWrapper, false);
        if (discardTake) {
            session_.discardTake();
        }
        notify("TAS stopped because the current mode is not supported", true);
        return;
    }
    if (!session_.isRunning() || !tas) {
        return;
    }

    auto& input = *static_cast<ControllerInput*>(params);
    const auto decision = session_.advance(input, settings_.triggers);
    if (decision.beganRecording) {
        ballTrackLock_.reset(!tas->frames.empty());
        pendingBallTouchPlayers_ = 0;
    }
    bool frameApplied = true;
    if (decision.replayFrame) {
        injectingDummyInput_ = true;
        frameApplied = WorldState::applyFrame(
            *gameWrapper, car, *tas, *decision.replayFrame, activePlayer_, input
        );
        injectingDummyInput_ = false;
    }
    if (!frameApplied) {
        session_.stop();
        WorldState::setSessionProtection(*gameWrapper, false);
        setGameSpeed(1.0f);
        notify("Replay stopped because frame state was unavailable", true);
        return;
    }

    if (decision.recordFrame) {
        const auto recordIndex = session_.branchFrame() + session_.pendingFrames();
        const bool continuePastReference = !tas->frames.empty() &&
            recordIndex >= tas->frames.size();
        const TasFrame* reference = nullptr;
        if (!tas->frames.empty() && !continuePastReference) {
            reference = &tas->frames[recordIndex];
        }
        TasFrame frame;
        injectingDummyInput_ = true;
        const bool frameCaptured = WorldState::captureFrame(
            *gameWrapper, car, input, *tas, reference, continuePastReference,
            activePlayer_, ballTrackLock_, frame
        );
        injectingDummyInput_ = false;
        const auto recordedTouches = pendingBallTouchPlayers_;
        pendingBallTouchPlayers_ = 0;
        if (!frameCaptured) {
            session_.stop();
            WorldState::setSessionProtection(*gameWrapper, false);
            setGameSpeed(1.0f);
            notify("Recording stopped because player state was unavailable", true);
            return;
        }
        frame.ballTouchPlayers |= recordedTouches;
        session_.appendRecordedFrame(std::move(frame));
    }

    if (decision.beganRecording) {
        alternateSpeed_ = false;
        applyModeSpeed();
        notify("Player " + std::to_string(activePlayer_ + 1) + " recording started");
    }
}

void BakkesTasPlugin::handleBallTouch(BallWrapper ball, void* params) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (!params || !ball || session_.mode() != RunMode::recording) {
        return;
    }

    const auto* tas = session_.loaded();
    const auto local = GameContext::localCar(*gameWrapper);
    if (!tas || !local) {
        return;
    }

    const auto playerCount = std::clamp(tas->playerCount, 1, 2);
    const auto active = std::min(
        activePlayer_,
        static_cast<std::size_t>(playerCount - 1)
    );
    const auto* touch = static_cast<const BallCarTouchParams*>(params);
    if (touch->hitCar == local.memory_address) {
        pendingBallTouchPlayers_ |= 1U << active;
        if (!ballTrackLock_.released) {
            ballTrackLock_.release();
            cvarManager->log("[TAS] Ball path released by active-player touch");
        }
        return;
    }

    if (ballTrackLock_.released) {
        if (playerCount > 1) {
            pendingBallTouchPlayers_ |= 1U << (1 - active);
        }
        return;
    }
    if (tas->frames.empty()) {
        return;
    }

    const auto referenceIndex = session_.branchFrame() + session_.pendingFrames();
    if (referenceIndex >= tas->frames.size()) {
        ballTrackLock_.reset(false);
        return;
    }
    WorldState::restoreReferenceBall(ball, tas->frames[referenceIndex].ball);
}

void BakkesTasPlugin::enqueue(std::function<void()> action) {
    gameWrapper->Execute([this, action = std::move(action)](GameWrapper*) mutable {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        action();
    });
}
