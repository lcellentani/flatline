# SDL3

Vendored full upstream source tree, pinned to tag `release-3.4.12`.

Source: https://github.com/libsdl-org/SDL.git (tag `release-3.4.12`, commit `f87239e71e42da91ca317a12eefb82cfbf3393eb`)

Vendored as a plain source drop (no `.git`, not a submodule), same as every other `third_party/` entry. Unlike `nlohmann/json.hpp`'s single-header vendor or Dear ImGui's selective file-list vendor, the **whole tree** is checked in unpruned: SDL3 is a full library with its own CMake build that conditionally compiles per-subsystem internals (video backends, audio, joystick/haptics, etc.), so hand-pruning it would be fragile and unsupported upstream. `add_subdirectory` in the top-level `CMakeLists.txt` only builds what our CMake options (`SDL_STATIC`/`SDL_SHARED`/`SDL_TEST_LIBRARY`) ask for.

Unmodified upstream — do not hand-edit anything under this directory. To update, re-clone a newer tag and replace this tree wholesale.
