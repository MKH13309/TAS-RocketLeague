#pragma once

#include "Model.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

class TasSession {
public:
    struct TickDecision {
        const TasFrame* replayFrame{};
        bool recordFrame{};
        bool beganRecording{};
    };

    void setTas(TasData tas, bool dirty);
    void clear();
    bool start(std::size_t startFrame = 0);
    void stop();
    TickDecision advance(const ControllerInput& input, const TriggerSettings& triggers);
    void appendRecordedFrame(TasFrame frame);
    bool commitTake(std::size_t playerIndex = 0);
    bool undoLastTake();
    bool redoLastTake();
    bool moveHistoryTo(std::size_t position);
    void discardTake();
    void markDirty();
    void markSaved();

    TasData* loaded();
    const TasData* loaded() const;
    RunMode mode() const;
    bool isRunning() const;
    bool hasPendingTake() const;
    bool canUndo() const;
    bool canRedo() const;
    bool isDirty() const;
    std::size_t cursor() const;
    std::size_t branchFrame() const;
    std::size_t pendingFrames() const;
    std::size_t historyPosition() const;
    std::size_t historyLength() const;
    std::size_t historyMemoryBytes() const;

private:
    struct HistoryEntry {
        std::size_t branchFrame{};
        unsigned int alternateRecordedPlayers{};
        std::vector<TasFrame> alternateTail;
    };

    static constexpr std::size_t historyLimitBytes_ = 64ULL * 1024ULL * 1024ULL;
    std::optional<TasData> tas_;
    RunMode mode_{RunMode::idle};
    std::size_t cursor_{};
    std::size_t branchFrame_{};
    std::vector<TasFrame> take_;
    std::deque<HistoryEntry> undoStack_;
    std::deque<HistoryEntry> redoStack_;
    bool recordingStarted_{};
    bool dirty_{};

    void beginRecording();
    bool swapHistory(std::deque<HistoryEntry>& from, std::deque<HistoryEntry>& to);
    void trimHistory();
};
