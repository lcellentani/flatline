---
paths:
  - "**/*.cpp"
  - "**/*.hpp"
  - "**/*.cc"
  - "**/*.hh"
  - "**/*.cxx"
  - "**/*.h"
  - "**/CMakeLists.txt"
---

# Modern C++ (C++17/20/23)

- Prefer **modern C++ features** over C-style constructs
- Use `auto` when the type is obvious from context
- Use `constexpr` for compile-time constants
- Use structured bindings: `auto [key, value] = map_entry;`

# Coding Style

Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) as its baseline, with the adjustments noted below. When in doubt, prefer consistency with the existing codebase over strict adherence to the guide.

## Adjustments to Google style

| Rule | Google default | This project |
|---|---|---|
| Indentation | 2 spaces | 4 spaces |
| Pointer alignment | either side | type side (`void* p`) |

Everything else follows Google style as written.

## Naming Conventions

### Types
Classes, structs, enums, and type aliases use `PascalCase`.

```cpp
class Renderer { ... };
struct InputState { ... };
enum class Tile : uint8_t { ... };
using TileMap = std::array<std::array<Tile, MAP_COLS>, MAP_ROWS>;
```

### Functions and methods
All functions and methods use `snake_case`.

```cpp
void clear(Color c);
void draw_rect(Rect r, Color c);
InputState poll_input();
```

### Variables
Local variables and function parameters use `snake_case`.

```cpp
int tile_size = 32;
float delta_time = 0.016f;
```

### Member variables
Member variables use `snake_case` with a trailing underscore.

```cpp
class Renderer {
private:
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};
```

### Constants and enumerators
Compile-time constants use `UPPER_SNAKE_CASE`. Enum values use `PascalCase`.

```cpp
export constexpr int MAP_COLS  = 21;
export constexpr int MAP_ROWS  = 21;
export constexpr int TILE_SIZE = 32;

enum class Tile : uint8_t {
    Empty  = 0,
    Wall   = 1,
    Pellet = 2,
};
```

### Modules
Module names use `lower.dot.separated` matching the project namespace hierarchy.

```cpp
export module engine.renderer;
export module engine.input;
export module engine.game;
```

### Files
| File type | Convention | Example |
|---|---|---|
| Module interface unit | `snake_case.ixx` | `renderer.ixx` |
| Module implementation unit | `snake_case.cpp` | `renderer.cpp` |
| Regular header (if needed) | `snake_case.h` | `types.h` |

# Formatting

- Use **clang-format** — no style debates
- Run `clang-format -i <file>` before committing

## Indentation
4 spaces. No tabs.

## Line length
100 characters maximum. Google specifies 80 — 100 is used here to accommodate module-qualified type names without forced wrapping.

## Braces
Opening brace on the same line as the statement that opens the block. No exceptions.

```cpp
// correct
void Renderer::clear(Color c) {
    if (!renderer_) {
        return;
    }
}

// wrong
void Renderer::clear(Color c)
{
    if (!renderer_)
    {
        return;
    }
}
```

## Spaces
Space after `if`, `for`, `while`, `switch`. No space between function name and `(`.

```cpp
// correct
if (is_valid()) {
    for (int i = 0; i < count; ++i) {
        do_something(i);
    }
}

// wrong
if(is_valid()){
    for(int i=0;i<count;i++){
        do_something(i);
    }
}
```

## Pointer and reference alignment
`*` and `&` bind to the type, not the variable name.

```cpp
// correct
SDL_Renderer* renderer_;
const InputState& input;
void draw_rect(Rect* r);

// wrong
SDL_Renderer *renderer_;
const InputState &input;
void draw_rect(Rect *r);
```

## Blank lines
One blank line between method definitions. Two blank lines between top-level declarations in a file. No blank line immediately after an opening brace or before a closing brace.

```cpp
void Renderer::clear(Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer_);
}

void Renderer::present() {
    SDL_RenderPresent(renderer_);
}
```
## C++20 specifics

### Module structure
Every module file follows this layout, in this order:

```cpp
module;                          // 1. global module fragment opens here
#include <external_headers.h>    // 2. all external includes go here (SDL2, STL)

export module engine.x;          // 3. module declaration

import engine.y;                  // 4. imports of other project modules only
import engine.z;

// 5. exported declarations
export class Foo { ... };
export void bar();

// 6. non-exported implementation (or in a separate .cpp)
```

### What goes where
- Standard library headers → global module fragment via `#include`
- Third-party C headers (SDL2) → global module fragment via `#include`
- Project modules → `import engine.x` after the module declaration
- Nothing gets `import <...>` (angle-bracket imports are disabled in this project)

### Concepts
Concepts are named with `PascalCase` and defined in the module that owns the constraint.

```cpp
export template<typename T>
concept Drawable = requires(T t, Renderer& r) {
    { t.draw(r) } -> std::same_as<void>;
};
```

### Coroutines
Coroutine return types are named with a `Task` or `Coroutine` suffix. `co_await`, `co_yield`, and `co_return` are always on their own line.

```cpp
Task ghost_behaviour(Ghost& ghost) {
    while (true) {
        co_await scatter_phase(ghost, SCATTER_DURATION);
        co_await chase_phase(ghost, CHASE_DURATION);
    }
}
```

## Class layout

Sections appear in this order, each preceded by its access specifier:

```cpp
export class Game {
public:
    // 1. constructors and destructor
    Game();
    ~Game() = default;

    // 2. deleted copy/move (if non-copyable)
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    // 3. public methods
    void update(const InputState& input);
    void render(Renderer& renderer);

    // 4. [[nodiscard]] queries last among public methods
    [[nodiscard]] bool is_running() const { return running_; }

private:
    // 5. private methods
    void render_map(Renderer& renderer);
    void render_player(Renderer& renderer);

    // 6. member variables — one per line, aligned trailing underscores
    TileMap  map_;
    Player   player_;
    bool     running_ = true;
};
```

## Comments

### When to comment
Comment *why*, not *what*. The code says what it does. Comments explain decisions, constraints, and non-obvious behaviour.

```cpp
// correct — explains a non-obvious decision
// Naive collision: check destination tile before moving.
// Good enough for Phase 1; replace with swept AABB in Phase 3.
if (tile_at(player_.x + dx, player_.y) != Tile::Wall)
    player_.x += dx;

// wrong — restates what the code already says
// check if tile is not a wall
if (tile_at(player_.x + dx, player_.y) != Tile::Wall)
    player_.x += dx;
```

### Format
Single-line comments use `//` with one space after. No `/* */` block comments except for file headers.

### Phase markers
Use `// TODO(phaseN):` to flag work deferred to a later phase.

```cpp
// TODO(phase3): replace with coroutine-based state machine
ghost.update_placeholder(player_);
```

# Reference

See skill: `cpp-coding-standards` for comprehensive C++ coding standards and guidelines.
