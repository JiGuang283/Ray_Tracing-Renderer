#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include "renderer.h"
#include "render_buffer.h"
#include "camera.h"
#include "hittable.h"

// 渲染配置结构
struct RenderConfig {
    int width = 400;
    int height = 225;
    int samples_per_pixel = 100;
    int max_depth = 50;

    std::shared_ptr<hittable> world;
    std::shared_ptr<camera> cam;
    color background;
    std::shared_ptr<Integrator> integrator;
    std::vector<shared_ptr<Light>> lights;
};

class RenderManager {
    public:
        enum class State {
            Idle,
            Rendering,
            Paused,
            Stopping
        };

        using CompletionCallback = std::function<void(bool success)>;

        RenderManager() = default;
        ~RenderManager();

        // 禁止拷贝，允许移动
        RenderManager(const RenderManager&) = delete;
        RenderManager& operator=(const RenderManager&) = delete;

        void start(const RenderConfig& config, CompletionCallback callback = nullptr);
        void pause();
        void resume();
        void stop();

        State get_state() const { return state_.load(std::memory_order_acquire); }
        float get_progress() const { return renderer_.get_progress(); }
        bool is_rendering() const { return state_ == State::Rendering; }

        std::shared_ptr<RenderBuffer> get_buffer() const { return render_buffer_; }

    private:
        void render_loop(const RenderConfig& config);
        void cleanup_thread();

        Renderer renderer_;
        std::shared_ptr<RenderBuffer> render_buffer_;
        std::thread render_thread_;
        std::atomic<State> state_{State::Idle};
        CompletionCallback completion_callback_;
        RenderConfig current_config_;
};

#endif //RENDERMANAGER_H
