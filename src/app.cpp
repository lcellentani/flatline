module;

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <string>

#include "expected.hpp"

module engine.app;

namespace {

constexpr const char* kGlslVersion = "#version 130";
constexpr const char* kWindowTitle = "Flatline - WoZ GUI";
constexpr ImVec4 kClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

}  // namespace

Expected<App, std::string> App::create() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return Expected<App, std::string>::err(std::string("SDL_Init(): ") + SDL_GetError());
    }

    // GL 3.0 core + GLSL 130: broadly compatible with Windows OpenGL drivers
    // without requiring a newer core profile than the ImGui backend needs.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window =
        SDL_CreateWindow(kWindowTitle, (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr) {
        // Not calling SDL_Quit() here: this failure is fatal to the process
        // (main() exits right after seeing the error), so the OS reclaims
        // the SDL subsystem state regardless.
        return Expected<App, std::string>::err(std::string("SDL_CreateWindow(): ") + SDL_GetError());
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        return Expected<App, std::string>::err(std::string("SDL_GL_CreateContext(): ") + SDL_GetError());
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO* io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Deliberately not ImGuiConfigFlags_ViewportsEnable: single OS window only.

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(kGlslVersion);

    return Expected<App, std::string>::ok(window, gl_context, io);
}

App::App(SDL_Window* window, SDL_GLContext gl_context, ImGuiIO* io)
    : window_(window), gl_context_(gl_context), io_(io) {}

App::~App() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void App::run(const FrameCallback& on_frame) {
    while (!done_) {
        poll_events();

        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        render_frame(on_frame);
    }
}

void App::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            done_ = true;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window_)) {
            done_ = true;
        }
    }
}

void App::render_frame(const FrameCallback& on_frame) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (on_frame) {
        on_frame();
    }

    ImGui::Render();

    glViewport(0, 0, (int)io_->DisplaySize.x, (int)io_->DisplaySize.y);
    glClearColor(kClearColor.x * kClearColor.w, kClearColor.y * kClearColor.w, kClearColor.z * kClearColor.w,
                 kClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
}
