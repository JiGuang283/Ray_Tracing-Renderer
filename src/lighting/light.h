#ifndef LIGHT_H
#define LIGHT_H

#include "ray.h"
#include "vec3.h"

struct LightSample {
    color Li;    // 到达着色点的辐射亮度 (Radiance)，让积分器自己乘 cos 和除 pdf
    vec3 wi;     // 入射方向 (指向光源)
    double pdf;  // 采样概率密度 (通常是关于立体角的)
    double dist; // 到光源的距离 (用于阴影射线遮挡测试)
    bool is_delta; // 是否是 Delta 光源 (点光源/平行光)
};

class Light {
  public:
    virtual ~Light() = default;

    // 统一采样接口
    // u: [0,1)^2 的随机数，用于面光源采样
    virtual LightSample sample(const point3 &p, const vec2 &u) const = 0;

    // 给定方向的 PDF (用于 MIS)
    virtual double pdf(const point3 &origin, const vec3 &direction) const {
        return 0.0;
    }

    // 是否是 Delta 光源
    virtual bool is_delta() const {
        return false;
    }

    // 是否是无限远环境光源
    virtual bool is_infinite() const {
        return false;
    }

    // BSDF 命中光源时获取辐射度
    virtual color Le(const ray &r) const {
        return color(0, 0, 0);
    }

    // 功率加权选择
    virtual color power() const {
        return color(0, 0, 0);
    }
};

#endif
