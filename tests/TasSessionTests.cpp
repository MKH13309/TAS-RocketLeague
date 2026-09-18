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
}

int main() {
    try {
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
        require(session.historyPosition() == 1, "First history position mismatch");

        require(session.start(1), "Branch replay should start");
        ControllerInput trigger{};
        trigger.Jump = 1;
        const auto branch = session.advance(trigger, {});
        require(branch.recordFrame && branch.beganRecording, "Branch should record");
        session.appendRecordedFrame(frame(9.0f));
        session.stop();
        require(session.commitTake(), "Second take should commit");
        requireFrames(session, {1.0f, 9.0f});
        require(session.historyPosition() == 2, "Second history position mismatch");

        require(session.undoLastTake(), "First undo should succeed");
        requireFrames(session, {1.0f, 2.0f});
        require(session.historyPosition() == 1, "Undo position mismatch");
        require(session.canRedo(), "Redo should be available");

        require(session.undoLastTake(), "Second undo should succeed");
        requireFrames(session, {});
        require(session.historyPosition() == 0, "Base history position mismatch");

        require(session.redoLastTake(), "Redo should succeed");
        requireFrames(session, {1.0f, 2.0f});
        require(session.moveHistoryTo(2), "History jump should succeed");
        requireFrames(session, {1.0f, 9.0f});
        require(session.historyPosition() == 2, "Final history position mismatch");
        require(session.historyMemoryBytes() > 0, "History memory should be tracked");

        std::cout << "TasSession history tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
