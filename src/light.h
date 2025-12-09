#ifndef LIGHT_H
#define LIGHT_H

#include "vec3.h"

struct LightSample {
    color Li;
    vec3 wi;
    double pdf;
    double dist;
};

class Light {
  public:
    virtual ~Light() = default;
    virtual LightSample sample(const point3 &p) const = 0;
    // 随机采样
    virtual LightSample sample(const point3 &p,
                               const vec2 &u) const = 0; // u 是 [0,1)² 随机数

    // 实现 MIS 时需要使用
    virtual double pdf(const point3 &origin, const vec3 &direction) const {
        return 0.0;
    }
};

#endif
