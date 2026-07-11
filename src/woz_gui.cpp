// Placeholder for Task 3.2 (CMakeLists.txt/link wiring) verification only.
// Exercises one real SDL3 call and one real ImGui call so a successful link
// actually proves both libraries are wired correctly, not just that an empty
// main() compiles. The real windowed shell is Task 3.3 (not started).
#include <SDL3/SDL.h>
#include <imgui.h>

int main() {
    SDL_Init(0);
    ImGui::CreateContext();
    ImGui::DestroyContext();
    SDL_Quit();
    return 0;
}
