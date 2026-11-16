/*
 * NESTS — native PS5 shell proof of concept.
 * SPDX-License-Identifier: MIT
 *
 * This source uses SDL2 public APIs for video and controller events.
 * The records below are fictional UI fixtures; no download is performed.
 */

#include <SDL.h>

#include <array>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr int screen_width = 1280;
constexpr int screen_height = 720;
constexpr const char *storage_directory = "/download0/nests";
constexpr const char *favorites_path = "/download0/nests/favorites.txt";
constexpr const char *temporary_path = "/download0/nests/favorites.tmp";

struct Game
{
    const char *id;
    const char *title;
    const char *system;
    const char *genre;
    const char *year;
};

constexpr std::array<Game, 6> games{{
    {"orbit-runner-nes", "ORBIT RUNNER", "NES", "PLATFORM", "2025"},
    {"pixel-harbor-gb", "PIXEL HARBOR", "GAME BOY", "ADVENTURE", "2024"},
    {"neon-circuit-md", "NEON CIRCUIT", "MEGA DRIVE", "ACTION", "2025"},
    {"moonlight-quest-snes", "MOONLIGHT QUEST", "SNES", "RPG", "2023"},
    {"tiny-tactics-gba", "TINY TACTICS", "GBA", "STRATEGY", "2024"},
    {"blue-horizon-sms", "BLUE HORIZON", "MASTER SYSTEM", "ACTION", "2025"},
}};

struct Glyph
{
    char character;
    std::array<std::uint8_t, 7> rows;
};

constexpr std::array<Glyph, 41> glyphs{{
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 14}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {31, 4, 4, 4, 4, 4, 31}}, {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'-', {0, 0, 0, 31, 0, 0, 0}}, {':', {0, 4, 4, 0, 4, 4, 0}},
    {'.', {0, 0, 0, 0, 0, 4, 4}}, {'*', {0, 21, 14, 31, 14, 21, 0}},
}};

std::array<bool, games.size()> favorites{};
std::size_t selected_index = 0;
bool favorites_only = false;
bool running = true;
SDL_GameController *controller = nullptr;

bool valid_id(const char *value) noexcept
{
    if (value == nullptr || *value == '\0')
        return false;
    std::size_t length = 0;
    for (const unsigned char *cursor = reinterpret_cast<const unsigned char *>(value);
         *cursor != '\0'; ++cursor)
    {
        if (!(std::islower(*cursor) || std::isdigit(*cursor) || *cursor == '-'))
            return false;
        if (++length > 80)
            return false;
    }
    return length >= 2;
}

int game_index(const char *id) noexcept
{
    if (!valid_id(id))
        return -1;
    for (std::size_t index = 0; index < games.size(); ++index)
    {
        if (std::strcmp(games[index].id, id) == 0)
            return static_cast<int>(index);
    }
    return -1;
}

void load_favorites() noexcept
{
    favorites.fill(false);
    std::FILE *file = std::fopen(favorites_path, "rb");
    if (file == nullptr)
        return;

    char line[96]{};
    while (std::fgets(line, sizeof(line), file) != nullptr)
    {
        const std::size_t length = std::strcspn(line, "\r\n");
        line[length] = '\0';
        const int index = game_index(line);
        if (index >= 0)
            favorites[static_cast<std::size_t>(index)] = true;
    }
    std::fclose(file);
}

bool save_favorites() noexcept
{
    if (::mkdir(storage_directory, 0700) != 0 && errno != EEXIST)
        return false;

    std::FILE *file = std::fopen(temporary_path, "wb");
    if (file == nullptr)
        return false;

    bool ok = true;
    for (std::size_t index = 0; index < games.size(); ++index)
    {
        if (favorites[index] &&
            (std::fputs(games[index].id, file) == EOF || std::fputc('\n', file) == EOF))
            ok = false;
    }
    if (std::fflush(file) != 0)
        ok = false;
    if (ok && ::fsync(::fileno(file)) != 0)
        ok = false;
    if (std::fclose(file) != 0)
        ok = false;
    if (!ok)
    {
        (void)std::remove(temporary_path);
        return false;
    }
    if (::rename(temporary_path, favorites_path) != 0)
    {
        (void)std::remove(temporary_path);
        return false;
    }
    const int directory_fd = ::open(storage_directory, O_RDONLY);
    if (directory_fd >= 0)
    {
        (void)::fsync(directory_fd);
        (void)::close(directory_fd);
    }
    return true;
}

const Glyph &find_glyph(char value) noexcept
{
    for (const auto &glyph : glyphs)
        if (glyph.character == value)
            return glyph;
    return glyphs[0];
}

void set_color(SDL_Renderer *renderer, std::uint8_t r, std::uint8_t g,
               std::uint8_t b, std::uint8_t a = 255) noexcept
{
    (void)SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void fill_rect(SDL_Renderer *renderer, int x, int y, int width, int height,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    SDL_Rect rectangle{x, y, width, height};
    set_color(renderer, r, g, b);
    (void)SDL_RenderFillRect(renderer, &rectangle);
}

void outline_rect(SDL_Renderer *renderer, int x, int y, int width, int height,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    SDL_Rect rectangle{x, y, width, height};
    set_color(renderer, r, g, b);
    (void)SDL_RenderDrawRect(renderer, &rectangle);
}

void draw_text(SDL_Renderer *renderer, int x, int y, std::string_view text,
               int scale, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    set_color(renderer, r, g, b);
    int cursor_x = x;
    for (const char raw : text)
    {
        const char value = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const auto &glyph = find_glyph(value);
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if ((glyph.rows[static_cast<std::size_t>(row)] & (1U << (4 - column))) != 0)
                {
                    SDL_Rect pixel{cursor_x + column * scale, y + row * scale, scale, scale};
                    (void)SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        cursor_x += 6 * scale;
    }
}

bool visible(std::size_t index) noexcept
{
    return !favorites_only || favorites[index];
}

void normalize_selection() noexcept
{
    if (selected_index >= games.size())
        selected_index = 0;
    if (visible(selected_index))
        return;
    for (std::size_t index = 0; index < games.size(); ++index)
    {
        if (visible(index))
        {
            selected_index = index;
            return;
        }
    }
    selected_index = 0;
}

void move_selection(int dx, int dy) noexcept
{
    normalize_selection();
    int candidate = static_cast<int>(selected_index);
    for (std::size_t attempt = 0; attempt < games.size(); ++attempt)
    {
        candidate += dx + dy * 3;
        if (candidate < 0)
            candidate = static_cast<int>(games.size()) - 1;
        if (candidate >= static_cast<int>(games.size()))
            candidate = 0;
        if (visible(static_cast<std::size_t>(candidate)))
        {
            selected_index = static_cast<std::size_t>(candidate);
            return;
        }
    }
}

void toggle_favorite() noexcept
{
    favorites[selected_index] = !favorites[selected_index];
    if (!save_favorites())
        SDL_Log("NESTS: failed to persist favorites in /download0/nests/");
    normalize_selection();
}

void draw_card(SDL_Renderer *renderer, std::size_t index, int x, int y) noexcept
{
    const Game &game = games[index];
    const bool selected = index == selected_index;
    const bool favorite = favorites[index];
    fill_rect(renderer, x, y, 300, 174,
              selected ? 24 : 16, selected ? 42 : 25, selected ? 66 : 38);
    outline_rect(renderer, x, y, 300, 174,
                 selected ? 65 : 42, selected ? 159 : 58, selected ? 232 : 82);
    fill_rect(renderer, x + 15, y + 15, 270, 78,
              static_cast<std::uint8_t>(32 + index * 3),
              static_cast<std::uint8_t>(57 + index * 4),
              static_cast<std::uint8_t>(82 + index * 5));
    draw_text(renderer, x + 27, y + 39, game.system, 2, 192, 222, 245);
    draw_text(renderer, x + 17, y + 108, game.title, 2, 245, 248, 252);
    draw_text(renderer, x + 17, y + 139, game.genre, 1, 143, 164, 187);
    if (favorite)
        draw_text(renderer, x + 260, y + 106, "*", 3, 91, 206, 246);
    draw_text(renderer, x + 245, y + 139, "DEMO", 1, 233, 190, 111);
}

void draw_scene(SDL_Renderer *renderer) noexcept
{
    set_color(renderer, 8, 11, 18);
    (void)SDL_RenderClear(renderer);
    fill_rect(renderer, 0, 0, 210, screen_height, 11, 16, 25);
    fill_rect(renderer, 25, 28, 44, 44, 18, 82, 145);
    draw_text(renderer, 34, 42, "N", 3, 245, 249, 255);
    draw_text(renderer, 82, 34, "NESTS", 2, 245, 249, 255);
    draw_text(renderer, 82, 53, "RETRO LIBRARY", 1, 120, 146, 172);
    draw_text(renderer, 28, 126, "STORE", 1, 89, 213, 236);
    draw_text(renderer, 28, 156, "HOME", 2, 237, 244, 250);
    draw_text(renderer, 28, 205, "LIBRARY", 2, 148, 165, 186);
    draw_text(renderer, 28, 254, "FAVORITES", 1, 148, 165, 186);
    fill_rect(renderer, 232, 26, 1018, 118, 16, 32, 51);
    outline_rect(renderer, 232, 26, 1018, 118, 42, 70, 101);
    draw_text(renderer, 260, 48, "RETRO GAME LIBRARY", 2, 116, 197, 248);
    draw_text(renderer, 260, 78, "DISCOVER HOMEBREW AND INDEPENDENT GAMES", 1, 197, 213, 231);
    draw_text(renderer, 260, 106, "DEMO CATALOG - DOWNLOADS DISABLED", 1, 233, 190, 111);
    draw_text(renderer, 235, 169, favorites_only ? "YOUR FAVORITES" : "EXPLORE THE CATALOG", 2, 239, 245, 251);
    draw_text(renderer, 235, 194, "D-PAD MOVE  CROSS FAVORITE  OPTIONS FILTER", 1, 132, 151, 175);

    std::size_t shown = 0;
    for (std::size_t index = 0; index < games.size(); ++index)
    {
        if (!visible(index))
            continue;
        const int column = static_cast<int>(shown % 3);
        const int row = static_cast<int>(shown / 3);
        draw_card(renderer, index, 235 + column * 320, 225 + row * 190);
        ++shown;
    }
    if (shown == 0)
    {
        draw_text(renderer, 260, 280, "NO FAVORITES YET", 2, 177, 194, 214);
        draw_text(renderer, 260, 315, "SELECT A GAME AND PRESS CROSS TO SAVE IT", 1, 126, 148, 173);
    }
    fill_rect(renderer, 235, 638, 950, 1, 42, 54, 72);
    draw_text(renderer, 235, 655, "NESTS 0.6 - LOCAL FAVORITES", 1, 105, 126, 151);
    draw_text(renderer, 1050, 655, "13.60: NOT TESTED", 1, 233, 190, 111);
    (void)SDL_RenderPresent(renderer);
}

void try_open_controller() noexcept
{
    if (controller != nullptr)
        return;
    for (int index = 0; index < SDL_NumJoysticks(); ++index)
    {
        if (SDL_IsGameController(index) == SDL_TRUE)
        {
            controller = SDL_GameControllerOpen(index);
            if (controller != nullptr)
                break;
        }
    }
}

void handle_key(SDL_Keycode key) noexcept
{
    switch (key)
    {
    case SDLK_LEFT: move_selection(-1, 0); break;
    case SDLK_RIGHT: move_selection(1, 0); break;
    case SDLK_UP: move_selection(0, -1); break;
    case SDLK_DOWN: move_selection(0, 1); break;
    case SDLK_RETURN:
    case SDLK_SPACE: toggle_favorite(); break;
    case SDLK_TAB:
    case SDLK_f: favorites_only = !favorites_only; normalize_selection(); break;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE: favorites_only = false; normalize_selection(); break;
    default: break;
    }
}
} // namespace

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        SDL_Log("NESTS: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    (void)SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_Window *window = SDL_CreateWindow("NESTS - Retro Game Library",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          screen_width, screen_height, SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        SDL_Log("NESTS: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr)
    {
        SDL_Log("NESTS: SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    load_favorites();
    try_open_controller();
    while (running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
            {
                if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL) != 0)
                    running = false;
                else
                    handle_key(event.key.keysym.sym);
            }
            else if (event.type == SDL_CONTROLLERDEVICEADDED)
                try_open_controller();
            else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
            {
                if (controller != nullptr &&
                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) ==
                        event.cdevice.which)
                {
                    SDL_GameControllerClose(controller);
                    controller = nullptr;
                    try_open_controller();
                }
            }
            else if (event.type == SDL_CONTROLLERBUTTONDOWN)
            {
                switch (event.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: move_selection(-1, 0); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: move_selection(1, 0); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: move_selection(0, -1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move_selection(0, 1); break;
                case SDL_CONTROLLER_BUTTON_A: toggle_favorite(); break;
                case SDL_CONTROLLER_BUTTON_B: favorites_only = false; normalize_selection(); break;
                case SDL_CONTROLLER_BUTTON_START: favorites_only = !favorites_only; normalize_selection(); break;
                default: break;
                }
            }
        }
        draw_scene(renderer);
        SDL_Delay(8);
    }

    (void)save_favorites();
    if (controller != nullptr)
        SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
