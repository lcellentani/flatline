# Dear ImGui (docking branch)

Selective vendor, pinned to the `docking` branch at commit `6029ee3789a2b7898f6423ec0c88cc4e5425f5a9` (2026-07-10).

Source: https://github.com/ocornut/imgui.git (`docking` branch)

**Why `docking`, not a tagged `master` release:** the GM WorkOrder/Log/Room-Description panels use Dear ImGui's docking feature (rearrangeable/resizable panels) for better live-play ergonomics than fixed quadrants. Docking has not merged into tagged `master` releases — it's an actively maintained separate branch, kept in sync with `master` (confirmed alive as of the pin date above), so there's no version tag to pin to; a commit SHA is the closest equivalent.

Vendored files (core + the two backends this project uses; nothing else):

```
imgui.h / imgui.cpp
imgui_internal.h          (DockBuilder API — internal but unmodified upstream, needed for first-run dockspace bootstrap)
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
imstb_rectpack.h / imstb_textedit.h / imstb_truetype.h   (imgui.cpp's own stb deps)
backends/imgui_impl_sdl3.h / imgui_impl_sdl3.cpp
backends/imgui_impl_opengl3.h / imgui_impl_opengl3.cpp / imgui_impl_opengl3_loader.h
```

`imgui_demo.cpp` is deliberately **not** vendored (nothing calls `ImGui::ShowDemoWindow()`). No separate GL loader library — the OpenGL3 backend's bundled `imgui_impl_opengl3_loader.h` covers it.

Unmodified upstream — do not hand-edit anything under this directory. To update, re-fetch the current `docking`-branch commit and replace these files wholesale.
