#pragma once

#include "JsonStore.h"
#include "TasSession.h"

#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "imgui.h"
#include "imgui_stdlib.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class BakkesTasPlugin final : public BakkesMod::Plugin::BakkesModPlugin,
                              public BakkesMod::Plugin::PluginSettingsWindow {
public:
    void onLoad() override;
    void onUnload() override;
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t context) override;
    void RenderSettings() override;

private:
    enum class PendingAction {
        none,
        update,
        load,
        remove
    };

    JsonStore store_;
    TasSession session_;
    PluginSettings settings_;
    std::vector<std::filesystem::path> files_;
    std::filesystem::path selectedFile_;
    std::filesystem::path actionFile_;
    PendingAction pendingAction_{PendingAction::none};
    std::string newTasName_{"New TAS"};
    std::recursive_mutex stateMutex_;
    std::size_t activePlayer_{};
    int newTasPlayerCount_{1};
    int startFrame_{};
    int historyTarget_{};
    bool openNewDialog_{};
    bool openConfirmDialog_{};
    bool newTasAcknowledged_{};
    BallTrackLock ballTrackLock_;
    unsigned int pendingBallTouchPlayers_{};
    bool injectingDummyInput_{};
    bool alternateSpeed_{};
    bool undoChordDown_{};
    bool redoChordDown_{};

    void registerCommands();
    void handleInput(CarWrapper car, void* params, const std::string& eventName);
    void handleBallTouch(class BallWrapper ball, void* params);
    void enqueue(std::function<void()> action);
    void createTas(const std::string& name, int playerCount);
    void startTas();
    void stopTas();
    void updateTas();
    void stopAndUpdate();
    void undoLastTake();
    void redoLastTake();
    void moveHistoryTo(int position);
    void toggleSpeed();
    void saveTas();
    void loadTas(const std::filesystem::path& path);
    void deleteTas(const std::filesystem::path& path);
    void refreshFiles();
    void setGameSpeed(float speed);
    void applyModeSpeed();
    void notify(const std::string& message, bool error = false);
    void requestConfirmation(PendingAction action, std::filesystem::path file = {});

    void renderControls();
    void renderLoadedTas();
    void renderFiles();
    void renderPluginSettings();
    void renderDialogs();
    void renderNewTasDialog();
    void renderConfirmationDialog();
};
