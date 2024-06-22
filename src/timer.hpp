#pragma once
#include <chrono>

#include "util/print.hpp"

class Timer {
  private:
    std::chrono::steady_clock::time_point time;

  public:
    auto reset() -> void {
        time = std::chrono::high_resolution_clock::now();
    }

    template <class T>
    auto elapsed() -> auto {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<T>(now - time).count();
    }

    Timer() {
        reset();
    }
};

class FPSTimer {
  private:
    Timer timer;
    int   count = 0;

  public:
    auto tick() -> void {
        count += 1;
        if(timer.elapsed<std::chrono::milliseconds>() < 1000) {
            return;
        }
        print("fps: ", count);
        timer.reset();
        count = 0;
    }
};
