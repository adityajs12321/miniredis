#pragma once

#include <atomic>
#include <chrono>

#include "miniredis/store/clock.hpp"

namespace miniredis::testing {

class ManualClock final : public store::Clock {
public:
    TimePoint now() const override { return TimePoint(Duration(nanos_.load())); }

    void advance(std::chrono::nanoseconds by) { nanos_ += by.count(); }

private:
    using Duration = TimePoint::duration;
    std::atomic<Duration::rep> nanos_{std::chrono::duration_cast<Duration>(std::chrono::hours(1)).count()};
};

}  // namespace miniredis::testing
