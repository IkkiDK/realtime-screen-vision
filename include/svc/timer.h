#pragma once
// ============================================================================
// Timer / Cooldown - Time helpers for real-time loops.
//
//   - Measure elapsed intervals (e.g. "time since last movement").
//   - Sleep with random jitter (to avoid robotic, perfectly-uniform timing).
//   - Rate-limit repeated actions.
// ============================================================================

#include <chrono>
#include <random>

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Timer();

    void reset();
    int64_t elapsedMs() const;
    double elapsedSec() const;
    bool hasElapsed(int64_t ms) const;

    // Sleep for base_ms +/- variation_ms (uniform), so timing is not perfectly
    // periodic. Always sleeps at least 1ms.
    static void humanDelay(int base_ms, int variation_ms = 50);

    // Plain sleep, no jitter.
    static void delay(int ms);

private:
    TimePoint start_;
};

// Minimum interval between repeated actions.
class Cooldown {
public:
    explicit Cooldown(int64_t cooldown_ms);

    // Returns true (and resets) once the interval has elapsed.
    bool ready();
    void reset();

    void setDuration(int64_t ms) { cooldown_ms_ = ms; }
    int64_t durationMs() const { return cooldown_ms_; }

private:
    int64_t cooldown_ms_;
    Timer timer_;
};
