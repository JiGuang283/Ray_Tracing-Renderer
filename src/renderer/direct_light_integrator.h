#ifndef DIRECT_LIGHT_INTEGRATOR_H
#define DIRECT_LIGHT_INTEGRATOR_H

#include "integrator.h"
#include "material.h"
#include "rtweekend.h"

class DirectLightIntegrator : public Integrator {
  public:
    DirectLightIntegrator() = default;

    void set_max_depth(int depth = 50) override {
        m_max_depth = depth;
    }

    void set_rr_start_depth(int depth) {
        m_rr_start_depth = depth;
    }

    virtual color Li(const ray &r, const hittable &scene,
                     const color &background) const override {
        return Li(r, scene, background, {});
    }

    virtual color
    Li(const ray &r, const hittable &scene, const color &background,
       const std::vector<shared_ptr<Light>> &lights) const override {
        color throughput(1.0, 1.0, 1.0);
        color L(0.0, 0.0, 0.0);
        ray current_ray = r;
        bool specular_bounce = false;

        for (int depth = 0; depth < m_max_depth; ++depth) {
            hit_record rec;
            if (!scene.hit(current_ray, 0.001, infinity, rec)) {
                // Check if there is an environment light in the lights list
                bool found_env = false;
                for (const auto &light : lights) {
                    if (light->is_infinite()) {
                        L += throughput * light->Le(current_ray);
                        found_env = true;
                    }
                }
                if (!found_env) {
                    L += throughput * background;
                }
                break;
            }

            vec3 wo = -unit_vector(current_ray.direction());

            if (depth == 0 || specular_bounce) {
                color emitted = rec.mat_ptr->emitted(rec, wo);
                L += throughput * emitted;
            }

            specular_bounce = rec.mat_ptr->is_specular();

            if (!specular_bounce && !lights.empty()) {
                L += throughput * sample_lights_direct(rec, wo, scene, lights);
            }

            BSDFSample bs;
            if (!rec.mat_ptr->sample(rec, wo, bs)) {
                break;
            }

            if (bs.pdf < 1e-8 && !bs.is_specular) {
                break;
            }

            specular_bounce = bs.is_specular;

            double cos_theta = std::abs(dot(bs.wi, rec.normal));
            if (bs.is_specular) {
                throughput *= bs.f;
            } else {
                throughput *= bs.f * cos_theta / bs.pdf;
            }

            current_ray = ray(rec.p, bs.wi, current_ray.time());

            if (depth >= m_rr_start_depth) {
                double p_survive =
                    std::max({throughput.x(), throughput.y(), throughput.z()});
                p_survive = clamp(p_survive, 0.05, 0.95);
                if (random_double() > p_survive) {
                    break;
                }
                throughput /= p_survive;
            }
        }
        return L;
    }

  private:
    color
    sample_lights_direct(const hit_record &rec, const vec3 &wo,
                         const hittable &scene,
                         const std::vector<shared_ptr<Light>> &lights) const {
        if (lights.empty()) {
            return color(0, 0, 0);
        }
        color L_direct(0, 0, 0);

        int light_idx = random_int(0, lights.size() - 1);
        const auto &light = lights[light_idx];
        double light_pdf = 1.0 / lights.size();

        vec2 u(random_double(), random_double());

        LightSample ls = light->sample(rec.p, u);

        if (ls.pdf > 0 && ls.Li.length_squared() > 0) {
            ray shadow_ray(rec.p, ls.wi, 0);
            hit_record shadow_rec;

            bool in_shadow =
                scene.hit(shadow_ray, 0.001, ls.dist - 0.001, shadow_rec);

            if (!in_shadow) {
                color f = rec.mat_ptr->eval(rec, wo, ls.wi);
                double cos_theta = std::abs(dot(ls.wi, rec.normal));

                if (ls.is_delta) {
                    L_direct += f * ls.Li * cos_theta / light_pdf;
                } else {
                    L_direct += f * ls.Li * cos_theta / (ls.pdf * light_pdf);
                }
            }
        }
        // Clamp high energy samples to reduce fireflies
        double max_radiance = 100.0;
        if (L_direct.x() > max_radiance)
            L_direct = L_direct * (max_radiance / L_direct.x());
        if (L_direct.y() > max_radiance)
            L_direct = L_direct * (max_radiance / L_direct.y());
        if (L_direct.z() > max_radiance)
            L_direct = L_direct * (max_radiance / L_direct.z());

        return L_direct;
    }

    int m_max_depth = 50;
    int m_rr_start_depth = 3;
};

#endif