#pragma once

#include <stdint.h>

#ifndef __min
template <typename T>
inline constexpr T __min(const T& a, const T& b) {
    return (a < b) ? a : b;
}
#endif

#ifndef __max
template <typename T>
inline constexpr T __max(const T& a, const T& b) {
    return (a > b) ? a : b;
}
#endif

inline constexpr float SDL_clamp(float value, float min_value, float max_value) {
    return value < min_value ? min_value : (value > max_value ? max_value : value);
}

#ifdef ARDUINO

#include <Arduino.h>

constexpr float SENSITIVITY  = 5.0f;

inline uint64_t getTime() {
    return micros();
}

#else

#include <chrono>

constexpr float SENSITIVITY  = 4.0f;

uint64_t getTime() {
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()/1000;
}

#endif