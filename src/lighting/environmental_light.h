// lighting/environment_light.h
#ifndef ENVIRONMENT_LIGHT_H
#define ENVIRONMENT_LIGHT_H

#include "light.h"
#include "rtw_stb_image.h"
#include <algorithm>
#include <numeric>

// 1D 分布（用于条件分布和边缘分布）
class Distribution1D {
  public:
    Distribution1D() = default;

    Distribution1D(const std::vector<double> &f) : func(f), cdf(f.size() + 1) {
        int n = func.size();
        cdf[0] = 0;
        for (int i = 1; i <= n; ++i) {
            cdf[i] = cdf[i - 1] + func[i - 1];
        }
        func_int = cdf[n];
        if (func_int > 0) {
            for (int i = 0; i <= n; ++i) {
                cdf[i] /= func_int;
            }
        }
    }

    // 采样，返回偏移量和对应的PDF
    double sample(double u, double &pdf_out, int &offset) const {
        // 二分查找
        auto it = std::lower_bound(cdf.begin(), cdf.end(), u);
        offset = std::max(0, int(it - cdf.begin()) - 1);
        offset = std::min(offset, int(func.size()) - 1);

        // 计算在区间内的插值位置
        double du = u - cdf[offset];
        if (cdf[offset + 1] - cdf[offset] > 0) {
            du /= (cdf[offset + 1] - cdf[offset]);
        }

        pdf_out = (func_int > 0) ? func[offset] / func_int : 0;
        return (offset + du) / func.size();
    }

    double pdf(int index) const {
        return (func_int > 0) ? func[index] / (func_int * func.size()) : 0;
    }

    double integral() const {
        return func_int;
    }

  private:
    std::vector<double> func;
    std::vector<double> cdf;
    double func_int = 0;
};

// 2D 分布
class Distribution2D {
  public:
    Distribution2D() = default;

    Distribution2D(const std::vector<double> &data, int nu, int nv) {
        // 构建每行的条件分布
        conditional.reserve(nv);
        std::vector<double> marginal_func(nv);

        for (int v = 0; v < nv; ++v) {
            std::vector<double> row(nu);
            for (int u = 0; u < nu; ++u) {
                row[u] = data[v * nu + u];
            }
            conditional.emplace_back(row);
            marginal_func[v] = conditional[v].integral();
        }

        // 构建边缘分布
        marginal = Distribution1D(marginal_func);
    }

    // 采样 (u, v)，返回 PDF
    vec2 sample(const vec2 &random, double &pdf_out) const {
        double pdfs[2];
        int v_idx;
        double v = marginal.sample(random.y(), pdfs[1], v_idx);

        int u_idx;
        double u = conditional[v_idx].sample(random.x(), pdfs[0], u_idx);

        pdf_out = pdfs[0] * pdfs[1];
        return vec2(u, v);
    }

    double pdf(double u, double v) const {
        int iu = clamp(int(u * conditional[0].integral()), 0,
                       int(conditional.size()) - 1);
        int iv =
            clamp(int(v * marginal.integral()), 0, int(conditional.size()) - 1);
        // 这里需要重新计算
        int nu = conditional.size() > 0 ? conditional[0].integral() : 0;
        int nv = conditional.size();

        if (nu == 0 || nv == 0)
            return 0;

        int u_idx = clamp(int(u * nu), 0, nu - 1);
        int v_idx = clamp(int(v * nv), 0, nv - 1);

        return conditional[v_idx].pdf(u_idx) * marginal.pdf(v_idx);
    }

  private:
    std::vector<Distribution1D> conditional;
    Distribution1D marginal;
    friend class EnvironmentLight;
};

class EnvironmentLight : public Light {
  public:
    EnvironmentLight(const char *map_filename) {
        int components_per_pixel = 3;
        float *data =
            stbi_loadf(map_filename, &width, &height, &components_per_pixel, 0);

        if (!data) {
            std::cerr << "ERROR: Could not load HDR environment map: "
                      << map_filename << std::endl;
            width = height = 0;
            return;
        }

        hdr_data = std::vector<float>(data, data + width * height * 3);
        stbi_image_free(data);

        // Check if it's a light probe (square aspect ratio)
        if (width > 0 && height > 0 && width == height) {
            is_light_probe = true;
        }

        // 构建亮度分布用于重要性采样
        build_distribution();
    }

    void build_distribution() {
        if (width == 0 || height == 0)
            return;

        std::vector<double> luminance_data(width * height);

        for (int v = 0; v < height; ++v) {
            // sin(theta) 立体角校正
            // theta = pi * (v + 0.5) / height
            double sin_theta = sin(pi * (v + 0.5) / height);

            for (int u = 0; u < width; ++u) {
                int idx = v * width + u;
                int pixel_idx = idx * 3;

                // 计算亮度 (luminance)
                double r = hdr_data[pixel_idx];
                double g = hdr_data[pixel_idx + 1];
                double b = hdr_data[pixel_idx + 2];
                double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;

                // 乘以 sin(theta) 校正立体角
                luminance_data[idx] = lum * sin_theta;
            }
        }

        distribution = Distribution2D(luminance_data, width, height);

        // 计算总功率用于归一化
        total_power = 0;
        for (double l : luminance_data) {
            total_power += l;
        }
        total_power *= (2 * pi * pi) / (width * height);
    }

    virtual LightSample sample(const point3 &p, const vec2 &u) const override {
        LightSample s;
        s.dist = infinity;
        s.is_delta = false;

        if (width == 0 || height == 0) {
            s.wi = random_unit_vector();
            s.pdf = 1.0 / (4.0 * pi);
            s.Li = color(1, 1, 1);
            return s;
        }

        // 使用2D分布进行重要性采样
        double map_pdf;
        vec2 uv = distribution.sample(u, map_pdf);

        if (map_pdf == 0) {
            s.Li = color(0, 0, 0);
            s.pdf = 0;
            return s;
        }

        // 将 (u, v) 转换为方向
        double phi, theta;
        if (is_light_probe) {
            // Light probe 映射的逆变换
            double uc = uv.x() * 2.0 - 1.0;
            double vc = (1.0 - uv.y()) * 2.0 - 1.0;
            double r = sqrt(uc * uc + vc * vc);

            if (r > 1.0) {
                s.Li = color(0, 0, 0);
                s.pdf = 0;
                return s;
            }

            theta = pi * r;
            phi = atan2(vc, uc);

            double sin_theta = sin(theta);
            s.wi = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos(theta));
        } else {
            // Equirectangular 映射的逆变换
            phi = uv.x() * 2 * pi - pi;
            theta = uv.y() * pi;

            double sin_theta = sin(theta);
            double cos_theta = cos(theta);

            // 方向向量
            s.wi = vec3(sin_theta * cos(phi), cos_theta, -sin_theta * sin(phi));
        }

        // 计算立体角 PDF
        // pdf_direction = pdf_uv * (width * height) / (2 * pi * pi * sin_theta)
        double sin_theta = sin(theta);
        if (sin_theta < 1e-6) {
            s.Li = color(0, 0, 0);
            s.pdf = 0;
            return s;
        }

        s.pdf = map_pdf * width * height / (2.0 * pi * pi * sin_theta);
        s.Li = Le(ray(p, s.wi));

        return s;
    }

    virtual color Le(const ray &r) const override {
        if (hdr_data.empty())
            return color(1, 1, 1);

        vec3 unit_dir = unit_vector(r.direction());
        double u, v;

        if (is_light_probe) {
            // Angular Map (Light Probe) Mapping
            double d =
                sqrt(unit_dir.x() * unit_dir.x() + unit_dir.y() * unit_dir.y());
            double r_coord =
                (d > 0) ? (1.0 / pi) * acos(unit_dir.z()) / d : 0.0;

            u = (unit_dir.x() * r_coord + 1.0) * 0.5;
            v = (unit_dir.y() * r_coord + 1.0) * 0.5;
            v = 1.0 - v;
        } else {
            // Equirectangular Mapping
            auto theta = acos(unit_dir.y());
            auto phi = atan2(-unit_dir.z(), unit_dir.x()) + pi;

            u = phi / (2 * pi);
            v = theta / pi;
        }

        // Bilinear Interpolation
        double u_img = u * width - 0.5;
        double v_img = v * height - 0.5;

        int i0 = static_cast<int>(floor(u_img));
        int j0 = static_cast<int>(floor(v_img));

        double du = u_img - i0;
        double dv = v_img - j0;

        color c00 = get_pixel(i0, j0);
        color c10 = get_pixel(i0 + 1, j0);
        color c01 = get_pixel(i0, j0 + 1);
        color c11 = get_pixel(i0 + 1, j0 + 1);

        color c0 = c00 * (1 - du) + c10 * du;
        color c1 = c01 * (1 - du) + c11 * du;

        return c0 * (1 - dv) + c1 * dv;
    }

    color get_pixel(int i, int j) const {
        // Wrap u
        if (i < 0)
            i += width;
        if (i >= width)
            i -= width;
        // Clamp v
        if (j < 0)
            j = 0;
        if (j >= height)
            j = height - 1;

        int index = 3 * (j * width + i);
        return color(hdr_data[index], hdr_data[index + 1], hdr_data[index + 2]);
    }

    // PDF for a given direction
    virtual double pdf(const point3 &origin,
                       const vec3 &direction) const override {
        if (width == 0 || height == 0)
            return 1.0 / (4.0 * pi);

        vec3 unit_dir = unit_vector(direction);
        double u, v;
        double theta;

        if (is_light_probe) {
            double d =
                sqrt(unit_dir.x() * unit_dir.x() + unit_dir.y() * unit_dir.y());
            double r_coord =
                (d > 0) ? (1.0 / pi) * acos(unit_dir.z()) / d : 0.0;

            u = (unit_dir.x() * r_coord + 1.0) * 0.5;
            v = (unit_dir.y() * r_coord + 1.0) * 0.5;
            v = 1.0 - v;
            theta = acos(unit_dir.z());
        } else {
            theta = acos(unit_dir.y());
            auto phi = atan2(-unit_dir.z(), unit_dir.x()) + pi;

            u = phi / (2 * pi);
            v = theta / pi;
        }

        double sin_theta = sin(theta);
        if (sin_theta < 1e-6)
            return 0;

        // 获取 (u, v) 位置的 PDF
        int u_idx = clamp(int(u * width), 0, width - 1);
        int v_idx = clamp(int(v * height), 0, height - 1);

        double map_pdf = distribution.conditional[v_idx].pdf(u_idx) *
                         distribution.marginal.pdf(v_idx);

        return map_pdf * width * height / (2.0 * pi * pi * sin_theta);
    }

    virtual bool is_delta() const override {
        return false;
    }
    virtual bool is_infinite() const override {
        return true;
    }

    virtual color power() const override {
        if (hdr_data.empty())
            return color(1, 1, 1);
        return color(total_power, total_power, total_power);
    }

  private:
    std::vector<float> hdr_data;
    int width = 0, height = 0;
    bool is_light_probe = false;
    Distribution2D distribution;
    double total_power = 0;
};

#endif
