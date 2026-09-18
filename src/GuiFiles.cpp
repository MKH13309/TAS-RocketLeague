#include "Plugin.h"

void BakkesTasPlugin::renderLoadedTas() {
    auto* tas = session_.loaded();
    if (!tas) {
        ImGui::TextWrapped("No TAS is loaded.");
        return;
    }

    ImGui::TextUnformatted(session_.isDirty() ? "Unsaved changes" : "Saved state");
    ImGui::Separator();
    if (ImGui::InputText("Name", &tas->name)) {
        session_.markDirty();
    }
    ImGui::Text("Map: %s", tas->expected.map.c_str());
    ImGui::Text("Expected hitbox: %s", tas->expected.hitbox.c_str());
    ImGui::Text("Steer sensitivity: %.3f", tas->expected.steerSensitivity);
    ImGui::Text("Air sensitivity: %.3f", tas->expected.airSensitivity);
    ImGui::Text("Frames: %llu", static_cast<unsigned long long>(tas->frames.size()));
    ImGui::Text("Replay speed: %.2fx", tas->replaySpeed);
    ImGui::Text("Record speed: %.2fx", tas->recordSpeed);
    ImGui::Spacing();

    if (ImGui::Button("Save TAS")) {
        saveTas();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("JSON folder: %s", store_.root().string().c_str());
}

void BakkesTasPlugin::renderFiles() {
    if (ImGui::Button("Refresh")) {
        refreshFiles();
    }
    ImGui::SameLine();
    ImGui::Text("%llu file(s)", static_cast<unsigned long long>(files_.size()));
    ImGui::Separator();

    if (files_.empty()) {
        ImGui::TextWrapped("No saved TAS files were found.");
    } else {
        ImGui::BeginChild("TASFiles", ImVec2(0.0f, 220.0f), true);
        for (const auto& file : files_) {
            const bool selected = file == selectedFile_;
            if (ImGui::Selectable(file.stem().string().c_str(), selected)) {
                selectedFile_ = file;
            }
        }
        ImGui::EndChild();
    }

    if (selectedFile_.empty()) {
        return;
    }

    ImGui::Text("Selected: %s", selectedFile_.filename().string().c_str());
    if (ImGui::Button("Load")) {
        if (settings_.popups.confirmLoad && session_.isDirty()) {
            requestConfirmation(PendingAction::load, selectedFile_);
        } else {
            const auto file = selectedFile_;
            enqueue([this, file] { loadTas(file); });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (settings_.popups.confirmDelete) {
            requestConfirmation(PendingAction::remove, selectedFile_);
        } else {
            const auto file = selectedFile_;
            enqueue([this, file] { deleteTas(file); });
        }
    }
}
