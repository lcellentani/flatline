#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include "gm_effects.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_rules.hpp"
#include "gm_turn.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

constexpr const char* kBlankWorkOrderSkeleton = R"({
      "interpretation": "",
      "ruling": "allow",
      "modifiers": [],
      "effects_on_success": [],
      "narration_on_success": "",
      "effects_on_failure": [],
      "narration_on_failure": ""
})";

constexpr const char* kRoomDescriptionTitle = "Room Description";
constexpr const char* kWorkOrderTitle = "GM WorkOrder";
constexpr const char* kLogTitle = "Log";

// Own session log per front end (woz_loop's is session_log.jsonl, the demo's is
// woz_log.jsonl) -- never interleaved.
constexpr const char* kGuiLogPath = "gui_session_log.jsonl";

// InputTextMultiline needs a raw, fixed-capacity buffer (imgui_stdlib.h's std::string
// convenience wrapper isn't vendored -- see Task 3.1's file list); wo_text stays the
// std::string source of truth for everything outside the widget itself, resynced with
// this buffer on every programmatic change (Submit reset, Load into editor) and read
// back from it once per frame right after the widget renders.
constexpr size_t kWorkOrderBufSize = 16384;

namespace fs = std::filesystem;

static std::string slurp(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Shared by the Log panel's entry list and the WorkOrder panel's This Session group --
// both render "Turn N -- <status> -- <teaser>" off the same stored LogEntry JSON.
static std::string entry_label(const nlohmann::json& entry) {
    const std::string status = entry.value("status", std::string{});
    const std::string teaser = status == "applied" ? entry.value("narration", std::string{})
                                                   : entry.value("reason", std::string{});
    return "Turn " + std::to_string(entry.value("turn_number", int64_t{0})) + " — " + status +
           " — " + teaser;
}

int main(int argc, char* argv[]) {
    using namespace gm;

    const char* room_path = (argc > 1) ? argv[1] : "room_guardroom.json";

    World world;
    try {
        world = parse_world(slurp(room_path));
    } catch (const std::exception& ex) {
        printf("Error: failed to load room '%s' : %s\n", room_path, ex.what());
        return 1;
    }

    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // Select GL version + let the backend select a GLSL version
    const char* glsl_version = "#version 130";
    // GL 3.0 + generally GLSL 130
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+OpenGL3 example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale; // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Main loop
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int64_t turn_number = 1;
    std::string wo_text = kBlankWorkOrderSkeleton;  // GM WorkOrder panel's editable submit buffer
    std::optional<std::string>
        parse_error;  // set on a bad Submit, shown inline, cleared on next good Submit
    std::vector<nlohmann::json>
        log_entries;          // one gm::log::LogEntry per process_turn call, see 4.5
    int selected_entry = -1;  // index into log_entries shown in the Log panel's detail pane

    std::vector<std::string>
        saved_workorder_files;       // filenames under workorders/, scanned once at startup
    int selected_session_turn = -1;  // index into log_entries currently previewed (WorkOrder panel)
    int selected_saved_file = -1;    // index into saved_workorder_files currently previewed
    std::string wo_preview_text;     // whichever of the above is selected, mutually exclusive

    bool session_over = false;

    char wo_text_buf[kWorkOrderBufSize];
    snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());

    try {
        if (fs::exists("workorders") && fs::is_directory("workorders")) {
            for (const auto& dir_entry : fs::directory_iterator("workorders")) {
                if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                    saved_workorder_files.push_back(dir_entry.path().filename().string());
                }
            }
        }
    } catch (const std::exception&) {
        // workorders/ is an optional, curated folder -- any scan failure just leaves
        // the Saved group empty rather than aborting startup.
    }

    bool done = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags host_window_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace Host", nullptr, host_window_flags);

        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        if (node == nullptr || !node->IsSplitNode()) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_id_left, dock_id_right;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.5f, &dock_id_left,
                                        &dock_id_right);

            ImGuiID dock_id_right_top, dock_id_right_bottom;
            ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.5f, &dock_id_right_top,
                                        &dock_id_right_bottom);

            ImGui::DockBuilderDockWindow(kRoomDescriptionTitle, dock_id_left);
            ImGui::DockBuilderDockWindow(kWorkOrderTitle, dock_id_right_top);
            ImGui::DockBuilderDockWindow(kLogTitle, dock_id_right_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();

        // --- Room Description panel -----------------------------------------------
        ImGui::Begin(kRoomDescriptionTitle);
        if (session_over) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "SESSION OVER: player is dead");
            ImGui::Separator();
        }
        ImGui::BeginChild("##RoomDescriptionScroll");
        ImGui::TextUnformatted(dump(world).c_str());
        ImGui::EndChild();
        ImGui::End();

        // --- GM WorkOrder panel ------------------------------------------------------
        ImGui::Begin(kWorkOrderTitle);

        ImGui::BeginChild("##WorkOrderList", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders);
        ImGui::SeparatorText("This Session");
        for (int i = static_cast<int>(log_entries.size()) - 1; i >= 0; --i) {
            bool is_selected = (selected_session_turn == i);
            if (ImGui::Selectable(entry_label(log_entries[static_cast<size_t>(i)]).c_str(),
                                  is_selected)) {
                selected_session_turn = i;
                selected_saved_file = -1;
                wo_preview_text = log_entries[static_cast<size_t>(i)].at("work_order").dump(2);
            }
        }
        ImGui::SeparatorText("Saved");
        for (int i = 0; i < static_cast<int>(saved_workorder_files.size()); ++i) {
            bool is_selected = (selected_saved_file == i);
            if (ImGui::Selectable(saved_workorder_files[static_cast<size_t>(i)].c_str(),
                                  is_selected)) {
                selected_saved_file = i;
                selected_session_turn = -1;
                wo_preview_text =
                    slurp(("workorders/" + saved_workorder_files[static_cast<size_t>(i)]).c_str());
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##WorkOrderMain");

        bool has_preview = (selected_session_turn >= 0) || (selected_saved_file >= 0);
        ImGui::BeginDisabled(!has_preview);
        if (ImGui::Button("Load into editor")) {
            wo_text = wo_preview_text;
            snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());
            parse_error.reset();
        }
        ImGui::EndDisabled();

        ImGui::BeginChild("##WorkOrderPreview", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(wo_preview_text.c_str());
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::InputTextMultiline("##WorkOrderEditor", wo_text_buf, sizeof(wo_text_buf),
                                  ImVec2(-1.0f, -60.0f));
        wo_text = wo_text_buf;

        ImGui::BeginDisabled(session_over);
        if (ImGui::Button("Submit Turn")) {
            try {
                WorkOrder wo = parse_work_order(wo_text);
                auto [world_after, outcome] =
                    turn::process_turn(world, turn_number, wo, kGuiLogPath);

                // Same LogEntry process_turn already built internally to append to
                // kGuiLogPath -- reconstructed here from values already in scope, then
                // serialized via gm_log.hpp's existing to_json, so the GUI's copy and
                // the on-disk line are guaranteed structurally identical.
                log_entries.push_back(nlohmann::json(log::LogEntry{turn_number, wo, outcome}));
                selected_entry = static_cast<int>(log_entries.size()) - 1;

                world = world_after;
                ++turn_number;  // advances on every process_turn call, Applied or rejected
                parse_error.reset();
                wo_text = kBlankWorkOrderSkeleton;
                snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());

                if (std::holds_alternative<log::Applied>(outcome) && rules::session_over(world)) {
                    session_over = true;
                }
            } catch (const std::exception& ex) {
                parse_error = ex.what();  // turn_number/World untouched, wo_text left as-is
            }
        }
        ImGui::EndDisabled();

        if (parse_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", parse_error->c_str());
        }

        ImGui::EndChild();

        ImGui::End();

        // --- Log panel -----------------------------------------------------------
        ImGui::Begin(kLogTitle);

        ImGui::BeginChild("##LogEntryList", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);
        for (int i = 0; i < static_cast<int>(log_entries.size()); ++i) {
            bool is_selected = (selected_entry == i);
            if (ImGui::Selectable(entry_label(log_entries[static_cast<size_t>(i)]).c_str(),
                                  is_selected)) {
                selected_entry = i;
            }
        }
        ImGui::EndChild();

        ImGui::BeginChild("##LogEntryDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        if (selected_entry >= 0 && selected_entry < static_cast<int>(log_entries.size())) {
            ImGui::TextUnformatted(
                log_entries[static_cast<size_t>(selected_entry)].dump(2).c_str());
        }
        ImGui::EndChild();

        ImGui::End();

        // Rendering
        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
