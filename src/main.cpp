/*The MIT License (MIT)

Copyright (c) 2021-Present, Wencong Yang (yangwc3@mail2.sysu.edu.cn).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

#include "WindowsApp.h"
#include "direct_light_integrator.h"
#include "path_integrator.h"
#include "pbr_path_integrator.h"
#include "render_buffer.h"
#include "renderer.h"
#include "rr_path_integrator.h"
#include "scenes.h"

namespace RenderConfig {
constexpr int kMaxDepth = 50;
constexpr double kTMin = 0.001;
constexpr double kShutterOpen = 0.0;
constexpr double kShutterClose = 1.0;
} // namespace RenderConfig

int main(int argc, char *args[]) {

    int scene_id = 11;
    if (argc > 1) {
        scene_id = std::atoi(args[1]);
    }

    SceneConfig config = select_scene(scene_id);

    auto cam = make_shared<camera>(
        config.lookfrom, config.lookat, config.vup, config.vfov,
        config.aspect_ratio, config.aperture, config.focus_dist,
        RenderConfig::kShutterOpen, RenderConfig::kShutterClose);

    int width = config.image_width;
    int height = static_cast<int>(width / config.aspect_ratio);
    auto render_buffer = make_shared<RenderBuffer>(width, height);

    auto integrator = make_shared<PathIntegrator>();
    auto rrIntegrator = make_shared<RRPathInterator>();
    auto pbrIntegrator = make_shared<PBRPathIntegrator>();
    auto dirlightIntegrator = make_shared<DirectLightIntegrator>();

    Renderer renderer;
    renderer.set_samples(config.samples_per_pixel);
    renderer.set_integrator(dirlightIntegrator);
    renderer.set_max_depth(50);

    // Create window app handle
    WindowsApp::ptr winApp =
        WindowsApp::getInstance(width, height, "CGAssignment4: Ray Tracing");
    if (winApp == nullptr) {
        std::cerr << "Error: failed to create a window handler" << std::endl;
        return -1;
    }

    std::thread renderingThread([&renderer, world = config.world, cam,
                                 render_buffer, bg = config.background,
                                 lights = config.lights]() {
        renderer.render(world, cam, bg, *render_buffer, lights);
    });

    // Window app loop
    while (!winApp->shouldWindowClose()) {
        // Process event
        winApp->processEvent();

        // Display to the screen
        winApp->updateScreenSurface(render_buffer->get_data());
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    renderer.cancel();

    if (renderingThread.joinable()) {
        renderingThread.join();
    }

    return 0;
}