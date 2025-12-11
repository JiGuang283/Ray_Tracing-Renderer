#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "hittable.h"
#include "light.h"
#include "ray.h"
#include "vec3.h"

class Integrator {
  public:
    virtual ~Integrator() = default;
    virtual color Li(const ray &r, const hittable &scene,
                     const color &background) const = 0;
    virtual color Li(const ray &r, const hittable &scene,
                     const color &background,
                     const std::vector<shared_ptr<Light>> &lights) const {
        // 默认实现：调用旧接口，忽略光源
        return Li(r, scene, background);
    }
    virtual void set_max_depth(int depth) = 0;
};

#endif