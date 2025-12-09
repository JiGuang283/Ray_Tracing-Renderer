#ifndef AABB_H
#define AABB_H

#include "ray.h"
#include "rtweekend.h"
#include "vec3.h"

class aabb {
  public:
    aabb() : minimum(point3(0, 0, 0)), maximum(point3(0, 0, 0)) {
    }
    aabb(const point3 &a, const point3 &b) {
        minimum = a;
        maximum = b;
    }

    point3 min() const {
        return minimum;
    }
    point3 max() const {
        return maximum;
    }

    bool hit(const ray &r, double t_min, double t_max) const;

  public:
    point3 minimum;
    point3 maximum;
};

inline bool aabb::hit(const ray &r, double t_min, double t_max) const {
    const vec3 &inv_dir = r.inv_direction();
    const int *sign = r.direction_sign();

    for (int a = 0; a < 3; a++) {
        auto t0 = (min()[a] - r.origin()[a]) * inv_dir[a];
        auto t1 = (max()[a] - r.origin()[a]) * inv_dir[a];
        if (sign[a]) {
            std::swap(t0, t1);
        }
        t_min = t0 > t_min ? t0 : t_min;
        t_max = t1 < t_max ? t1 : t_max;
        if (t_max <= t_min) {
            return false;
        }
    }
    return true;
}

inline aabb surrounding_box(aabb box0, aabb box1) {
    point3 small(fmin(box0.min().x(), box1.min().x()),
                 fmin(box0.min().y(), box1.min().y()),
                 fmin(box0.min().z(), box1.min().z()));
    point3 big(fmax(box0.max().x(), box1.max().x()),
               fmax(box0.max().y(), box1.max().y()),
               fmax(box0.max().z(), box1.max().z()));

    return aabb(small, big);
}

#endif