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
#include <cmath>

class Renderer {
  public:
    struct Settings {
        int samples_per_pixel = 10;
    };

    Renderer() : m_is_rendering(false), m_was_cancelled(false), m_processed_tiles(), m_total_tiles(0) {
    }

    void set_integrator(std::shared_ptr<Integrator> integrator) {
        m_integrator = integrator;
    }

    void render(shared_ptr<hittable> world, shared_ptr<camera> cam,
                const color &background, RenderBuffer &target_buffer,
                const std::vector<shared_ptr<Light>> &lights = {}) {
        m_is_rendering = true;
        m_was_cancelled = false;
        m_processed_tiles = 0; // 重置进度

        auto start_time = std::chrono::high_resolution_clock::now();

        const int image_width = target_buffer.width();
        const int image_height = target_buffer.height();

        constexpr int TILE_SIZE = 16;

        int tiles_x = (image_width + TILE_SIZE - 1) / TILE_SIZE;
        int tiles_y = (image_height + TILE_SIZE - 1) / TILE_SIZE;
        int total_tiles = tiles_x * tiles_y;

        m_total_tiles = total_tiles;

        std::atomic<int> next_tile_index(0);

        int num_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (num_threads <= 0) {
            num_threads = 1;
        }
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

                const int tile_w = x_end - x_start;
                const int tile_h = y_end - y_start;

                std::vector<float> tile_r;
                std::vector<float> tile_g;
                std::vector<float> tile_b;
                tile_r.resize(static_cast<size_t>(tile_w) * tile_h);
                tile_g.resize(static_cast<size_t>(tile_w) * tile_h);
                tile_b.resize(static_cast<size_t>(tile_w) * tile_h);

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

                        auto scale = 1.0 / m_settings.samples_per_pixel;
                        auto r = pixel_color.x() * scale;
                        auto g = pixel_color.y() * scale;
                        auto b = pixel_color.z() * scale;

                        if (std::isnan(r)) r = 0.0;
                        if (std::isnan(g)) g = 0.0;
                        if (std::isnan(b)) b = 0.0;

                        r = std::max(0.0, r);
                        g = std::max(0.0, g);
                        b = std::max(0.0, b);

                        const int lx = i - x_start;
                        const int ly = j - y_start;
                        const size_t local_idx = static_cast<size_t>(ly) * tile_w + lx;
                        tile_r[local_idx] = static_cast<float>(r);
                        tile_g[local_idx] = static_cast<float>(g);
                        tile_b[local_idx] = static_cast<float>(b);
                    }
                }

                target_buffer.commit_tile(x_start, y_start, x_end, y_end, tile_r, tile_g, tile_b);

                ++m_processed_tiles; // 更新已处理的片数
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
        if (!m_was_cancelled) {
            std::cout << "Rendering finished in " << elapsed.count() << " seconds."
                      << std::endl;
        }
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
        m_was_cancelled = true;
        m_is_rendering = false;
    }
    bool is_rendering() const {
        return m_is_rendering;
    }

    bool is_cancelled() const {
        return m_was_cancelled;
    }

    void reset() {
        m_processed_tiles = 0;
        m_total_tiles = 0;
        m_was_cancelled = false;
    }

    float get_progress() const {    //  获取进度
        if (m_total_tiles == 0) {
            return 0.0f;
        }
        return static_cast<float>(m_processed_tiles) / m_total_tiles;
    }


  private:
    Settings m_settings;

    std::atomic<bool> m_is_rendering;
    std::atomic<bool> m_was_cancelled;

    std::atomic<int> m_processed_tiles;
    int m_total_tiles;

    std::shared_ptr<Integrator> m_integrator;
};

#endif