#ifndef RAY_H
#define RAY_H

#include "vec3.h"

class ray {
  public:
    ray() = default;
    ray(const point3 &origin, const vec3 &direction, double time = 0.0) noexcept
        : orig(origin), dir(direction), tm(time) {
        inv_dir =
            vec3(1.0 / direction.x(), 1.0 / direction.y(), 1.0 / direction.z());
        dir_sign[0] = (inv_dir.x() < 0);
        dir_sign[1] = (inv_dir.y() < 0);
        dir_sign[2] = (inv_dir.z() < 0);
    }

    point3 origin() const noexcept {
        return orig;
    }
    vec3 direction() const noexcept {
        return dir;
    }
    vec3 inv_direction() const noexcept {
        return inv_dir;
    }
    const int *direction_sign() const noexcept {
        return dir_sign;
    }
    double time() const noexcept {
        return tm;
    }

    point3 at(double t) const noexcept {
        return orig + t * dir;
    }

  private:
    point3 orig;
    vec3 dir;
    vec3 inv_dir;
    int dir_sign[3];
    double tm = 0.0;
};

#endif