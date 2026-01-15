#ifndef APPLICATION_H
#define APPLICATION_H

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "WindowsApp.h"
#include "RenderManager.h"
#include "ImageProcessor.h"
#include "PerformanceMonitor.h"
#include "direct_light_integrator.h"
#include "path_integrator.h"
#include "pbr_path_integrator.h"
#include "rr_path_integrator.h"
#include "mis_path_integrator.h"
#include "image_ops.h"

// UI 状态
struct UIState {
    // 渲染配置
    int scene_gui_index = 0; // 对应 kSceneNames 数组的索引 (0, 1, 2...)
    int scene_id = 0;  // 实际传递给 select_scene 的 ID (1, 2, 4...)
    int samples_per_pixel = 100;
    int max_depth = 50;
    int integrator_idx = 0;

    // 相机参数
    float vfov = 20.0f;
    float aperture = 0.0f;
    float focus_dist = 10.0f;

    // 分辨率
    int image_width = 400;
    int aspect_w = 16;
    int aspect_h = 9;

    // 图像处理
    int tone_mapping_type = 0;
    float gamma = 2.0f;
    bool enable_post_process = false;
    int post_process_type = 0;

    // 状态标志
    bool restart_render = false;
    bool need_display_update = true;

    // 计时
    double render_start_time = 0.0;
    float last_fps = 0.0f;
    float last_ms = 0.0f;
    float log_height = 150.0f;

    // 保存
    int save_format_idx = 1;

    // 获取当前积分器
    std::shared_ptr<Integrator> get_current_integrator() const {
        switch (integrator_idx) {
            case 1: return std::make_shared<RRPathInterator>();
            case 2: return std::make_shared<PBRPathIntegrator>();
            case 3: return std::make_shared<MISPathIntegrator>();
            case 4: return std::make_shared<DirectLightIntegrator>();
            default: return std::make_shared<PathIntegrator>();
        }
    }
};

class Application {

    public:
        explicit Application(int initial_scene_id = 0);
        ~Application() = default;

        bool init();
        void run();

    private:
        // UI 相关
        void render_ui();
        void render_control_panel();
        void render_log_panel();

        // 事件处理
        void handle_events();
        void handle_render_completion(bool success);

        // 渲染控制
        void start_render();
        void pause_render();
        void resume_render();
        void stop_render();

        // 显示更新
        void update_display();
        bool should_update_display() const;

        // 工具函数
        void save_image();
        void log(const std::string& msg);
        RenderConfig create_render_config();void reset_ui_from_scene_config(int scene_id);

        // 核心组件
        WindowsApp::ptr window_;
        RenderManager render_manager_;
        ImageProcessor image_processor_;
        PerformanceMonitor perf_monitor_;

        // 状态
        UIState ui_state_;
        std::vector<unsigned char> image_data_;
        int width_ = 0;
        int height_ = 0;

        // 渲染缓冲快照（UI 线程只读快照，避免与渲染线程数据竞争）
        std::shared_ptr<RenderBuffer> render_buffer_snapshot_;

        // 日志
        std::deque<std::string> logs_;
        mutable std::mutex log_mutex_;

        // 计时
        double last_texture_update_ = 0.0;
};

#endif // APPLICATION_H
