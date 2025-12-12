#ifndef VEC3_H
#define VEC3_H

#include "rtweekend.h"
#include <cmath>
#include <iostream>

using std::sqrt;

constexpr double kNearZeroThreshold = 1e-8;

class vec3 {
  public:
    vec3() : e{0, 0, 0} {
    }
    vec3(double e0, double e1, double e2) : e{e0, e1, e2} {
    }

    double x() const noexcept {
        return e[0];
    }
    double y() const noexcept {
        return e[1];
    }
    double z() const noexcept {
        return e[2];
    }

    vec3 operator-() const {
        return vec3(-e[0], -e[1], -e[2]);
    }
    double operator[](int i) const {
        return e[i];
    }
    double &operator[](int i) {
        return e[i];
    }

    vec3 &operator+=(const vec3 &v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    vec3 &operator*=(const double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    vec3 &operator*=(const vec3 &v) {
        e[0] *= v.e[0];
        e[1] *= v.e[1];
        e[2] *= v.e[2];
        return *this;
    }

    vec3 &operator/=(const double t) {
        return *this *= 1 / t;
    }

    double length() const {
        return sqrt(length_squared());
    }

    double length_squared() const noexcept {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    inline static vec3 random() {
        return vec3(random_double(), random_double(), random_double());
    }

    inline static vec3 random(double min, double max) {
        return vec3(random_double(min, max), random_double(min, max),
                    random_double(min, max));
    }

    bool near_zero() const noexcept {
        return (fabs(e[0]) < kNearZeroThreshold) &&
               (fabs(e[1]) < kNearZeroThreshold) &&
               (fabs(e[2]) < kNearZeroThreshold);
    }

  public:
    double e[3];
};

class vec2 {
  public:
    vec2() : e{0, 0} {
    }
    vec2(double e0, double e1) : e{e0, e1} {
    }

    double x() const {
        return e[0];
    }
    double y() const {
        return e[1];
    }

    double operator[](int i) const {
        return e[i];
    }
    double &operator[](int i) {
        return e[i];
    }

    vec2 operator-() const {
        return vec2(-e[0], -e[1]);
    }

    vec2 &operator+=(const vec2 &v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        return *this;
    }

    vec2 &operator*=(double t) {
        e[0] *= t;
        e[1] *= t;
        return *this;
    }

    vec2 &operator/=(double t) {
        return *this *= 1 / t;
    }

    double length_squared() const {
        return e[0] * e[0] + e[1] * e[1];
    }

    double length() const {
        return sqrt(length_squared());
    }

    // 如果需要，可以添加 random() 静态方法，类似于 vec3
    inline static vec2 random() {
        return vec2(random_double(), random_double());
    }

  public:
    double e[2];
};

// vec2 Utility Functions (放在类外)

inline std::ostream &operator<<(std::ostream &out, const vec2 &v) {
    return out << v.e[0] << ' ' << v.e[1];
}

inline vec2 operator+(const vec2 &u, const vec2 &v) {
    return vec2(u.e[0] + v.e[0], u.e[1] + v.e[1]);
}

inline vec2 operator-(const vec2 &u, const vec2 &v) {
    return vec2(u.e[0] - v.e[0], u.e[1] - v.e[1]);
}

inline vec2 operator*(double t, const vec2 &v) {
    return vec2(t * v.e[0], t * v.e[1]);
}

inline vec2 operator*(const vec2 &v, double t) {
    return t * v;
}

inline vec2 operator/(vec2 v, double t) {
    return (1 / t) * v;
}

inline double dot(const vec2 &u, const vec2 &v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1];
}

// Type aliases for vec3
using point3 = vec3; // 3D point
using color = vec3;  // RGB color

// vec3 Utility Functions
inline std::ostream &operator<<(std::ostream &out, const vec3 &v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline vec3 operator+(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline vec3 operator+(const vec3 &u, double t) {
    return vec3(u.e[0] + t, u.e[1] + t, u.e[2] + t);
}

inline vec3 operator+(double t, const vec3 &u) {
    return u + t;
}

inline vec3 operator-(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline vec3 operator*(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline vec3 operator*(double t, const vec3 &v) {
    return vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline vec3 operator*(const vec3 &v, double t) {
    return t * v;
}

inline vec3 operator/(vec3 v, double t) {
    return (1 / t) * v;
}

inline vec3 operator/(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] / v.e[0], u.e[1] / v.e[1], u.e[2] / v.e[2]);
}

inline double dot(const vec3 &u, const vec3 &v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

inline vec3 cross(const vec3 &u, const vec3 &v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline vec3 unit_vector(const vec3 &v) {
    return v / v.length();
}

inline vec3 random_in_unit_sphere() {
    while (true) {
        auto p = vec3::random(-1, 1);
        if (p.length_squared() >= 1)
            continue;
        return p;
    }
}

inline vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

inline vec3 reflect(const vec3 &v, const vec3 &n) {
    return v - 2 * dot(v, n) * n;
}

inline vec3 refract(const vec3 &uv, const vec3 &n, double etai_over_etat) {
    auto cos_theta = fmin(dot(-uv, n), 1.0);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    vec3 r_out_parallel = -sqrt(fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

inline vec3 random_in_unit_disk() {
    while (true) {
        auto p = vec3(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() >= 1)
            continue;
        return p;
    }
}

// 余弦加权半球采样

inline vec3 random_cosine_direction() {
    auto r1 = random_double();
    auto r2 = random_double();
    auto z = sqrt(1 - r2);
    auto phi = 2 * pi * r1;
    auto x = cos(phi) * sqrt(r2);
    auto y = sin(phi) * sqrt(r2);
    return vec3(x, y, z);
}

#endif