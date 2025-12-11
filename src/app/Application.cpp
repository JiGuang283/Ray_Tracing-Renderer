#include "Application.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <deque>
#include <mutex>
#include "image_ops.h"

#include "camera.h"
#include "imgui.h"
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
    stop_render();
}

// Init
bool Application::init() {
    log("Initializing Application...");
    SceneConfig cfg = select_scene(ui_.scene_id);
    width_ = cfg.image_width;
    height_ = static_cast<int>(width_ / cfg.aspect_ratio);

    // 初始化 UI 参数为场景默认值
    ui_.vfov = cfg.vfov;
    ui_.aperture = cfg.aperture;
    ui_.focus_dist = cfg.focus_dist;
    ui_.max_depth = RenderConfig::kMaxDepth;
    ui_.gamma = 2.0f; // Default Gamma

    win_app_ = WindowsApp::getInstance(width_, height_, "Ray_Tracing-Renderer");
    if (!win_app_) {
        std::cerr << "Error: failed to create window" << std::endl;
        return false;
    }

    image_data_.resize(width_ * height_ * 4); // RGBA
    start_render(false);
    ui_.restart_render = false;
    log("Initialization complete.");
    return true;
}

void Application::run() {
    double last_texture_update_time = 0.0;
    const double texture_update_interval = 1.0 / 30.0;

    while (!win_app_->shouldWindowClose()) { // Render loop
        win_app_->processEvent();

        if (ui_.restart_render && !ui_.is_rendering && !ui_.is_paused) {
            start_render(false);
            ui_.restart_render = false;
            ui_.need_display_update = true;
        }

        // 获取当前时间
        double current_time = ImGui::GetTime();

        // 只有当 "正在渲染且达到更新间隔" 或者 "强制需要更新" 时，才执行繁重的纹理更新
        if ((ui_.is_rendering && (current_time - last_texture_update_time >= texture_update_interval)) || ui_.need_display_update) {
            update_display_from_buffer();
            last_texture_update_time = current_time;

            // 如果不是在渲染中（即处于暂停或完成状态），说明这是一次性更新（如调整Gamma），
            // 更新完后重置标记，下一帧就不再执行此函数，直到再次触发。
            if (!ui_.is_rendering) {
                ui_.need_display_update = false;
            }
        }


        win_app_->beginRender();
        render_ui();
        win_app_->endRender();
    }
}

void Application::log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    logs_.push_back(msg);
    if (logs_.size() > 100) logs_.pop_front();
}

void Application::stop_render() {
    if (render_thread_.joinable()) {
        renderer_.cancel();
        render_thread_.join();
    }
    ui_.is_rendering = false;
    ui_.is_paused = false;

    ui_.need_display_update = true;

    log("Render stopped by user.");
}

void Application::pause_render() {
    if (ui_.is_rendering) {
        renderer_.cancel();
        if (render_thread_.joinable()) {
            render_thread_.join();
        }
        ui_.is_rendering = false;
        ui_.is_paused = true;

        ui_.need_display_update = true;

        log("Render paused.");
    }
}

void Application::start_render(bool resume) {
    if (render_thread_.joinable()) {
        renderer_.cancel();
        render_thread_.join();
    }

    if (resume) {
        if (!render_buffer_ || render_buffer_->get_width() != ui_.image_width) {
            resume = false;
            log("Cannot resume: buffer invalid or resolution changed. Starting new render.");
        }
    }

    renderer_.reset();
    ui_.is_rendering = true;
    ui_.is_paused = false;
    ui_.render_start_time = ImGui::GetTime(); // 记录开始时间

    SceneConfig config = select_scene(ui_.scene_id);
    float ratio_val = static_cast<float>(ui_.aspect_w) / static_cast<float>(ui_.aspect_h);

    if (!resume) {
        width_ = ui_.image_width;
        height_ = static_cast<int>(width_ / ratio_val);
        win_app_->setRenderSize(width_, height_);
        image_data_.resize(width_ * height_ * 4);
        render_buffer_ = std::make_shared<RenderBuffer>(width_, height_);
        render_buffer_->clear();
        log("Starting render: " + std::to_string(width_) + "x" + std::to_string(height_));
    } else {
        log("Resuming render...");
    }

    auto cam = std::make_shared<camera>(
        config.lookfrom, config.lookat, config.vup, 
        ui_.vfov, 
        ratio_val, 
        ui_.aperture, 
        ui_.focus_dist, 
        RenderConfig::kShutterOpen, RenderConfig::kShutterClose);

    switch (ui_.integrator_idx) {
        case 0:
            renderer_.set_integrator(ui_.integrator);
            break;

        case 1:
            renderer_.set_integrator(ui_.rrIntegrator);
            break;

        case 2:
            renderer_.set_integrator(ui_.pbrIntegrator);
            break;

        default:
            renderer_.set_integrator(ui_.integrator);
            break;
    }

    renderer_.set_samples(ui_.samples_per_pixel);
    renderer_.set_max_depth(ui_.max_depth);

    render_thread_ = std::thread([this, world = config.world, cam, bg = config.background]() {
        renderer_.render(world, cam, bg, *render_buffer_);
        if (!renderer_.is_cancelled()) {
            float fps = ImGui::GetIO().Framerate;
            ui_.last_fps = fps;
            ui_.last_ms = fps > 0.0f ? (1000.0f / fps) : 0.0f;
            log("Render finished successfully.");
        }
        ui_.is_rendering = false;
    });
}

void Application::save_image() const {
    ImageOps::save_image_to_disk(image_data_, width_, height_, ui_.save_format_idx);
}

void Application::update_display_from_buffer() {
    if (!render_buffer_ || render_buffer_->get_width() != width_ || render_buffer_->get_height() != height_) {
        return;
    }
    const auto &pixels = render_buffer_->get_data();
    float inv_gamma = 1.0f / ui_.gamma;

    // 预计算 Gamma LUT（0..1 区间映射到 0..255），256 档足够 LDR 显示
    static thread_local std::array<unsigned char, 256> gamma_lut;
    static thread_local float last_inv_gamma = -1.0f;
    if (std::fabs(last_inv_gamma - inv_gamma) > 1e-6f) {
        for (int i = 0; i < 256; ++i) {
            float v = i / 255.0f;
            gamma_lut[i] = static_cast<unsigned char>(255.999f * std::pow(v, inv_gamma));
        }
        last_inv_gamma = inv_gamma;
    }
    auto tone_map = [&](vec3 c) {
        switch (ui_.tone_mapping_type) {
        case 1: // Reinhard
            return c / (c + vec3(1.0, 1.0, 1.0));
        case 2: // ACES
            return ImageOps::ACESFilm(c);
        default:
            return c;
        }
    };

    #pragma omp parallel for collapse(2)
    for (int j = 0; j < height_; ++j) {
        for (int i = 0; i < width_; ++i) {
            vec3 c = pixels[height_ - 1 - j][i];
            c = tone_map(c);

            // clamp 到 [0,1]
            float r = std::fmax(0.0f, std::fmin(1.0f, static_cast<float>(c.x())));
            float g = std::fmax(0.0f, std::fmin(1.0f, static_cast<float>(c.y())));
            float b = std::fmax(0.0f, std::fmin(1.0f, static_cast<float>(c.z())));

            int idx = (j * width_ + i) * 4;
            image_data_[idx + 0] = gamma_lut[static_cast<int>(r * 255.0f + 0.5f)];
            image_data_[idx + 1] = gamma_lut[static_cast<int>(g * 255.0f + 0.5f)];
            image_data_[idx + 2] = gamma_lut[static_cast<int>(b * 255.0f + 0.5f)];
            image_data_[idx + 3] = 255;
        }
    }

    if (ui_.enable_post_process) {
        apply_post_processing();
    }

    win_app_->updateTexture(image_data_.data());
}

void Application::apply_post_processing() {
    ImageOps::apply_post_processing(image_data_, width_, height_, ui_.post_process_type);
}

void Application::render_ui() {
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
    float control_width = 450.0f; // 控制面板宽度
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    // 限制日志区域高度范围
    float min_log_h = 50.0f;
    float max_log_h = avail.y - 100.0f;
    if (ui_.log_height < min_log_h) ui_.log_height = min_log_h;
    if (ui_.log_height > max_log_h) ui_.log_height = max_log_h;

    float splitter_height = 5.0f;
    float top_height = avail.y - ui_.log_height - splitter_height;
    float image_w = avail.x - control_width; 

    // --- Top Area ---
    ImGui::BeginChild("TopArea", ImVec2(0, top_height), false);

    // 左侧：渲染画面
    ImGui::BeginChild("RenderView", ImVec2(image_w, 0), false);
    if (win_app_->getTexture()) {
        ImGui::Image((void*)win_app_->getTexture(), ImGui::GetContentRegionAvail());
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("ControlsView", ImVec2(0, 0), true);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImGui::Text("Renderer Controls");
    ImGui::Separator();

    if (ui_.is_rendering) {
        float fps = ImGui::GetIO().Framerate;
        float ms  = fps > 0.f ? 1000.0f / fps : 0.f;
        ImGui::Text("Render Time: %.3f ms/frame (%.1f FPS)", ms, fps);
        float progress = renderer_.get_progress();
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
        ImGui::Text("Rendering... %.1f%%", progress * 100.0f);

        if (progress > 0.001f) {
            double current_time = ImGui::GetTime();
            double elapsed = current_time - ui_.render_start_time;
            double remaining = (elapsed / progress) - elapsed;
            
            if (remaining < 0) remaining = 0;

            int r_hour = static_cast<int>(remaining) / 3600;
            int r_min = (static_cast<int>(remaining) % 3600) / 60;
            int r_sec = static_cast<int>(remaining) % 60;
            
            if (r_hour > 0)
                ImGui::Text("ETA: %dh %dm %ds", r_hour, r_min, r_sec);
            else
                ImGui::Text("ETA: %dm %ds", r_min, r_sec);
        } else {
            ImGui::Text("ETA: Calculating...");
        }
    } else if (ui_.is_paused) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Paused");
    } else {
        ImGui::Text("Last Render: %.3f ms/frame (%.1f FPS)", ui_.last_ms, ui_.last_fps);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Idle");
    }

    ImGui::Separator();

    const char *scene_names[] = {"Random Spheres", "Example Light", "Two Spheres", "Perlin Spheres", "Earth", "Simple Light", "Cornell Box", "Cornell Smoke", "Final Scene"};
    ImGui::Text("1. Select Scene");
    
    int old_scene = ui_.scene_id;
    if (ImGui::Combo("##Scene", &ui_.scene_id, scene_names, IM_ARRAYSIZE(scene_names))) {
        if (ui_.scene_id != old_scene) {
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
    ImGui::SliderInt("Max Depth", &ui_.max_depth, 1, 100);
    
    if (ui_.samples_per_pixel < 1) ui_.samples_per_pixel = 1;
    ImGui::TextDisabled("(Higher SPP = Less Noise, More Time)");

    const char* integrator_names[] = { "Path Integrator", "RR Path Integrator", "PBR Path Integrator" };
    ImGui::Combo("Integrator", &ui_.integrator_idx, integrator_names, IM_ARRAYSIZE(integrator_names));

    ImGui::Separator();
    ImGui::Text("5. Post Processing");

    // Tone Mapping Control
    const char* tm_types[] = { "None (Clamp)", "Reinhard", "ACES (Filmic)" };
    if (ImGui::Combo("Tone Mapping", &ui_.tone_mapping_type, tm_types, IM_ARRAYSIZE(tm_types))) {
        ui_.need_display_update = true;
    }

    if (ImGui::SliderFloat("Gamma", &ui_.gamma, 0.1f, 5.0f)) {
        ui_.need_display_update = true;
    }

    if (ImGui::Checkbox("Enable Filters", &ui_.enable_post_process)) {
        ui_.need_display_update = true;
    }

    if (ui_.enable_post_process) {
        const char* pp_types[] = { "Simple Denoise (Blur)", "Sharpen", "Grayscale", "Invert Colors", "Median (Despeckle)" };
        if (ImGui::Combo("Filter Type", &ui_.post_process_type, pp_types, IM_ARRAYSIZE(pp_types))) {
            ui_.need_display_update = true;
        }
    }

    ImGui::Separator();

    if (ui_.is_rendering) {
        if (ImGui::Button("Pause", ImVec2(100, 40))) {
            pause_render();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(100, 40))) {
            stop_render();
        }
    } else if (ui_.is_paused) {
        if (ImGui::Button("Resume", ImVec2(100, 40))) {
            start_render(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(100, 40))) {
            stop_render();
        }
    } else {
        if (ImGui::Button("Start Render", ImVec2(-1.0f, 40.0f))) {
            start_render(false);
        }
    }

    ImGui::Separator();
    
    const char* formats[] = { "PPM (Raw)", "PNG (Lossless)", "BMP (Bitmap)", "JPG (Compressed)" };
    ImGui::Combo("Format", &ui_.save_format_idx, formats, IM_ARRAYSIZE(formats));

    if (ImGui::Button("Save Image", ImVec2(-1.0f, 30.0f))) {
        save_image();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::EndChild();

    // Splitter (Resizable Handle)
    ImGui::InvisibleButton("hsplitter", ImVec2(-1, splitter_height));
    if (ImGui::IsItemActive()) {
        ui_.log_height -= ImGui::GetIO().MouseDelta.y;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    // 绘制分隔条视觉效果
    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, ImGui::GetColorU32(ImGuiCol_Separator));

    // Bottom Area (Log)
    ImGui::BeginChild("LogArea", ImVec2(0, 0), true);
    ImGui::Text("Log / Console");
    ImGui::SetWindowFontScale(1.7f);
    ImGui::Separator();
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::SetWindowFontScale(1.5f); // 放大日志输出文字
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        for (const auto& msg : logs_) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::EndChild(); // End LogArea

    ImGui::End(); // End MainDock
}