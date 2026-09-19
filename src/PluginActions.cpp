#include "Plugin.h"
#include "GameContext.h"
#include "GameMode.h"
#include "WorldState.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace {
int lastFrameIndex(const TasData& tas) {
    if (tas.frames.empty()) {
        return 0;
    }
    return static_cast<int>(std::min(
        tas.frames.size() - 1,
        static_cast<std::size_t>(std::numeric_limits<int>::max())
    ));
}
}

void BakkesTasPlugin::createTas(const std::string& name, int playerCount) {
    if (session_.isRunning()) {
        stopTas();
    }
    TasData tas;
    tas.name = name;
    tas.playerCount = std::clamp(playerCount, 1, static_cast<int>(maxTasPlayers));
    if (const auto* previous = session_.loaded()) {
        tas.replaySpeed = previous->replaySpeed;
        tas.recordSpeed = previous->recordSpeed;
    }

    std::string error;
    if (!WorldState::capture(*gameWrapper, tas, error)) {
        notify(error, true);
        return;
    }
    session_.setTas(std::move(tas), true);
    ballTrackLock_.reset(false);
    pendingBallTouchPlayers_ = 0;
    activePlayer_ = 0;
    startFrame_ = 0;
    historyTarget_ = 0;
    notify(playerCount > 1 ? "New two-player TAS captured" : "New TAS captured");
}

void BakkesTasPlugin::startTas() {
    const auto* tas = session_.loaded();
    if (!tas) {
        notify("Create or load a TAS first", true);
        return;
    }
    activePlayer_ = std::min<std::size_t>(
        activePlayer_, static_cast<std::size_t>(tas->playerCount - 1)
    );
    if (activePlayer_ == 1 && (tas->recordedPlayers & 1U) == 0) {
        notify("Record and update Player 1 before recording Player 2", true);
        return;
    }

    const bool discardedTake = session_.hasPendingTake();
    if (session_.isRunning()) {
        session_.stop();
        WorldState::setSessionProtection(*gameWrapper, false);
    }
    if (session_.hasPendingTake()) {
        session_.discardTake();
    }
    alternateSpeed_ = false;
    setGameSpeed(1.0f);
    if (discardedTake) {
        notify("Uncommitted take discarded for replay");
    }

    startFrame_ = std::clamp(startFrame_, 0, lastFrameIndex(*tas));
    std::string error;
    if (!WorldState::validate(*gameWrapper, *tas, error)) {
        notify(error, true);
        return;
    }

    const bool restored = startFrame_ == 0
        ? WorldState::restore(*gameWrapper, *tas, activePlayer_, error)
        : WorldState::restoreFrame(
            *gameWrapper,
            *tas,
            tas->frames[static_cast<std::size_t>(startFrame_)],
            activePlayer_,
            error
        );
    if (!restored) {
        notify(error, true);
        return;
    }
    ballTrackLock_.reset(!tas->frames.empty());
    pendingBallTouchPlayers_ = 0;
    WorldState::setSessionProtection(*gameWrapper, true);
    if (!session_.start(static_cast<std::size_t>(startFrame_))) {
        WorldState::setSessionProtection(*gameWrapper, false);
        ballTrackLock_.reset(false);
    pendingBallTouchPlayers_ = 0;
        notify("TAS could not be started", true);
        return;
    }

    alternateSpeed_ = false;
    applyModeSpeed();
    const auto player = std::to_string(activePlayer_ + 1);
    if (session_.mode() == RunMode::replaying) {
        notify("Player " + player + " replay started at frame " + std::to_string(startFrame_));
    } else {
        notify("Player " + player + " recording started");
    }
}

void BakkesTasPlugin::stopTas() {
    if (!session_.isRunning()) {
        WorldState::setSessionProtection(*gameWrapper, false);
        notify("TAS is already stopped", true);
        return;
    }
    const bool discardedTake = session_.hasPendingTake();
    session_.stop();
    WorldState::setSessionProtection(*gameWrapper, false);
    ballTrackLock_.reset(false);
    pendingBallTouchPlayers_ = 0;
    if (discardedTake) {
        session_.discardTake();
    }
    alternateSpeed_ = false;
    setGameSpeed(1.0f);
    notify(discardedTake ? "Stopped. Take discarded" : "Replay stopped");
}

void BakkesTasPlugin::updateTas() {
    if (!session_.commitTake(activePlayer_)) {
        notify("No stopped take is ready to update", true);
        return;
    }
    ballTrackLock_.reset(false);
    pendingBallTouchPlayers_ = 0;
    const auto* tas = session_.loaded();
    startFrame_ = tas ? std::clamp(startFrame_, 0, lastFrameIndex(*tas)) : 0;
    historyTarget_ = static_cast<int>(session_.historyPosition());
    notify("Player " + std::to_string(activePlayer_ + 1) + " take updated");
}

void BakkesTasPlugin::stopAndUpdate() {
    if (session_.isRunning()) {
        session_.stop();
        WorldState::setSessionProtection(*gameWrapper, false);
        alternateSpeed_ = false;
        setGameSpeed(1.0f);
    }
    updateTas();
}

void BakkesTasPlugin::undoLastTake() {
    if (session_.isRunning() || session_.hasPendingTake()) {
        notify("Stop and resolve the pending take before using history", true);
        return;
    }
    if (!session_.undoLastTake()) {
        notify("There is no earlier take revision", true);
        return;
    }
    const auto* tas = session_.loaded();
    startFrame_ = tas ? std::clamp(startFrame_, 0, lastFrameIndex(*tas)) : 0;
    historyTarget_ = static_cast<int>(session_.historyPosition());
    notify("Moved back one take");
}

void BakkesTasPlugin::redoLastTake() {
    if (session_.isRunning() || session_.hasPendingTake()) {
        notify("Stop and resolve the pending take before using history", true);
        return;
    }
    if (!session_.redoLastTake()) {
        notify("There is no later take revision", true);
        return;
    }
    const auto* tas = session_.loaded();
    startFrame_ = tas ? std::clamp(startFrame_, 0, lastFrameIndex(*tas)) : 0;
    historyTarget_ = static_cast<int>(session_.historyPosition());
    notify("Moved forward one take");
}

void BakkesTasPlugin::moveHistoryTo(int position) {
    if (session_.isRunning() || session_.hasPendingTake()) {
        notify("Stop and resolve the pending take before changing history", true);
        historyTarget_ = static_cast<int>(session_.historyPosition());
        return;
    }
    const auto maximum = static_cast<int>(std::min(
        session_.historyLength(),
        static_cast<std::size_t>(std::numeric_limits<int>::max())
    ));
    const auto target = std::clamp(position, 0, maximum);
    if (!session_.moveHistoryTo(static_cast<std::size_t>(target))) {
        historyTarget_ = static_cast<int>(session_.historyPosition());
        return;
    }
    const auto* tas = session_.loaded();
    startFrame_ = tas ? std::clamp(startFrame_, 0, lastFrameIndex(*tas)) : 0;
    historyTarget_ = static_cast<int>(session_.historyPosition());
    notify(
        "Take history " + std::to_string(historyTarget_) +
        " of " + std::to_string(session_.historyLength())
    );
}

void BakkesTasPlugin::toggleSpeed() {
    if (!session_.isRunning()) {
        notify("Start a TAS before changing speed", true);
        return;
    }
    alternateSpeed_ = !alternateSpeed_;
    applyModeSpeed();
    notify(alternateSpeed_ ? "Alternate speed active" : "Mode speed restored");
}

void BakkesTasPlugin::saveTas() {
    const auto* tas = session_.loaded();
    if (!tas) {
        notify("No TAS is loaded", true);
        return;
    }
    if (session_.isRunning() || session_.hasPendingTake()) {
        notify("Use Stop & Update to keep the take, or Stop to discard it", true);
        return;
    }
    std::string error;
    const auto path = store_.save(*tas, error);
    if (path.empty()) {
        notify("Save failed: " + error, true);
        return;
    }
    session_.markSaved();
    refreshFiles();
    selectedFile_ = path;
    notify("Saved " + path.filename().string());
}

void BakkesTasPlugin::loadTas(const std::filesystem::path& path) {
    if (session_.isRunning()) {
        stopTas();
    }
    std::string error;
    auto tas = store_.load(path, error);
    if (!tas) {
        notify("Load failed: " + error, true);
        return;
    }
    session_.setTas(std::move(*tas), false);
    ballTrackLock_.reset(false);
    pendingBallTouchPlayers_ = 0;
    selectedFile_ = path;
    activePlayer_ = 0;
    startFrame_ = 0;
    historyTarget_ = 0;
    alternateSpeed_ = false;
    notify("Loaded " + path.filename().string());
}

void BakkesTasPlugin::deleteTas(const std::filesystem::path& path) {
    std::string error;
    if (!store_.erase(path, error)) {
        notify("Delete failed: " + error, true);
        return;
    }
    if (selectedFile_ == path) {
        selectedFile_.clear();
    }
    refreshFiles();
    notify("Deleted " + path.filename().string());
}

void BakkesTasPlugin::refreshFiles() {
    std::string error;
    files_ = store_.list(error);
    if (!error.empty()) {
        notify("File scan failed: " + error, true);
    }
}

void BakkesTasPlugin::setGameSpeed(float speed) {
    if (!GameMode::isSupported(*gameWrapper)) {
        return;
    }
    auto server = GameContext::server(*gameWrapper);
    if (server) {
        server.SetGameSpeed(std::clamp(speed, 0.01f, 2.0f));
    }
}

void BakkesTasPlugin::applyModeSpeed() {
    const auto* tas = session_.loaded();
    if (!tas || !session_.isRunning()) {
        return;
    }
    const bool replaying = session_.mode() == RunMode::replaying;
    const float normal = replaying ? tas->replaySpeed : tas->recordSpeed;
    const float alternate = replaying ? tas->recordSpeed : tas->replaySpeed;
    setGameSpeed(alternateSpeed_ ? alternate : normal);
}

void BakkesTasPlugin::notify(const std::string& message, bool error) {
    const auto manager = cvarManager;
    gameWrapper->Execute([manager, message, error](GameWrapper* game) {
        manager->log("[TAS] " + message);
        game->Toast("TAS", message, "default", 4.0f, error ? 1 : 0);
    });
}

void BakkesTasPlugin::requestConfirmation(
    PendingAction action,
    std::filesystem::path file
) {
    pendingAction_ = action;
    actionFile_ = std::move(file);
    openConfirmDialog_ = true;
}
