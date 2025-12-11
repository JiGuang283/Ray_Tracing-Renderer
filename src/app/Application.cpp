#include "Application.h"

#include <cmath>
#include <iostream>

#include "camera.h"
#include "imgui.h"
#include "path_integrator.h"
#include "scenes.h"

namespace RenderConfig {
    constexpr int kMaxDepth = 50;
    constexpr double kShutterOpen = 0.0;
    constexpr double kShutterClose = 1.0;
} // namespace RenderConfig


Application::Application(int initial_scene_id) {
    ui_.scene_id = initial_scene_id;
}

Application::~Application() {
    if (render_thread_.joinable()) {
        renderer_.cancel();
        render_thread_.join();
    }
}

// Init
bool Application::init() {
    SceneConfig cfg = select_scene(ui_.scene_id);
    width_ = cfg.image_width;
    height_ = static_cast<int>(width_ / cfg.aspect_ratio);

    win_app_ = WindowsApp::getInstance(width_, height_, "CGAssignment4: Ray Tracing");
    if (!win_app_) {
        std::cerr << "Error: failed to create window" << std::endl;
        return false;
    }

    image_data_.resize(width_ * height_ * 4); // RGBA
    start_render();
    ui_.restart_render = false;
    return true;
}

void Application::run() {
    while (!win_app_->shouldWindowClose()) { // Render loop
        win_app_->processEvent();

        if (ui_.restart_render && !ui_.is_rendering) {
            start_render();
            ui_.restart_render = false;
        }

        update_display_from_buffer(); // Update texture

        win_app_->beginRender();
        render_ui();
        win_app_->endRender();
    }
}

void Application::start_render() {
    if (render_thread_.joinable()) {
        renderer_.cancel();
        render_thread_.join();
    }
    renderer_.reset();
    ui_.is_rendering = true;

    SceneConfig config = select_scene(ui_.scene_id);
    float ratio_val = static_cast<float>(ui_.aspect_w) / static_cast<float>(ui_.aspect_h);

    width_ = ui_.image_width;
    height_ = static_cast<int>(width_ / ratio_val);

    win_app_->setRenderSize(width_, height_);
    image_data_.resize(width_ * height_ * 4);

    auto cam = std::make_shared<camera>(
        config.lookfrom, config.lookat, config.vup, config.vfov,
        ratio_val, config.aperture, config.focus_dist,
        RenderConfig::kShutterOpen, RenderConfig::kShutterClose);

    render_buffer_ = std::make_shared<RenderBuffer>(width_, height_);
    render_buffer_->clear();

    auto integrator = std::make_shared<PathIntegrator>();
    renderer_.set_integrator(integrator);
    renderer_.set_samples(ui_.samples_per_pixel);
    renderer_.set_max_depth(RenderConfig::kMaxDepth);

    render_thread_ = std::thread([this, world = config.world, cam, bg = config.background]() {
        renderer_.render(world, cam, bg, *render_buffer_);
        if (!renderer_.is_cancelled()) {
            float fps = ImGui::GetIO().Framerate;
            ui_.last_fps = fps;
            ui_.last_ms = fps > 0.0f ? (1000.0f / fps) : 0.0f;
        }
        ui_.is_rendering = false;
    });
}

void Application::update_display_from_buffer() {
    if (!render_buffer_ || render_buffer_->get_width() != width_ || render_buffer_->get_height() != height_) {
        return;
    }
    const auto &pixels = render_buffer_->get_data();
    #pragma omp parallel for
    for (int j = 0; j < height_; ++j) {
        for (int i = 0; i < width_; ++i) {
            const auto &c = pixels[height_ - 1 - j][i];
            int idx = (j * width_ + i) * 4;
            image_data_[idx + 0] = static_cast<unsigned char>(255.999 * sqrt(c.x()));
            image_data_[idx + 1] = static_cast<unsigned char>(255.999 * sqrt(c.y()));
            image_data_[idx + 2] = static_cast<unsigned char>(255.999 * sqrt(c.z()));
            image_data_[idx + 3] = 255;
        }
    }
    win_app_->updateTexture(image_data_.data());
}

void Application::render_ui() {
    // Viewport
    if (win_app_->getTexture()) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::Image((void*)win_app_->getTexture(), ImGui::GetContentRegionAvail());
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // Controls
    ImGui::Begin("Controls");

    if (ui_.is_rendering) {
        float fps = ImGui::GetIO().Framerate;
        float ms  = fps > 0.f ? 1000.0f / fps : 0.f;
        ImGui::Text("Render Time: %.3f ms/frame (%.1f FPS)", ms, fps);
        float progress = renderer_.get_progress();
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
        ImGui::Text("Rendering... %.1f%%", progress * 100.0f);
    } else {
        ImGui::Text("Last Render: %.3f ms/frame (%.1f FPS)", ui_.last_ms, ui_.last_fps);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Idle");
    }

    ImGui::Separator();

    const char *scene_names[] = {"Random Spheres", "Example Light", "Two Spheres", "Perlin Spheres", "Earth", "Simple Light", "Cornell Box", "Cornell Smoke", "Final Scene"};
    ImGui::Text("1. Select Scene");
    ImGui::Combo("##Scene", &ui_.scene_id, scene_names, IM_ARRAYSIZE(scene_names));

    ImGui::Separator();
    ImGui::Text("2. Resolution Settings");

    ImGui::InputInt("Width (px)", &ui_.image_width, 10, 100);
    if (ui_.image_width < 64) ui_.image_width = 64;

    ImGui::Text("Aspect Ratio:"); ImGui::SameLine();
    ImGui::PushItemWidth(50); ImGui::InputInt("##W", &ui_.aspect_w, 0, 0); ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::Text(":"); ImGui::SameLine();
    ImGui::PushItemWidth(50); ImGui::InputInt("##H", &ui_.aspect_h, 0, 0); ImGui::PopItemWidth();
    if (ui_.aspect_w < 1) ui_.aspect_w = 1;
    if (ui_.aspect_h < 1) ui_.aspect_h = 1;

    int current_height = static_cast<int>(ui_.image_width * (static_cast<float>(ui_.aspect_h) / ui_.aspect_w));
    ImGui::TextDisabled("Output Size: %d x %d", ui_.image_width, current_height);

    ImGui::Separator();
    ImGui::Text("3. Quality Settings");

    ImGui::InputInt("SPP (Quality)", &ui_.samples_per_pixel, 10, 100);
    if (ui_.samples_per_pixel < 1) ui_.samples_per_pixel = 1;
    ImGui::TextDisabled("(Higher SPP = Less Noise, More Time)");

    ImGui::Separator();

    if (ImGui::Button("Apply & Start Render", ImVec2(-1.0f, 40.0f))) {
        if (!ui_.is_rendering) {
            ui_.restart_render = true;
        }
    }

    ImGui::End();
}