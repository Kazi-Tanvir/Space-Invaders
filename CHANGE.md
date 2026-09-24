# CHANGE LOG — Space Invaders (raylib)

All changes to `main.c` are recorded here in reverse-chronological order.

---

## [8] — 2026-09-24 · Bug fix: Enemy Groups system

**File:** `main.c`  **Author:** AI

### Bugs found & fixed

| # | Location | Bug | Fix |
|---|----------|-----|-----|
| 1 | Line 27 | `#define ENEMY_GROUPS` used `ENEMY_ROWS` **before** `ENEMY_ROWS` was defined → macro expanded to nothing | Moved `ENEMY_ROWS` define before `ENEMY_GROUPS` |
| 2 | `EnemyGroup` struct | Field named `enemy` but all code read/wrote `.type` → compile error | Renamed field to `type` |
| 3 | Init block | `groups[]` was initialized by reading `enemies[i][g*4].type` **before** `Enemy enemies[][]` was declared | Declared `enemies[][]` first; moved group init after enemy init; extracted `InitGroups()` helper |
| 4 | Movement loop | `group->type` → was the wrong field name (same as bug 2) | Fixed by bug 2 |
| 5 | Color define | `SABER_GREEN` was deleted but still used in draw | Restored `#define SABER_GREEN` |
| 6 | Shoot + reset | `grid.shootTimer`, `grid.offsetX`, etc. still referenced after `EnemyGrid` was deleted | Replaced with plain `float shootTimer` |
| 7 | Reset block | Used deleted `ENEMY_NOOB` / `ENEMY_MEDIUM` / `ENEMY_STRONG` constants | Replaced with `InitEnemy()` helper using new `EnemyType` enum |
| 8 | Draw — inside loop | Old `ENEMY_NOOB`/`ENEMY_MEDIUM`/`ENEMY_STRONG` switch cases + `DrawTextureEx( , ...)` with missing texture name | Replaced with `DUMMY/BASIC/ZIGZAG/RAPID/TANK` cases and correct textures |
| 9 | Draw — outside loop | Dangling `switch(enemies[i][j].type)` block at line 602 — `i`, `j`, `pos` not in scope | Removed; draw switch is now inside the `for i / for j` loop |

### New helpers added
- `InitEnemy(Enemy *e, EnemyType type)` — sets type, speed, health, active in one place (used for init and reset)
- `InitGroups(EnemyGroup groups[], Enemy enemies[][])` — fills group array from already-initialized enemy array

---


## [7] — 2026-09-24 · REVERT to single grid movement

**File:** `main.c`  **Author:** AI (user request)

### What changed
Reverted to the state equivalent to after change **[4]** — enemies move together as one rectangular grid again.

### State of the code
- `EnemyGrid { offsetX, offsetY, speed, shootTimer }` — single shared grid
- `Enemy { int type }` — simple int: `ENEMY_DEAD=0`, `ENEMY_NOOB=1`, `ENEMY_MEDIUM=2`, `ENEMY_STRONG=3`
- Grid bounces left/right at `ENEMY_LEFT_BOUND=60` and dynamic `ENEMY_RIGHT_BOUND`
- Grid drops `ENEMY_DROP_STEP` on each wall bounce
- Grid speed: `100.0f` px/s
- Player bullets: `SABER_GREEN` hot-pink color
- Starfield background: 3 parallax layers (260 stars total)

---


## [6] — 2026-09-24 · Full Clean Rewrite (Fixes broken state + per-row movement)

**File:** `main.c`  **Author:** AI

### Why
The user's series of partial edits left the file in an uncompilable state:
- `InitializeEnemy()` was nested inside `main()` (C doesn't allow nested functions)
- `grid`, `enemies`, and `enemyCount` were never declared (their init code was deleted)
- `enemy.type` draw switch referenced an undeclared variable `enemy`
- `PINK` macro clashed with raylib's built-in; `SABER_GREEN` was deleted but still used
- Old `ENEMY_DEAD/NOOB/MEDIUM/STRONG` int constants mixed with new `EnemyType` enum

### What the rewrite delivers

#### Kept from user's intent
- New `EnemyType` enum: `ETYPE_DEAD`, `ETYPE_DUMMY`, `ETYPE_BASIC`, `ETYPE_ZIGZAG`, `ETYPE_RAPID`, `ETYPE_TANK`
- Extended `Enemy` struct: `type`, `health`, `maxHealth`
- `enemyTexture1..5` naming for the 5 textures

#### Enemy movement — per-row independent
Each of the 5 rows has its own `EnemyRow { offsetX, offsetY, speed }`:

| Row | Type   | Speed   | Start dir |
|-----|--------|---------|-----------|
| 0   | TANK   | 160 px/s | → right |
| 1   | RAPID  | 130 px/s | ← left  |
| 2   | ZIGZAG | 100 px/s | → right |
| 3   | BASIC  |  75 px/s | ← left  |
| 4   | DUMMY  |  50 px/s | → right |

Each row bounces and drops independently — they weave past each other.

#### Health system
- TANK has 3 HP (takes 3 hits to kill); all others have 1 HP
- A red health bar is drawn above each TANK enemy

#### Score table
| Type   | Points |
|--------|--------|
| DUMMY  | 5      |
| BASIC  | 10     |
| ZIGZAG | 15     |
| RAPID  | 20     |
| TANK   | 30     |

---



All changes to `main.c` are recorded here in reverse-chronological order.

---

## [5] — 2026-09-24 · Per-Row Independent Enemy Movement

**File:** `main.c`  **Author:** AI

### What changed
Replaced the single monolithic `EnemyGrid` (one `offsetX/offsetY/speed` shared by all rows) with an `EnemyRow` array — each of the 5 enemy rows now moves completely independently.

### Details

#### `EnemyGrid` → `EnemyRow` struct
```c
typedef struct EnemyRow {
    float offsetX;   // horizontal drift of this row
    float offsetY;   // accumulated vertical drop
    float speed;     // current speed (sign = direction)
} EnemyRow;
```
`shootTimer` removed from the struct; promoted to a plain `float shootTimer` in `main()`.

#### Per-row speed multipliers
```c
static const float rowSpeedMul[5] = { 1.6f, 1.3f, 1.0f, 0.75f, 0.5f };
```
- Row 0 (Strong, top): 160 px/s
- Row 1 (Medium): 130 px/s
- Row 2 (Medium): 100 px/s
- Row 3 (Noob): 75 px/s
- Row 4 (Noob, bottom): 50 px/s
- Odd-indexed rows start moving in the **opposite** direction to even rows → weaving/staggered look

#### Bounce logic
Each row independently hits `ENEMY_LEFT_BOUND`/`ENEMY_RIGHT_BOUND` and reverses, dropping `ENEMY_DROP_STEP` on its own schedule.

#### All call sites updated
- `GetEnemyPosition(row, col, EnemyGrid)` → `GetEnemyPosition(row, col, EnemyRow)`
- Shooting, collision, draw, and reset code all updated to use `rows[i]`.

#### Bonus fix
Renamed user-defined `#define PINK` → `#define SABER_GREEN` to avoid a name clash with raylib's built-in `PINK` macro.

---

## [4] — 2026-09-24 · User Manual Edits

**File:** `main.c`  **Author:** User

### Changes made
| Change | Detail |
|--------|--------|
| Added color define | `#define SABER_GREEN (Color){255, 20, 147, 255}` (hot-pink neon for player bullets) |
| Player bullet color | Changed from `WHITE` → `SABER_GREEN` |
| Enemy bounds | `ENEMY_RIGHT_BOUND` changed to `(WINDOW_WIDTH - ENEMY_GRID_X - (ENEMY_COLS*ENEMY_CELL_SIZE))` (dynamic, fits window) |
| Enemy bounds | `ENEMY_LEFT_BOUND` changed from `-60` → `60` |
| Enemy speed | Increased from `25.0f` → `100.0f` |

---



All changes to `main.c` are recorded here in reverse-chronological order.

---

## [2] — 2026-09-24 · Moving Starfield Background

**File:** `main.c`

### What changed
Added a 3-layer parallax starfield that scrolls downward across the screen to give a "flying through space" feel.

### Details

#### New `#define` constants (after enemy type IDs)
```c
#define STAR_COUNT_FAR    150   // slow, tiny, dim stars
#define STAR_COUNT_MID     80   // medium stars
#define STAR_COUNT_NEAR    30   // fast, large, bright stars
#define STAR_TOTAL        (STAR_COUNT_FAR + STAR_COUNT_MID + STAR_COUNT_NEAR)  // 260 total
#define STAR_SPEED_FAR    30.0f
#define STAR_SPEED_MID    70.0f
#define STAR_SPEED_NEAR  160.0f
```

#### New `Star` struct (added to Structs section)
```c
typedef struct Star {
    float x, y;
    float speed;
    float size;
    unsigned char brightness;
} Star;
```

#### Initialisation (inside `main()`, before texture loading)
- 260 stars allocated on the stack as `Star stars[STAR_TOTAL]`
- Each layer given random positions spread across the full window
- Far layer: size 0.5–1 px, brightness 80–140
- Mid layer: size 1–1.5 px, brightness 140–200
- Near layer: size 1–2 px, brightness 200–255

#### Update (top of game loop, before `if (state == PLAYING)`)
- Each star moves down by `speed * dt` per frame
- When a star exits the bottom (`y > WINDOW_HEIGHT`) it wraps to `y = 0` at a new random `x`

#### Draw (immediately after `ClearBackground`, before HUD/enemies)
- Stars with size <= 1 px drawn with `DrawPixel` (perf)
- Larger stars drawn with `DrawCircleV`
- Drawn as grayscale: `Color{b, b, b, 255}`

---

## [1] — Initial State (before AI-assisted changes)

**File:** `main.c`

### Baseline features
- Window: 1200 x 900, 60 FPS
- Player ship with left/right movement, clamped to screen bounds
- Player bullets (max 25) fired with SPACE, 0.5 s cooldown
- Enemy grid: 5 rows x 11 cols, 3 types (Noob / Medium / Strong)
- Enemy grid moves side-to-side and drops on boundary hit
- Enemy random shooting (max 10 bullets, 1 s cooldown)
- Collision detection: player bullets vs enemies, enemy bullets vs player
- Explosion effect (growing circle) on hit
- Lives system (3 hearts), displayed with heart.png texture
- Score tracking (10 / 20 / 30 pts per enemy type)
- Win / Lose overlay with ENTER to restart
- HUD: score, lives, enemy count
- Textures: spaceship.png, noob.png, medium.png, strong.png, heart.png
- Background: solid dark colour (8, 8, 12)
