#include "Plugin.h"

std::string BakkesTasPlugin::GetPluginName() {
    return "TAS";
}

void BakkesTasPlugin::SetImGuiContext(uintptr_t context) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(context));
}

void BakkesTasPlugin::RenderSettings() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    const auto& io = ImGui::GetIO();
    if (!io.WantTextInput && io.KeyCtrl) {
        if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !undoChordDown_) {
            undoChordDown_ = true;
            enqueue([this] { undoLastTake(); });
        }
        const bool redoPressed = ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
            (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false));
        if (redoPressed && !redoChordDown_) {
            redoChordDown_ = true;
            enqueue([this] { redoLastTake(); });
        }
    }
    if (!io.KeyCtrl) {
        undoChordDown_ = false;
        redoChordDown_ = false;
    }

    if (ImGui::BeginTabBar("TASTabs")) {
        if (ImGui::BeginTabItem("Controls")) {
            renderControls();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Loaded TAS")) {
            renderLoadedTas();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Files")) {
            renderFiles();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            renderPluginSettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    renderDialogs();
}

void BakkesTasPlugin::renderDialogs() {
    renderNewTasDialog();
    renderConfirmationDialog();
}

void BakkesTasPlugin::renderNewTasDialog() {
    if (openNewDialog_) {
        newTasAcknowledged_ = false;
        ImGui::OpenPopup("New TAS");
        openNewDialog_ = false;
    }

    bool open = true;
    if (!ImGui::BeginPopupModal("New TAS", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::InputText("Name", &newTasName_);
    bool twoPlayers = newTasPlayerCount_ == 2;
    if (ImGui::Checkbox("Two-player TAS", &twoPlayers)) {
        newTasPlayerCount_ = twoPlayers ? 2 : 1;
    }
    if (twoPlayers) {
        ImGui::TextDisabled("Requires an offline exhibition match with another car.");
    }

    const bool needsAcknowledgement = settings_.popups.confirmNew && session_.isDirty();
    if (needsAcknowledgement) {
        ImGui::Spacing();
        ImGui::TextWrapped("The loaded TAS has unsaved changes.");
        ImGui::Checkbox("Replace it without saving", &newTasAcknowledged_);
    }
    const bool canCreate = !newTasName_.empty() &&
        (!needsAcknowledgement || newTasAcknowledged_);

    if (ImGui::Button("Create") && canCreate) {
        const auto name = newTasName_;
        const auto playerCount = newTasPlayerCount_;
        enqueue([this, name, playerCount] { createTas(name, playerCount); });
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void BakkesTasPlugin::renderConfirmationDialog() {
    if (openConfirmDialog_) {
        ImGui::OpenPopup("Confirm action");
        openConfirmDialog_ = false;
    }

    bool open = true;
    if (!ImGui::BeginPopupModal("Confirm action", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    switch (pendingAction_) {
        case PendingAction::update:
            ImGui::TextWrapped("Replace the active player track from the branch frame?");
            break;
        case PendingAction::load:
            ImGui::TextWrapped("Load another TAS and discard unsaved changes?");
            break;
        case PendingAction::remove:
            ImGui::TextWrapped("Delete this TAS JSON file permanently?");
            break;
        default:
            ImGui::TextUnformatted("Continue?");
            break;
    }

    if (ImGui::Button("Confirm")) {
        const auto action = pendingAction_;
        const auto file = actionFile_;
        pendingAction_ = PendingAction::none;
        ImGui::CloseCurrentPopup();
        switch (action) {
            case PendingAction::update:
                enqueue([this] { updateTas(); });
                break;
            case PendingAction::load:
                enqueue([this, file] { loadTas(file); });
                break;
            case PendingAction::remove:
                enqueue([this, file] { deleteTas(file); });
                break;
            default:
                break;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        pendingAction_ = PendingAction::none;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
