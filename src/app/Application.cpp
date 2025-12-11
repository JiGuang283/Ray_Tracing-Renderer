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

    // 初始化 UI 参数为场景默认值
    ui_.vfov = cfg.vfov;
    ui_.aperture = cfg.aperture;
    ui_.focus_dist = cfg.focus_dist;
    ui_.max_depth = RenderConfig::kMaxDepth;

    win_app_ = WindowsApp::getInstance(width_, height_, "Ray_Tracing-Renderer");
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

    // 使用 UI 参数创建相机
    auto cam = std::make_shared<camera>(
        config.lookfrom, config.lookat, config.vup, 
        ui_.vfov, // 使用 UI 中的 FOV
        ratio_val, 
        ui_.aperture, // 使用 UI 中的光圈
        ui_.focus_dist, // 使用 UI 中的对焦距离
        RenderConfig::kShutterOpen, RenderConfig::kShutterClose);

    render_buffer_ = std::make_shared<RenderBuffer>(width_, height_);
    render_buffer_->clear();

    auto integrator = std::make_shared<PathIntegrator>();
    renderer_.set_integrator(integrator);
    renderer_.set_samples(ui_.samples_per_pixel);
    renderer_.set_max_depth(ui_.max_depth); // 使用 UI 中的最大深度

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
    // 设置主窗口占满整个屏幕，无标题栏，作为背景容器
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainDock", nullptr, window_flags);
    ImGui::PopStyleVar(2); // 恢复 Padding 和 Rounding

    // 定义布局参数
    float control_width = 320.0f; // 控制面板宽度
    float avail_w = ImGui::GetContentRegionAvail().x;
    float image_w = avail_w - control_width; // 剩余宽度给渲染画面

    // 左侧：渲染画面 (Render Output)
    ImGui::BeginChild("RenderView", ImVec2(image_w, 0), false);
    if (win_app_->getTexture()) {
        // 在该区域内绘制图片，自动缩放以适应区域
        ImGui::Image((void*)win_app_->getTexture(), ImGui::GetContentRegionAvail());
    }
    ImGui::EndChild();

    ImGui::SameLine(); // 让下一个窗口显示在右侧

    // 右侧：控制面板 (Controls)
    // 给控制面板加一点背景色区分
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("ControlsView", ImVec2(0, 0), true); // 0,0 代表填满剩余空间
    ImGui::PopStyleColor();

    // 添加一些 Padding 让控件不要贴边
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    // 注意：这里需要重新开启一个像 Window 一样的区域来应用 Padding，
    // 或者直接在 Child 内部排版。简单起见，我们直接开始绘制控件。

    ImGui::Text("Renderer Controls");
    ImGui::Separator();

    //  Controls 窗口的代码逻辑
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
    
    // 检测场景是否切换
    int old_scene = ui_.scene_id;
    if (ImGui::Combo("##Scene", &ui_.scene_id, scene_names, IM_ARRAYSIZE(scene_names))) {
        if (ui_.scene_id != old_scene) {
            // 切换场景时，重新加载该场景的默认参数
            SceneConfig cfg = select_scene(ui_.scene_id);
            ui_.vfov = cfg.vfov;
            ui_.aperture = cfg.aperture;
            ui_.focus_dist = cfg.focus_dist;
            ui_.restart_render = true;
        }
    }

    ImGui::Separator();
    ImGui::Text("2. Camera Settings");
    
    ImGui::SliderFloat("FOV", &ui_.vfov, 1.0f, 120.0f);
    ImGui::SliderFloat("Aperture", &ui_.aperture, 0.0f, 2.0f);
    ImGui::SliderFloat("Focus Dist", &ui_.focus_dist, 0.1f, 50.0f);

    ImGui::Separator();
    ImGui::Text("3. Resolution Settings");

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
    ImGui::Text("4. Quality Settings");

    ImGui::InputInt("SPP (Samples)", &ui_.samples_per_pixel, 10, 100);
    ImGui::SliderInt("Max Depth", &ui_.max_depth, 1, 100); // 添加最大深度控制
    
    if (ui_.samples_per_pixel < 1) ui_.samples_per_pixel = 1;
    ImGui::TextDisabled("(Higher SPP = Less Noise, More Time)");

    ImGui::Separator();

    if (ImGui::Button("Apply & Start Render", ImVec2(-1.0f, 40.0f))) {
        if (!ui_.is_rendering) {
            ui_.restart_render = true;
        }
    }

    ImGui::PopStyleVar(); // Pop Padding
    ImGui::EndChild(); // End ControlsView

    ImGui::End(); // End MainDock
}