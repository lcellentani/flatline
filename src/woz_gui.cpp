#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>
#include <stdio.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#include "gm_effects.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_rules.hpp"
#include "gm_turn.hpp"

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

static std::string generate_session_log_path() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm local_tm{};
    localtime_s(&local_tm, &seconds);

    char path[64];
    snprintf(path, sizeof(path), "gui_session_log-%04d%02d%02d-%02d%02d%02d-%03lld.jsonl", local_tm.tm_year + 1900,
             local_tm.tm_mon + 1, local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
             static_cast<long long>(millis));
    return path;
}

// InputTextMultiline needs a raw, fixed-capacity buffer (imgui_stdlib.h's std::string
// convenience wrapper isn't vendored -- see Task 3.1's file list); wo_text stays the
// std::string source of truth for everything outside the widget itself, resynced with
// this buffer on every programmatic change (Submit reset, Load into editor) and read
// back from it once per frame right after the widget renders.
constexpr size_t kWorkOrderBufSize = 16384;

constexpr size_t kSaveFileNameBufSize = 256;

// --- Utilities ------------------------------------------------------------------------------------------------------

namespace fs = std::filesystem;

static std::string slurp(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Scans workorders/ for *.json filenames -- shared by the startup scan and the
// WorkOrder panel's Saved-group Refresh button, so there's one scan implementation,
// not two independently-maintained copies.
static std::vector<std::string> scan_workorders_dir() {
    std::vector<std::string> files;
    try {
        if (fs::exists("workorders") && fs::is_directory("workorders")) {
            for (const auto& dir_entry : fs::directory_iterator("workorders")) {
                if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                    files.push_back(dir_entry.path().filename().string());
                }
            }
        }
    } catch (const std::exception&) {
        // workorders/ is an optional, curated folder -- any scan failure just leaves
        // the Saved group empty rather than aborting the scan.
    }
    return files;
}

// Shared by the Log panel's entry list and the WorkOrder panel's This Session group --
// both render "Turn N -- <status> -- <teaser>" off the same stored LogEntry JSON.
static std::string entry_label(const nlohmann::json& entry) {
    const std::string status = entry.value("status", std::string{});
    const std::string teaser =
        status == "applied" ? entry.value("narration", std::string{}) : entry.value("reason", std::string{});
    return "Turn " + std::to_string(entry.value("turn_number", int64_t{0})) + " — " + status + " — " + teaser;
}

constexpr bool is_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

constexpr std::string_view trim(std::string_view str) noexcept {
    const auto start = std::find_if_not(str.begin(), str.end(), is_whitespace);
    if (start == str.end()) {
        return {};
    }
    const auto end = std::find_if_not(str.rbegin(), str.rend(), is_whitespace).base();
    const auto offset = std::distance(str.begin(), start);
    const auto count = std::distance(start, end);
    return {str.data() + offset, static_cast<std::size_t>(count)};
}

constexpr std::string_view strip_json_suffix(std::string_view filename) noexcept {
    constexpr std::string_view kExt = ".json";
    if (filename.size() > kExt.size() && filename.ends_with(kExt)) {
        return filename.substr(0, filename.size() - kExt.size());
    }
    return filename;
}

constexpr bool is_valid_filename(std::string_view filename) noexcept {
    if (filename.empty()) {
        return false;
    }
    return std::all_of(filename.begin(), filename.end(), [](char ch) {
        const auto uc = static_cast<unsigned char>(ch);
        return std::isalnum(uc) || ch == '_' || ch == '-';
    });
}

// --- Splitters ------------------------------------------------------------------------------------------------------

constexpr float kMinSplitPx = 120.0f;
constexpr float kSplitterThickness = 4.0f;
constexpr const char* kLayoutPath = "gui_layout.json";

// Back-computed from today's hardcoded pixel baselines (220/120/150) against a
// rounded 640x400 reference -- half the SDL window's default 1280x800
// (woz_gui.cpp:138-139), matching the WorkOrder/Log panels' actual 0.5/0.5
// DockBuilder split (woz_gui.cpp:266,269). Reused as-is on first launch (no
// gui_layout.json yet) and as the fallback whenever the file is missing,
// unreadable, or fails to parse -- one set of defaults, not two.
struct LayoutRatios {
    float sidebar = 220.0f / 640.0f;
    float preview = 120.0f / 400.0f;
    float log_list = 150.0f / 400.0f;
};

static LayoutRatios load_layout_or_default() {
    try {
        const std::string raw = slurp(kLayoutPath);
        if (raw.empty()) {
            return LayoutRatios{};
        }
        const nlohmann::json j = nlohmann::json::parse(raw);
        LayoutRatios layout;
        layout.sidebar = j.at("sidebar").get<float>();
        layout.preview = j.at("preview").get<float>();
        layout.log_list = j.at("log_list").get<float>();
        return layout;
    } catch (const std::exception&) {
        return LayoutRatios{};
    }
}
static void save_layout(const LayoutRatios& layout) {
    nlohmann::json j;
    j["sidebar"] = layout.sidebar;
    j["preview"] = layout.preview;
    j["log_list"] = layout.log_list;
    std::ofstream(kLayoutPath) << j.dump(2);
}

// Floor-clamps `ratio` against `total_extent` for *this frame's* rendering
// only -- never mutates the caller's stored ratio. Recomputed fresh every
// frame so a ratio squeezed by a small window immediately reflects its true
// value again once the window grows back (spec §4: "don't clamp the stored
// fraction").
static float clamp_ratio(float ratio, float total_extent) {
    const float min_ratio = total_extent > 2.0f * kMinSplitPx ? kMinSplitPx / total_extent : 0.5f;
    return std::clamp(ratio, min_ratio, 1.0f - min_ratio);
}

// Renders a thin drag bar between two regions. While the user is actively
// dragging, updates `ratio` in place (via clamp_ratio, so it can never
// represent less than the floor at this frame's total_extent) -- this is what
// avoids a drag-reversal dead zone: the persisted value tracks the clamped
// value live during interaction, so reversing direction after an overshoot
// moves it immediately instead of needing to re-travel the overshoot first.
// Idle frames (no active drag) never touch `ratio`, only the caller's own
// clamp_ratio() call (used to size the region drawn just before this widget).
// Returns true on exactly the frame the drag ends.
static bool splitter(const char* id, float& ratio, float total_extent, bool horizontal) {
    ImVec2 size = horizontal ? ImVec2(kSplitterThickness, -1.0f) : ImVec2(-1.0f, kSplitterThickness);
    ImGui::InvisibleButton(id, size);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) {
        ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    }

    const ImGuiCol color =
        active ? ImGuiCol_SeparatorActive : (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                              ImGui::GetColorU32(color));

    if (active) {
        const float delta = horizontal ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
        if (delta != 0.0f) {
            ratio = clamp_ratio(ratio + delta / total_extent, total_extent);
            // InvisibleButton doesn't call this internally the way Slider/Drag
            // widgets do -- without it, IsItemDeactivatedAfterEdit() below never
            // fires (ImGui only treats an item as "edited" if something marks
            // it). imgui_internal.h is already vendored and included here for
            // the DockBuilder API (woz_gui.cpp:6); confirm MarkItemEdited's
            // signature against the pinned commit at implementation time.
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
    }

    return ImGui::IsItemDeactivatedAfterEdit();
}

// --- App ------------------------------------------------------------------------------------------------------------

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
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be
    // your SDL_AppInit() function]
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
    SDL_WindowFlags window_flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+OpenGL3 example", (int)(1280 * main_scale),
                                          (int)(800 * main_scale), window_flags);
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
    SDL_GL_SetSwapInterval(1);  // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style
                                      // scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;  // Set initial font scale. (in docking branch: using
                                      // io.ConfigDpiScaleFonts=true automatically overrides this
                                      // for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Main loop
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int64_t turn_number = 1;
    std::string wo_text = kBlankWorkOrderSkeleton;  // GM WorkOrder panel's editable submit buffer
    std::optional<std::string> parse_error;         // set on a bad Submit, shown inline, cleared on next good Submit

    // Deliberately separate from parse_error: this one is owned and rendered by the Room
    // Description panel, so sharing parse_error would either show a reload failure in the
    // wrong window or let the WorkOrder panel's Submit/Save handlers clobber it later in
    // the same frame. Only New Session ever reads or writes it.
    std::optional<std::string> room_reload_error;

    std::vector<nlohmann::json> log_entries;  // one gm::log::LogEntry per process_turn call, see 4.5
    int selected_entry = -1;                  // index into log_entries shown in the Log panel's detail pane

    std::vector<std::string> saved_workorder_files;  // filenames under workorders/, populated at startup + Refresh
    int selected_session_turn = -1;                  // index into log_entries currently previewed (WorkOrder panel)
    int selected_saved_file = -1;                    // index into saved_workorder_files currently previewed
    std::string wo_preview_text;                     // whichever of the above is selected, mutually exclusive

    std::string log_path = generate_session_log_path();

    LayoutRatios layout = load_layout_or_default();

    bool session_over = false;

    char wo_text_buf[kWorkOrderBufSize];
    snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());

    std::string save_filename;
    char save_filename_buf[kSaveFileNameBufSize];
    snprintf(save_filename_buf, sizeof(save_filename_buf), "%s", save_filename.c_str());

    saved_workorder_files = scan_workorders_dir();

    bool done = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
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

        ImGuiWindowFlags host_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                             ImGuiWindowFlags_NoDocking;

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
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.5f, &dock_id_left, &dock_id_right);

            ImGuiID dock_id_right_top, dock_id_right_bottom;
            ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.5f, &dock_id_right_top, &dock_id_right_bottom);

            ImGui::DockBuilderDockWindow(kRoomDescriptionTitle, dock_id_left);
            ImGui::DockBuilderDockWindow(kWorkOrderTitle, dock_id_right_top);
            ImGui::DockBuilderDockWindow(kLogTitle, dock_id_right_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();

        // --- Room Description panel -----------------------------------------------
        ImGui::Begin(kRoomDescriptionTitle);

        // Always enabled, unlike Submit -- New Session must work mid-session too, and it
        // renders above the banner/error so a growing message can never push it offscreen.
        if (ImGui::Button("New Session")) {
            try {
                // Reload into a local first: the reset below is one atomic commit, so a
                // bad room file on disk leaves the live session completely untouched --
                // same "parse before mutate" shape as Submit's and Save's handlers.
                World reloaded = parse_world(slurp(room_path));

                world = reloaded;
                turn_number = 1;
                session_over = false;

                log_entries.clear();
                selected_entry = -1;

                // wo_preview_text is shared by both selections; it's stale only if the
                // This-Session one was live. A Saved-entry preview still reflects an
                // on-disk file New Session never touches, so it survives untouched --
                // as does selected_saved_file, which no line here assigns.
                const bool had_session_selection = selected_session_turn >= 0;
                selected_session_turn = -1;
                if (had_session_selection) {
                    wo_preview_text.clear();
                }

                wo_text = kBlankWorkOrderSkeleton;
                snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());

                save_filename.clear();
                save_filename_buf[0] = '\0';

                parse_error.reset();

                log_path = generate_session_log_path();
                room_reload_error.reset();
            } catch (const std::exception& ex) {
                room_reload_error = ex.what();  // world/turn_number/log_entries/log_path untouched
            }
        }

        if (session_over) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "SESSION OVER: player is dead");
            ImGui::Separator();
        }

        if (room_reload_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", room_reload_error->c_str());
        }

        ImGui::BeginChild("##RoomDescriptionScroll");
        ImGui::TextUnformatted(dump(world).c_str());
        ImGui::EndChild();
        ImGui::End();

        // --- GM WorkOrder panel ------------------------------------------------------
        ImGui::Begin(kWorkOrderTitle);

        const float wo_panel_width = ImGui::GetContentRegionAvail().x;
        const float sidebar_width = clamp_ratio(layout.sidebar, wo_panel_width) * wo_panel_width;

        ImGui::BeginChild("##WorkOrderList", ImVec2(sidebar_width, 0.0f), ImGuiChildFlags_Borders);
        ImGui::SeparatorText("This Session");
        for (int i = static_cast<int>(log_entries.size()) - 1; i >= 0; --i) {
            bool is_selected = (selected_session_turn == i);
            if (ImGui::Selectable(entry_label(log_entries[static_cast<size_t>(i)]).c_str(), is_selected)) {
                selected_session_turn = i;
                selected_saved_file = -1;
                wo_preview_text = log_entries[static_cast<size_t>(i)].at("work_order").dump(2);
            }
        }
        ImGui::SeparatorText("Saved");
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            // Only a Saved-group selection is in play here -- selected_session_turn's
            // preview is untouched by a workorders/ rescan, so leave it alone entirely.
            const bool had_saved_selection =
                selected_saved_file >= 0 && selected_saved_file < static_cast<int>(saved_workorder_files.size());
            const std::string selected_filename =
                had_saved_selection ? saved_workorder_files[static_cast<size_t>(selected_saved_file)] : std::string{};

            saved_workorder_files = scan_workorders_dir();

            if (had_saved_selection) {
                selected_saved_file = -1;
                for (int i = 0; i < static_cast<int>(saved_workorder_files.size()); ++i) {
                    if (saved_workorder_files[static_cast<size_t>(i)] == selected_filename) {
                        selected_saved_file = i;
                        break;
                    }
                }
                if (selected_saved_file < 0) {
                    wo_preview_text.clear();
                }
            }
        }
        for (int i = 0; i < static_cast<int>(saved_workorder_files.size()); ++i) {
            bool is_selected = (selected_saved_file == i);
            if (ImGui::Selectable(saved_workorder_files[static_cast<size_t>(i)].c_str(), is_selected)) {
                selected_saved_file = i;
                selected_session_turn = -1;
                wo_preview_text = slurp(("workorders/" + saved_workorder_files[static_cast<size_t>(i)]).c_str());
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (splitter("##SidebarSplit", layout.sidebar, wo_panel_width, /*horizontal=*/true)) {
            save_layout(layout);
        }

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

        const float main_content_height = ImGui::GetContentRegionAvail().y;
        const float preview_height = clamp_ratio(layout.preview, main_content_height) * main_content_height;

        ImGui::BeginChild("##WorkOrderPreview", ImVec2(0.0f, preview_height), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(wo_preview_text.c_str());
        ImGui::EndChild();

        if (splitter("##PreviewSplit", layout.preview, main_content_height, /*horizontal=*/false)) {
            save_layout(layout);
        }

        ImGui::Separator();

        const float footer_height = ImGui::GetFrameHeightWithSpacing() * 2.0f;
        ImGui::InputTextMultiline("##WorkOrderEditor", wo_text_buf, sizeof(wo_text_buf), ImVec2(-1.0f, -footer_height));
        wo_text = wo_text_buf;

        const bool is_blank = trim(wo_text) == trim(kBlankWorkOrderSkeleton);

        ImGui::BeginDisabled(session_over || is_blank);
        if (ImGui::Button("Submit Turn")) {
            try {
                WorkOrder wo = parse_work_order(wo_text);
                // TODO(phase2): pass the panel's captured player input here --
                // this front end is the one that actually collects it.
                auto [world_after, outcome] = turn::process_turn(world, turn_number, wo, log_path, "");

                // Same LogEntry process_turn already built internally to append to
                // log_path -- reconstructed here from values already in scope, then
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

        ImGui::SameLine();

        ImGui::BeginDisabled(is_blank);
        if (ImGui::Button("Reset")) {
            wo_text = kBlankWorkOrderSkeleton;
            snprintf(wo_text_buf, sizeof(wo_text_buf), "%s", wo_text.c_str());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::InputText("##filename", save_filename_buf, sizeof(save_filename_buf));
        save_filename = save_filename_buf;

        ImGui::SameLine();

        ImGui::BeginDisabled(save_filename.empty());
        if (ImGui::Button("Save")) {
            const std::string_view bare = strip_json_suffix(save_filename);
            if (!is_valid_filename(bare)) {
                parse_error = "Invalid filename '" + save_filename + "': only letters, digits, '_' and '-' allowed.";
            } else {
                std::string name(bare);
                name += ".json";
                const std::string path = "workorders/" + name;

                try {
                    WorkOrder wo = parse_work_order(wo_text);
                    if (fs::exists(path)) {
                        parse_error = "A saved WorkOrder named '" + name + "' already exists.";
                    } else {
                        std::ofstream(path) << wo_text;
                        saved_workorder_files.push_back(name);
                        parse_error.reset();
                        save_filename.clear();
                        save_filename_buf[0] = '\0';
                    }
                } catch (const std::exception& ex) {
                    parse_error = ex.what();
                }
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

        const float log_panel_height = ImGui::GetContentRegionAvail().y;
        const float log_list_height = clamp_ratio(layout.log_list, log_panel_height) * log_panel_height;

        ImGui::BeginChild("##LogEntryList", ImVec2(0.0f, log_list_height), ImGuiChildFlags_Borders);
        for (int i = 0; i < static_cast<int>(log_entries.size()); ++i) {
            bool is_selected = (selected_entry == i);
            if (ImGui::Selectable(entry_label(log_entries[static_cast<size_t>(i)]).c_str(), is_selected)) {
                selected_entry = i;
            }
        }
        ImGui::EndChild();

        if (splitter("##LogListSplit", layout.log_list, log_panel_height, /*horizontal=*/false)) {
            save_layout(layout);
        }

        ImGui::BeginChild("##LogEntryDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        if (selected_entry >= 0 && selected_entry < static_cast<int>(log_entries.size())) {
            ImGui::TextUnformatted(log_entries[static_cast<size_t>(selected_entry)].dump(2).c_str());
        }
        ImGui::EndChild();

        ImGui::End();

        // Rendering
        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                     clear_color.w);
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
