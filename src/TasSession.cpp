#include "TasSession.h"

#include <algorithm>
#include <iterator>
#include <utility>

void TasSession::setTas(TasData tas, bool dirty) {
    stop();
    tas_ = std::move(tas);
    dirty_ = dirty;
    take_.clear();
    undoStack_.clear();
    redoStack_.clear();
    recordingStarted_ = false;
}

void TasSession::clear() {
    stop();
    tas_.reset();
    take_.clear();
    undoStack_.clear();
    redoStack_.clear();
    dirty_ = false;
    recordingStarted_ = false;
}

bool TasSession::start(std::size_t startFrame) {
    if (!tas_ || mode_ != RunMode::idle || recordingStarted_) {
        return false;
    }
    cursor_ = tas_->frames.empty()
        ? 0
        : std::min(startFrame, tas_->frames.size() - 1);
    branchFrame_ = cursor_;
    take_.clear();
    mode_ = tas_->frames.empty() ? RunMode::recording : RunMode::replaying;
    recordingStarted_ = mode_ == RunMode::recording;
    return true;
}

void TasSession::stop() {
    mode_ = RunMode::idle;
}

TasSession::TickDecision TasSession::advance(
    const ControllerInput& input,
    const TriggerSettings& triggers
) {
    if (!tas_ || mode_ == RunMode::idle) {
        return {};
    }
    if (mode_ == RunMode::recording) {
        return {nullptr, true, false};
    }

    const auto liveInput = InputFrame::capture(input);
    if (triggers.matches(liveInput) || cursor_ >= tas_->frames.size()) {
        beginRecording();
        return {nullptr, true, true};
    }

    const auto* frame = &tas_->frames[cursor_++];
    return {frame, false, false};
}

void TasSession::appendRecordedFrame(TasFrame frame) {
    if (mode_ == RunMode::recording && recordingStarted_) {
        take_.push_back(std::move(frame));
    }
}

bool TasSession::commitTake() {
    if (!tas_ || mode_ != RunMode::idle || !recordingStarted_) {
        return false;
    }

    const auto branch = std::min(branchFrame_, tas_->frames.size());
    HistoryEntry history;
    history.branchFrame = branch;
    history.alternateTail.reserve(tas_->frames.size() - branch);
    history.alternateTail.insert(
        history.alternateTail.end(),
        std::make_move_iterator(tas_->frames.begin() + static_cast<std::ptrdiff_t>(branch)),
        std::make_move_iterator(tas_->frames.end())
    );

    tas_->frames.erase(
        tas_->frames.begin() + static_cast<std::ptrdiff_t>(branch),
        tas_->frames.end()
    );
    tas_->frames.insert(
        tas_->frames.end(),
        std::make_move_iterator(take_.begin()),
        std::make_move_iterator(take_.end())
    );
    undoStack_.push_back(std::move(history));
    redoStack_.clear();
    std::vector<TasFrame>().swap(take_);
    trimHistory();
    recordingStarted_ = false;
    dirty_ = true;
    cursor_ = 0;
    branchFrame_ = 0;
    return true;
}

bool TasSession::undoLastTake() {
    return swapHistory(undoStack_, redoStack_);
}

bool TasSession::redoLastTake() {
    return swapHistory(redoStack_, undoStack_);
}

bool TasSession::moveHistoryTo(std::size_t position) {
    const auto target = std::min(position, historyLength());
    bool changed = false;
    while (undoStack_.size() > target) {
        changed = undoLastTake() || changed;
    }
    while (undoStack_.size() < target && !redoStack_.empty()) {
        changed = redoLastTake() || changed;
    }
    return changed;
}

void TasSession::discardTake() {
    if (mode_ != RunMode::idle) {
        return;
    }
    std::vector<TasFrame>().swap(take_);
    recordingStarted_ = false;
    cursor_ = 0;
    branchFrame_ = 0;
}

void TasSession::markDirty() {
    dirty_ = true;
}

void TasSession::markSaved() {
    dirty_ = false;
}

TasData* TasSession::loaded() {
    return tas_ ? &*tas_ : nullptr;
}

const TasData* TasSession::loaded() const {
    return tas_ ? &*tas_ : nullptr;
}

RunMode TasSession::mode() const {
    return mode_;
}

bool TasSession::isRunning() const {
    return mode_ != RunMode::idle;
}

bool TasSession::hasPendingTake() const {
    return recordingStarted_;
}

bool TasSession::canUndo() const {
    return mode_ == RunMode::idle && !recordingStarted_ && !undoStack_.empty();
}

bool TasSession::canRedo() const {
    return mode_ == RunMode::idle && !recordingStarted_ && !redoStack_.empty();
}

bool TasSession::isDirty() const {
    return dirty_;
}

std::size_t TasSession::cursor() const {
    return cursor_;
}

std::size_t TasSession::branchFrame() const {
    return branchFrame_;
}

std::size_t TasSession::pendingFrames() const {
    return take_.size();
}

std::size_t TasSession::historyPosition() const {
    return undoStack_.size();
}

std::size_t TasSession::historyLength() const {
    return undoStack_.size() + redoStack_.size();
}

std::size_t TasSession::historyMemoryBytes() const {
    const auto bytes = [](const auto& stack) {
        std::size_t result = 0;
        for (const auto& entry : stack) {
            result += entry.alternateTail.capacity() * sizeof(TasFrame);
        }
        return result;
    };
    return bytes(undoStack_) + bytes(redoStack_);
}

void TasSession::beginRecording() {
    branchFrame_ = cursor_;
    take_.clear();
    recordingStarted_ = true;
    mode_ = RunMode::recording;
}

bool TasSession::swapHistory(
    std::deque<HistoryEntry>& from,
    std::deque<HistoryEntry>& to
) {
    if (!tas_ || mode_ != RunMode::idle || recordingStarted_ || from.empty()) {
        return false;
    }

    auto history = std::move(from.back());
    from.pop_back();
    const auto branch = std::min(history.branchFrame, tas_->frames.size());
    std::vector<TasFrame> currentTail;
    currentTail.reserve(tas_->frames.size() - branch);
    currentTail.insert(
        currentTail.end(),
        std::make_move_iterator(tas_->frames.begin() + static_cast<std::ptrdiff_t>(branch)),
        std::make_move_iterator(tas_->frames.end())
    );
    tas_->frames.erase(
        tas_->frames.begin() + static_cast<std::ptrdiff_t>(branch),
        tas_->frames.end()
    );
    tas_->frames.insert(
        tas_->frames.end(),
        std::make_move_iterator(history.alternateTail.begin()),
        std::make_move_iterator(history.alternateTail.end())
    );
    history.alternateTail = std::move(currentTail);
    to.push_back(std::move(history));
    dirty_ = true;
    cursor_ = 0;
    branchFrame_ = 0;
    return true;
}

void TasSession::trimHistory() {
    while (undoStack_.size() > 1 && historyMemoryBytes() > historyLimitBytes_) {
        undoStack_.pop_front();
    }
}
