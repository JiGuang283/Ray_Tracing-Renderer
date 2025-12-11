#ifndef APPLICATION_H
#define APPLICATION_H

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "WindowsApp.h"
#include "path_integrator.h"
#include "pbr_path_integrator.h"
#include "renderer.h"
#include "render_buffer.h"
#include "rr_path_integrator.h"

// UI 状态
struct UIState {

    int scene_id = 0; // 场景ID
    int samples_per_pixel = 100; // 采样数
    int max_depth = 50; // 最大光线弹射深度
    float vfov = 20.0f; // 垂直视场角
    float aperture = 0.0f; // 光圈大小
    float focus_dist = 10.0f; // 对焦距离
    bool restart_render = true; // 是否重新渲染
    std::atomic<bool> is_rendering = {false}; // 是否正在渲染
    bool is_paused = false; // 是否暂停
    float last_fps = 0.0f; // 上一帧的帧率
    float last_ms = 0.0f; // 上一帧的毫秒数
    int image_width = 400; // 图片宽度

    // 图片宽高比
    int aspect_w = 16;
    int aspect_h = 9;
    int save_format_idx = 1; // 0: PPM, 1: PNG, 2: BMP, 3: JPG

    double render_start_time = 0.0; // 渲染开始时间
    float log_height = 150.0f; // 日志区域高度

    // 输出后处理
    bool enable_post_process = false;
    int post_process_type = 0; // 0: Blur, 1: Sharpen, 2: Grayscale, 3: Invert
    float gamma = 2.0f;

    //积分器类型
    int integrator_idx = 0; // 0: Path, 1: RR, 2: PBR
    std::shared_ptr<Integrator> integrator = std::make_shared<PathIntegrator>();
    std::shared_ptr<Integrator> rrIntegrator = std::make_shared<RRPathInterator>();
    std::shared_ptr<Integrator> pbrIntegrator = std::make_shared<PBRPathIntegrator>();

};

class Application {
public:
    Application(int initial_scene_id);
    ~Application();

    bool init();
    void run();

private:
    void start_render(bool resume = false); // 启动渲染 (支持继续)
    void stop_render(); // 停止渲染
    void pause_render(); // 暂停渲染
    
    void save_image() const;   // 保存图片
    void update_display_from_buffer(); // 从缓冲区更新显示
    void apply_post_processing(); // 应用后处理

    void render_ui(); // 渲染UI
    void log(const std::string& msg); // 记录日志

private:
    UIState ui_;  // UI状态
    WindowsApp::ptr win_app_;  // 窗口
    Renderer renderer_; // 渲染器
    std::shared_ptr<RenderBuffer> render_buffer_; // 渲染缓冲区
    std::thread render_thread_; // 渲染线程
    std::vector<unsigned char> image_data_; // 图片数据
    int width_ = 0, height_ = 0; // 窗口宽高

    std::deque<std::string> logs_; // 日志队列
    std::mutex log_mutex_; // 日志锁
};

#endif //APPLICATION_H