#ifndef TEXTURE_H
#define TEXTURE_H

#include "perlin.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"
#include "vec3.h"

#include <iostream>

class texture {
  public:
    virtual color value(double u, double v, const point3 &p) const = 0;

    virtual double value_scalar(double u, double v, const point3 &p) const {
        return value(u, v, p).x();
    }

    virtual vec3 value_normal(double u, double v, const point3 &p) const {
        color c = value(u, v, p);
        return unit_vector(c * 2.0 - color(1, 1, 1));
    }

    virtual double value_roughness(double u, double v, const point3 &p) const {
        return value_scalar(u, v, p);
    }

    virtual double value_metallic(double u, double v, const point3 &p) const {
        return value_scalar(u, v, p);
    }

    virtual ~texture() = default;
};

class solid_color : public texture {
  public:
    solid_color() {
    }
    solid_color(color c) : color_value(c) {
    }

    solid_color(double red, double green, double blue)
        : solid_color(color(red, green, blue)) {
    }

    virtual color value(double u, double v, const vec3 &p) const override {
        return color_value;
    }

  private:
    color color_value;
};

class checker_texture : public texture {
  public:
    checker_texture() {
    }

    checker_texture(shared_ptr<texture> _even, shared_ptr<texture> _odd)
        : even(_even), odd(_odd) {
    }

    checker_texture(color c1, color c2)
        : even(make_shared<solid_color>(c1)),
          odd(make_shared<solid_color>(c2)) {
    }

    virtual color value(double u, double v, const point3 &p) const override {
        auto sines = sin(10 * p.x()) * sin(10 * p.y()) * sin(10 * p.z());
        if (sines < 0) {
            return odd->value(u, v, p);
        } else {
            return even->value(u, v, p);
        }
    }

  public:
    shared_ptr<texture> odd;
    shared_ptr<texture> even;
};

class image_texture : public texture {
  public:
    // 禁止拷贝
    image_texture(const image_texture &) = delete;
    image_texture &operator=(const image_texture &) = delete;

    // 允许移动（可选，如需要）
    image_texture(image_texture &&) = default;
    image_texture &operator=(image_texture &&) = default;

    image_texture()
        : data(nullptr), width(0), height(0), bytes_per_scanline(0) {
    }

    image_texture(const char *filename) {
        auto components_per_pixel = bytes_per_pixel;
        data = stbi_load(filename, &width, &height, &components_per_pixel,
                         components_per_pixel);

        if (!data) {
            std::cerr << "ERROR: Could not load texture image file '"
                      << filename << "'.\n";
            width = height = 0;
        }
        bytes_per_scanline = bytes_per_pixel * width;
    }

    ~image_texture() {
        if (data) {
            stbi_image_free(data);
        }
    }

    virtual color value(double u, double v, const vec3 &p) const override {
        if (data == nullptr) {
            return color(0, 1, 1);
        }

        u = clamp(u, 0.0, 1.0);
        v = 1.0 - clamp(v, 0.0, 1.0);

        auto i = static_cast<int>(u * width);
        auto j = static_cast<int>(v * height);

        if (i >= width) {
            i = width - 1;
        }
        if (j >= height) {
            j = height - 1;
        }

        const auto color_scale = 1.0 / 255.0;
        auto pixel = data + j * bytes_per_scanline + i * bytes_per_pixel;

        return color(color_scale * pixel[0], color_scale * pixel[1],
                     color_scale * pixel[2]);
    }

  private:
    static constexpr int bytes_per_pixel = 3;
    unsigned char *data = nullptr;
    int width = 0;
    int height = 0;
    int bytes_per_scanline = 0;
};

class noise_texture : public texture {
  public:
    noise_texture() {
    }
    noise_texture(double sc) : scale(sc) {
    }

    virtual color value(double u, double v, const point3 &p) const override {
        return color(1, 1, 1) * 0.5 *
               (1 + sin(scale * p.z() + 10 * noise.turb(p)));
    }

  public:
    perlin noise;
    double scale;
};

#endif