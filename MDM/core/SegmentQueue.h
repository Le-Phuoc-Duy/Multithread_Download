#pragma once
#include <vector>
#include <mutex>
#include <optional>
#include "utils.h"

class SegmentQueue {
public:
    explicit SegmentQueue(std::vector<Segment>& segments);

    std::optional<Segment> getNext();
    void markDone(std::uint64_t segmentIndex);

    bool hasPending() const;
    std::vector<Segment> snapshot() const;

private:
    std::vector<Segment>& segmentsRef;
    mutable std::mutex mtx;
};
