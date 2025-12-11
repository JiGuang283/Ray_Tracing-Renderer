#ifndef RR_PATH_INTEGRATOR_H
#define RR_PATH_INTEGRATOR_H

#include "integrator.h"
#include "material.h"
#include "rtweekend.h"
#include <algorithm> 

class RRPathInterator : public Integrator {
  public:
    RRPathInterator() = default;

    void set_max_depth(int depth = 50) override {
        m_max_depth = depth;
    }

    void set_rr_start_depth(int depth) {
        m_rr_start_depth = depth;
    }

    virtual color Li(const ray &r, const hittable &scene,
                     const color &background) const override {
        color throughput(1.0, 1.0, 1.0);
        color L(0.0, 0.0, 0.0);
        ray current_ray = r;

        for (int depth = 0; depth < m_max_depth; ++depth) {
            hit_record rec;

            if (!scene.hit(current_ray, 0.001, infinity, rec)) {
                L += throughput * background;
                break;
            }

            color emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
            L += throughput * emitted;

            ray scattered;
            color attenuation;
            if (!rec.mat_ptr->scatter(current_ray, rec, attenuation,
                                      scattered)) {
                break;
            }
            throughput *= attenuation;

            if (depth >= m_rr_start_depth) {
                double p_survive =
                    std::max({throughput.x(), throughput.y(), throughput.z()});
                p_survive = clamp(p_survive, 0.005, 0.95);

                if (random_double() > p_survive) {
                    break;
                }
                throughput /= p_survive;
            }
            current_ray = scattered;
        }
        return L;
    }

  private:
    int m_max_depth = 50;
    int m_rr_start_depth = 3;
};

#endif