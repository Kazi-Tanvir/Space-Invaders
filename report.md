# Space Invaders (main.c) - Comprehensive Code Review & Audit Report

**Date of Audit:** September 27, 2026  
**Target File:** [`main.c`](file:///d:/02_CODE/06_BUET_CSE/02_GAMES/raylib_template/raylib_template/main.c) (Total Lines: 2925)  
**Compiler Status:** GCC MinGW 64-bit (`-Wall -Wextra` passes with 0 warnings, 0 errors)

---

## Executive Summary

The codebase is well-structured, featuring clean 60 FPS state-driven gameplay in Raylib, custom parallax starfield effects, sprite trimming and frame cycling, robust audio streaming, and high score/savegame persistence. 

During this in-depth line-by-line review of all 2,925 lines, we identified:
- **1 Critical Logic Bug** (double bonus score & redundant state assignment on boss defeat)
- **1 File Corruption Vulnerability** (space character in player name breaks leaderboard file parsing)
- **1 State Desynchronization Bug** (boss rage mode not serialized in `SaveGame` / `LoadGame`)
- **Multiple Instances of Redundant Code** (duplicated scroll limits, redundant mouse clamping, duplicate back-button geometry)
- **Several Stale / Informal Comments** left over from development

No code was modified during this review, as instructed.

---

## 1. Bugs, Logic Flaws & Edge Cases

### 1.1 Critical: Double Win & Double Score Bonus on Boss Defeat
* **Location:** Lines 2080–2090 and Lines 2163–2168
* **Code in Question:**
  ```c
  // Location A (Inside player bullet collision with boss, lines 2080-2090):
  if (boss.health <= 0)
  {
      shakeTimer = 0.6f;
      SpawnParticles(particles, boss.x + BOSS_WIDTH / 2.0f, boss.y + BOSS_HEIGHT / 2.0f, 15);
      boss.active = false;
      score += 200;
      state = GAME_WON;
  }

  // Location B (At the end of boss fight update block, lines 2163-2168):
  // --- Boss win condition: boss death ends the level immediately ---
  if (boss.health <= 0)
  {
      boss.active = false;
      score += 200; // bonus for defeating boss
      state = GAME_WON;
  }
  ```
* **Analysis:**
  Both condition checks run in the same frame when the boss's health reaches zero. 
  When the final bullet hits the boss, Location A adds `+200` points and sets `state = GAME_WON`. Then execution proceeds to line 2163 where `if (boss.health <= 0)` is evaluated again, adding another `+200` points!
* **Impact:** The player is awarded `+400` points instead of the intended and documented `+200` bonus points.
* **Suggested Fix:** Remove the redundant block at lines 2163–2168 entirely.

---

### 1.2 Leaderboard File Corruption When Name Contains Spaces
* **Location:** Lines 1482–1486, Line 539, and Lines 527–534
* **Code in Question:**
  ```c
  // In NAME_ENTRY state (line 1482):
  if (nameLen < MAX_NAME_LEN - 1 && ch >= 32 && ch <= 125)
  {
      nameBuffer[nameLen++] = (char)ch;
      nameBuffer[nameLen] = '\0';
  }

  // In SaveLeaderboard (line 543):
  fprintf(f, "%s %d %d\n", lb[i].name, lb[i].score, lb[i].level);

  // In LoadLeaderboard (line 529):
  if (fscanf(f, "%15s %d %d", lb[*count].name, &lb[*count].score, &lb[*count].level) != 3)
      break;
  ```
* **Analysis:**
  The name input check allows ASCII character 32 (space `' '`). If a user inputs `"JOHN DOE"`, `SaveLeaderboard` writes:
  ```text
  JOHN DOE 1500 2
  ```
  When `LoadLeaderboard` reads this file on next startup, `%15s` halts reading at the space character, placing `"JOHN"` into `name` and attempting to parse `"DOE"` into the integer `score` field. This causes `fscanf` to return `1` instead of `3`, triggering `break` and discarding all remaining valid leaderboard records.
* **Suggested Fix:**
  Either prevent spaces during input (`ch > 32`), convert spaces to underscores `_`, or format the text with quotes / delimiters in `fprintf`/`fscanf`.

---

### 1.3 State Desynchronization: Boss Rage Not Saved in SaveGame
* **Location:** `SaveGame` (lines 644–700) and `LoadGame` (lines 702–826)
* **Code in Question:**
  ```c
  // SaveGame serializes boss position, velocity, and attack phases, but misses rage state:
  fprintf(f, "BA %d\n", boss->active ? 1 : 0);
  fprintf(f, "BH %d\n", boss->health);
  ...
  fprintf(f, "BP %d\n", boss->attackPhase);
  // Missing: boss->ragePhase, boss->rageTimer, boss->rageTriggered
  ```
* **Analysis:**
  The `Boss` structure contains `ragePhase`, `rageTimer`, and `rageTriggered` (lines 286–288). If the player pauses/quits or an auto-save occurs while fighting the boss at `<25%` HP, upon loading the game:
  `boss->rageTriggered` defaults to `false` and `boss->ragePhase` defaults to `0`. As a result, the boss re-triggers the rage alarm sound and dash sequence anew upon every resume.
* **Suggested Fix:** Add `boss->ragePhase`, `boss->rageTimer`, and `boss->rageTriggered` to the `SaveGame` and `LoadGame` routines.

---

### 1.4 Random Shooter Selection Loop Risk When Few Enemies Remain
* **Location:** Lines 1733–1740
* **Code in Question:**
  ```c
  int attempts = 0;
  int randRow, randCol;
  do
  {
      randCol = GetRandomValue(0, ENEMY_COLS - 1);
      randRow = GetRandomValue(0, numRows - 1);
      attempts++;
  } while ((enemies[randRow][randCol].type == ENEMY_DEAD ||
            enemies[randRow][randCol].type == ENEMY_DUMMY) &&
           attempts < 100);
  ```
* **Analysis:**
  When only 1 or 2 living enemies remain on the board (e.g. 1 out of 44 grid cells, a ~2.2% chance per random pick), 100 random attempts have a `(1 - 0.022)^100 ≈ 10.7%` probability of missing the living enemy entirely. When this happens, no bullet is fired during that interval.
* **Suggested Fix:** If `attempts >= 100`, fall back to a linear scan to select the first or a random living enemy rather than skipping the shot.

---

### 1.5 Unchecked `fscanf` Error Handling in `LoadGame`
* **Location:** Lines 731–823
* **Analysis:**
  While `LoadGame` checks the initial file header:
  `if (fscanf(f, "%15s %7s", hdr, ver) != 2 || strcmp(hdr, "SAVEGAME") != 0)`
  all subsequent `fscanf` calls throughout the function discard their return values. If `savegame.txt` is truncated or corrupted, the game will read partial data without notification, leaving entities in an undefined state.

---

### 1.6 Player Bullet Centering Magic Number
* **Location:** Line 1579
* **Code in Question:**
  ```c
  playerBullets[i].position.x = player.position.x + (player.width / 2) - 3;
  ```
* **Analysis:**
  `BULLET_WIDTH` is defined as `5`. To perfectly center a 5px bullet on a 120px ship:
  `(player.width - BULLET_WIDTH) / 2.0f = (120 - 5) / 2.0f = 57.5f`.
  The expression `(player.width / 2) - 3` gives `60 - 3 = 57.0f`, leaving a hardcoded magic number `- 3`.

---

## 2. Redundant & Dead Code

| # | Item | Location | Description |
|---|---|---|---|
| **1** | **Duplicate Boss Win Block** | Lines 2163–2168 | Exact duplicate of win logic already executed at lines 2080–2090. Causes double scoring bug. |
| **2** | **Redundant Software Mouse Clamp** | Lines 1530–1540 | `SetMousePosition()` clamp loop inside gameplay block is redundant because Win32 hardware `ClipCursor` already clips the mouse to window client coordinates at lines 1070–1075. |
| **3** | **Duplicated Scroll Max Constants** | Lines 1323 & 2618 (`HOW_TO_PLAY`), Lines 1401 & 2763 (`CREDITS`) | `float maxScroll = 320.0f;` and `float maxScroll = 340.0f;` are redeclared in both update and draw blocks. Should be defined once as constants. |
| **4** | **Duplicated Back Arrow Geometry** | Lines 1314, 1388, 1466 (update) & 2485, 2555, 2648 (draw) | `(Rectangle){20, 20, 50, 50}` is duplicated 6 times across 3 menu states. |
| **5** | **Level 2 Row Count Mismatch** | Line 87, 336 vs 337 | Comments say `Level 2: 7 rows`, but `LevelConfig` specifies `.numRows = 6` with 6 entries in `.rowTypes`. |
| **6** | **Unused Menu Slots** | Lines 1027–1037 | `levelMenu` and `pauseMenu` define a 5th item `""` with `.enabled = false`, but `.count = 4` already bounds all loops, making the 5th item unused dead memory. |
| **7** | **Dual Mouse Initialization Flags** | Lines 596 vs 1040–1041 & 1150–1162 | `UpdateMenu` maintains its own `static Vector2 lastMouse`, while `main()` also tracks `mainMenuMouseInit` and `mainMenuLastMouse`. |

---

## 3. Unnecessary, Informal, & Stale Comments

The codebase contains several comments that are either informal developer scratchpad notes or refer to historical code changes that no longer exist:

### 3.1 Informal Sound Cue Comments
These comments use arrows and onomatopoeia rather than professional descriptions:
* **Line 1583:** `PlaySound(sndShoot); // ← laser pew`
* **Line 1798:** `PlaySound(sndEnemyDie); // ← explosion pop`
* **Line 1869:** `PlaySound(sndBossRage); // ← dramatic alarm`
* **Line 2078:** `PlaySound(sndBossHit); // ← deep thud`

*Recommendation:* Replace with standard comments like `// Trigger player shoot SFX` or remove them.

### 3.2 Stale Historical Implementation Notes
* **Line 459:** `// (GetRowEnemyType removed — row types are now defined in LevelConfig.rowTypes[])`  
  *Context:* Stale note referencing an old helper function that was refactored out.
* **Line 1872:** `// --- Rage state machine (no visual text/border) ---`  
  *Context:* Historical artifact from when UI text and borders were stripped from rage mode.
* **Line 2092:** `// No explosion circle on boss hit (misaligned with sprite) — particles handle the effect`  
  *Context:* Developer note explaining why a visual effect was deliberately excluded.
* **Line 2171:** `// (Win/Lose/Pause/Leaderboard/NameEntry handled above in the main else-if chain)`  
  *Context:* Explanatory divider note explaining loop structure.

### 3.3 Documentation Contradiction
* **Line 87:** `#define MAX_ENEMY_ROWS 8 // max rows any level can have (sized for level 2's 7 rows)`
* **Line 336:** `// Level 2: 7 rows, all 5 enemy types, 1.4x speed`  
  *Context:* Contradicts line 337 (`.numRows = 6`). Level 2 actually has 6 rows.

---

## 4. Architectural & Performance Observations

1. **CPU Alpha Border Trimming at Startup:**
   `LoadTrimmedSprite()` scans pixel alpha values on the CPU for 17 sprites. While this ensures perfect pixel-accurate hitboxes, it creates a ~2.5s loading time at startup. (This was properly mitigated by the delta-time clamping fix, but for long-term scalability, sprite bounding boxes could be precomputed as constant `Rectangle` data).
2. **String Scans in Leaderboard Draw Loop:**
   Lines 2521–2529 use a `while (MeasureText(...) > maxNameWidth && strlen(displayName) > 0)` loop calling `strlen()` multiple times per iteration. While harmless for names capped at 16 characters, storing length in a local variable is standard C practice.
3. **Clean Audio & Texture Teardown:**
   Lines 2901–2920 properly and exhaustively unload all textures, sounds, and music streams in the reverse order of initialization, preventing any GPU or audio memory leaks upon normal game termination.

---

## 5. Priority Action Items (For Future Updates)

| Priority | Item | Impact | Effort |
|:---:|---|---|:---:|
| **HIGH** | Remove duplicate boss win check (lines 2163–2168) | Fixes +200 double scoring bug on boss kill | 1 min |
| **HIGH** | Sanitize player name input (reject or replace spaces) | Prevents leaderboard data corruption | 2 mins |
| **MEDIUM** | Serialize boss rage state in `SaveGame` / `LoadGame` | Prevents boss rage re-triggering upon load | 5 mins |
| **LOW** | Consolidate `maxScroll` and back button rect constants | Eliminates magic number redundancy | 3 mins |
| **LOW** | Clean up informal and stale comments | Improves codebase readability and professionalism | 3 mins |
