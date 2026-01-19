#ifndef MIS_PATH_INTEGRATOR_H
#define MIS_PATH_INTEGRATOR_H

#include "integrator.h"
#include "material.h"
#include "rtweekend.h"

class MISPathIntegrator : public Integrator {
  public:
    MISPathIntegrator() = default;

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
        double prev_bsdf_pdf = 0.0; // 上一次 BSDF 采样的 pdf

        for (int depth = 0; depth < m_max_depth; ++depth) {
            hit_record rec;

            if (!scene.hit(current_ray, 0.001, infinity, rec)) {
                color env_L(0, 0, 0);
                bool found_env = false;

                for (const auto &light : lights) {
                    if (light->is_infinite()) {
                        env_L += light->Le(current_ray);
                        found_env = true;
                    }
                }

                if (!found_env) {
                    L += throughput * background;
                } else {
                    if (depth == 0 || specular_bounce) {
                        L += throughput * env_L;
                    } else {
                        double light_pdf = 0.0;
                        double light_select_pdf = 1.0 / lights.size();
                        for (const auto &light : lights) {
                            light_pdf += light->pdf(current_ray.origin(),
                                                    current_ray.direction()) *
                                         light_select_pdf;
                        }
                        double mis_weight =
                            power_heuristic(prev_bsdf_pdf, light_pdf);
                        L += throughput * env_L * mis_weight;
                    }
                }
                break;
            }

            vec3 wo = -unit_vector(current_ray.direction());

            // 处理发射光（带 MIS 权重）
            color emitted = rec.mat_ptr->emitted(rec, wo);
            if (emitted.length_squared() > 0) {
                color L_emit(0, 0, 0);
                if (depth == 0 || specular_bounce) {
                    // 第一次命中或镜面反射后：无 MIS
                    L_emit = throughput * emitted;
                } else if (!lights.empty()) {
                    // 计算 MIS 权重（BSDF 采样命中光源）
                    double light_pdf =
                        compute_light_pdf(rec, wo, lights, current_ray);
                    double mis_weight =
                        power_heuristic(prev_bsdf_pdf, light_pdf);
                    L_emit = throughput * emitted * mis_weight;
                } else {
                    L_emit = throughput * emitted;
                }

                if (depth == 0) {
                    L += L_emit;
                } else {
                    L += clamp_radiance(L_emit);
                }
            }

            specular_bounce = rec.mat_ptr->is_specular();

            // 对于非镜面材质，进行显式光源采样（带 MIS）
            if (!specular_bounce && !lights.empty()) {
                color L_direct =
                    throughput * sample_lights_mis(rec, wo, scene, lights);
                L += clamp_radiance(L_direct);
            }

            // BSDF 采样
            BSDFSample bs;
            if (!rec.mat_ptr->sample(rec, wo, bs)) {
                ray scattered;
                color attenuation;
                if (!rec.mat_ptr->scatter(current_ray, rec, attenuation,
                                          scattered)) {
                    break;
                }
                throughput *= attenuation;
                current_ray = scattered;
                specular_bounce = false;
                prev_bsdf_pdf = 0.0;
            } else {
                if (bs.pdf < 1e-8 && !bs.is_specular) {
                    break;
                }

                specular_bounce = bs.is_specular;
                prev_bsdf_pdf = bs.is_specular ? 0.0 : bs.pdf;

                double cos_theta = std::abs(dot(bs.wi, rec.normal));
                if (bs.is_specular) {
                    throughput *= bs.f;
                } else {
                    throughput *= bs.f * cos_theta / bs.pdf;
                }

                current_ray = ray(rec.p, bs.wi, current_ray.time());
            }

            // 俄罗斯轮盘赌
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
    // Clamping helper to reduce fireflies
    static color clamp_radiance(const color &L, double max_value = 100.0) {
        if (L.x() > max_value || L.y() > max_value || L.z() > max_value) {
            double max_c = std::max({L.x(), L.y(), L.z()});
            if (max_c > max_value) {
                return L * (max_value / max_c);
            }
        }
        return L;
    }

    // Power heuristic for MIS (beta = 2)
    static double power_heuristic(double pdf_a, double pdf_b) {
        double a2 = pdf_a * pdf_a;
        double b2 = pdf_b * pdf_b;
        double denom = a2 + b2;
        return denom > 0 ? a2 / denom : 0.0;
    }

    // 计算 BSDF 采样方向对应的光源 PDF
    double compute_light_pdf(const hit_record &rec, const vec3 &wo,
                             const std::vector<shared_ptr<Light>> &lights,
                             const ray &current_ray) const {
        // 这里需要知道命中的是哪个光源，然后计算其 pdf
        // 简化实现：遍历所有光源计算 pdf
        double total_pdf = 0.0;
        double light_select_pdf = 1.0 / lights.size();

        for (const auto &light : lights) {
            total_pdf +=
                light->pdf(current_ray.origin(), current_ray.direction()) *
                light_select_pdf;
        }

        return total_pdf;
    }

    // 显式光源采样（带 MIS 权重）
    color
    sample_lights_mis(const hit_record &rec, const vec3 &wo,
                      const hittable &scene,
                      const std::vector<shared_ptr<Light>> &lights) const {
        if (lights.empty())
            return color(0, 0, 0);

        color L_direct(0, 0, 0);

        // 随机选择一个光源
        int light_idx = random_int(0, lights.size() - 1);
        const auto &light = lights[light_idx];
        double light_select_pdf = 1.0 / lights.size();

        vec2 u(random_double(), random_double());
        LightSample ls = light->sample(rec.p, u);

        if (ls.pdf > 0 && ls.Li.length_squared() > 0) {
            // 阴影测试
            ray shadow_ray(rec.p, ls.wi, 0);
            hit_record shadow_rec;
            bool in_shadow =
                scene.hit(shadow_ray, 0.001, ls.dist - 0.001, shadow_rec);

            if (!in_shadow) {
                color f = rec.mat_ptr->eval(rec, wo, ls.wi);
                double cos_theta = std::abs(dot(ls.wi, rec.normal));

                if (ls.is_delta) {
                    // Delta 光源无法用 BSDF 采样命中，权重为 1
                    L_direct += f * ls.Li * cos_theta / light_select_pdf;
                } else {
                    // 计算 BSDF 的 pdf
                    double bsdf_pdf = rec.mat_ptr->pdf(rec, wo, ls.wi);
                    double light_pdf = ls.pdf * light_select_pdf;
                    double mis_weight = power_heuristic(light_pdf, bsdf_pdf);

                    L_direct += f * ls.Li * cos_theta * mis_weight / light_pdf;
                }
            }
        }

        return L_direct;
    }

    int m_max_depth = 50;
    int m_rr_start_depth = 3;
};

#endif