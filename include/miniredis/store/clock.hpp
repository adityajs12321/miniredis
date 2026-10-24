#pragma once

#include <chrono>

namespace miniredis::store {

class Clock {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    virtual ~Clock() = default;
    virtual TimePoint now() const = 0;
};

class SteadyClock final : public Clock {
public:
    TimePoint now() const override { return std::chrono::steady_clock::now(); }
};

}  // namespace miniredis::store
