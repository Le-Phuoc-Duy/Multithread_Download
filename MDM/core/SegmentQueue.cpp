#include "SegmentQueue.h"

SegmentQueue::SegmentQueue(std::vector<Segment>& segments)
    : segmentsRef(segments) {
}

std::optional<Segment> SegmentQueue::getNext() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& seg : segmentsRef) {
        if (seg.state == SegmentState::Pending) {
            seg.state = SegmentState::InProgress;
            return seg;
        }
    }
    return std::nullopt;
}

void SegmentQueue::markDone(std::uint64_t segmentIndex) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& seg : segmentsRef) {
        if (seg.index == segmentIndex) {
            seg.state = SegmentState::Done;
            break;
        }
    }
}

bool SegmentQueue::hasPending() const {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& seg : segmentsRef) {
        if (seg.state == SegmentState::Pending) {
            return true;
        }
    }
    return false;
}

std::vector<Segment> SegmentQueue::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx);
    return segmentsRef;
}
