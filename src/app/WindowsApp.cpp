#include "WindowsApp.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <cstring>
#include <iomanip>
#include <iostream>

WindowsApp::ptr WindowsApp::m_instance = nullptr;

WindowsApp::ptr WindowsApp::getInstance(int width, int height, const std::string &title) {
    if (m_instance == nullptr) {
        m_instance = std::shared_ptr<WindowsApp>(new WindowsApp());
        if (!m_instance->setup(width, height, title)) {
            m_instance = nullptr;
        }
    }
    return m_instance;
}

WindowsApp::~WindowsApp() {
    cleanup();
}

bool WindowsApp::setup(int width, int height, std::string title) {
    m_screen_width = width;
    m_screen_height = height;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError()
                  << std::endl;
        return false;
    }

    // 创建窗口
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    m_window_handle = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    // 创建窗口失败
    if (!m_window_handle) {
        return false;
    }
    // 设置缩放质量 (在创建纹理前设置)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    // 创建 SDL_Renderer
    // 优先尝试硬件加速，如果失败则尝试软件渲染 (增加 Windows 兼容性)
    m_renderer = SDL_CreateRenderer(m_window_handle, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::cerr << "Hardware accelerated renderer creation failed, trying software..." << std::endl;
        m_renderer = SDL_CreateRenderer(m_window_handle, -1, SDL_RENDERER_SOFTWARE);
    }
    
    if (!m_renderer) {
        std::cerr << "SDL_Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // 初始化 ImGui 后端 (SDL2 + SDL_Renderer2)
    ImGui_ImplSDL2_InitForSDLRenderer(m_window_handle, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    // 创建用于显示光线追踪结果的 SDL 纹理
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!m_texture) {
        std::cerr << "SDL_Texture could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void WindowsApp::recreateTexture(int width, int height) {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    m_screen_width = width;
    m_screen_height = height;
    // 创建 RGBA8888 目标纹理
    m_texture = SDL_CreateTexture(m_renderer,
                                 SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 m_screen_width, m_screen_height);
    // 设置缩放质量
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

void WindowsApp::setRenderSize(int width, int height) {
    // 尺寸不同时才重建
    if (width != m_screen_width || height != m_screen_height) {
        recreateTexture(width, height);
    }
}

void WindowsApp::processEvent() {
    while (SDL_PollEvent(&m_events)) {
        ImGui_ImplSDL2_ProcessEvent(&m_events);
        if (m_events.type == SDL_QUIT) {
            m_quit = true;
        }
        if (m_events.type == SDL_WINDOWEVENT && m_events.window.event == SDL_WINDOWEVENT_CLOSE && m_events.window.windowID == SDL_GetWindowID(m_window_handle)) {
            m_quit = true;
        }
    }
}

void WindowsApp::updateTexture(const unsigned char *rgba) const {
    auto t0 = Clock::now();

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) == 0) {
        // pitch 是每行字节数，逐行拷贝
        const int row_bytes = m_screen_width * 4;
        for (int y = 0; y < m_screen_height; ++y) {
            std::memcpy(static_cast<unsigned char*>(pixels) + y * pitch,
                        rgba + y * row_bytes,
                        row_bytes);
        }
        SDL_UnlockTexture(m_texture);
    }
    auto t1 = Clock::now();
    const_cast<WindowsApp*>(this)->m_timings.upload_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void WindowsApp::beginRender() {
    // 开始新的一帧
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void WindowsApp::endRender() const {
    auto t0 = Clock::now();

    // ImGui 准备渲染数据
    ImGui::Render();
    // 清除屏幕（黑色背景）
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // 将 ImGui 的内容画在最上层（渲染结果纹理由 ImGui::Image 绘制，避免重复 RenderCopy）
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);
    // 交换缓冲区显示
    SDL_RenderPresent(m_renderer);

    auto t1 = Clock::now();
    const_cast<WindowsApp*>(this)->m_timings.render_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void WindowsApp::cleanup() const {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (m_texture) {
        SDL_DestroyTexture(m_texture);
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }
    if (m_window_handle) {
        SDL_DestroyWindow(m_window_handle);
    }
    SDL_Quit();
}

void WindowsApp::markCpuStart() {
    m_cpu_start = Clock::now();
}
void WindowsApp::markCpuEnd() {
    auto t1 = Clock::now();
    m_timings.cpu_ms = std::chrono::duration<double, std::milli>(t1 - m_cpu_start).count();
}

