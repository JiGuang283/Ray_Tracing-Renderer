// lighting/environment_light.h
#ifndef ENVIRONMENT_LIGHT_H
#define ENVIRONMENT_LIGHT_H

#include "light.h"
#include "rtw_stb_image.h"

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
        }

        if (data) {
            hdr_data = std::vector<float>(data, data + width * height * 3);
            stbi_image_free(data);
        }

        // Check if it's a light probe (square aspect ratio)
        if (width > 0 && height > 0 && width == height) {
            is_light_probe = true;
        }
    }

    virtual LightSample sample(const point3 &p, const vec2 &u) const override {
        LightSample s;
        s.wi = random_unit_vector();
        s.dist = infinity;
        s.is_delta = false;

        s.pdf = 1.0 / (4.0 * pi);

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
            // Assuming probe looks down -Z axis (Forward)
            // Center of image corresponds to -Z direction
            // Edge of circle corresponds to +Z direction
            double d =
                sqrt(unit_dir.x() * unit_dir.x() + unit_dir.y() * unit_dir.y());
            double r_coord =
                (d > 0) ? (1.0 / pi) * acos(unit_dir.z()) / d : 0.0;

            u = (unit_dir.x() * r_coord + 1.0) * 0.5;
            v = (unit_dir.y() * r_coord + 1.0) * 0.5;

            // Flip v to match image coordinate system (Top-Left origin)
            v = 1.0 - v;
        } else {
            // Equirectangular Mapping
            // u = phi / 2pi, v = theta / pi
            // Fix: Use acos(y) instead of acos(-y) to map +Y (Up) to v=0 (Top)
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

    // PDF
    virtual double pdf(const point3 &origin,
                       const vec3 &direction) const override {
        return 1.0 / (4.0 * pi);
    }

    virtual bool is_delta() const override {
        return false;
    }
    virtual bool is_infinite() const override {
        return true;
    }

  private:
    std::vector<float> hdr_data;
    int width, height;
    bool is_light_probe = false;
};

#endif
