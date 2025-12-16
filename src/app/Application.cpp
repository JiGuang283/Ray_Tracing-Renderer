#include "Application.h"
#include "scenes.h"
#include "camera.h"
#include "image_ops.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {
    constexpr double kTextureUpdateInterval = 1.0 / 30.0;
    constexpr int kMaxLogs = 100;
    constexpr int kMinImageWidth = 64;
    constexpr static int kDefaultLutSize = 4096;

    const char* kSceneNames[] = {
        "Random Spheres (Case 1)",          // Index 0
        "Two Spheres (Case 2)",             // Index 1
        "Earth (Case 4)",                   // Index 2
        "Simple Light (Case 5)",            // Index 3
        "Example Light (Case 6)",           // Index 4
        "Cornell Box (Case 7)",             // Index 5
        "Cornell Smoke (Case 8)",           // Index 6
        "Final Scene Book 1 (Case 9)",      // Index 7
        "Perlin Spheres (Case 10)",         // Index 8
        "PBR Test Scene (Case 11)",         // Index 9
        "PBR Spheres Grid (Case 12)",       // Index 10
        "PBR Gallery (Case 13)",            // Index 11
        "PBR Reference (Case 14)",          // Index 12
        "Point Light (Case 15)",            // Index 13
        "MIS Demo (Case 16)",               // Index 14
        "Directional Light (Case 17)",      // Index 15
        "Spot Light (Case 18)",             // Index 16
        "Environment Light (Case 19)",      // Index 17
        "Quad Light (Case 20)",             // Index 18
        "Cornell Box NEE (Case 21)",        // Index 19
        "Final Scene NEE (Case 22)",        // Index 20
        "MIS Comparison (Case 23)",         // Index 21
        "HDR: Studio (Case 24)",            // Index 22
        "HDR: Sunset (Case 25)",            // Index 23
        "HDR: RNL Probe (Case 26)",         // Index 24
        "HDR: St. Peters (Case 27)",        // Index 25
        "HDR: Uffizi (Case 28)",            // Index 26
        "Materials Showcase (Case 30)",     // Index 27
        "Cornell Extended (Case 31)",       // Index 28
        "Interior Lighting (Case 32)",      // Index 29
        "Jewelry Display (Case 33)",        // Index 30
        "Glass Caustics (Case 34)",         // Index 31
        "PBR Texture Demo (Case 35)",       // Index 32
        "PBR Floating Spheres (Case 36)",   // Index 33
        "PBR Grid Explicit (Case 37)",      // Index 34
        "Soft Shadow Demo (Case 38)",       // Index 35
        "Jewelry Simplified (Case 39)",     // Index 36
        "Multi-Light Demo (Case 40)",       // Index 37
        "CMY Shadows (Case 41)",            // Index 38
        "Infinity Mirror (Case 42)"         // Index 39
    };

    // 实际的 Scene ID (传给 select_scene 的参数)
    const int kSceneIds[] = {
        1, 2, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, 26, 27, 28,
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42
    };

    // 确保两个数组大小一致
    static_assert(IM_ARRAYSIZE(kSceneNames) == IM_ARRAYSIZE(kSceneIds), "Scene Arrays Mismatch!");

    const char* kIntegratorNames[] = {
        "Path Integrator", "RR Path Integrator", "PBR Path Integrator",
        "MIS Path Integrator", "Direct Light Integrator"
    };

    const char* kToneMapTypes[] = {
        "None (Clamp)", "Reinhard", "ACES (Filmic)"
    };

    const char* kPostProcessTypes[] = {
        "Simple Denoise (Blur)", "Sharpen",
        "Grayscale", "Invert Colors",
        "Median (Despeckle)", "Bilateral Filter"
    };

    const char* kSaveFormats[] = {
        "PPM (Raw)", "PNG (Lossless)",
        "BMP (Bitmap)", "JPG (Compressed)"
    };
}

Application::Application(int initial_scene_id)
    : perf_monitor_(60) {
    ui_state_.scene_id = initial_scene_id;
}

bool Application::init() {
    log("Initializing Application...");

    // 查找 initial_scene_id 在 kSceneIds 中的索引
    ui_state_.scene_gui_index = 0; // 默认
    for (int i = 0; i < IM_ARRAYSIZE(kSceneIds); ++i) {
        if (kSceneIds[i] == ui_state_.scene_id) {
            ui_state_.scene_gui_index = i;
            break;
        }
    }

    // 使用新函数来初始化UI状态
    reset_ui_from_scene_config(ui_state_.scene_id);

    SceneConfig cfg = select_scene(ui_state_.scene_id);
    width_ = cfg.image_width;

    height_ = static_cast<int>(width_ * (static_cast<float>(ui_state_.aspect_h) / ui_state_.aspect_w));

    // 初始化图像处理器
    ImageProcessConfig img_cfg;
    img_cfg.gamma = ui_state_.gamma;
    img_cfg.tone_mapping_type = ui_state_.tone_mapping_type;
    img_cfg.enable_post_process = ui_state_.enable_post_process;
    img_cfg.post_process_type = ui_state_.post_process_type;
    image_processor_.update_config(img_cfg);

    // 创建窗口
    window_ = WindowsApp::getInstance(width_, height_, "Ray Tracing Renderer");
    if (!window_) {
        log("ERROR: Failed to create window");
        return false;
    }

    image_data_.resize(width_ * height_ * 4);

    log("Initialization complete.");
    return true;
}

void Application::run() {
    while (!window_->shouldWindowClose()) {
        window_->processEvent();

        handle_events();

        if (should_update_display()) {
            update_display();
        }

        // 性能监控
        const auto& timings = window_->timings();
        PerformanceMonitor::Metrics metrics;
        metrics.cpu_ms = timings.cpu_ms;
        metrics.upload_ms = timings.upload_ms;
        metrics.render_ms = timings.render_ms;
        metrics.fps = ImGui::GetIO().Framerate;
        perf_monitor_.update(metrics);

        double current_time = ImGui::GetTime();
        if (perf_monitor_.should_report(current_time, 1.0)) {
            std::cout << perf_monitor_.get_report() << std::endl;
            perf_monitor_.mark_reported(current_time);
        }

        window_->beginRender();
        render_ui();
        window_->endRender();
    }
}

void Application::handle_events() {
    auto state = render_manager_.get_state();

    // 处理渲染完成
    if (state == RenderManager::State::Idle && render_manager_.get_buffer()) {
        // 渲染刚完成，确保最后一次显示更新
        ui_state_.need_display_update = true;
    }

    // 处理重启请求
    if (ui_state_.restart_render && state == RenderManager::State::Idle) {
        start_render();
        ui_state_.restart_render = false;
    }
}

void Application::handle_render_completion(bool success) {
    if (success) {
        float fps = ImGui::GetIO().Framerate;
        ui_state_.last_fps = fps;
        ui_state_.last_ms = fps > 0.0f ? (1000.0f / fps) : 0.0f;
        log("Render finished successfully.");
    } else {
        log("Render was cancelled or failed.");
    }
    ui_state_.need_display_update = true;
}

bool Application::should_update_display() const {
    double current_time = ImGui::GetTime();
    bool time_elapsed = (current_time - last_texture_update_) >= kTextureUpdateInterval;
    bool is_rendering = render_manager_.is_rendering();

    return (is_rendering && time_elapsed) || ui_state_.need_display_update;
}

void Application::update_display() {
    auto buffer = render_manager_.get_buffer();
    if (!buffer || buffer->get_width() != width_ || buffer->get_height() != height_) {
        return;
    }

    window_->markCpuStart();

    // 1. 获取当前 UI 设置的配置
    ImageProcessConfig active_config = image_processor_.get_config();

    // 2. 检查当前渲染状态
    bool is_rendering = render_manager_.is_rendering();

    // 3. 智能覆盖配置：
    // 如果正在渲染中 (Rendering)，强制关闭后处理 (Filter)，无论 UI 是否勾选。
    // 只有在 空闲 (Idle) 或 暂停 (Paused) 状态下，才允许应用 Filter。
    if (is_rendering) {
        if (active_config.enable_post_process) {
            // 临时关闭后处理，保证渲染性能
            active_config.enable_post_process = false;
            // 更新 ImageProcessor 使用这个临时配置
            image_processor_.update_config(active_config);
        }
    } else {
        // 如果不在渲染（已完成或暂停），确保配置与 UI 状态一致
        // 这里需要重新从 ui_state_ 读取，防止之前被覆盖
        ImageProcessConfig ui_cfg;
        ui_cfg.gamma = ui_state_.gamma;
        ui_cfg.tone_mapping_type = ui_state_.tone_mapping_type;
        ui_cfg.enable_post_process = ui_state_.enable_post_process; // UI 勾选状态
        ui_cfg.post_process_type = ui_state_.post_process_type;

        image_processor_.update_config(ui_cfg);
    }

    image_processor_.process(*buffer, image_data_, width_, height_);

    window_->markCpuEnd();
    window_->updateTexture(image_data_.data());

    last_texture_update_ = ImGui::GetTime();

    if (!render_manager_.is_rendering()) {
        ui_state_.need_display_update = false;
    }
}

RenderConfig Application::create_render_config() {
    SceneConfig scene_cfg = select_scene(ui_state_.scene_id);

    RenderConfig config;

    config.width = ui_state_.image_width;

    config.height = static_cast<int>(config.width *
        static_cast<float>(ui_state_.aspect_h) / ui_state_.aspect_w);
    config.samples_per_pixel = ui_state_.samples_per_pixel;
    config.max_depth = ui_state_.max_depth;
    config.world = scene_cfg.world;
    config.background = scene_cfg.background;
    config.lights = scene_cfg.lights;

    // 创建相机
    float aspect_ratio = static_cast<float>(ui_state_.aspect_w) / ui_state_.aspect_h;
    config.cam = std::make_shared<camera>(
        scene_cfg.lookfrom, scene_cfg.lookat, scene_cfg.vup,
        ui_state_.vfov,
        aspect_ratio,
        ui_state_.aperture,
        ui_state_.focus_dist,
        0.0, 1.0
    );

    // 选择积分器
    config.integrator = ui_state_.get_current_integrator();

    return config;
}

void Application::reset_ui_from_scene_config(int scene_id) {
    // 1. 获取新场景的配置
    SceneConfig cfg = select_scene(scene_id);

    // 2. 更新UI状态 (ui_state_)
    ui_state_.vfov = cfg.vfov;
    ui_state_.aperture = cfg.aperture;
    ui_state_.focus_dist = cfg.focus_dist;
    ui_state_.samples_per_pixel = cfg.samples_per_pixel;

    ui_state_.image_width = cfg.image_width;

    // 计算目标高度
    int target_height = static_cast<int>(cfg.image_width / cfg.aspect_ratio);

    // 将宽高比的分子分母设置为实际像素值，保证 float aspect_ratio 精度无误
    ui_state_.aspect_w = cfg.image_width;
    ui_state_.aspect_h = target_height;

    // 自动加载场景推荐的积分器
    // 如果场景包含 "Explicit Lights" (Case 37, 40等)，最好切换到 NEE 或 MIS
    if (scene_id >= 20 || scene_id == 15 || scene_id == 16) {
        // 倾向于使用 MIS 或 NEE
        if (ui_state_.integrator_idx == 0) { // 如果当前是普通 Path Tracing
            ui_state_.integrator_idx = 3; // 自动切到 MIS
        }
    }
}

void Application::start_render() {
    auto config = create_render_config();

    width_ = config.width;
    height_ = config.height;
    window_->setRenderSize(width_, height_);
    image_data_.resize(width_ * height_ * 4);

    ui_state_.render_start_time = ImGui::GetTime();

    render_manager_.start(config, [this](bool success) {
        handle_render_completion(success);
    });

    log("Render started: " + std::to_string(width_) + "x" + std::to_string(height_));
}

void Application::pause_render() {
    render_manager_.pause();
    ui_state_.need_display_update = true;
    log("Render paused.");
}

void Application::resume_render() {
    render_manager_.resume();
    log("Render resumed.");
}

void Application::stop_render() {
    render_manager_.stop();
    ui_state_.need_display_update = true;
    log("Render stopped.");
}

void Application::save_image() {
    ImageOps::save_image_to_disk(image_data_, width_, height_, ui_state_.save_format_idx);
    log("Image saved successfully.");
}

void Application::log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    logs_.push_back(msg);
    if (logs_.size() > kMaxLogs) {
        logs_.pop_front();
    }
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
    ImGui::PopStyleVar(2);

    // 布局参数
    float control_width = 450.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // 限制日志区域高度
    float min_log_h = 50.0f;
    float max_log_h = avail.y - 100.0f;
    if (ui_state_.log_height < min_log_h) ui_state_.log_height = min_log_h;
    if (ui_state_.log_height > max_log_h) ui_state_.log_height = max_log_h;

    float splitter_height = 5.0f;
    float top_height = avail.y - ui_state_.log_height - splitter_height;
    float image_w = avail.x - control_width;

    // --- Top Area ---
    ImGui::BeginChild("TopArea", ImVec2(0, top_height), false);

    // 左侧：渲染画面
    ImGui::BeginChild("RenderView", ImVec2(image_w, 0), false);
    if (window_->getTexture()) {
        ImGui::Image((void*)window_->getTexture(), ImGui::GetContentRegionAvail());
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // 右侧：控制面板
    render_control_panel();

    ImGui::EndChild();

    // Splitter (可调整大小的分隔条)
    ImGui::InvisibleButton("hsplitter", ImVec2(-1, splitter_height));
    if (ImGui::IsItemActive()) {
        ui_state_.log_height -= ImGui::GetIO().MouseDelta.y;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, ImGui::GetColorU32(ImGuiCol_Separator));

    // --- Bottom Area (Log) ---
    render_log_panel();

    ImGui::End();
}

void Application::render_control_panel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("ControlsView", ImVec2(0, 0), true);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImGui::Text("Renderer Controls");
    ImGui::Separator();

    // 渲染状态显示
    auto state = render_manager_.get_state();
    if (state == RenderManager::State::Rendering) {
        float fps = ImGui::GetIO().Framerate;
        float ms = fps > 0.f ? 1000.0f / fps : 0.f;
        ImGui::Text("Render Time: %.3f ms/frame (%.1f FPS)", ms, fps);

        float progress = render_manager_.get_progress();
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
        ImGui::Text("Rendering... %.1f%%", progress * 100.0f);

        // ETA 计算
        if (progress > 0.001f) {
            double current_time = ImGui::GetTime();
            double elapsed = current_time - ui_state_.render_start_time;
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
    } else if (state == RenderManager::State::Paused) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Paused");
    } else {
        ImGui::Text("Last Render: %.3f ms/frame (%.1f FPS)", ui_state_.last_ms, ui_state_.last_fps);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Idle");
    }

    ImGui::Separator();

    // 1. 场景选择
    ImGui::Text("1. Select Scene");
    int old_scene = ui_state_.scene_gui_index;
    if (ImGui::Combo("##Scene", &ui_state_.scene_gui_index, kSceneNames, IM_ARRAYSIZE(kSceneNames))) {
        if (ui_state_.scene_gui_index != old_scene) {
            // 1. 从映射数组获取真实的 Scene ID
            int real_scene_id = kSceneIds[ui_state_.scene_gui_index];
            // 2. 更新 State
            ui_state_.scene_id = real_scene_id;
            // 3. 根据新场景的配置重置 UI 参数
            reset_ui_from_scene_config(real_scene_id);
            // 4. 标记需要重启渲染
            ui_state_.restart_render = true;
        }
    }

    ImGui::Separator();

    // 2. 相机设置
    ImGui::Text("2. Camera Settings");
    ImGui::SliderFloat("FOV", &ui_state_.vfov, 1.0f, 120.0f);
    ImGui::SliderFloat("Aperture", &ui_state_.aperture, 0.0f, 2.0f);
    ImGui::SliderFloat("Focus Dist", &ui_state_.focus_dist, 0.1f, 50.0f);

    ImGui::Separator();

    // 3. 分辨率设置
    ImGui::Text("3. Resolution Settings");
    ImGui::InputInt("Width (px)", &ui_state_.image_width, 10, 100);
    if (ui_state_.image_width < kMinImageWidth) {
        ui_state_.image_width = kMinImageWidth;
    }

    ImGui::Text("Aspect Ratio:"); ImGui::SameLine();
    ImGui::PushItemWidth(50);
    ImGui::InputInt("##W", &ui_state_.aspect_w, 0, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::Text(":"); ImGui::SameLine();
    ImGui::PushItemWidth(50);
    ImGui::InputInt("##H", &ui_state_.aspect_h, 0, 0);
    ImGui::PopItemWidth();

    if (ui_state_.aspect_w < 1) ui_state_.aspect_w = 1;
    if (ui_state_.aspect_h < 1) ui_state_.aspect_h = 1;

    int current_height = static_cast<int>(ui_state_.image_width *
        (static_cast<float>(ui_state_.aspect_h) / ui_state_.aspect_w));
    ImGui::TextDisabled("Output Size: %d x %d", ui_state_.image_width, current_height);

    ImGui::Separator();

    // 4. 质量设置
    ImGui::Text("4. Quality Settings");
    ImGui::InputInt("SPP (Samples)", &ui_state_.samples_per_pixel, 10, 100);
    ImGui::SliderInt("Max Depth", &ui_state_.max_depth, 1, 100);
    if (ui_state_.samples_per_pixel < 1) ui_state_.samples_per_pixel = 1;
    ImGui::TextDisabled("(Higher SPP = Less Noise, More Time)");

    ImGui::Combo("Integrator", &ui_state_.integrator_idx, kIntegratorNames, IM_ARRAYSIZE(kIntegratorNames));

    ImGui::Separator();

    // 5. 后处理
    ImGui::Text("5. Post Processing");

    if (ImGui::Combo("Tone Mapping", &ui_state_.tone_mapping_type, kToneMapTypes, IM_ARRAYSIZE(kToneMapTypes))) {
        ImageProcessConfig cfg = image_processor_.get_config();
        cfg.tone_mapping_type = ui_state_.tone_mapping_type;
        image_processor_.update_config(cfg);
        ui_state_.need_display_update = true;
    }

    if (ImGui::SliderFloat("Gamma", &ui_state_.gamma, 0.1f, 5.0f)) {
        ImageProcessConfig cfg = image_processor_.get_config();
        cfg.gamma = ui_state_.gamma;
        image_processor_.update_config(cfg);
        ui_state_.need_display_update = true;
    }

    if (ImGui::Checkbox("Enable Filters", &ui_state_.enable_post_process)) {
        ImageProcessConfig cfg = image_processor_.get_config();
        cfg.enable_post_process = ui_state_.enable_post_process;
        image_processor_.update_config(cfg);
        ui_state_.need_display_update = true;
    }

    if (ui_state_.enable_post_process) {
        if (ImGui::Combo("Filter Type", &ui_state_.post_process_type, kPostProcessTypes, IM_ARRAYSIZE(kPostProcessTypes))) {
            ImageProcessConfig cfg = image_processor_.get_config();
            cfg.post_process_type = ui_state_.post_process_type;
            image_processor_.update_config(cfg);
            ui_state_.need_display_update = true;
        }
    }

    ImGui::Separator();

    // 渲染控制按钮
    if (state == RenderManager::State::Rendering) {
        if (ImGui::Button("Pause", ImVec2(100, 40))) {
            pause_render();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(100, 40))) {
            stop_render();
        }
    } else if (state == RenderManager::State::Paused) {
        if (ImGui::Button("Resume", ImVec2(100, 40))) {
            resume_render();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(100, 40))) {
            stop_render();
        }
    } else {
        if (ImGui::Button("Start Render", ImVec2(-1.0f, 40.0f))) {
            start_render();
        }
    }

    ImGui::Separator();

    // 保存图像
    ImGui::Combo("Format", &ui_state_.save_format_idx, kSaveFormats, IM_ARRAYSIZE(kSaveFormats));
    if (ImGui::Button("Save Image", ImVec2(-1.0f, 30.0f))) {
        save_image();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void Application::render_log_panel() {
    ImGui::BeginChild("LogArea", ImVec2(0, 0), true);
    ImGui::Text("Log / Console");
    ImGui::SetWindowFontScale(1.7f);
    ImGui::Separator();

    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::SetWindowFontScale(1.5f);

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        for (const auto& msg : logs_) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();
}
