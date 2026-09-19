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

TasFrame frame(float player1, float player2 = 0.0f) {
    TasFrame result;
    result.players[0].input.throttle = player1;
    result.players[0].car.rigidBody.location.x = player1;
    result.players[1].input.throttle = player2;
    result.players[1].car.rigidBody.location.x = player2;
    result.ball.location.x = (player1 + player2) * 10.0f;
    return result;
}

void requireFrames(
    const TasSession& session,
    std::size_t player,
    std::initializer_list<float> values
) {
    const auto* tas = session.loaded();
    require(tas != nullptr, "TAS should be loaded");
    require(tas->frames.size() == values.size(), "Frame count mismatch");
    std::size_t index = 0;
    for (const auto value : values) {
        require(
            tas->frames[index].players[player].car.rigidBody.location.x == value,
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
    require(session.commitTake(0), "First take should commit");
    requireFrames(session, 0, {1.0f, 2.0f});

    require(session.start(1), "Branch replay should start");
    ControllerInput trigger{};
    trigger.Jump = 1;
    const auto branch = session.advance(trigger, {});
    require(branch.recordFrame && branch.beganRecording, "Branch should record");
    session.appendRecordedFrame(frame(9.0f));
    session.stop();
    require(session.commitTake(0), "Second take should commit");
    requireFrames(session, 0, {1.0f, 9.0f});

    require(session.undoLastTake(), "First undo should succeed");
    requireFrames(session, 0, {1.0f, 2.0f});
    require(session.undoLastTake(), "Second undo should succeed");
    requireFrames(session, 0, {});
    require(session.redoLastTake(), "Redo should succeed");
    requireFrames(session, 0, {1.0f, 2.0f});
    require(session.moveHistoryTo(2), "History jump should succeed");
    requireFrames(session, 0, {1.0f, 9.0f});
    require(session.historyMemoryBytes() > 0, "History memory should be tracked");
}

void testTwoPlayerHistory() {
    TasSession session;
    TasData tas;
    tas.playerCount = 2;
    session.setTas(std::move(tas), true);

    ControllerInput neutral{};
    require(session.start(), "Player 1 recording should start");
    session.appendRecordedFrame(frame(1.0f, -1.0f));
    session.stop();
    require(session.commitTake(0), "Player 1 take should commit");
    require(session.loaded()->recordedPlayers == 1U, "Player 1 mask missing");

    require(session.start(), "Player 2 replay should start");
    ControllerInput trigger{};
    trigger.Jump = 1;
    require(session.advance(trigger, {}).recordFrame, "Player 2 branch should record");
    session.appendRecordedFrame(frame(1.0f, 8.0f));
    session.stop();
    require(session.commitTake(1), "Player 2 take should commit");
    require(session.loaded()->recordedPlayers == 3U, "Both player masks should be set");
    requireFrames(session, 0, {1.0f});
    requireFrames(session, 1, {8.0f});

    require(session.undoLastTake(), "Player 2 undo should succeed");
    require(session.loaded()->recordedPlayers == 1U, "Player 2 mask should undo");
    requireFrames(session, 1, {-1.0f});
    require(session.redoLastTake(), "Player 2 redo should succeed");
    require(session.loaded()->recordedPlayers == 3U, "Player 2 mask should redo");
}

void testBallTrackLock() {
    BallTrackLock lock;
    lock.reset(true);
    require(!lock.released, "Reference ball should start locked");
    lock.release();
    require(lock.released, "Active-player touch should release ball");
    lock.reset(false);
    require(lock.released, "New recording should start with a live ball");

    TasFrame reference;
    reference.markBallTouch(0);
    require(reference.ballTouchedBy(0), "Player 1 touch ownership missing");
    require(!reference.ballTouchedBy(1), "Player 2 touch ownership was invented");
    lock.reset(true);
    if (reference.ballTouchedBy(1)) {
        lock.release();
    }
    require(!lock.released, "Other-player hit should remain locked");
    if (reference.ballTouchedBy(0)) {
        lock.release();
    }
    require(lock.released, "Edited-player old hit should release the reference");
}

void testJson() {
    TasData source;
    source.name = "Two-player test";
    source.playerCount = 2;
    source.recordedPlayers = 3;
    source.expected.mode = "exhibition";
    source.expected.map = "Stadium_P";
    source.expected.hitbox = "Octane";
    source.startCars[1].rigidBody.location.x = 25.0f;
    source.frames.push_back(frame(4.0f, 7.0f));
    source.frames[0].markBallTouch(1);

    const auto encoded = JsonCodec::encodeTas(source);
    const auto decoded = JsonCodec::decodeTas(encoded);
    require(decoded.schemaVersion == 4, "Schema 4 was not used");
    require(decoded.playerCount == 2, "Player count was not preserved");
    require(decoded.recordedPlayers == 3U, "Recorded track mask was not preserved");
    require(decoded.startCars[1].rigidBody.location.x == 25.0f, "Player 2 start missing");
    require(decoded.frames[0].players[1].input.throttle == 7.0f, "Player 2 frame missing");
    require(decoded.frames[0].ballTouchedBy(1), "Touch ownership was not preserved");

    auto legacy = encoded;
    legacy["schema_version"] = 2;
    legacy.erase("player_count");
    legacy.erase("recorded_players");
    legacy["start"]["car"] = legacy["start"]["players"][0];
    legacy["start"].erase("players");
    for (auto& legacyFrame : legacy["frames"]) {
        legacyFrame["input"] = legacyFrame["players"][0]["input"];
        legacyFrame["car"] = legacyFrame["players"][0]["car"];
        legacyFrame.erase("players");
        legacyFrame.erase("ball_touch_players");
    }
    const auto migrated = JsonCodec::decodeTas(legacy);
    require(migrated.playerCount == 1, "Schema 2 should migrate as single-player");
    require(migrated.schemaVersion == 4, "Schema 2 should migrate to schema 4");
    require(migrated.frames[0].ballTouchedBy(0), "Legacy touch was not inferred");
}
}

int main() {
    try {
        testHistory();
        testTwoPlayerHistory();
        testBallTrackLock();
        testJson();
        std::cout << "TAS core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
