#include "Plugin.h"

void BakkesTasPlugin::renderPluginSettings() {
    auto& trigger = settings_.triggers;
    ImGui::TextWrapped("Selected inputs leave replay and begin a new recording branch.");
    ImGui::Separator();
    ImGui::Checkbox("Throttle", &trigger.throttle);
    ImGui::Checkbox("Steer", &trigger.steer);
    ImGui::Checkbox("Pitch", &trigger.pitch);
    ImGui::Checkbox("Yaw", &trigger.yaw);
    ImGui::Checkbox("Roll", &trigger.roll);
    ImGui::Checkbox("Jump", &trigger.jump);
    ImGui::Checkbox("Boost", &trigger.boost);
    ImGui::Checkbox("Handbrake", &trigger.handbrake);
    ImGui::SliderFloat("Analog threshold", &trigger.analogThreshold, 0.01f, 0.95f, "%.2f");

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Popup Settings")) {
        auto& popup = settings_.popups;
        ImGui::Checkbox("Confirm replacing a TAS", &popup.confirmNew);
        ImGui::Checkbox("Confirm loading over changes", &popup.confirmLoad);
        ImGui::Checkbox("Confirm updating a take", &popup.confirmUpdate);
        ImGui::Checkbox("Confirm deleting a file", &popup.confirmDelete);
    }

    ImGui::Spacing();
    if (ImGui::Button("Save Settings")) {
        std::string error;
        if (store_.saveSettings(settings_, error)) {
            notify("Settings saved");
        } else {
            notify("Settings save failed: " + error, true);
        }
    }
}
