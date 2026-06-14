#pragma once
#include <chrono>
#include <utility>

class Timer {
  private:
    std::chrono::system_clock::time_point time;

  public:
    auto reset() -> void {
        time = std::chrono::system_clock::now();
    }

    template <class T>
    auto elapsed() -> auto {
        const auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<T>(now - time).count();
    }

    Timer() {
        reset();
    }
};

class FPSCounter {
  private:
    Timer timer;
    int   count = 0;

  public:
    auto tick() -> int {
        count += 1;
        if(timer.elapsed<std::chrono::milliseconds>() < 1000) {
            return -1;
        } else {
            timer.reset();
            return std::exchange(count, 0);
        }
    }
};
