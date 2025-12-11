#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "hittable.h"
#include "integrator.h"
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
    };

    Renderer() : m_is_rendering(false) {
    }

    void set_integrator(std::shared_ptr<Integrator> integrator) {
        m_integrator = integrator;
    }

    void render(shared_ptr<hittable> world, shared_ptr<camera> cam,
                const color &background, RenderBuffer &target_buffer,
                const std::vector<shared_ptr<Light>> &lights = {}) {
        m_is_rendering = true;

        auto start_time = std::chrono::high_resolution_clock::now();

        int image_width = target_buffer.width();
        int image_height = target_buffer.height();

        constexpr int TILE_SIZE = 16;

        int tiles_x = (image_width + TILE_SIZE - 1) / TILE_SIZE;
        int tiles_y = (image_height + TILE_SIZE - 1) / TILE_SIZE;
        int total_tiles = tiles_x * tiles_y;

        std::atomic<int> next_tile_index(0);

        const int num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        auto render_worker = [&]() {
            while (true) {
                int tile_index = next_tile_index.fetch_add(1);
                if (tile_index >= total_tiles) {
                    break;
                }
                if (!m_is_rendering) {
                    break;
                }

                int tile_y = (tiles_y - 1) - tile_index / tiles_x;
                int tile_x = tile_index % tiles_x;

                int x_start = tile_x * TILE_SIZE;
                int y_start = tile_y * TILE_SIZE;
                int x_end = std::min(x_start + TILE_SIZE, image_width);
                int y_end = std::min(y_start + TILE_SIZE, image_height);

                for (int j = y_end - 1; j >= y_start; j--) {
                    for (int i = x_start; i < x_end; i++) {
                        color pixel_color(0, 0, 0);
                        for (int s = 0; s < m_settings.samples_per_pixel; ++s) {
                            auto u = (i + random_double()) / (image_width - 1);
                            auto v = (j + random_double()) / (image_height - 1);
                            ray r = cam->get_ray(u, v);
                            if (m_integrator) {
                                pixel_color += m_integrator->Li(
                                    r, *world, background, lights);
                            }
                        }
                        write_color_to_buffer(target_buffer, i, j, pixel_color,
                                              m_settings.samples_per_pixel);
                    }
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
        if (m_integrator) {
            m_integrator->set_max_depth(depth);
        }
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

    std::shared_ptr<Integrator> m_integrator;

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