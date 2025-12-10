#ifndef PATH_INTEGRATOR_H
#define PATH_INTEGRATOR_H

#include "integrator.h"
#include "material.h"
#include "rtweekend.h"

class PathIntegrator : public Integrator {
  public:
    PathIntegrator() = default;

    void set_max_depth(int depth = 50) override {
        m_max_depth = depth;
    }

    virtual color Li(const ray &r, const hittable &scene,
                     const color &background) const override {
        return Li_internal(r, scene, background, m_max_depth);
    }

  private:
    color Li_internal(const ray &r, const hittable &scene,
                      const color &background, int depth) const {
        hit_record rec;

        if (depth <= 0) {
            return color(0, 0, 0);
        }

        if (!scene.hit(r, 0.001, infinity, rec)) {
            return background;
        }

        vec3 wo = unit_vector(-r.direction());
        color emitted = rec.mat_ptr->emitted(rec, wo);

        BSDFSample sampled;
        if (!rec.mat_ptr->sample(rec, wo, sampled)) {
            return emitted;
        }

        ray scattered(rec.p, sampled.wi, r.time());
        color throughput = sampled.f;

        if (!sampled.is_specular) {
            double cos_theta = dot(rec.normal, sampled.wi);
            if (cos_theta < 0) cos_theta = 0;
            if (sampled.pdf > 1e-6) {
                throughput = throughput * cos_theta / sampled.pdf;
            } else {
                throughput = color(0, 0, 0);
            }
        }

        return emitted + throughput * Li_internal(scattered, scene, background,
                                                   depth - 1);
    }
    int m_max_depth;
};

#endif