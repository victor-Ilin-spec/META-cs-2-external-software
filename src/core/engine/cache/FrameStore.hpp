#pragma once

#include "FrameData.hpp"

#include <atomic>
#include <cstdint>

// Triple buffer: writer never waits, reader never waits, reader never sees a torn frame.
//
// slots: 0,1,2
// published_ — last completed frame (acquire on read)
// reading_   — slot Latest() last handed out
//
// BeginWrite picks a slot that is neither published nor currently read.
class FrameStore {
public:
    static FrameStore& Get()
    {
        static FrameStore i;
        return i;
    }

    // Logic thread only. Returns a slot that render is not reading.
    FrameData& BeginWrite()
    {
        const int pub = published_.load(std::memory_order_relaxed);
        const int rd = reading_.load(std::memory_order_acquire);
        int slot = write_hint_;
        for (int i = 0; i < 3; ++i)
        {
            if (slot != pub && slot != rd)
                break;
            slot = (slot + 1) % 3;
        }
        write_hint_ = (slot + 1) % 3;
        return slots_[slot];
    }

    void Publish(const FrameData& slot)
    {
        const int idx = static_cast<int>(&slot - slots_);
        published_.store(idx, std::memory_order_release);
        ++seq_;
    }

    // Render / any reader. Pointer is valid until the next two publishes.
    // Use immediately in this frame; do not store across Present.
    const FrameData& Latest() const
    {
        const int idx = published_.load(std::memory_order_acquire);
        reading_.store(idx, std::memory_order_release);
        return slots_[idx];
    }
    uint64_t Seq() const { return seq_.load(std::memory_order_relaxed); }

private:
    FrameData slots_[3]{};
    std::atomic<int> published_{ 0 };
    mutable std::atomic<int> reading_{ 0 };
    std::atomic<uint64_t> seq_{ 0 };
    int write_hint_ = 1;
};
