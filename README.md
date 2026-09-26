w# 🚀 Space Invaders — Build Guide (Raylib + C)

A complete, step-by-step tutorial to build a Space Invaders game from scratch using **raylib 6.0** and **C**.  
By the end of this guide you will have a fully playable game with a player ship, alien waves, bullets, collisions, scoring, lives, and game-over/win screens.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Prerequisites & Project Setup](#2-prerequisites--project-setup)
3. [How to Compile & Run](#3-how-to-compile--run)
4. [Game Design Document (What We're Building)](#4-game-design-document-what-were-building)
5. [Step 1 — Window, Game Loop & Background](#step-1--window-game-loop--background)
6. [Step 2 — The Player Ship](#step-2--the-player-ship)
7. [Step 3 — Player Shooting](#step-3--player-shooting)
8. [Step 4 — Grid Alien Formation (4x5)](#step-4--grid-alien-formation-4x5)
9. [Step 5 — Alien Movement](#step-5--alien-movement)
10. [Step 6 — Collision Detection (Bullets ↔ Aliens)](#step-6--collision-detection-bullets--aliens)
11. [Step 7 — Alien Shooting Back](#step-7--alien-shooting-back)
12. [Step 8 — Player Lives & Getting Hit](#step-8--player-lives--getting-hit)
13. [Step 9 — Scoring & HUD](#step-9--scoring--hud)
14. [Step 10 — Game Over & Win Screens](#step-10--game-over--win-screens)
15. [Step 11 — Polish (Sound, Textures, Particles)](#step-11--polish-sound-textures-particles)
16. [Full Source Code Skeleton](#full-source-code-skeleton)
17. [Key Raylib Functions Cheat-Sheet](#key-raylib-functions-cheat-sheet)
18. [Common Mistakes & Debugging Tips](#common-mistakes--debugging-tips)
19. [Next Steps & Challenges](#next-steps--challenges)

---

## 1. Project Overview

**Space Invaders** is one of the best first games to build because it teaches you:

| Concept | What You'll Learn |
|---|---|
| Game loop | `Update → Draw → Repeat` at 60 FPS |
| Player input | Reading keyboard state every frame |
| Entity management | Arrays/structs for bullets, aliens |
| Movement patterns | Horizontal sweep + drop-down for aliens |
| Collision detection | Axis-Aligned Bounding Box (AABB) |
| Game state | Menu → Playing → Game Over flow |
| HUD / UI | Drawing score, lives on screen |

---

## 2. Prerequisites & Project Setup

### Your Current Directory Structure

```
raylib_template/
├── main.c                        ← Your game source code
├── main.exe                      ← Compiled game
├── resources/
│   └── spaceship.png             ← Player sprite (already here!)
├── raylib/
│   └── raylib-6.0_win64_mingw-w64/
│       ├── include/              ← raylib.h, raymath.h
│       └── lib/                  ← libraylib.a
└── .vscode/
```

### What You Need Installed

- **MinGW-w64 (GCC)** — you already have this working ✅
- **Raylib 6.0 pre-compiled** — already in your `raylib/` folder ✅

---

## 3. How to Compile & Run

Every time you change `main.c`, compile and run with:

```bash
gcc -g main.c -I./raylib/raylib-6.0_win64_mingw-w64/include ./raylib/raylib-6.0_win64_mingw-w64/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -Wl,--defsym,stat64i32=_stat64 -o main.exe

.\main.exe
```

> **Tip:** You can paste this into a `build.bat` file so you just double-click to compile:
> ```bat
> @echo off
> gcc -g main.c -I./raylib/raylib-6.0_win64_mingw-w64/include ./raylib/raylib-6.0_win64_mingw-w64/lib/libraylib.a -lopengl32 -lgdi32 -lwinmm -Wl,--defsym,stat64i32=_stat64 -o main.exe
> if %errorlevel%==0 main.exe
> pause
> ```

---

## 4. Game Design Document (What We're Building)

```
┌──────────────────────────────────────────┐
│  SCORE: 0350          ♥ ♥ ♥  LIVES      │  ← HUD
│                                          │
│  👾 👾 👾 👾 👾 👾 👾 👾 👾 👾          │  ← Alien rows
│  👾 👾 👾 👾 👾 👾 👾 👾 👾 👾          │     (move left/right,
│  👾 👾 👾 👾 👾 👾 👾 👾 👾 👾          │      drop down)
│  👾 👾 👾 👾 👾 👾 👾 👾 👾 👾          │
│  👾 👾 👾 👾 👾 👾 👾 👾 👾 👾          │
│             |                            │  ← Alien bullets
│             |                            │
│                                          │
│                  |                       │  ← Player bullets
│                  🚀                      │  ← Player ship
└──────────────────────────────────────────┘
```

### Core Rules

- Player moves **left/right** with arrow keys or A/D.
- Player shoots with **Space** (max 1 bullet on screen at a time, or a cooldown).
- Aliens move **horizontally** as a group; when they hit a wall they **drop one row** and reverse.
- Aliens periodically **shoot downward**.
- If a bullet hits an alien → alien dies, score increases.
- If an alien bullet hits the player → player loses a life.
- **Game Over** when lives reach 0 or aliens reach the player's row.
- **You Win** when all aliens are destroyed.

---

## Step 1 — Window, Game Loop & Background

### Concepts: `InitWindow`, `BeginDrawing/EndDrawing`, `ClearBackground`

This is your starting point. You already have this working!

```c
#include "raylib.h"
#include "raymath.h"

// ── Constants ──────────────────────────────────
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

int main(void)
{
    // 1. Create the window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Space Invaders");
    SetTargetFPS(60);   // Lock to 60 frames per second

    // 2. Game loop — runs once per frame (60 times/second)
    while (!WindowShouldClose())    // Runs until user presses ESC or closes window
    {
        // ── UPDATE phase (game logic goes here) ──

        // ── DRAW phase ──
        BeginDrawing();
            ClearBackground(BLACK);     // Space is black!
            DrawText("Space Invaders!", 280, 280, 30, GREEN);
        EndDrawing();
    }

    // 3. Cleanup
    CloseWindow();
    return 0;
}
```

### What's Happening

| Line | Explanation |
|---|---|
| `InitWindow(800, 600, ...)` | Opens an 800×600 pixel window with a title |
| `SetTargetFPS(60)` | Caps the loop at 60 iterations per second |
| `WindowShouldClose()` | Returns `true` when user clicks ✕ or presses ESC |
| `BeginDrawing()` / `EndDrawing()` | Everything between these two calls gets drawn to screen |
| `ClearBackground(BLACK)` | Fills the entire screen with black before drawing anything new |

> **✅ Checkpoint:** Compile & run. You should see a black window with green text.

---

## Step 2 — The Player Ship

### Concepts: Structs, Keyboard Input, Texture Loading, Screen Bounds

### 2.1 — Define the Player Struct

Add this **above** `main()`:

```c
// ── Player ─────────────────────────────────────
typedef struct Player {
    Vector2 position;   // (x, y) center of the ship
    float speed;        // pixels per second
    int width;
    int height;
    int lives;
    Color color;
} Player;
```

> **Why a struct?** It groups all player data together. As you add aliens and bullets you'll use the same pattern. This is how game entities are organized.

### 2.2 — Initialize the Player

Inside `main()`, **before** the game loop:

```c
    // Initialize Player
    Player player = {
        .position = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 50 },
        .speed = 300.0f,        // 300 pixels per second
        .width = 50,
        .height = 30,
        .lives = 3,
        .color = GREEN
    };
```

### 2.3 — Handle Input & Move the Player (UPDATE phase)

Inside the game loop, **before** `BeginDrawing()`:

```c
        // ── UPDATE ──
        float dt = GetFrameTime();  // Time since last frame (~0.016s at 60FPS)

        // Player movement
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
            player.position.x -= player.speed * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
            player.position.x += player.speed * dt;

        // Clamp to screen edges
        if (player.position.x - player.width / 2 < 0)
            player.position.x = player.width / 2;
        if (player.position.x + player.width / 2 > SCREEN_WIDTH)
            player.position.x = SCREEN_WIDTH - player.width / 2;
```

> **Why `GetFrameTime()`?** If we just wrote `position.x -= 5`, the ship would move faster on faster computers. Multiplying by `dt` (delta time) makes movement **frame-rate independent** — the ship moves the same speed whether the game runs at 30 or 120 FPS.

### 2.4 — Draw the Player (DRAW phase)

Inside `BeginDrawing()` ... `EndDrawing()`:

```c
        // Draw player as a rectangle (we'll replace with a texture later)
        DrawRectangle(
            (int)(player.position.x - player.width / 2),
            (int)(player.position.y - player.height / 2),
            player.width,
            player.height,
            player.color
        );
```

> **✅ Checkpoint:** Compile & run. A green rectangle should move left/right with arrow keys.

### 2.5 — (Optional) Use Your Spaceship Texture

You have `resources/spaceship.png`! Load it like this:

```c
    // Before game loop:
    Texture2D shipTexture = LoadTexture("resources/spaceship.png");

    // In the DRAW phase (replace the DrawRectangle):
    DrawTextureEx(shipTexture,
        (Vector2){ player.position.x - player.width / 2,
                   player.position.y - player.height / 2 },
        0.0f,                       // rotation
        (float)player.width / shipTexture.width,   // scale
        WHITE                       // tint (WHITE = no tint)
    );

    // After the game loop, before CloseWindow():
    UnloadTexture(shipTexture);
```

---

## Step 3 — Player Shooting

### Concepts: Arrays of structs, Cooldown timers, Bullet lifecycle

### 3.1 — Define the Bullet Struct

```c
#define MAX_PLAYER_BULLETS 10
#define BULLET_SPEED       500.0f

typedef struct Bullet {
    Vector2 position;
    float speed;
    bool active;        // false = this slot is free to reuse
    Color color;
} Bullet;
```

### 3.2 — Initialize the Bullet Array

```c
    // Before game loop:
    Bullet playerBullets[MAX_PLAYER_BULLETS] = { 0 };
    float shootCooldown = 0.0f;     // seconds until player can shoot again
```

### 3.3 — Shoot on Space Press (UPDATE phase)

```c
        // Shooting
        shootCooldown -= dt;
        if (IsKeyPressed(KEY_SPACE) && shootCooldown <= 0.0f) {
            // Find an inactive bullet slot
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
                if (!playerBullets[i].active) {
                    playerBullets[i].active = true;
                    playerBullets[i].position = (Vector2){
                        player.position.x,
                        player.position.y - player.height / 2
                    };
                    playerBullets[i].speed = BULLET_SPEED;
                    playerBullets[i].color = YELLOW;
                    shootCooldown = 0.3f;   // 0.3 second cooldown
                    break;
                }
            }
        }
```

> **Why `IsKeyPressed` vs `IsKeyDown`?**
> - `IsKeyPressed` → triggers **once** when the key is first pushed down.
> - `IsKeyDown` → triggers **every frame** while the key is held.
>
> For shooting we want `IsKeyPressed` so the player taps to shoot, not spray continuously.

### 3.4 — Move Bullets Upward (UPDATE phase)

```c
        // Update player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
            if (playerBullets[i].active) {
                playerBullets[i].position.y -= playerBullets[i].speed * dt;

                // Deactivate if off-screen
                if (playerBullets[i].position.y < 0)
                    playerBullets[i].active = false;
            }
        }
```

### 3.5 — Draw Bullets (DRAW phase)

```c
        // Draw player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
            if (playerBullets[i].active) {
                DrawRectangle(
                    (int)playerBullets[i].position.x - 2,
                    (int)playerBullets[i].position.y,
                    4, 15,
                    playerBullets[i].color
                );
            }
        }
```

> **✅ Checkpoint:** Press Space — a yellow bullet should fly upward from the ship.

---

## Step 4 — Grid Alien Formation (4x5)

### Concepts: arrays, fixed positions, grid layout

### 4.1 — Define the Alien Grid Settings

Use a fixed-size alien formation instead of random sky spawning.

```c
#define ALIEN_ROWS 4
#define ALIEN_COLS 5
#define ALIEN_COUNT (ALIEN_ROWS * ALIEN_COLS)
#define ALIEN_WIDTH 30
#define ALIEN_HEIGHT 24
#define ALIEN_GAP_X 25
#define ALIEN_GAP_Y 20

typedef struct Alien {
    Rectangle rect;
    bool active;
    float speed;
    Color color;
} Alien;
```

### 4.2 — Create the Aliens in a Grid

This creates a 4x5 formation in a neat pattern across the screen:

```c
    // Before the game loop:
    Alien aliens[ALIEN_COUNT] = { 0 };

    int startX = (SCREEN_WIDTH - (ALIEN_COLS * ALIEN_WIDTH + (ALIEN_COLS - 1) * ALIEN_GAP_X)) / 2;
    int startY = 60;
    int index = 0;

    for (int row = 0; row < ALIEN_ROWS; row++) {
        for (int col = 0; col < ALIEN_COLS; col++) {
            aliens[index].rect = (Rectangle){
                .x = startX + col * (ALIEN_WIDTH + ALIEN_GAP_X),
                .y = startY + row * (ALIEN_HEIGHT + ALIEN_GAP_Y),
                .width = ALIEN_WIDTH,
                .height = ALIEN_HEIGHT
            };
            aliens[index].active = true;
            aliens[index].speed = 30.0f + row * 10.0f;
            aliens[index].color = (Color){
                180,
                120 + col * 20,
                100 + row * 30,
                255
            };
            index++;
        }
    }
```

### 4.3 — Draw the Fixed Grid

```c
        // Draw aliens as a static formation
        for (int i = 0; i < ALIEN_COUNT; i++) {
            if (aliens[i].active) {
                DrawRectangleRec(aliens[i].rect, aliens[i].color);
            }
        }
```

> **✅ Checkpoint:** You should see a neat formation of 20 aliens: 4 rows and 5 columns.
>
> This is the correct layout for your game. Do not spawn enemies randomly from the top of the screen.


---

## Step 5 — Alien Movement

### Concepts: Group movement, Edge detection, Direction reversal

### 5.1 — Add Movement Variables

```c
    // Before game loop:
    float alienMoveSpeed = 30.0f;       // pixels per second (starts slow)
    int alienDirection = 1;             // 1 = right, -1 = left
    float alienDropDistance = 20.0f;    // how far they drop when reversing
```

### 5.2 — Move Aliens as a Group (UPDATE phase)

```c
        // ── Alien movement ──
        bool shouldReverse = false;

        // Check if any alien hits a wall
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (!aliens[row][col].active) continue;
                float nextX = aliens[row][col].rect.x + alienMoveSpeed * alienDirection * dt;
                if (nextX <= 0 || nextX + ALIEN_WIDTH >= SCREEN_WIDTH) {
                    shouldReverse = true;
                    break;
                }
            }
            if (shouldReverse) break;
        }

        // Apply movement
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (!aliens[row][col].active) continue;
                if (shouldReverse) {
                    aliens[row][col].rect.y += alienDropDistance;
                } else {
                    aliens[row][col].rect.x += alienMoveSpeed * alienDirection * dt;
                }
            }
        }
        if (shouldReverse) alienDirection *= -1;
```

> **How does group movement work?**
> All aliens share the same `alienDirection` and `alienMoveSpeed`. Each frame we move every active alien by the same amount. When ANY alien touches a wall, ALL aliens drop down one step and reverse direction. This creates the classic "sweep and drop" pattern.

> **✅ Checkpoint:** Aliens should march left and right, dropping down when they hit the edges.

---

## Step 6 — Collision Detection (Bullets ↔ Aliens)

### Concepts: AABB collision, `CheckCollisionRecs()`

This is where the game becomes **playable**. We check every active bullet against every active alien each frame.

### 6.1 — Add Collision Check (UPDATE phase, after bullet & alien updates)

```c
        // ── Bullet-Alien collisions ──
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
            if (!playerBullets[i].active) continue;

            Rectangle bulletRect = {
                playerBullets[i].position.x - 2,
                playerBullets[i].position.y,
                4, 15
            };

            for (int row = 0; row < ALIEN_ROWS; row++) {
                for (int col = 0; col < ALIEN_COLS; col++) {
                    if (!aliens[row][col].active) continue;

                    if (CheckCollisionRecs(bulletRect, aliens[row][col].rect)) {
                        // Hit!
                        aliens[row][col].active = false;
                        playerBullets[i].active = false;
                        aliensAlive--;

                        // Score based on alien type
                        // (We'll add scoring in Step 9)

                        break;  // This bullet is consumed
                    }
                }
                if (!playerBullets[i].active) break;
            }
        }
```

> **What is AABB collision?**
> AABB = Axis-Aligned Bounding Box. We treat every entity as a rectangle. Two rectangles overlap if they overlap on **both** the X and Y axes. Raylib's `CheckCollisionRecs()` does exactly this check for you.

> **✅ Checkpoint:** Shoot at the aliens — they should disappear when hit!

---

## Step 7 — Alien Shooting Back

### Concepts: Random selection, Timer-based spawning

### 7.1 — Define Alien Bullets

```c
#define MAX_ALIEN_BULLETS 20
#define ALIEN_BULLET_SPEED 200.0f
```

```c
    // Before game loop:
    Bullet alienBullets[MAX_ALIEN_BULLETS] = { 0 };
    float alienShootTimer = 0.0f;
    float alienShootInterval = 1.5f;    // an alien shoots every 1.5 seconds
```

### 7.2 — Random Alien Fires (UPDATE phase)

```c
        // ── Alien shooting ──
        alienShootTimer += dt;
        if (alienShootTimer >= alienShootInterval && aliensAlive > 0) {
            alienShootTimer = 0.0f;

            // Pick a random active alien
            int attempts = 0;
            while (attempts < 100) {
                int r = GetRandomValue(0, ALIEN_ROWS - 1);
                int c = GetRandomValue(0, ALIEN_COLS - 1);
                if (aliens[r][c].active) {
                    // Find a free bullet slot
                    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
                        if (!alienBullets[i].active) {
                            alienBullets[i].active = true;
                            alienBullets[i].position = (Vector2){
                                aliens[r][c].rect.x + ALIEN_WIDTH / 2.0f,
                                aliens[r][c].rect.y + ALIEN_HEIGHT
                            };
                            alienBullets[i].speed = ALIEN_BULLET_SPEED;
                            alienBullets[i].color = RED;
                            break;
                        }
                    }
                    break;
                }
                attempts++;
            }
        }
```

### 7.3 — Move Alien Bullets Downward (UPDATE phase)

```c
        // Update alien bullets
        for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
            if (alienBullets[i].active) {
                alienBullets[i].position.y += alienBullets[i].speed * dt;
                if (alienBullets[i].position.y > SCREEN_HEIGHT)
                    alienBullets[i].active = false;
            }
        }
```

### 7.4 — Draw Alien Bullets (DRAW phase)

```c
        // Draw alien bullets
        for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
            if (alienBullets[i].active) {
                DrawRectangle(
                    (int)alienBullets[i].position.x - 2,
                    (int)alienBullets[i].position.y,
                    4, 12,
                    alienBullets[i].color
                );
            }
        }
```

> **✅ Checkpoint:** Red bullets should now rain down from random aliens.

---

## Step 8 — Player Lives & Getting Hit

### Concepts: Collision with alien bullets, Invincibility frames

### 8.1 — Check Alien Bullet → Player Collision (UPDATE phase)

```c
        // ── Alien bullet hits player ──
        Rectangle playerRect = {
            player.position.x - player.width / 2,
            player.position.y - player.height / 2,
            player.width,
            player.height
        };

        for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
            if (!alienBullets[i].active) continue;

            Rectangle abRect = {
                alienBullets[i].position.x - 2,
                alienBullets[i].position.y,
                4, 12
            };

            if (CheckCollisionRecs(playerRect, abRect)) {
                alienBullets[i].active = false;
                player.lives--;
                // Optional: reset player position
                player.position.x = SCREEN_WIDTH / 2.0f;
                break;
            }
        }
```

### 8.2 — Check If Aliens Reached the Player Row (UPDATE phase)

```c
        // ── Aliens reached the bottom? ──
        for (int row = 0; row < ALIEN_ROWS; row++) {
            for (int col = 0; col < ALIEN_COLS; col++) {
                if (aliens[row][col].active &&
                    aliens[row][col].rect.y + ALIEN_HEIGHT >= player.position.y - player.height / 2)
                {
                    player.lives = 0;   // Instant game over
                }
            }
        }
```

> **✅ Checkpoint:** Get hit by a red bullet — your lives should decrease (we'll display them next).

---

## Step 9 — Scoring & HUD

### Concepts: DrawText, TextFormat (printf-style formatting for screen text)

### 9.1 — Add a Score Variable

```c
    // Before game loop:
    int score = 0;
```

### 9.2 — Award Points on Kill

Go back to your **bullet-alien collision** code (Step 6) and add scoring where the `// Score based on alien type` comment was:

```c
                    if (CheckCollisionRecs(bulletRect, aliens[row][col].rect)) {
                        aliens[row][col].active = false;
                        playerBullets[i].active = false;
                        aliensAlive--;

                        // Award points based on alien type
                        switch (aliens[row][col].type) {
                            case 0: score += 30; break;     // Top rows (hardest)
                            case 1: score += 20; break;     // Middle rows
                            default: score += 10; break;    // Bottom rows
                        }

                        break;
                    }
```

### 9.3 — Draw the HUD (DRAW phase, draw LAST so it's on top)

```c
        // ── HUD ──
        DrawText(TextFormat("SCORE: %04d", score), 10, 10, 20, GREEN);

        // Draw lives as small hearts/ships
        DrawText("LIVES:", SCREEN_WIDTH - 180, 10, 20, GREEN);
        for (int i = 0; i < player.lives; i++) {
            DrawRectangle(SCREEN_WIDTH - 90 + i * 25, 10, 20, 20, GREEN);
        }

        // Draw a line separating HUD from game area
        DrawLine(0, 40, SCREEN_WIDTH, 40, DARKGREEN);
```

> **`TextFormat()`** works exactly like `printf()` but returns a string you can pass to `DrawText()`. Very handy!

> **✅ Checkpoint:** Score and lives should be visible at the top of the screen.

---

## Step 10 — Game Over & Win Screens

### Concepts: Game state management with enums

### 10.1 — Define Game States

Add this near the top of your file:

```c
typedef enum {
    GAME_PLAYING,
    GAME_OVER,
    GAME_WIN
} GameState;
```

```c
    // Before game loop:
    GameState state = GAME_PLAYING;
```

### 10.2 — Trigger State Changes (UPDATE phase)

```c
        // ── Check game state transitions ──
        if (player.lives <= 0)
            state = GAME_OVER;
        if (aliensAlive <= 0)
            state = GAME_WIN;
```

### 10.3 — Only Run Game Logic When Playing

Wrap your entire UPDATE and entity-drawing code inside:

```c
        if (state == GAME_PLAYING) {
            // ... all UPDATE logic from Steps 2-9 ...
        }
```

### 10.4 — Draw End Screens (DRAW phase)

```c
        // ── End screens (drawn OVER everything) ──
        if (state == GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 60) / 2,
                     SCREEN_HEIGHT / 2 - 50, 60, RED);
            DrawText(TextFormat("Final Score: %d", score),
                     SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
                     SCREEN_HEIGHT / 2 + 30, 30, WHITE);
            DrawText("Press ENTER to Restart",
                     SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Restart", 20) / 2,
                     SCREEN_HEIGHT / 2 + 80, 20, GRAY);
        }

        if (state == GAME_WIN) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("YOU WIN!", SCREEN_WIDTH / 2 - MeasureText("YOU WIN!", 60) / 2,
                     SCREEN_HEIGHT / 2 - 50, 60, GREEN);
            DrawText(TextFormat("Final Score: %d", score),
                     SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
                     SCREEN_HEIGHT / 2 + 30, 30, WHITE);
            DrawText("Press ENTER to Restart",
                     SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Restart", 20) / 2,
                     SCREEN_HEIGHT / 2 + 80, 20, GRAY);
        }
```

### 10.5 — Restart on Enter

Add this in the UPDATE phase:

```c
        // ── Restart ──
        if ((state == GAME_OVER || state == GAME_WIN) && IsKeyPressed(KEY_ENTER)) {
            // Reset everything
            player.position = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 50 };
            player.lives = 3;
            score = 0;
            aliensAlive = ALIEN_ROWS * ALIEN_COLS;
            alienDirection = 1;
            alienMoveSpeed = 30.0f;

            for (int row = 0; row < ALIEN_ROWS; row++) {
                for (int col = 0; col < ALIEN_COLS; col++) {
                    aliens[row][col].rect.x = 70 + col * (ALIEN_WIDTH + ALIEN_PADDING);
                    aliens[row][col].rect.y = 60 + row * (ALIEN_HEIGHT + ALIEN_PADDING);
                    aliens[row][col].active = true;
                }
            }
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++) playerBullets[i].active = false;
            for (int i = 0; i < MAX_ALIEN_BULLETS; i++) alienBullets[i].active = false;

            state = GAME_PLAYING;
        }
```

> **✅ Checkpoint:** Die or kill all aliens — you should see the end screen. Press Enter to restart.

---

## Step 11 — Polish (Sound, Textures, Particles)

Once the game is playable, add polish:

### 11.1 — Sound Effects

```c
    // Before game loop:
    InitAudioDevice();
    Sound shootSound = LoadSound("resources/shoot.wav");
    Sound explosionSound = LoadSound("resources/explosion.wav");

    // When player shoots:
    PlaySound(shootSound);

    // When alien dies:
    PlaySound(explosionSound);

    // Before CloseWindow():
    UnloadSound(shootSound);
    UnloadSound(explosionSound);
    CloseAudioDevice();
```

> You can find free sound effects at [OpenGameArt.org](https://opengameart.org/) or [freesound.org](https://freesound.org/).

### 11.2 — Star Background

```c
    // Before game loop:
    #define NUM_STARS 100
    Vector2 stars[NUM_STARS];
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i] = (Vector2){ GetRandomValue(0, SCREEN_WIDTH), GetRandomValue(0, SCREEN_HEIGHT) };
    }

    // In DRAW phase (right after ClearBackground):
    for (int i = 0; i < NUM_STARS; i++) {
        DrawPixel((int)stars[i].x, (int)stars[i].y, (Color){255, 255, 255, GetRandomValue(100, 255)});
    }
```

### 11.3 — Speed Up as Aliens Die

Add this in the collision handling:

```c
    // After an alien is killed:
    alienMoveSpeed = 30.0f + (ALIEN_ROWS * ALIEN_COLS - aliensAlive) * 2.0f;
```

This makes the last few aliens move **fast** — just like the original game!

---

## Full Source Code Skeleton

Here's how your final `main.c` structure should look:

```c
#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>

// ── Constants ──
#define SCREEN_WIDTH       800
#define SCREEN_HEIGHT      600
#define MAX_PLAYER_BULLETS 10
#define MAX_ALIEN_BULLETS  20
#define BULLET_SPEED       500.0f
#define ALIEN_BULLET_SPEED 200.0f
#define ALIEN_ROWS         5
#define ALIEN_COLS         10
#define ALIEN_WIDTH        35
#define ALIEN_HEIGHT       25
#define ALIEN_PADDING      10

// ── Types ──
typedef enum { GAME_PLAYING, GAME_OVER, GAME_WIN } GameState;

typedef struct Player {
    Vector2 position;
    float speed;
    int width, height, lives;
    Color color;
} Player;

typedef struct Bullet {
    Vector2 position;
    float speed;
    bool active;
    Color color;
} Bullet;

typedef struct Alien {
    Rectangle rect;
    bool active;
    int type;
} Alien;

// ── Main ──
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Space Invaders");
    SetTargetFPS(60);

    // ... Initialize all entities (Steps 2–10) ...

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ── UPDATE ──
        // ... Restart check ...
        // ... Player movement ...
        // ... Player shooting ...
        // ... Move bullets ...
        // ... Alien movement ...
        // ... Alien shooting ...
        // ... Collisions ...
        // ... State transitions ...

        // ── DRAW ──
        BeginDrawing();
        ClearBackground(BLACK);
            // ... Draw stars ...
            // ... Draw aliens ...
            // ... Draw bullets ...
            // ... Draw player ...
            // ... Draw HUD ...
            // ... Draw end screens ...
        EndDrawing();
    }

    // Cleanup
    CloseWindow();
    return 0;
}
```

---

## Key Raylib Functions Cheat-Sheet

| Function | Purpose |
|---|---|
| `InitWindow(w, h, title)` | Create game window |
| `CloseWindow()` | Destroy window and free resources |
| `SetTargetFPS(fps)` | Cap frame rate |
| `WindowShouldClose()` | Check if ESC pressed or window closed |
| `GetFrameTime()` | Seconds since last frame (delta time) |
| `IsKeyDown(key)` | True while key is held |
| `IsKeyPressed(key)` | True only on the frame key is first pressed |
| `BeginDrawing()` / `EndDrawing()` | Wrap all draw calls |
| `ClearBackground(color)` | Fill screen with color |
| `DrawRectangle(x,y,w,h,color)` | Draw filled rectangle |
| `DrawRectangleRec(rect, color)` | Draw from a `Rectangle` struct |
| `DrawText(text, x, y, size, color)` | Draw text string |
| `TextFormat(fmt, ...)` | Printf-style text formatting |
| `MeasureText(text, size)` | Get text width in pixels (for centering) |
| `CheckCollisionRecs(r1, r2)` | AABB collision between two rectangles |
| `LoadTexture(path)` / `UnloadTexture(t)` | Load/free image file |
| `DrawTextureEx(tex, pos, rot, scale, tint)` | Draw texture with transforms |
| `LoadSound(path)` / `PlaySound(s)` | Load and play audio |
| `GetRandomValue(min, max)` | Random integer in range |

---

## Common Mistakes & Debugging Tips

| Mistake | Fix |
|---|---|
| Game runs at different speeds on different PCs | Always multiply movement by `GetFrameTime()` |
| Bullets pass through aliens | Make sure collision check runs **after** moving bullets but **before** drawing |
| Aliens don't reverse at walls | Check if you're using `<= 0` not `< 0` for left wall |
| Nothing draws | Did you forget `BeginDrawing()`? Is `ClearBackground` overwriting everything? |
| Texture shows white rectangle | Wrong file path — use `"resources/spaceship.png"` (relative to .exe) |
| Sound doesn't play | Call `InitAudioDevice()` before loading sounds |
| Score doesn't update | Make sure the score update is inside the collision `if` block, not after it |
| Game crashes when shooting | Bullet array too small, or forgot to `break` after finding a free slot |

---

## Next Steps & Challenges

Once you have the base game working, try these enhancements to level up:

1. **🛡️ Defense Barriers** — Add destructible cover for the player (like the original).
2. **🛸 Mystery Ship** — A bonus UFO that flies across the top for extra points.
3. **📈 Multiple Waves** — After clearing all aliens, spawn a new, faster wave.
4. **🎆 Explosions** — Draw a brief animation when an alien dies.
5. **🏆 High Score** — Save the high score to a file using `SaveFileText()`.
6. **🎵 Background Music** — Use `LoadMusicStream()` and `UpdateMusicStream()`.
7. **📱 Menu Screen** — Title screen with "Press ENTER to Start".
8. **🎨 Sprite Sheet Aliens** — Draw different alien designs using textures instead of rectangles.

---

## Resources

- [Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) — Quick reference for all functions
- [Raylib Examples](https://www.raylib.com/examples.html) — Official code examples
- [Raylib Wiki](https://github.com/raysan5/raylib/wiki) — Guides and tutorials
- [OpenGameArt.org](https://opengameart.org/) — Free game assets (sprites, sounds)

---

**Happy coding! 🎮 Build it step by step, test after each step, and you'll have Space Invaders running in no time.**
