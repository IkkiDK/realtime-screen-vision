#include "svc/timer.h"

#include <algorithm>
#include <thread>

Timer::Timer() : start_(Clock::now()) {}

void Timer::reset() {
    start_ = Clock::now();
}

int64_t Timer::elapsedMs() const {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
}

double Timer::elapsedSec() const {
    return elapsedMs() / 1000.0;
}

bool Timer::hasElapsed(int64_t ms) const {
    return elapsedMs() >= ms;
}

void Timer::humanDelay(int base_ms, int variation_ms) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(-variation_ms, variation_ms);
    int actual = std::max(1, base_ms + dist(rng));
    std::this_thread::sleep_for(std::chrono::milliseconds(actual));
}

void Timer::delay(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

Cooldown::Cooldown(int64_t cooldown_ms) : cooldown_ms_(cooldown_ms) {}

bool Cooldown::ready() {
    if (timer_.hasElapsed(cooldown_ms_)) {
        timer_.reset();
        return true;
    }
    return false;
}

void Cooldown::reset() {
    timer_.reset();
}
