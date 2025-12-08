#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "hittable.h"
#include "material.h"
#include "render_buffer.h"
#include "rtweekend.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

class Renderer {
  public:
    struct Settings {
        int samples_per_pixel = 10;
        int max_depth = 50;
    };

    Renderer() : m_is_rendering(false) {
    }

    void render(shared_ptr<hittable> world, shared_ptr<camera> cam,
                const color &background, RenderBuffer &target_buffer) {
        m_is_rendering = true;

        auto start_time = std::chrono::high_resolution_clock::now();

        int image_width = target_buffer.width();
        int image_height = target_buffer.height();

        const int num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        std::atomic<int> next_row(image_height - 1);
        auto render_worker = [&]() {
            while (true) {
                int j = next_row.fetch_sub(1);
                if (j < 0)
                    break;
                if (!m_is_rendering)
                    break;

                for (int i = 0; i < image_width; i++) {
                    color pixel_color(0, 0, 0);
                    for (int s = 0; s < m_settings.samples_per_pixel; ++s) {
                        auto u = (i + random_double()) / (image_width - 1);
                        auto v = (j + random_double()) / (image_height - 1);
                        ray r = cam->get_ray(u, v);
                        pixel_color += ray_color(r, background, *world,
                                                 m_settings.max_depth);
                    }
                    write_color_to_buffer(target_buffer, i, j, pixel_color,
                                          m_settings.samples_per_pixel);
                }
            }
        };

        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back(render_worker);
        }

        for (auto &t : threads) {
            t.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        m_is_rendering = false;
        std::cout << "Rendering finished in " << elapsed.count() << " seconds."
                  << std::endl;
    }

    void set_samples(int samples) {
        m_settings.samples_per_pixel = samples;
    }
    void set_max_depth(int depth) {
        m_settings.max_depth = depth;
    }

    void cancel() {
        m_is_rendering = false;
    }
    bool is_rendering() const {
        return m_is_rendering;
    }

  private:
    Settings m_settings;
    std::atomic<bool> m_is_rendering;

    color ray_color(const ray &r, const color &background,
                    const hittable &world, int depth) {
        hit_record rec;

        if (depth <= 0) {
            return color(0, 0, 0);
        }

        if (!world.hit(r, 0.001, infinity, rec)) {
            return background;
        }

        ray scattered;
        color attenuation;
        color emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);

        if (!rec.mat_ptr->scatter(r, rec, attenuation, scattered)) {
            return emitted;
        }

        return emitted +
               attenuation * ray_color(scattered, background, world, depth - 1);
    }

    void write_color_to_buffer(RenderBuffer &buffer, int x, int y,
                               color pixel_color, int samples) {
        auto r = pixel_color.x();
        auto g = pixel_color.y();
        auto b = pixel_color.z();

        auto scale = 1.0 / samples;
        r = sqrt(scale * r);
        g = sqrt(scale * g);
        b = sqrt(scale * b);

        buffer.set_pixel(
            x, y,
            color(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0)));
    }
};

#endif