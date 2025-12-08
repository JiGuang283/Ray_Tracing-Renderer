#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <random>
#include <thread>

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::sqrt;
using std::unique_ptr;

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double pi = 3.1415926535897932385;

inline constexpr double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double random_double() {

    static thread_local uint32_t seed =
        std::hash<std::thread::id>{}(std::this_thread::get_id());

    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed * 2.3283064365386963e-10;

    // thread_local static std::mt19937 generator(
    //     std::random_device{}() +
    //     static_cast<unsigned>(
    //         std::hash<std::thread::id>{}(std::this_thread::get_id())));
    // thread_local static std::uniform_real_distribution<double> distribution(
    //     0.0, 1.0);
    // return distribution(generator);
}

inline double random_double(double min, double max) noexcept {
    return min + (max - min) * random_double();
}

inline double clamp(double x, double min, double max) noexcept {
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}

inline int random_int(int min, int max) {
    return static_cast<int>(random_double(min, max + 1));
}

#endif