#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "onb.h"
#include "ray.h"
#include "rtweekend.h"
#include "texture.h"
#include <algorithm>

struct hit_record;

struct BSDFSample {
    vec3 wi;          // 采样生成的入射方向
    color f;          // BSDF 吞吐量, 存 f (BSDF值)，让积分器自己乘 cos 和除
                      // pdf(delta材质除外)。
    double pdf;       // 采样该方向的概率密度
    bool is_specular; // 是否是镜面反射（Delta分布）
    bool is_transmission = false; // 区分透射
};

class material {
  public:
    virtual ~material() = default;

    // 原emitted函数（保留旧接口）
    virtual color emitted(double u, double v, const point3 &p) const {
        return color(0, 0, 0);
    }

    // 新 emitted 接口
    virtual color emitted(const hit_record &rec, const vec3 &wo) const {
        return color(0, 0, 0);
    }

    // 判断材质是否含有完美镜面（Delta 分布）成分
    virtual bool is_specular() const {
        return false;
    }

    // 采样 BSDF 生成入射方向 (Importance Sampling)
    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const {
        return false;
    }

    // 给定两个方向，计算反射比率（BSDF值）
    virtual color eval(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const {
        return color(0, 0, 0);
    }

    // 计算pdf
    virtual double pdf(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const {
        return 0.0;
    }

    // Deprecated scatter
    virtual bool scatter(const ray &r_in, const hit_record &rec, color &albedo,
                         ray &scattered, double &pdf_val) const {
        return false;
    }

    // 保留旧接口
    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const {
        return false;
    }
};

class lambertian : public material {
  public:
    lambertian(const color &a) : albedo(make_shared<solid_color>(a)) {
    }
    lambertian(shared_ptr<texture> a) : albedo(a) {
    }

    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const override {
        vec3 scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero()) {
            scatter_direction = rec.normal;
        }
        sampled.wi = unit_vector(scatter_direction);
        sampled.pdf = dot(rec.normal, sampled.wi) / pi;
        sampled.f = albedo->value(rec.u, rec.v, rec.p) / pi;
        sampled.is_specular = false;
        return true;
    }

    virtual double pdf(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const override {
        auto cosine = dot(rec.normal, unit_vector(wi));
        return cosine < 0 ? 0 : cosine / pi;
    }

    virtual color eval(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const override {
        return albedo->value(rec.u, rec.v, rec.p) / pi;
    }

    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const override {
        auto scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero()) {
            scatter_direction = rec.normal;
        }
        scattered = ray(rec.p, scatter_direction, r_in.time());
        attenuation = albedo->value(rec.u, rec.v, rec.p);
        return true;
    }

  public:
    shared_ptr<texture> albedo;
};

class metal : public material {
  public:
    metal(const color &a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {
    }

    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const override {
        vec3 reflected = reflect(unit_vector(-wo), rec.normal);
        sampled.wi = unit_vector(reflected + fuzz * random_in_unit_sphere());
        sampled.f = albedo;
        sampled.pdf = 1.0;
        sampled.is_specular = true;
        return (dot(sampled.wi, rec.normal) > 0);
    }

    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const override {
        vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        scattered =
            ray(rec.p, reflected + fuzz * random_in_unit_sphere(), r_in.time());
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }

  public:
    color albedo;
    double fuzz;
};

class dielectric : public material {
  public:
    dielectric(double index_of_refraction) : ir(index_of_refraction) {
    }

    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const override {
        sampled.f = color(1.0, 1.0, 1.0);
        sampled.is_specular = true;
        sampled.pdf = 1.0;

        double refraction_ratio = rec.front_face ? (1.0 / ir) : ir;
        vec3 unit_direction = -wo;
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = refraction_ratio * sin_theta > 1.0;

        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > random_double()) {
            sampled.wi = reflect(unit_direction, rec.normal);
            sampled.is_transmission = false;
        } else {
            sampled.wi = refract(unit_direction, rec.normal, refraction_ratio);
            sampled.is_transmission = true;
        }
        return true;
    }

    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const override {
        attenuation = color(1.0, 1.0, 1.0);
        double refraction_ratio = rec.front_face ? (1.0 / ir) : ir;
        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;
        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > random_double()) {
            direction = reflect(unit_direction, rec.normal);
        } else {
            direction = refract(unit_direction, rec.normal, refraction_ratio);
        }
        scattered = ray(rec.p, direction, r_in.time());
        return true;
    }

  public:
    double ir;

  private:
    static double reflectance(double cosine, double ref_idx) {
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        r0 = r0 * r0;
        return r0 + (1 - r0) * pow((1 - cosine), 5);
    }
};

class diffuse_light : public material {
  public:
    diffuse_light(shared_ptr<texture> a) : emit(a) {
    }
    diffuse_light(color c) : emit(make_shared<solid_color>(c)) {
    }

    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const override {
        return false;
    }

    virtual color emitted(double u, double v, const point3 &p) const override {
        return emit->value(u, v, p);
    }

    virtual color emitted(const hit_record &rec,
                          const vec3 &wo) const override {
        if (rec.front_face)
            return emit->value(rec.u, rec.v, rec.p);
        return color(0, 0, 0);
    }

    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const override {
        return false;
    }

  public:
    shared_ptr<texture> emit;
};

class PBRMaterial : public material {
  public:
    PBRMaterial(shared_ptr<texture> a, shared_ptr<texture> r,
                shared_ptr<texture> m, shared_ptr<texture> n = nullptr)
        : albedo(a), roughness(r), metallic(m), normal_map(n) {
    }

    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const override {
        vec3 N = rec.normal;
        if (normal_map) {
            onb uvw;
            // 改进的 TBN 构建：尝试对齐 UV 方向 (假设 U 是水平的)
            uvw.axis[2] = N;
            if (fabs(N.y()) > 0.999) {
                uvw.axis[0] = vec3(1, 0, 0); // 极点处理
            } else {
                uvw.axis[0] = unit_vector(cross(N, vec3(0, 1, 0))); // 水平切线
            }
            uvw.axis[1] = cross(N, uvw.axis[0]); // 副切线 (向下)

            vec3 local_n = normal_map->value_normal(rec.u, rec.v, rec.p);
            N = unit_vector(uvw.local(local_n));
        }

        double rough = roughness->value_roughness(rec.u, rec.v, rec.p);
        rough = clamp(rough, 0.01, 1.0);

        // 50% chance to sample specular (GGX), 50% diffuse (Cosine)
        if (random_double() < 0.5) {
            // Sample Specular (GGX)
            onb uvw;
            uvw.build_from_w(N);
            double r1 = random_double();
            double r2 = random_double();
            double a = rough * rough;
            double phi = 2.0 * pi * r1;

            double cos_theta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
            double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

            vec3 H_local(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
            vec3 H = uvw.local(H_local);
            vec3 L = reflect(-wo, H);

            if (dot(N, L) <= 0)
                return false;
            sampled.wi = L;
        } else {
            // Sample Diffuse (Cosine)
            onb uvw;
            uvw.build_from_w(N);
            vec3 L = uvw.local(random_cosine_direction());
            if (dot(N, L) <= 0)
                L = N; // Should not happen with cosine sample but safety
            sampled.wi = unit_vector(L);
        }

        sampled.is_specular = false;
        sampled.pdf = pdf(rec, wo, sampled.wi);
        sampled.f = eval(rec, wo, sampled.wi);

        if (sampled.pdf < 1e-6)
            return false;
        return true;
    }

    virtual double pdf(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const override {
        vec3 N = rec.normal;
        if (normal_map) {
            onb uvw;
            // 改进的 TBN 构建
            uvw.axis[2] = N;
            if (fabs(N.y()) > 0.999) {
                uvw.axis[0] = vec3(1, 0, 0);
            } else {
                uvw.axis[0] = unit_vector(cross(N, vec3(0, 1, 0)));
            }
            uvw.axis[1] = cross(N, uvw.axis[0]);

            vec3 local_n = normal_map->value_normal(rec.u, rec.v, rec.p);
            N = unit_vector(uvw.local(local_n));
        }

        if (dot(N, wi) <= 0)
            return 0;

        double rough = roughness->value_roughness(rec.u, rec.v, rec.p);
        rough = clamp(rough, 0.01, 1.0);

        // Diffuse PDF
        double pdf_diff = dot(N, wi) / pi;

        // Specular PDF
        vec3 H = unit_vector(wo + wi);
        double D = DistributionGGX(N, H, rough);
        double NdotH = std::max(dot(N, H), 0.0);
        double HdotV = std::max(dot(H, wo), 0.0);
        double pdf_spec = (D * NdotH) / (4.0 * HdotV + 0.0001);

        return 0.5 * pdf_diff + 0.5 * pdf_spec;
    }

    virtual color eval(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const override {
        vec3 N = rec.normal;
        if (normal_map) {
            onb uvw;
            // 改进的 TBN 构建
            uvw.axis[2] = N;
            if (fabs(N.y()) > 0.999) {
                uvw.axis[0] = vec3(1, 0, 0);
            } else {
                uvw.axis[0] = unit_vector(cross(N, vec3(0, 1, 0)));
            }
            uvw.axis[1] = cross(N, uvw.axis[0]);

            vec3 local_n = normal_map->value_normal(rec.u, rec.v, rec.p);
            N = unit_vector(uvw.local(local_n));
        }

        double NdotL = dot(N, wi);
        double NdotV = dot(N, wo);
        if (NdotL <= 0 || NdotV <= 0)
            return color(0, 0, 0);

        double rough = roughness->value_roughness(rec.u, rec.v, rec.p);
        double metal = metallic->value_metallic(rec.u, rec.v, rec.p);
        color base_color = albedo->value(rec.u, rec.v, rec.p);
        rough = clamp(rough, 0.01, 1.0);

        vec3 H = unit_vector(wo + wi);

        // Fresnel
        vec3 F0 = vec3(0.04, 0.04, 0.04);
        vec3 metal_vec(metal, metal, metal);
        F0 = (vec3(1.0, 1.0, 1.0) - metal_vec) * F0 + metal_vec * base_color;
        vec3 F = fresnelSchlick(std::max(dot(H, wo), 0.0), F0);

        // NDF
        double D = DistributionGGX(N, H, rough);

        // Geometry
        double G = GeometrySmith(N, wo, wi, rough);

        // Specular BRDF
        vec3 numerator = D * G * F;
        double denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        // Diffuse BRDF
        vec3 kS = F;
        vec3 kD = vec3(1.0, 1.0, 1.0) - kS;
        kD *= (1.0 - metal);
        vec3 diffuse = kD * base_color / pi;

        return diffuse + specular;
    }

    double DistributionGGX(vec3 N, vec3 H, double roughness) const {
        double a = roughness * roughness;
        double a2 = a * a;
        double NdotH = std::max(dot(N, H), 0.0);
        double NdotH2 = NdotH * NdotH;

        double nom = a2;
        double denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = pi * denom * denom;

        return nom / denom;
    }

    double GeometrySchlickGGX(double NdotV, double roughness) const {
        double a = roughness;
        double k = (a * a) / 2.0;

        double nom = NdotV;
        double denom = NdotV * (1.0 - k) + k;

        return nom / denom;
    }

    double GeometrySmith(vec3 N, vec3 V, vec3 L, double roughness) const {
        double NdotV = std::max(dot(N, V), 0.0);
        double NdotL = std::max(dot(N, L), 0.0);
        double ggx2 = GeometrySchlickGGX(NdotV, roughness);
        double ggx1 = GeometrySchlickGGX(NdotL, roughness);

        return ggx1 * ggx2;
    }

    vec3 fresnelSchlick(double cosTheta, vec3 F0) const {
        return F0 + (vec3(1.0, 1.0, 1.0) - F0) * pow(1.0 - cosTheta, 5.0);
    }

  public:
    shared_ptr<texture> albedo;
    shared_ptr<texture> roughness;
    shared_ptr<texture> metallic;
    shared_ptr<texture> normal_map;
};

#endif