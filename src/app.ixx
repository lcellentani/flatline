module;

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <functional>
#include <string>

#include "expected.hpp"

export module engine.app;

// Owns the SDL3 + OpenGL3 + Dear ImGui window/context lifecycle and the main
// loop. Deliberately has no knowledge of what gets drawn: `run`'s callback is
// invoked once per frame, between `ImGui::NewFrame()` and `ImGui::Render()`,
// so panel/UI code lives entirely outside this module.
//
// Construction can fail (window/GL context creation) and this project does
// not use exceptions (see coding_style.md), so there is no public
// constructor -- use App::create(), which returns an Expected instead of
// throwing; only a fully-initialized App ever reaches the private
// constructor.
export class App {
public:
    using FrameCallback = std::function<void()>;

    [[nodiscard]] static Expected<App, std::string> create();

    ~App();

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

    void run(const FrameCallback& on_frame = {});

    // Public only because Expected<App, E> constructs App in place inside a
    // std::variant, whose internals can't practically be granted friend
    // access. Not meant to be called directly -- use App::create(), which
    // is the only place that has valid arguments for it.
    App(SDL_Window* window, SDL_GLContext gl_context, ImGuiIO* io);

private:
    void poll_events();
    void render_frame(const FrameCallback& on_frame);

    SDL_Window*   window_;
    SDL_GLContext gl_context_;
    ImGuiIO*      io_;
    bool          done_ = false;
};
