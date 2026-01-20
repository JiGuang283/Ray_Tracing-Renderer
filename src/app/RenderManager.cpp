#include "RenderManager.h"

RenderManager::~RenderManager() {
    stop();
}

void RenderManager::start(const RenderConfig& config, CompletionCallback callback) {
    // 确保之前的渲染已完成
    cleanup_thread();

    state_.store(State::Rendering, std::memory_order_release);
    completion_callback_ = callback;
    current_config_ = config;

    // 创建新的渲染缓冲区
    render_buffer_ = std::make_shared<RenderBuffer>(config.width, config.height);
    render_buffer_->clear();

    // 配置渲染器
    renderer_.reset();
    renderer_.set_integrator(config.integrator);
    renderer_.set_samples(config.samples_per_pixel);
    renderer_.set_max_depth(config.max_depth);

    // 启动渲染线程
    render_thread_ = std::thread([this, config]() {
        render_loop(config);
    });
}

void RenderManager::pause() {
    State expected = State::Rendering;
    if (state_.compare_exchange_strong(expected, State::Paused)) {
        renderer_.cancel();
    }
}

void RenderManager::resume() {
    State expected = State::Paused;
    if (state_.compare_exchange_strong(expected, State::Rendering)) {
        // 使用当前配置继续渲染
        start(current_config_, completion_callback_);
    }
}

void RenderManager::stop() {
    State current = state_.load(std::memory_order_acquire);
    if (current != State::Idle) {
        state_.store(State::Stopping, std::memory_order_release);
        renderer_.cancel();
        cleanup_thread();
        state_.store(State::Idle, std::memory_order_release);
    }
}

void RenderManager::render_loop(const RenderConfig& config) {
    bool success = false;

    try {
        renderer_.render(config.world, config.cam, config.background, *render_buffer_, config.lights);
        success = !renderer_.is_cancelled();
    } catch (const std::exception& e) {
        // 记录错误但不崩溃
        success = false;
    }

    // 只有在正常完成时才改变状态
    State expected = State::Rendering;
    if (state_.compare_exchange_strong(expected, State::Idle)) {
        if (completion_callback_) {
            completion_callback_(success);
        }
    }
}

void RenderManager::cleanup_thread() {
    if (render_thread_.joinable()) {
        render_thread_.join();
    }
}
