#include "Plugin.h"

#include <algorithm>
#include <limits>

namespace {
const char* modeName(RunMode mode) {
    switch (mode) {
        case RunMode::replaying:
            return "Replaying";
        case RunMode::recording:
            return "Recording";
        default:
            return "Stopped";
    }
}
}

void BakkesTasPlugin::renderControls() {
    ImGui::TextUnformatted("TAS session");
    ImGui::Separator();
    if (ImGui::Button("New TAS")) {
        openNewDialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Start")) {
        enqueue([this] { startTas(); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop (Discard)")) {
        enqueue([this] { stopTas(); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop & Update")) {
        enqueue([this] { stopAndUpdate(); });
    }

    const auto* tas = session_.loaded();
    if (!tas) {
        ImGui::Spacing();
        ImGui::TextWrapped("Create a TAS from the current supported offline session or load one from Files.");
        return;
    }

    auto* editable = session_.loaded();
    const auto maxFrameValue = tas->frames.empty()
        ? std::size_t{0}
        : std::min(tas->frames.size() - 1, static_cast<std::size_t>(std::numeric_limits<int>::max()));
    const auto maxStartFrame = static_cast<int>(maxFrameValue);
    startFrame_ = std::clamp(startFrame_, 0, maxStartFrame);

    ImGui::Spacing();
    ImGui::Text("Status: %s", modeName(session_.mode()));
    ImGui::Text("Frames: %llu", static_cast<unsigned long long>(tas->frames.size()));
    ImGui::Text("Pending: %llu", static_cast<unsigned long long>(session_.pendingFrames()));
    if (session_.mode() == RunMode::replaying) {
        ImGui::Text("Current frame: %llu", static_cast<unsigned long long>(session_.cursor()));
    }

    ImGui::SliderInt("Start frame", &startFrame_, 0, maxStartFrame, "%d");

    const auto historyLengthValue = std::min(
        session_.historyLength(),
        static_cast<std::size_t>(std::numeric_limits<int>::max())
    );
    const auto historyLength = static_cast<int>(historyLengthValue);
    historyTarget_ = std::clamp(historyTarget_, 0, historyLength);
    ImGui::SliderInt("Take history", &historyTarget_, 0, historyLength, "%d");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        const auto target = historyTarget_;
        enqueue([this, target] { moveHistoryTo(target); });
    }

    const auto historyMegabytes = static_cast<double>(session_.historyMemoryBytes()) /
        (1024.0 * 1024.0);
    ImGui::TextDisabled(
        "Revision %llu / %llu, %.2f MB RAM (64 MB limit)",
        static_cast<unsigned long long>(session_.historyPosition()),
        static_cast<unsigned long long>(session_.historyLength()),
        historyMegabytes
    );

    ImGui::Spacing();
    if (ImGui::SliderFloat("Replay speed", &editable->replaySpeed, 0.05f, 1.0f, "%.2fx")) {
        session_.markDirty();
        if (session_.isRunning()) {
            enqueue([this] { applyModeSpeed(); });
        }
    }
    if (ImGui::SliderFloat("Record speed", &editable->recordSpeed, 0.05f, 1.0f, "%.2fx")) {
        session_.markDirty();
        if (session_.isRunning()) {
            enqueue([this] { applyModeSpeed(); });
        }
    }
    if (ImGui::Button("Toggle replay / record speed")) {
        enqueue([this] { toggleSpeed(); });
    }
}
