#include "Plugin.h"

#include <algorithm>
#include <limits>
#include <string>

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

const char* trackState(const TasData& tas, int player) {
    return (tas.recordedPlayers & (1U << player)) != 0 ? "Recorded" : "Not recorded";
}
}

void BakkesTasPlugin::renderControls() {
    ImGui::TextUnformatted("TAS session");
    ImGui::Separator();

    if (ImGui::Button("New TAS")) {
        openNewDialog_ = true;
    }

    const auto* tas = session_.loaded();
    if (tas && tas->playerCount > 1) {
        activePlayer_ = std::min<std::size_t>(activePlayer_, 1);
        ImGui::Spacing();
        ImGui::TextUnformatted("Active player track");
        if (!session_.isRunning()) {
            if (ImGui::RadioButton("Player 1", activePlayer_ == 0)) {
                activePlayer_ = 0;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Player 2", activePlayer_ == 1)) {
                activePlayer_ = 1;
            }
        } else {
            ImGui::Text("Player %llu", static_cast<unsigned long long>(activePlayer_ + 1));
        }
        ImGui::TextDisabled("Player 1: %s", trackState(*tas, 0));
        ImGui::SameLine();
        ImGui::TextDisabled("Player 2: %s", trackState(*tas, 1));
        if (activePlayer_ == 1 && (tas->recordedPlayers & 1U) == 0) {
            ImGui::TextWrapped("Record and update Player 1 before starting Player 2.");
        }
    } else {
        activePlayer_ = 0;
    }

    ImGui::Spacing();
    const std::string startLabel = tas && tas->playerCount > 1
        ? "Start Player " + std::to_string(activePlayer_ + 1)
        : "Start";
    if (ImGui::Button(startLabel.c_str())) {
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
    if (session_.mode() == RunMode::recording && !tas->frames.empty()) {
        ImGui::TextDisabled(
            "Ball path: %s",
            ballTrackLock_.released ? "Live rewritten path"
                                    : "Reference before edited-player hit"
        );
    }
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
