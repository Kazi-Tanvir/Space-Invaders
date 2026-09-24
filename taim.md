# Architecture & Code Changes: `main.c` vs `tamim.c`

This document details all structural, algorithmic, and visual modifications made when transforming the base implementation (`main.c`) into the multi-level game (`tamim.c`), incorporating all mechanics from `reference.c`.

---

## 1. Window & Global Configuration

| Parameter | `main.c` | `tamim.c` | Rationale |
|---|---|---|---|
| **Window Resolution** | `1000 x 800` | `1200 x 900` | Expands battlefield width and height for boss movement and multi-lane enemy patterns. |
| **Player Speed** | `300.0f` | `350.0f` | Tuned for higher mobility on the larger canvas and dodging boss radial bursts. |
| **Player Shoot Cooldown** | `0.5f` (2 shots/s) | `0.20f` (5 shots/s) | Balanced for boss HP pool (50 HP) and denser enemy waves. |
| **Max Player Bullets** | `25` | `30` | Supports faster firing rate without dropping bullet inputs. |
| **Max Enemy Bullets** | `10` | `40` | Accommodates simultaneous fire from multiple enemy types across the screen. |
| **Background Color** | `BLACK` (`0, 0, 0, 255`) | `BG_COLOR` (`8, 8, 12, 255`) | Deep space tinted backdrop matching `reference.c`. |
| **Player Bullet Color** | `WHITE` | `SABER_GREEN` (`255, 20, 147, 255`) | Distinct high-visibility neon laser tint. |

---

## 2. Data Structure Extensions

### 2.1 Bullet Vector Physics
* **`main.c`**:
  ```c
  typedef struct Bullet {
      Vector2 position;
      float speed;
      bool active;
  } Bullet;
  ```
  *Bullets could only travel purely vertically.*
* **`tamim.c`**:
  ```c
  typedef struct Bullet {
      float x, y;
      float vx, vy;
      bool active;
  } Bullet;
  ```
  *Added 2D velocity components (`vx`, `vy`) to support angled shots (Boss Spread Cone) and omnidirectional velocity vectors (Boss 360° Radial Burst).*

### 2.2 Player Struct
* Added `invincibleTimer` (seconds remaining where player blinks and cannot take damage after a hit).

### 2.3 Game States (`GameState`)
* **`main.c`** (3 states):
  * `PLAYING`, `GAME_WON`, `GAME_LOST`
* **`tamim.c`** (6 states):
  * `STATE_TITLE`: Interactive main menu with level & boss selection.
  * `STATE_LEVEL_TRANSITION`: 2.5-second interstitial banner displaying the upcoming level and hint.
  * `STATE_PLAYING`: Normal wave progression across Levels 1, 2, and 3.
  * `STATE_BOSS_FIGHT`: Dedicated boss battle encounter with unique HUD and attack phases.
  * `STATE_GAME_WON`: Victory screen with score summary and ENTER to return to title.
  * `STATE_GAME_LOST`: Game over screen with ENTER to return to title.

### 2.4 New Entities & Mechanics
* **`Boss` Struct**:
  * Tracks position (`x, y`), dimensions (`width, height`), health & maxHealth (`50`), phase state machine (`BossPhase`), phase and attack timers, horizontal sway direction, hit flash frames, and threshold spawn flags (`minionSpawnFlags[3]`).
* **`BossPhase` Enum**:
  * `BOSS_IDLE`: 2.0s horizontal sway without firing.
  * `BOSS_DIRECT`: Aimed shots fired towards the player's X coordinate every 0.3s.
  * `BOSS_SPREAD`: 3-bullet cone (center, -120px/s, +120px/s) fired every 0.6s.
  * `BOSS_RAIN`: 16-bullet 360° radial burst fired every 1.5s for 4.5s.
* **`Particle` Struct**:
  * Replaces the single static circle explosion with dynamic, multi-colored debris particles (`RED`, `ORANGE`, `YELLOW`, `WHITE`) that scatter with randomized velocities and shrink over time.

---

## 3. Enemy Architecture: Rigid Grid vs. Autonomous Agents

### 3.1 Enemy Storage & Spawning
* **`main.c`**:
  * Used a fixed 2D array: `Enemy enemies[5][11]` bound to a single `EnemyGrid` struct (`offsetX`, `offsetY`, `speed`).
  * All enemies moved together uniformly as an invader grid bouncing off left/right walls.
* **`tamim.c`**:
  * Replaced with a flat agent pool: `Enemy enemies[MAX_ENEMIES]` (size 50).
  * Implemented `SpawnLevelEnemies(enemies, level)` creating bespoke wave formations per level:
    * **Level 1**: 24 enemies (Dummies, Basics, Rapids).
    * **Level 2**: 28 enemies (Tanks, Zigzags, Basics).
    * **Level 3**: 12 support enemies (Rapids, Dummies) followed by the Boss.

### 3.2 Individual Enemy AI & Behaviors
Each enemy in `tamim.c` executes autonomous logic:
1. **`ENEMY_DUMMY`**: Drifts straight down at 40 px/s; never shoots.
2. **`ENEMY_BASIC`**: Horizontal bouncing patrol at 80 px/s with a slow 15 px/s downward drift; periodic single shots.
3. **`ENEMY_ZIGZAG`**: Sine-wave horizontal sweep (`sinf(moveTimer * 3.0f) * 120.0f`) with rapid downward drift (60 px/s).
4. **`ENEMY_TANK`**: High durability (3 HP), slow 10 px/s drift, renders a dedicated health bar above its sprite, and flashes red when hit.
5. **`ENEMY_RAPID`**: High-speed horizontal bouncing (120 px/s) locked to the top 40% of the screen; fires frequent projectiles (cooldown 0.5–1.0s).

### 3.3 Enemy Mutual Collision Avoidance
* `tamim.c` adds an $O(N^2)$ collision solver between active enemies:
  * When two enemies overlap, they push each other apart by half the overlap distance and invert their horizontal movement directions (`direction *= -1.0f`).

---

## 4. Boss Battle System (`STATE_BOSS_FIGHT`)

`main.c` had no boss encounter. `tamim.c` implements:
1. **Attack State Machine**:
   * Cycles continuously through `BOSS_IDLE` -> `BOSS_DIRECT` -> `BOSS_SPREAD` -> `BOSS_RAIN`.
2. **Circular Burst (Replaced Bullet Rain)**:
   * Fires 16 bullets in a 360-degree circle evenly spaced by $\Delta\theta = \frac{2\pi}{16}$, computing:
     $$v_x = \sin(\theta) \cdot v_{\text{boss}}, \quad v_y = \cos(\theta) \cdot v_{\text{boss}}$$
3. **Threshold Minion Spawns**:
   * Evaluates boss HP against 75%, 50%, and 25% thresholds.
   * When crossed for the first time, spawns 1–2 `ENEMY_DUMMY` minions directly beneath the boss flanking left and right.
4. **Boss HUD**:
   * Full-width health bar with current/max HP text display at the top of the screen.

---

## 5. Visual Feedback & Polish

1. **Screen Shake System**:
   * Triggers on player taking damage (`0.3s`) and on boss defeat (`0.6s`).
   * Computes random offsets `sx` and `sy` (`-3` to `+3` px) and applies them to all drawing coordinates.
2. **Invincibility Blinking**:
   * Following damage, the player ship blinks at 10 Hz for 1.5 seconds (`player.invincibleTimer`).
3. **Hit Flash**:
   * `hitFlashFrames` tints enemies and the boss `RED` when damaged.
4. **Particle Debris**:
   * Exploding enemies release 5 multi-color particles; boss release 15 particles.
5. **Interactive Title Menu**:
   * `UP`/`DOWN` keys navigate a selector with highlighted borders and arrow markers (`»`).
   * Direct access to Level 1, Level 2, or Level 3 (Boss Fight directly).

---

## 6. Summary Comparison Table

| Feature | `main.c` | `tamim.c` |
|---|---|---|
| **Code Structure** | Single-wave grid arcade shooter | Multi-level space shooter with boss fight |
| **Total Lines** | 662 | 870 |
| **Levels** | 1 (Grid of 55 enemies) | 3 Levels + Selectable Boss Fight |
| **Menu System** | None | Interactive Title Screen with level selection |
| **Movement Engine** | Fixed grid offset displacement | Per-enemy velocity, sine waves, bouncing, & collision pushing |
| **Boss Encounter** | None | Multi-phase boss + minion thresholds + 360° burst |
| **Bullet Angles** | Vertical only ($v_y$) | Full 2D vector physics ($v_x, v_y$) |
| **Particle System** | Single expanding circle | Multi-particle emitter with lifetime, velocity, & decay |
| **Camera Effects** | Static screen | Dynamic screen shake translation |
