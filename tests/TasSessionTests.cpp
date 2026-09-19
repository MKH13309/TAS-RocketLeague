#include "JsonCodec.h"
#include "TasSession.h"

#include <initializer_list>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

TasFrame frame(float value) {
    TasFrame result;
    result.input.throttle = value;
    result.car.rigidBody.location.x = value;
    result.ball.location.x = value * 10.0f;
    return result;
}

void requireFrames(const TasSession& session, std::initializer_list<float> values) {
    const auto* tas = session.loaded();
    require(tas != nullptr, "TAS should be loaded");
    require(tas->frames.size() == values.size(), "Frame count mismatch");
    std::size_t index = 0;
    for (const auto value : values) {
        require(
            tas->frames[index].car.rigidBody.location.x == value,
            "Frame value mismatch"
        );
        ++index;
    }
}

void testHistory() {
    TasSession session;
    TasData tas;
    tas.name = "History test";
    session.setTas(std::move(tas), true);

    ControllerInput neutral{};
    require(session.start(), "Initial recording should start");
    require(session.advance(neutral, {}).recordFrame, "Frame should record");
    session.appendRecordedFrame(frame(1.0f));
    session.appendRecordedFrame(frame(2.0f));
    session.stop();
    require(session.commitTake(), "First take should commit");
    requireFrames(session, {1.0f, 2.0f});

    require(session.start(1), "Branch replay should start");
    ControllerInput trigger{};
    trigger.Jump = 1;
    const auto branch = session.advance(trigger, {});
    require(branch.recordFrame && branch.beganRecording, "Branch should record");
    session.appendRecordedFrame(frame(9.0f));
    session.stop();
    require(session.commitTake(), "Second take should commit");
    requireFrames(session, {1.0f, 9.0f});

    require(session.undoLastTake(), "First undo should succeed");
    requireFrames(session, {1.0f, 2.0f});
    require(session.undoLastTake(), "Second undo should succeed");
    requireFrames(session, {});
    require(session.redoLastTake(), "Redo should succeed");
    requireFrames(session, {1.0f, 2.0f});
    require(session.moveHistoryTo(2), "History jump should succeed");
    requireFrames(session, {1.0f, 9.0f});
    require(session.historyMemoryBytes() > 0, "History memory should be tracked");
}

void testCompatibilityJson() {
    TasData source;
    source.name = "Training test";
    source.expected.mode = "custom_training";
    source.expected.map = "Stadium_P";
    source.expected.matchType = "Training";
    source.expected.trainingShot = 4;
    source.expected.hitbox = "Octane";

    const auto decoded = JsonCodec::decodeTas(JsonCodec::encodeTas(source));
    require(decoded.expected.mode == "custom_training", "Mode was not preserved");
    require(decoded.expected.map == "Stadium_P", "Map was not preserved");
    require(decoded.expected.matchType == "Training", "Match type was not preserved");
    require(decoded.expected.trainingShot == 4, "Training shot was not preserved");
}
}

int main() {
    try {
        testHistory();
        testCompatibilityJson();
        std::cout << "TAS core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
