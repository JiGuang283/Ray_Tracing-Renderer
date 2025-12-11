#ifndef APPLICATION_H
#define APPLICATION_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "WindowsApp.h"
#include "renderer.h"
#include "render_buffer.h"

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
    float last_fps = 0.0f; // 上一帧的帧率
    float last_ms = 0.0f; // 上一帧的毫秒数
    int image_width = 400; // 图片宽度
    // 图片宽高比
    int aspect_w = 16;
    int aspect_h = 9;
};

class Application {
public:
    Application(int initial_scene_id);
    ~Application();

    bool init();
    void run();

private:
    void start_render(); // 启动渲染
    void update_display_from_buffer(); // 从缓冲区更新显示

    void render_ui(); // 渲染UI

private:
    UIState ui_;  // UI状态
    WindowsApp::ptr win_app_;  // 窗口
    Renderer renderer_; // 渲染器
    std::shared_ptr<RenderBuffer> render_buffer_; // 渲染缓冲区
    std::thread render_thread_; // 渲染线程
    std::vector<unsigned char> image_data_; // 图片数据
    int width_ = 0, height_ = 0; // 窗口宽高
};

#endif //APPLICATION_H