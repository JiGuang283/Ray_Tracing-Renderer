#ifndef RTWINDOWSAPP_H
#define RTWINDOWSAPP_H

#include "SDL2/SDL.h"

#include <memory>
#include <string>
#include <chrono>

class WindowsApp final {

    public:
        using ptr = std::shared_ptr<WindowsApp>;

        WindowsApp(WindowsApp &) = delete;
        WindowsApp &operator=(const WindowsApp &) = delete;

        ~WindowsApp();

        // Event
        void processEvent();
        bool shouldWindowClose() const {
            return m_quit;
        }

        void beginRender();
        void endRender() const;

        void setRenderSize(int width, int height);

        //  更新 GPU 纹理
        void updateTexture(const unsigned char *data) const;

        // 获取 SDL 纹理指针
        SDL_Texture* getTexture() const { return m_texture; }

        static WindowsApp::ptr getInstance(int width, int height, const std::string &title);

        // 计时接口
        void markCpuStart();
        void markCpuEnd();

        struct FrameTimings {
            double cpu_ms   = 0.0;
            double upload_ms= 0.0;
            double render_ms= 0.0;
        };

        const FrameTimings& timings() const { return m_timings; }

    private:

        WindowsApp() = default;
        bool setup(int width, int height, std::string title);
        void cleanup() const;
        void recreateTexture(int width, int height);

    private:
        // Screen size
        int m_screen_width = 0;
        int m_screen_height = 0;
        bool m_quit = false;

        // Window handler
        SDL_Window *m_window_handle = nullptr;
        SDL_Renderer *m_renderer = nullptr;
        SDL_Event m_events;

        SDL_Texture *m_texture = nullptr; // 用于显示光线追踪图像的 SDL 纹理

        // Singleton pattern
        static WindowsApp::ptr m_instance;

        using Clock = std::chrono::steady_clock;

        FrameTimings m_timings{};
        Clock::time_point m_cpu_start{};
};

#endif
