#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Window settings
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 800

// Starfield settings
#define STAR_COUNT_FAR 150
#define STAR_COUNT_MID 80
#define STAR_COUNT_NEAR 30
#define STAR_TOTAL (STAR_COUNT_FAR + STAR_COUNT_MID + STAR_COUNT_NEAR)
#define STAR_SPEED_FAR 30.0f
#define STAR_SPEED_MID 70.0f
#define STAR_SPEED_NEAR 160.0f

// Player settings
#define PLAYER_START_X (WINDOW_WIDTH / 2 - 60) // Centered horizontally
#define PLAYER_Y (WINDOW_HEIGHT - 100)         // Positioned near the bottom
#define PLAYER_SPEED 300.0f
#define PLAYER_WIDTH 120
#define PLAYER_HEIGHT 60
#define PLAYER_MAX_X (WINDOW_WIDTH - PLAYER_WIDTH)
#define PLAYER_LIVES 3
#define PLAYER_INVINCIBLE_TIME 1.5f
#define SHAKE_DURATION 0.3f

// Player bullet settings
#define MAX_PLAYER_BULLETS 25
#define BULLET_SPEED 1000.0f
#define BULLET_WIDTH 5
#define BULLET_HEIGHT 15
#define PLAYER_SHOOT_COOLDOWN 0.2f

// Enemy matrix layout
#define MAX_ENEMY_ROWS 8 // max rows any level can have (sized for level 2's 7 rows)
#define ENEMY_COLS 11
#define ENEMY_SPACING_X 55 // horizontal spacing (50px enemy + 5px gap)
#define ENEMY_SPACING_Y 60 // vertical spacing between rows
#define ENEMY_HITBOX 50    // hitbox matches visual size
#define ENEMY_START_X 90   // left edge of formation
#define ENEMY_START_Y 100  // top edge of formation
#define ENEMY_BOUND_LEFT 10
#define ENEMY_BOUND_RIGHT (WINDOW_WIDTH - ENEMY_HITBOX - 10)
#define ENEMY_DROP_STEP 20.0f
#define ENEMY_DROP_INTERVAL 8.0f // seconds between Y-drops

// Level system
#define NUM_LEVELS 3

// Zigzag settings
#define ZIGZAG_PERIOD 2.0f     // seconds for one full triangle wave cycle
#define ZIGZAG_AMPLITUDE 15.0f // vertical oscillation in pixels (peak to center)

// Enemy bullet settings
#define MAX_ENEMY_BULLETS 10
#define ENEMY_BULLET_SPEED 300.0f
#define ENEMY_BULLET_WIDTH 5
#define ENEMY_BULLET_HEIGHT 15
#define ENEMY_SHOOT_COOLDOWN 0.5f

// Explosion effect settings
#define EXPLOSION_GROW_RATE 6.0f
#define EXPLOSION_MAX_TIME 1.0f
#define EXPLOSION_SCALE 12.0f
#define EXPLOSION_OFFSET 15

// Particle effect settings
#define MAX_PARTICLES 100
#define PARTICLE_LIFETIME 0.5f

// Boss settings
#define BOSS_WIDTH 300
#define BOSS_HEIGHT 300
#define BOSS_MAX_HEALTH 50
#define BOSS_SPEED 60.0f
#define BOSS_START_Y 60
#define MAX_BOSS_BULLETS 150
#define BOSS_RAPID_SPEED 350.0f
#define BOSS_STAR_SPEED 150.0f
#define BOSS_CIRCLE_SPEED 120.0f
#define BOSS_RAPID_COOLDOWN 0.2f
#define BOSS_STAR_COOLDOWN 0.6f
#define BOSS_CIRCLE_COOLDOWN 1.5f
#define BOSS_PHASE_ATTACK 3.0f  // seconds per attack phase
#define BOSS_PHASE_PAUSE 1.5f   // seconds pause between attacks
#define BOSS_MINION_WAVE1_HP 35 // spawn BASIC minions at this HP
#define BOSS_MINION_WAVE2_HP 20 // spawn RAPID minions at this HP
#define BOSS_MINION_COLS 5      // enemies per minion wave
// Bullet fire-point: center-X, ~60% down the sprite (crab body/mouth)
#define BOSS_BULLET_Y_RATIO   0.60f
// Rage mode: triggered at 25% HP — boss rushes to mid-screen, not below
#define BOSS_RAGE_HP_THRESHOLD (BOSS_MAX_HEALTH / 4)
#define BOSS_RAGE_SPEED_MUL   2.5f
#define BOSS_RAGE_DURATION    3.0f
// Y target = mid-screen, ensuring boss stays in upper half
#define BOSS_RAGE_TARGET_Y    (WINDOW_HEIGHT / 2.0f - BOSS_HEIGHT / 2.0f)
// Hitbox trim: crab body is in upper ~55% of sprite; 18% horizontal margin each side
// Keeps collision aligned with the visible shell, not the transparent leg/claw area
#define BOSS_HIT_X_MARGIN  (BOSS_WIDTH  * 0.18f)  // trim from each side
#define BOSS_HIT_Y_OFFSET  (BOSS_HEIGHT * 0.08f)  // small top gap (antenna space)
#define BOSS_HIT_WIDTH     (BOSS_WIDTH  - BOSS_HIT_X_MARGIN * 2)
#define BOSS_HIT_HEIGHT    (BOSS_HEIGHT * 0.50f)   // only the solid body, no legs

// Score values per enemy type
#define SCORE_DUMMY 5
#define SCORE_BASIC 10
#define SCORE_ZIGZAG 15
#define SCORE_RAPID 20
#define SCORE_TANK 30

//  Structs

typedef struct Star
{
    float x, y;
    float speed;
    float size;
    unsigned char brightness;
} Star;

typedef struct Player
{
    Vector2 position;
    float speed;
    int width;
    int height;
    int lives;
    float invincibleTimer;
} Player;

typedef struct Bullet
{
    Vector2 position;
    float speed;
    bool active;
} Bullet;

typedef enum EnemyType
{
    ENEMY_DEAD = 0,
    ENEMY_DUMMY,
    ENEMY_BASIC,
    ENEMY_ZIGZAG,
    ENEMY_TANK,
    ENEMY_RAPID,
} EnemyType;

typedef struct Enemy
{
    EnemyType type;
    float x, y;          // World position (absolute)
    float baseY;         // Base Y position — zigzag oscillates around this, drops modify this
    float speed;         // Movement speed
    float direction;     // +1.0 = moving right, -1.0 = moving left
    float moveTimer;     // Elapsed time — used for triangle wave (zigzag)
    float shootTimer;    // Time until next shot
    float shootCooldown; // How often this enemy fires
    int health;
    int maxHealth;
    int hitFlashFrames; // >0 means draw with RED tint
    bool active;
} Enemy;

typedef enum GameState
{
    LOADING,      // startup loading bar (2.5 s)
    MAIN_MENU,    // New Game / Resume / Leaderboard / Exit
    LEVEL_SELECT, // pick level 1 / 2 / 3
    PLAYING,
    PAUSED, // in-game pause menu
    GAME_WON,
    GAME_LOST,
    BOSS_FIGHT,
    LEADERBOARD, // view top-5 scores
    NAME_ENTRY,  // type name after new high score
} GameState;

typedef struct Explosion
{
    Vector2 position;
    float timer;
    bool active;
} Explosion;

typedef struct Particle
{
    float x, y;
    float vx, vy;
    float size;
    float life;
    Color color;
    bool active;
} Particle;

// Directional bullet for boss attacks (arbitrary vx,vy instead of just speed)
typedef struct BossBullet
{
    float x, y;
    float vx, vy; // velocity components — allows any direction
    bool active;
} BossBullet;

typedef struct Boss
{
    float x, y;      // top-left of hitbox
    float speed;     // horizontal movement speed
    float direction; // +1.0 right, -1.0 left
    float yDir;      // +1.0 down, -1.0 up (gentle Y drift)
    float yTimer;    // elapsed time for Y bounce period
    int health;
    int maxHealth;
    int hitFlashFrames;   // >0 = draw RED tint
    int attackPhase;      // 0=rapid,1=pause,2=star,3=pause,4=circle,5=pause (then loops)
    float phaseTimer;     // time spent in current phase
    float shootTimer;     // cooldown within current attack phase
    bool minionsSpawned1; // true after wave 1 (BASIC) spawned
    bool minionsSpawned2; // true after wave 2 (RAPID) spawned
    bool active;
    // Rage mode
    int   ragePhase;     // 0=normal,1=dash-to-mid,2=raging,3=returning
    float rageTimer;     // time spent at mid-screen during rage
    bool  rageTriggered; // one-shot flag
} Boss;

// Loading screen
#define LOAD_DURATION 2.5f

// Menu system (supports up to 5 items)
#define MAX_MENU_ITEMS 5

typedef struct Menu
{
    const char *title;
    const char *items[MAX_MENU_ITEMS];
    int count;
    int selected;
    bool enabled[MAX_MENU_ITEMS]; // false = grayed-out, skipped by keyboard
} Menu;

// Leaderboard
#define MAX_LEADERBOARD 5
#define MAX_NAME_LEN 16

typedef struct LeaderEntry
{
    char name[MAX_NAME_LEN];
    int score;
    int level;
} LeaderEntry;

typedef struct LevelConfig
{
    int numRows;                        // active enemy rows for this level
    EnemyType rowTypes[MAX_ENEMY_ROWS]; // enemy type per row (top to bottom)
    float speedMultiplier;              // scales all enemy base speeds
} LevelConfig;

// Level definitions (index 0 = level 1, index 1 = level 2, index 2 = level 3)
static const LevelConfig levels[NUM_LEVELS] = {
    // Level 1: 4 rows, 3 enemy types, normal speed
    {
        .numRows = 4,
        .rowTypes = {ENEMY_TANK, ENEMY_RAPID, ENEMY_BASIC, ENEMY_BASIC},
        .speedMultiplier = 1.0f,
    },
    // Level 2: 7 rows, all 5 enemy types, 1.4x speed
    {
        .numRows = 6,
        .rowTypes = {ENEMY_TANK, ENEMY_RAPID, ENEMY_ZIGZAG, ENEMY_ZIGZAG,
                     ENEMY_BASIC, ENEMY_DUMMY},
        .speedMultiplier = 1.4f,
    },
    // Level 3: boss fight (no formation — handled by BOSS_FIGHT state)
    {
        .numRows = 0,
        .rowTypes = {0},
        .speedMultiplier = 1.0f,
    },
};

//  Score per enemy type

static int GetEnemyScore(int type)
{
    switch (type)
    {
    case ENEMY_DUMMY:
        return SCORE_DUMMY;
    case ENEMY_BASIC:
        return SCORE_BASIC;
    case ENEMY_ZIGZAG:
        return SCORE_ZIGZAG;
    case ENEMY_RAPID:
        return SCORE_RAPID;
    case ENEMY_TANK:
        return SCORE_TANK;
    default:
        return 0;
    }
}

// Reset single enemy into a fresh state — positions computed from row/col
// speedMul scales the base speed for the current level (1.0 = normal, 1.4 = level 2, etc.)
static void InitEnemy(Enemy *e, int row, int col, int type, float speedMul)
{
    e->type = type;
    e->x = ENEMY_START_X + col * ENEMY_SPACING_X;
    e->y = ENEMY_START_Y + row * ENEMY_SPACING_Y;
    e->baseY = e->y;
    e->direction = 1.0f; // all start moving right (row-coherent)
    e->moveTimer = 0;
    e->shootTimer = (float)GetRandomValue(0, 200) / 100.0f; // stagger initial shots
    e->hitFlashFrames = 0;
    e->active = true;
    switch (type)
    {
    case ENEMY_DUMMY:
        e->speed = 20 * speedMul;
        e->health = 2;             // spec: health 2
        e->shootCooldown = 999.0f; // dummy doesn't fire
        e->maxHealth = e->health;
        break;
    case ENEMY_BASIC:
        e->speed = 45 * speedMul;
        e->health = 1;
        e->shootCooldown = 1.5f;
        e->maxHealth = e->health;
        break;
    case ENEMY_ZIGZAG:
        e->speed = 60 * speedMul;
        e->health = 1; // spec: health 1
        e->shootCooldown = 1.0f;
        e->maxHealth = e->health;
        break;
    case ENEMY_TANK:
        e->speed = 30 * speedMul;
        e->health = 3; // spec: health 3
        e->shootCooldown = 3.0f;
        e->maxHealth = e->health;
        break;
    case ENEMY_RAPID:
        e->speed = 100 * speedMul;
        e->health = 1;
        e->shootCooldown = 0.5f;
        e->maxHealth = e->health;
        break;
    case ENEMY_DEAD:
        e->speed = 0;
        e->active = false;
        break;
    }
}

// Initialize/reset the full enemy matrix for a given level.
// Also clears rows beyond the level's numRows to prevent stale data.
static void ResetLevel(Enemy enemies[][ENEMY_COLS], int *enemyCount, int *numRows,
                       const LevelConfig *level, Bullet playerBullets[],
                       Bullet enemyBullets[], Player *player,
                       float *enemyShootTimer, float *dropTimer, int *score)
{
    *numRows = level->numRows;
    *enemyCount = level->numRows * ENEMY_COLS;
    *enemyShootTimer = 0.0f;
    *dropTimer = 0.0f;
    *score = 0;
    player->lives = PLAYER_LIVES;
    player->invincibleTimer = 0.0f;
    player->position.x = PLAYER_START_X;

    // Init active rows from level config
    for (int i = 0; i < level->numRows; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            InitEnemy(&enemies[i][j], i, j, level->rowTypes[i], level->speedMultiplier);

    // Clear any rows beyond this level's numRows (prevents stale data from a larger level)
    for (int i = level->numRows; i < MAX_ENEMY_ROWS; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            enemies[i][j].type = ENEMY_DEAD;

    // Reset all bullets
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        playerBullets[i].active = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        enemyBullets[i].active = false;
}

// (GetRowEnemyType removed — row types are now defined in LevelConfig.rowTypes[])

// Initialise/reset the boss for a fresh fight
static void ResetBoss(Boss *b)
{
    b->x = WINDOW_WIDTH / 2.0f - BOSS_WIDTH / 2.0f;
    b->y = BOSS_START_Y;
    b->speed = BOSS_SPEED;
    b->direction = 1.0f;
    b->health = BOSS_MAX_HEALTH;
    b->maxHealth = BOSS_MAX_HEALTH;
    b->hitFlashFrames = 0;
    b->attackPhase = 0;
    b->phaseTimer = 0.0f;
    b->shootTimer = 0.0f;
    b->minionsSpawned1 = false;
    b->minionsSpawned2 = false;
    b->active = true;
    b->ragePhase = 0;
    b->rageTimer = 0.0f;
    b->rageTriggered = false;
}

static void SpawnParticles(Particle particles[], float px, float py, int count)
{
    Color particleColors[4] = {RED, ORANGE, YELLOW, WHITE};
    int spawned = 0;

    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++)
    {
        if (!particles[i].active)
        {
            particles[i].active = true;
            particles[i].x = px;
            particles[i].y = py;
            particles[i].vx = (float)GetRandomValue(-150, 150);
            particles[i].vy = (float)GetRandomValue(-200, 50);
            particles[i].size = (float)GetRandomValue(3, 8);
            particles[i].life = PARTICLE_LIFETIME;
            particles[i].color = particleColors[GetRandomValue(0, 3)];
            spawned++;
        }
    }
}

// --- Leaderboard helpers ---

static void LoadLeaderboard(const char *path, LeaderEntry lb[], int *count)
{
    *count = 0;
    FILE *f = fopen(path, "r");
    if (!f)
    {
        // First run: seed with 5 default 'tamim' entries
        int defScores[MAX_LEADERBOARD] = {500, 400, 300, 200, 100};
        int defLevels[MAX_LEADERBOARD] = {3, 2, 2, 1, 1};
        for (int i = 0; i < MAX_LEADERBOARD; i++)
        {
            strncpy(lb[i].name, "tamim", MAX_NAME_LEN - 1);
            lb[i].name[MAX_NAME_LEN - 1] = '\0';
            lb[i].score = defScores[i];
            lb[i].level = defLevels[i];
        }
        *count = MAX_LEADERBOARD;
        return;
    }
    while (*count < MAX_LEADERBOARD)
    {
        if (fscanf(f, "%15s %d %d",
                   lb[*count].name, &lb[*count].score, &lb[*count].level) != 3)
            break;
        (*count)++;
    }
    fclose(f);
}

static void SaveLeaderboard(const char *path, const LeaderEntry lb[], int count)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    for (int i = 0; i < count; i++)
        fprintf(f, "%s %d %d\n", lb[i].name, lb[i].score, lb[i].level);
    fclose(f);
}

static bool IsHighScore(const LeaderEntry lb[], int count, int score)
{
    if (count < MAX_LEADERBOARD)
        return true;
    return score > lb[count - 1].score;
}

static void InsertScore(LeaderEntry lb[], int *count,
                        const char *name, int score, int level)
{
    int pos = *count;
    for (int i = 0; i < *count; i++)
        if (score > lb[i].score)
        {
            pos = i;
            break;
        }
    int newCount = (*count < MAX_LEADERBOARD) ? *count + 1 : MAX_LEADERBOARD;
    for (int i = newCount - 1; i > pos; i--)
        lb[i] = lb[i - 1];
    strncpy(lb[pos].name, name, MAX_NAME_LEN - 1);
    lb[pos].name[MAX_NAME_LEN - 1] = '\0';
    lb[pos].score = score;
    lb[pos].level = level;
    *count = newCount;
}

// --- Menu helpers (arrow up/down, mouse hover, ENTER/click confirm) ---

static int UpdateMenu(Menu *menu)
{
    if (IsKeyPressed(KEY_UP))
    {
        int prev = menu->selected;
        do
        {
            menu->selected = (menu->selected - 1 + menu->count) % menu->count;
        } while (!menu->enabled[menu->selected] && menu->selected != prev);
    }
    if (IsKeyPressed(KEY_DOWN))
    {
        int prev = menu->selected;
        do
        {
            menu->selected = (menu->selected + 1) % menu->count;
        } while (!menu->enabled[menu->selected] && menu->selected != prev);
    }
    // Mouse hover — only update highlight if the mouse has actually moved
    // (prevents stationary cursor from overriding keyboard selection every frame)
    static Vector2 lastMouse = {-9999.0f, -9999.0f};
    Vector2 mouse = GetMousePosition();
    if (mouse.x != lastMouse.x || mouse.y != lastMouse.y)
    {
        lastMouse = mouse;
        for (int i = 0; i < menu->count; i++)
        {
            Rectangle r = {(float)(WINDOW_WIDTH / 2 - 200),
                           (float)(320 + i * 55 - 5), 400.0f, 44.0f};
            if (CheckCollisionPointRec(mouse, r) && menu->enabled[i])
                menu->selected = i;
        }
    }
    // Keyboard confirm
    if (IsKeyPressed(KEY_ENTER) && menu->enabled[menu->selected])
        return menu->selected;
    // Mouse click confirm — only when cursor is over the highlighted item
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Rectangle selR = {(float)(WINDOW_WIDTH / 2 - 200),
                          (float)(320 + menu->selected * 55 - 5), 400.0f, 44.0f};
        if (CheckCollisionPointRec(mouse, selR) && menu->enabled[menu->selected])
            return menu->selected;
    }
    return -1;
}

static void DrawMenu(const Menu *menu)
{
    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 215});
    int titleW = MeasureText(menu->title, 46);
    DrawText(menu->title, WINDOW_WIDTH / 2 - titleW / 2, 190, 46, WHITE);
    for (int i = 0; i < menu->count; i++)
    {
        int y = 320 + i * 55;
        Color c = !menu->enabled[i]
                      ? DARKGRAY
                      : (i == menu->selected ? YELLOW : WHITE);
        if (i == menu->selected && menu->enabled[i])
            DrawRectangle(WINDOW_WIDTH / 2 - 200, y - 5, 400, 44,
                          (Color){255, 255, 255, 25});
        int tw = MeasureText(menu->items[i], 28);
        DrawText(menu->items[i], WINDOW_WIDTH / 2 - tw / 2, y, 28, c);
    }
}

// --- Save / Load full game state ---

static void SaveGame(const char *path, int stateVal, int level, int score,
                     const Player *player, float shootCd,
                     const Enemy enemies[][ENEMY_COLS], int numRows, int enemyCount,
                     float enemyShootTimer, float dropTimer,
                     const Bullet pBullets[], const Bullet eBullets[],
                     const Boss *boss, const BossBullet bBullets[])
{
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "SAVEGAME v1\n");
    fprintf(f, "state %d\n", stateVal);
    fprintf(f, "level %d\n", level);
    fprintf(f, "score %d\n", score);
    fprintf(f, "px %.3f\n", player->position.x);
    fprintf(f, "plives %d\n", player->lives);
    fprintf(f, "pcd %.5f\n", shootCd);
    fprintf(f, "numRows %d\n", numRows);
    fprintf(f, "eCount %d\n", enemyCount);
    fprintf(f, "est %.5f\n", enemyShootTimer);
    fprintf(f, "dt %.5f\n", dropTimer);
    for (int i = 0; i < MAX_ENEMY_ROWS; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            fprintf(f, "E %d %d %d %.3f %.3f %.3f %.3f %.3f %.5f %.5f %.5f %d %d %d %d\n",
                    i, j, (int)enemies[i][j].type,
                    enemies[i][j].x, enemies[i][j].y, enemies[i][j].baseY,
                    enemies[i][j].speed, enemies[i][j].direction,
                    enemies[i][j].moveTimer, enemies[i][j].shootTimer,
                    enemies[i][j].shootCooldown, enemies[i][j].health,
                    enemies[i][j].maxHealth, enemies[i][j].hitFlashFrames,
                    enemies[i][j].active ? 1 : 0);
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        fprintf(f, "PB %d %d %.3f %.3f %.3f\n",
                i, pBullets[i].active ? 1 : 0,
                pBullets[i].position.x, pBullets[i].position.y, pBullets[i].speed);
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        fprintf(f, "EB %d %d %.3f %.3f %.3f\n",
                i, eBullets[i].active ? 1 : 0,
                eBullets[i].position.x, eBullets[i].position.y, eBullets[i].speed);
    fprintf(f, "BA %d\n", boss->active ? 1 : 0);
    fprintf(f, "BH %d\n", boss->health);
    fprintf(f, "BX %.3f\n", boss->x);
    fprintf(f, "BY %.3f\n", boss->y);
    fprintf(f, "BD %.5f\n", boss->direction);
    fprintf(f, "BYT %.5f\n", boss->yTimer);
    fprintf(f, "BPT %.5f\n", boss->phaseTimer);
    fprintf(f, "BST %.5f\n", boss->shootTimer);
    fprintf(f, "BP %d\n", boss->attackPhase);
    fprintf(f, "BM1 %d\n", boss->minionsSpawned1 ? 1 : 0);
    fprintf(f, "BM2 %d\n", boss->minionsSpawned2 ? 1 : 0);
    for (int i = 0; i < MAX_BOSS_BULLETS; i++)
        fprintf(f, "BB %d %d %.3f %.3f %.3f %.3f\n",
                i, bBullets[i].active ? 1 : 0,
                bBullets[i].x, bBullets[i].y, bBullets[i].vx, bBullets[i].vy);
    fprintf(f, "END\n");
    fclose(f);
}

static bool LoadGame(const char *path, int *stateVal, int *level, int *score,
                     Player *player, float *shootCd,
                     Enemy enemies[][ENEMY_COLS], int *numRows, int *enemyCount,
                     float *enemyShootTimer, float *dropTimer,
                     Bullet pBullets[], Bullet eBullets[],
                     Boss *boss, BossBullet bBullets[])
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    char hdr[16], ver[8];
    if (fscanf(f, "%15s %7s", hdr, ver) != 2 || strcmp(hdr, "SAVEGAME") != 0)
    {
        fclose(f);
        return false;
    }

    // Clear arrays before loading
    for (int i = 0; i < MAX_ENEMY_ROWS; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            enemies[i][j].type = ENEMY_DEAD;
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        pBullets[i].active = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        eBullets[i].active = false;
    for (int i = 0; i < MAX_BOSS_BULLETS; i++)
        bBullets[i].active = false;

    char k[8];
    fscanf(f, "%7s %d", k, stateVal);
    fscanf(f, "%7s %d", k, level);
    fscanf(f, "%7s %d", k, score);
    fscanf(f, "%7s %f", k, &player->position.x);
    fscanf(f, "%7s %d", k, &player->lives);
    fscanf(f, "%7s %f", k, shootCd);
    fscanf(f, "%7s %d", k, numRows);
    fscanf(f, "%7s %d", k, enemyCount);
    fscanf(f, "%7s %f", k, enemyShootTimer);
    fscanf(f, "%7s %f", k, dropTimer);

    for (int i = 0; i < MAX_ENEMY_ROWS; i++)
    {
        for (int j = 0; j < ENEMY_COLS; j++)
        {
            int ri, rj, type, health, maxH, hflash, act;
            float x, y, bY, spd, dir, mt, st, sc;
            fscanf(f, "%7s %d %d %d %f %f %f %f %f %f %f %f %d %d %d %d",
                   k, &ri, &rj, &type, &x, &y, &bY, &spd, &dir, &mt, &st, &sc,
                   &health, &maxH, &hflash, &act);
            enemies[i][j].type = (EnemyType)type;
            enemies[i][j].x = x;
            enemies[i][j].y = y;
            enemies[i][j].baseY = bY;
            enemies[i][j].speed = spd;
            enemies[i][j].direction = dir;
            enemies[i][j].moveTimer = mt;
            enemies[i][j].shootTimer = st;
            enemies[i][j].shootCooldown = sc;
            enemies[i][j].health = health;
            enemies[i][j].maxHealth = maxH;
            enemies[i][j].hitFlashFrames = hflash;
            enemies[i][j].active = act ? true : false;
        }
    }
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {
        int idx, act;
        float x, y, spd;
        fscanf(f, "%7s %d %d %f %f %f", k, &idx, &act, &x, &y, &spd);
        pBullets[i].active = act ? true : false;
        pBullets[i].position.x = x;
        pBullets[i].position.y = y;
        pBullets[i].speed = spd;
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
        int idx, act;
        float x, y, spd;
        fscanf(f, "%7s %d %d %f %f %f", k, &idx, &act, &x, &y, &spd);
        eBullets[i].active = act ? true : false;
        eBullets[i].position.x = x;
        eBullets[i].position.y = y;
        eBullets[i].speed = spd;
    }
    int bAct, bH, bPh, bM1, bM2;
    float bX, bY2, bD, bYT, bPT, bST;
    fscanf(f, "%7s %d", k, &bAct);
    fscanf(f, "%7s %d", k, &bH);
    fscanf(f, "%7s %f", k, &bX);
    fscanf(f, "%7s %f", k, &bY2);
    fscanf(f, "%7s %f", k, &bD);
    fscanf(f, "%7s %f", k, &bYT);
    fscanf(f, "%7s %f", k, &bPT);
    fscanf(f, "%7s %f", k, &bST);
    fscanf(f, "%7s %d", k, &bPh);
    fscanf(f, "%7s %d", k, &bM1);
    fscanf(f, "%7s %d", k, &bM2);
    boss->active = bAct ? true : false;
    boss->health = bH;
    boss->maxHealth = BOSS_MAX_HEALTH;
    boss->x = bX;
    boss->y = bY2;
    boss->direction = bD;
    boss->speed = BOSS_SPEED;
    boss->yTimer = bYT;
    boss->phaseTimer = bPT;
    boss->shootTimer = bST;
    boss->attackPhase = bPh;
    boss->minionsSpawned1 = bM1 ? true : false;
    boss->minionsSpawned2 = bM2 ? true : false;
    boss->hitFlashFrames = 0;
    for (int i = 0; i < MAX_BOSS_BULLETS; i++)
    {
        int idx, act;
        float x, y, vx, vy;
        fscanf(f, "%7s %d %d %f %f %f %f", k, &idx, &act, &x, &y, &vx, &vy);
        bBullets[i].active = act ? true : false;
        bBullets[i].x = x;
        bBullets[i].y = y;
        bBullets[i].vx = vx;
        bBullets[i].vy = vy;
    }
    fclose(f);
    return true;
}

//  Main

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Game");
    SetTargetFPS(60);
    HideCursor(); // player ship acts as the cursor
    InitAudioDevice();

    // --- Starfield initialization: 3 parallax layers ---
    Star stars[STAR_TOTAL];
    // Far layer: many tiny dim stars (deepest background)
    for (int i = 0; i < STAR_COUNT_FAR; i++)
    {
        stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
        stars[i].y = (float)GetRandomValue(0, WINDOW_HEIGHT);
        stars[i].speed = STAR_SPEED_FAR;
        stars[i].size = GetRandomValue(1, 2) * 0.5f;
        stars[i].brightness = (unsigned char)GetRandomValue(80, 140);
    }
    // Mid layer: medium-brightness stars
    for (int i = STAR_COUNT_FAR; i < STAR_COUNT_FAR + STAR_COUNT_MID; i++)
    {
        stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
        stars[i].y = (float)GetRandomValue(0, WINDOW_HEIGHT);
        stars[i].speed = STAR_SPEED_MID;
        stars[i].size = GetRandomValue(2, 3) * 0.5f;
        stars[i].brightness = (unsigned char)GetRandomValue(140, 200);
    }
    // Near layer: fewer bright fast stars (closest to camera)
    for (int i = STAR_COUNT_FAR + STAR_COUNT_MID; i < STAR_TOTAL; i++)
    {
        stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
        stars[i].y = (float)GetRandomValue(0, WINDOW_HEIGHT);
        stars[i].speed = STAR_SPEED_NEAR;
        stars[i].size = GetRandomValue(2, 4) * 0.5f;
        stars[i].brightness = (unsigned char)GetRandomValue(200, 255);
    }

    // Load all textures
    Texture2D spaceshipTex = LoadTexture("resources/spaceship.png");
    Texture2D dummy = LoadTexture("resources/dummy.png");     // ENEMY_DUMMY
    Texture2D basicTex = LoadTexture("resources/dummy.png");  // ENEMY_BASIC (same sprite, cyan tint)
    Texture2D zigzag = LoadTexture("resources/zigzag.png");   // ENEMY_ZIGZAG
    Texture2D rapidTex = LoadTexture("resources/zigzag.png"); // ENEMY_RAPID (same sprite, yellow tint)
    Texture2D tank = LoadTexture("resources/tank.png");       // ENEMY_TANK
    Texture2D heartTex = LoadTexture("resources/heart.png");
    Texture2D bossTex = LoadTexture("resources/boss.png");    // Level 3 boss sprite
    // Source rect covers the full boss texture; dest rect sizes it to BOSS_WIDTH x BOSS_HEIGHT
    Rectangle bossTexSrc = {0, 0, (float)bossTex.width, (float)bossTex.height};
    Rectangle bossTexDst = {0, 0, BOSS_WIDTH, BOSS_HEIGHT}; // x/y set each frame
    Vector2   bossTexOrigin = {0, 0};

    // Load sound effects
    Sound sndShoot     = LoadSound("resources/shoot.wav");      // player fires
    Sound sndEnemyDie  = LoadSound("resources/enemy_die.wav");  // enemy killed
    Sound sndBossHit   = LoadSound("resources/boss_hit.wav");   // boss takes damage
    Sound sndBossRage  = LoadSound("resources/boss_rage.wav");  // rage mode trigger
    Sound sndEnemyMove = LoadSound("resources/enemy_move.wav"); // enemy march beat
    // Enemy march beat timer
    float enemyMoveTimer   = 0.0f;
    float enemyMoveBeat    = 0.55f; // seconds between march blips (speeds up below)

    // Deep-space ambient background track (loops seamlessly)
    Music bgMusic = LoadMusicStream("resources/bg_space.wav");
    SetMusicVolume(bgMusic, 0.40f); // sits quietly under the SFX
    PlayMusicStream(bgMusic);

    // Set up the player
    Player player = {
        .position = {PLAYER_START_X, PLAYER_Y},
        .speed = PLAYER_SPEED,
        .width = PLAYER_WIDTH,
        .height = PLAYER_HEIGHT,
        .lives = PLAYER_LIVES,
        .invincibleTimer = 0.0f,
    };

    // Player bullets
    Bullet playerBullets[MAX_PLAYER_BULLETS] = {0};
    float shootCooldown = 0.0f;

    // Enemy matrix — sized for the largest possible level
    Enemy enemies[MAX_ENEMY_ROWS][ENEMY_COLS];
    int numRows = 0;      // set by ResetLevel() from the chosen level config
    int enemyCount = 0;   // set by ResetLevel()
    int currentLevel = 0; // index into levels[] (0-based)

    // Global timers
    float enemyShootTimer = 0.0f;
    float dropTimer = 0.0f;

    // Enemy bullets pool
    Bullet enemyBullets[MAX_ENEMY_BULLETS] = {0};

    // Boss and boss bullet pool
    Boss boss = {0};
    BossBullet bossBullets[MAX_BOSS_BULLETS] = {0};

    // Explosion state
    Explosion explosion = {.position = {0, 0}, .timer = 0, .active = false};

    // Particle state
    Particle particles[MAX_PARTICLES] = {0};

    // Screen shake state
    float shakeTimer = 0.0f;
    int shakeOffsetX = 0;
    int shakeOffsetY = 0;

    // Score, game state, and new system variables
    int score = 0;
    GameState state = LOADING; // always starts with loading screen
    float loadTimer = 0.0f;
    bool shouldExit = false;
    GameState returnState = MAIN_MENU; // where LEADERBOARD goes back to
    GameState pausedFrom = PLAYING;    // PLAYING or BOSS_FIGHT before pause
    float autoSaveTimer = 0.0f;        // periodic auto-save every 10 s during gameplay
#define AUTOSAVE_INTERVAL 10.0f

    // Leaderboard — load from file (or seed defaults on first run)
    LeaderEntry leaderboard[MAX_LEADERBOARD];
    int leaderCount = 0;
    LoadLeaderboard("leaderboard.txt", leaderboard, &leaderCount);

    // Name entry buffer
    char nameBuffer[MAX_NAME_LEN] = {0};
    int nameLen = 0;

    // Menu definitions
    Menu mainMenu = {
        .title = "SPACE INVADERS",
        .items = {"New Game", "Resume Game", "Leaderboard", "Exit", ""},
        .count = 4,
        .selected = 0,
        .enabled = {true, false, true, true, false}};
    Menu levelMenu = {
        .title = "SELECT LEVEL",
        .items = {"Level 1  -  Easy", "Level 2  -  Hard",
                  "Level 3  -  Boss", "Back", ""},
        .count = 4,
        .selected = 0,
        .enabled = {true, true, true, true, false}};
    Menu pauseMenu = {
        .title = "PAUSED",
        .items = {"Resume", "New Game", "Leaderboard", "Exit", ""},
        .count = 4,
        .selected = 0,
        .enabled = {true, true, true, true, false}};

    // Init enemy array to dead so nothing is drawn before a level is chosen
    for (int i = 0; i < MAX_ENEMY_ROWS; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            enemies[i][j].type = ENEMY_DEAD;

    // Rects for drawing the player ship texture
    Rectangle source = {0, 0, spaceshipTex.width, spaceshipTex.height};
    Rectangle destination = {player.position.x, player.position.y, player.width, player.height};
    Vector2 origin = {0, 0};

    // Game loop
    while (!WindowShouldClose() && !shouldExit)
    {
        float dt = GetFrameTime();

        // Pump the looping background music every frame (required by raylib)
        UpdateMusicStream(bgMusic);

        // --- Update starfield (always runs) ---
        for (int i = 0; i < STAR_TOTAL; i++)
        {
            stars[i].y += stars[i].speed * dt;
            if (stars[i].y > WINDOW_HEIGHT)
            {
                stars[i].y = 0;
                stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
            }
        }

        // --- Screen shake update ---
        if (player.invincibleTimer > 0.0f)
            player.invincibleTimer -= dt;

        if (shakeTimer > 0.0f)
        {
            shakeTimer -= dt;
            shakeOffsetX = GetRandomValue(-3, 3);
            shakeOffsetY = GetRandomValue(-3, 3);
        }
        else
        {
            shakeOffsetX = 0;
            shakeOffsetY = 0;
        }

        // --- Particle update ---
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (!particles[i].active)
                continue;
            particles[i].life -= dt;
            if (particles[i].life <= 0.0f)
            {
                particles[i].active = false;
                continue;
            }
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
            float ratio = particles[i].life / PARTICLE_LIFETIME;
            particles[i].size = particles[i].size * ratio + 0.5f;
        }

        // --- Loading screen ---
        if (state == LOADING)
        {
            loadTimer += dt;
            if (loadTimer >= LOAD_DURATION)
                state = MAIN_MENU;
        }

        // --- Main menu ---
        else if (state == MAIN_MENU)
        {
            // Refresh Resume availability every frame
            FILE *chk = fopen("savegame.txt", "r");
            mainMenu.enabled[1] = (chk != NULL);
            if (chk)
                fclose(chk);

            int mchoice = UpdateMenu(&mainMenu);
            if (mchoice == 0)
            {
                state = LEVEL_SELECT;
                levelMenu.selected = 0;
            }
            else if (mchoice == 1)
            {
                int ls = (int)PLAYING;
                if (LoadGame("savegame.txt", &ls, &currentLevel, &score,
                             &player, &shootCooldown, enemies, &numRows, &enemyCount,
                             &enemyShootTimer, &dropTimer, playerBullets, enemyBullets,
                             &boss, bossBullets))
                    state = (GameState)ls;
            }
            else if (mchoice == 2)
            {
                returnState = MAIN_MENU;
                state = LEADERBOARD;
            }
            else if (mchoice == 3)
                shouldExit = true;
        }

        // --- Level select ---
        else if (state == LEVEL_SELECT)
        {
            if (IsKeyPressed(KEY_ESCAPE))
            {
                state = MAIN_MENU;
                mainMenu.selected = 0;
            }
            int lchoice = UpdateMenu(&levelMenu);
            if (lchoice == 0)
            {
                currentLevel = 0;
                ResetLevel(enemies, &enemyCount, &numRows, &levels[0],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                state = PLAYING;
            }
            else if (lchoice == 1)
            {
                currentLevel = 1;
                ResetLevel(enemies, &enemyCount, &numRows, &levels[1],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                state = PLAYING;
            }
            else if (lchoice == 2)
            {
                currentLevel = 2;
                ResetLevel(enemies, &enemyCount, &numRows, &levels[2],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                    bossBullets[i].active = false;
                ResetBoss(&boss);
                state = BOSS_FIGHT;
            }
            else if (lchoice == 3)
            {
                state = MAIN_MENU;
                mainMenu.selected = 0;
            }
        }

        // --- Win/Lose handlers ---
        else if (state == GAME_WON && IsKeyPressed(KEY_ENTER))
        {
            remove("savegame.txt");
            if (IsHighScore(leaderboard, leaderCount, score))
            {
                nameBuffer[0] = '\0';
                nameLen = 0;
                state = NAME_ENTRY;
            }
            else
                state = MAIN_MENU;
        }
        else if (state == GAME_LOST && IsKeyPressed(KEY_ENTER))
        {
            remove("savegame.txt");
            state = MAIN_MENU;
        }

        // --- Pause menu (ESC/P = quick resume, or navigate with arrows) ---
        else if (state == PAUSED)
        {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P))
            {
                state = pausedFrom; // quick resume without menu
            }
            else
            {
                int pchoice = UpdateMenu(&pauseMenu);
                if (pchoice == 0)
                    state = pausedFrom;
                else if (pchoice == 1)
                {
                    state = LEVEL_SELECT;
                    levelMenu.selected = 0;
                }
                else if (pchoice == 2)
                {
                    returnState = PAUSED;
                    state = LEADERBOARD;
                }
                else if (pchoice == 3)
                    shouldExit = true;
            }
        }

        // --- Leaderboard view ---
        else if (state == LEADERBOARD)
        {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
                state = returnState;
        }

        // --- Name entry after new high score ---
        else if (state == NAME_ENTRY)
        {
            int ch = GetCharPressed();
            while (ch > 0)
            {
                if (nameLen < MAX_NAME_LEN - 1 && ch >= 32 && ch <= 125)
                {
                    nameBuffer[nameLen++] = (char)ch;
                    nameBuffer[nameLen] = '\0';
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && nameLen > 0)
                nameBuffer[--nameLen] = '\0';
            // Require at least 1 character before submitting
            if (IsKeyPressed(KEY_ENTER) && nameLen > 0)
            {
                InsertScore(leaderboard, &leaderCount,
                            nameBuffer, score, currentLevel + 1);
                SaveLeaderboard("leaderboard.txt", leaderboard, leaderCount);
                remove("savegame.txt");
                state = MAIN_MENU;
            }
        }

        // --- ESC or P pauses (auto-saves state to savegame.txt) ---
        else if ((state == PLAYING || state == BOSS_FIGHT) &&
                 (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)))
        {
            pausedFrom = state;
            SaveGame("savegame.txt", (int)state, currentLevel, score,
                     &player, shootCooldown, enemies, numRows, enemyCount,
                     enemyShootTimer, dropTimer, playerBullets, enemyBullets,
                     &boss, bossBullets);
            state = PAUSED;
            pauseMenu.selected = 0;
            autoSaveTimer = 0.0f; // reset autosave timer after manual save
        }

        // --- Active gameplay (PLAYING + BOSS_FIGHT share player/enemy logic) ---
        else if (state == PLAYING || state == BOSS_FIGHT)
        {
            // --- Periodic auto-save (every 10 s) ---
            autoSaveTimer += dt;
            if (autoSaveTimer >= AUTOSAVE_INTERVAL)
            {
                SaveGame("savegame.txt", (int)state, currentLevel, score,
                         &player, shootCooldown, enemies, numRows, enemyCount,
                         enemyShootTimer, dropTimer, playerBullets, enemyBullets,
                         &boss, bossBullets);
                autoSaveTimer = 0.0f;
            }

            // --- Player movement ---
            // Mouse: snap to cursor only when mouse has actually moved this frame.
            // When mouse is stationary, keyboard has full control (no conflict).
            {
                static int lastMouseX = -1;
                int curMouseX = GetMouseX();
                if (IsCursorOnScreen() && curMouseX != lastMouseX)
                {
                    player.position.x = (float)curMouseX - player.width / 2.0f;
                    lastMouseX = curMouseX;
                }
            }
            // Keyboard: left/right always apply on top of (or instead of) mouse
            if (IsKeyDown(KEY_LEFT))
                player.position.x -= player.speed * dt;
            if (IsKeyDown(KEY_RIGHT))
                player.position.x += player.speed * dt;

            // Clamp to screen bounds
            if (player.position.x < 0)
                player.position.x = 0;
            if (player.position.x > PLAYER_MAX_X)
                player.position.x = PLAYER_MAX_X;

            // Player shooting with cooldown
            shootCooldown -= dt;

            if ((IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) && shootCooldown <= 0.0f)
            {
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    if (!playerBullets[i].active)
                    {
                        playerBullets[i].active = true;
                        playerBullets[i].position.x = player.position.x + (player.width / 2) - 3;
                        playerBullets[i].position.y = PLAYER_Y;
                        playerBullets[i].speed = BULLET_SPEED;
                        shootCooldown = PLAYER_SHOOT_COOLDOWN;
                        PlaySound(sndShoot); // ← laser pew
                        break;
                    }
                }
            }

            // Move all player bullets upward
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (playerBullets[i].active)
                {
                    playerBullets[i].position.y -= playerBullets[i].speed * dt;
                    if (playerBullets[i].position.y < 0)
                        playerBullets[i].active = false;
                }
            }

            // --- Per-row enemy movement (row-coherent) ---
            for (int row = 0; row < numRows; row++)
            {
                // Find the speed and direction for this row from any alive enemy
                float rowSpeed = 0;
                float rowDir = 0;
                bool hasAlive = false;

                for (int col = 0; col < ENEMY_COLS; col++)
                {
                    if (enemies[row][col].type != ENEMY_DEAD)
                    {
                        rowSpeed = enemies[row][col].speed;
                        rowDir = enemies[row][col].direction;
                        hasAlive = true;
                        break;
                    }
                }

                if (!hasAlive)
                    continue;

                // Find leftmost and rightmost alive enemy X positions in this row
                float leftmostX = (float)WINDOW_WIDTH;
                float rightmostX = 0.0f;
                for (int col = 0; col < ENEMY_COLS; col++)
                {
                    if (enemies[row][col].type != ENEMY_DEAD)
                    {
                        if (enemies[row][col].x < leftmostX)
                            leftmostX = enemies[row][col].x;
                        if (enemies[row][col].x > rightmostX)
                            rightmostX = enemies[row][col].x;
                    }
                }

                // Check if row needs to reverse direction at screen boundaries
                float nextRight = rightmostX + rowSpeed * rowDir * dt;
                float nextLeft = leftmostX + rowSpeed * rowDir * dt;

                if (nextRight > ENEMY_BOUND_RIGHT && rowDir > 0)
                {
                    rowDir = -1.0f;
                }
                else if (nextLeft < ENEMY_BOUND_LEFT && rowDir < 0)
                {
                    rowDir = 1.0f;
                }

                // Move all alive enemies in this row
                for (int col = 0; col < ENEMY_COLS; col++)
                {
                    if (enemies[row][col].type != ENEMY_DEAD)
                    {
                        enemies[row][col].direction = rowDir;
                        enemies[row][col].x += rowSpeed * rowDir * dt;
                        enemies[row][col].moveTimer += dt;

                        // Decrement hit flash
                        if (enemies[row][col].hitFlashFrames > 0)
                            enemies[row][col].hitFlashFrames--;

                        // Zigzag: apply triangle wave Y offset relative to baseY
                        if (enemies[row][col].type == ENEMY_ZIGZAG)
                        {
                            float t = fmodf(enemies[row][col].moveTimer, ZIGZAG_PERIOD) / ZIGZAG_PERIOD;
                            float wave = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);  // 0→1→0 triangle
                            float yOffset = (wave - 0.5f) * (ZIGZAG_AMPLITUDE * 2.0f); // ±ZIGZAG_AMPLITUDE
                            enemies[row][col].y = enemies[row][col].baseY + yOffset;
                        }
                    }
                }
            }

            // --- Timed Y-drop: all enemies descend periodically ---
            dropTimer += dt;
            if (dropTimer >= ENEMY_DROP_INTERVAL)
            {
                for (int i = 0; i < numRows; i++)
                {
                    for (int j = 0; j < ENEMY_COLS; j++)
                    {
                        if (enemies[i][j].type != ENEMY_DEAD)
                        {
                            enemies[i][j].baseY += ENEMY_DROP_STEP;
                            // Non-zigzag: update y directly (zigzag recalculates y each frame)
                            if (enemies[i][j].type != ENEMY_ZIGZAG)
                            {
                                enemies[i][j].y += ENEMY_DROP_STEP;
                            }
                        }
                    }
                }
                dropTimer = 0.0f;
            }
            // --- Enemy march sound beat ---
            // Plays enemy_move.wav periodically while enemies are alive;
            // beat speeds up as enemy count drops (classic Space Invaders feel)
            if (state == PLAYING && enemyCount > 0)
            {
                // Beat interval shrinks from 0.55 s (full grid) down to 0.15 s (last few)
                enemyMoveBeat = 0.15f + 0.40f * ((float)enemyCount / (float)(MAX_ENEMY_ROWS * ENEMY_COLS));
                enemyMoveTimer += dt;
                if (enemyMoveTimer >= enemyMoveBeat)
                {
                    PlaySound(sndEnemyMove);
                    enemyMoveTimer = 0.0f;
                }
            }
            else
            {
                enemyMoveTimer = 0.0f; // reset when no enemies
            }

            // --- Enemy shooting (global timer, random pick, dummy excluded) ---
            enemyShootTimer += dt;

            if (enemyShootTimer >= ENEMY_SHOOT_COOLDOWN && enemyCount > 0)
            {
                // Try to find a non-dead, non-dummy enemy to shoot
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

                // Only fire if we found a valid shooter (not dead, not dummy)
                if (enemies[randRow][randCol].type != ENEMY_DEAD &&
                    enemies[randRow][randCol].type != ENEMY_DUMMY)
                {
                    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                    {
                        if (!enemyBullets[i].active)
                        {
                            enemyBullets[i].position.x = enemies[randRow][randCol].x + ENEMY_HITBOX / 2;
                            enemyBullets[i].position.y = enemies[randRow][randCol].y + ENEMY_HITBOX;
                            enemyBullets[i].speed = ENEMY_BULLET_SPEED;
                            enemyBullets[i].active = true;
                            break;
                        }
                    }
                }
                enemyShootTimer = 0.0f;
            }

            // Move all enemy bullets downward
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (enemyBullets[i].active)
                {
                    enemyBullets[i].position.y += enemyBullets[i].speed * dt;
                    if (enemyBullets[i].position.y > WINDOW_HEIGHT)
                        enemyBullets[i].active = false;
                }
            }

            // Check if player bullets hit any enemies (with health system)
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;

                for (int j = 0; j < numRows; j++)
                {
                    for (int k = 0; k < ENEMY_COLS; k++)
                    {
                        if (enemies[j][k].type != ENEMY_DEAD)
                        {
                            Rectangle enemyRect = {enemies[j][k].x, enemies[j][k].y, ENEMY_HITBOX, ENEMY_HITBOX};

                            if (CheckCollisionPointRec(playerBullets[i].position, enemyRect))
                            {
                                playerBullets[i].active = false;
                                enemies[j][k].health--;
                                enemies[j][k].hitFlashFrames = 5; // brief red flash

                                if (enemies[j][k].health <= 0)
                                {
                                    score += GetEnemyScore(enemies[j][k].type);
                                    enemies[j][k].type = ENEMY_DEAD;
                                    enemies[j][k].active = false;
                                    enemyCount--;
                                    PlaySound(sndEnemyDie); // ← explosion pop

                                    SpawnParticles(particles,
                                                   enemies[j][k].x + ENEMY_HITBOX / 2.0f,
                                                   enemies[j][k].y + ENEMY_HITBOX / 2.0f,
                                                   5);

                                    explosion.position = (Vector2){enemies[j][k].x, enemies[j][k].y};
                                    explosion.active = true;
                                    explosion.timer = 0;
                                }
                                goto next_bullet; // bullet consumed, check next
                            }
                        }
                    }
                }
            next_bullet:;
            }

            // Check if enemy bullets hit the player
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (enemyBullets[i].active)
                {
                    Rectangle bulletRect = {enemyBullets[i].position.x, enemyBullets[i].position.y, ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT};
                    Rectangle playerRect = {player.position.x, PLAYER_Y, player.width, player.height};

                    if (CheckCollisionRecs(bulletRect, playerRect) && player.invincibleTimer <= 0.0f)
                    {
                        enemyBullets[i].active = false;
                        player.lives--;
                        player.invincibleTimer = PLAYER_INVINCIBLE_TIME;
                        shakeTimer = SHAKE_DURATION;

                        SpawnParticles(particles,
                                       player.position.x + player.width / 2.0f,
                                       PLAYER_Y + player.height / 2.0f,
                                       3);

                        explosion.position = (Vector2){player.position.x + player.width / 2, PLAYER_Y + player.height / 2};
                        explosion.active = true;
                        explosion.timer = 0;
                    }
                }
            }

            // Update explosion timer
            if (explosion.active)
            {
                explosion.timer += EXPLOSION_GROW_RATE * dt;
                if (explosion.timer > EXPLOSION_MAX_TIME)
                    explosion.active = false;
            }

            // Lose condition (shared: applies to PLAYING and BOSS_FIGHT)
            if (player.lives <= 0)
                state = GAME_LOST;

            // Win condition: formation cleared (PLAYING only)
            if (state == PLAYING && enemyCount <= 0)
                state = GAME_WON;
        }

        // --- Boss fight update ---
        if (state == BOSS_FIGHT && boss.active)
        {
            // --- Rage-mode trigger at 25% HP ---
            if (!boss.rageTriggered && boss.health <= BOSS_RAGE_HP_THRESHOLD)
            {
                boss.rageTriggered = true;
                boss.ragePhase = 1; // begin dash toward mid-screen
                PlaySound(sndBossRage); // ← dramatic alarm
            }

            // --- Rage state machine (no visual text/border) ---
            if (boss.ragePhase == 1) // dashing to mid-screen
            {
                float targetX = WINDOW_WIDTH / 2.0f - BOSS_WIDTH / 2.0f;
                float targetY = BOSS_RAGE_TARGET_Y; // = screen centre, never below
                float spd = 600.0f * dt;
                float dx = targetX - boss.x, dy = targetY - boss.y;
                float d  = sqrtf(dx * dx + dy * dy);
                if (d < spd)
                {
                    boss.x = targetX; boss.y = targetY;
                    boss.ragePhase = 2; boss.rageTimer = 0.0f;
                }
                else { boss.x += (dx / d) * spd; boss.y += (dy / d) * spd; }
            }
            else if (boss.ragePhase == 2) // raging at mid-screen
            {
                float rageSpd = BOSS_SPEED * BOSS_RAGE_SPEED_MUL;
                boss.x += rageSpd * boss.direction * dt;
                if (boss.x > WINDOW_WIDTH - BOSS_WIDTH - 20)
                    { boss.x = WINDOW_WIDTH - BOSS_WIDTH - 20; boss.direction = -1.0f; }
                if (boss.x < 20)
                    { boss.x = 20; boss.direction = 1.0f; }
                boss.y = BOSS_RAGE_TARGET_Y; // locked at mid-screen Y
                boss.rageTimer += dt;
                if (boss.rageTimer >= BOSS_RAGE_DURATION) boss.ragePhase = 3;
            }
            else if (boss.ragePhase == 3) // returning to top zone
            {
                float targetX = WINDOW_WIDTH / 2.0f - BOSS_WIDTH / 2.0f;
                float targetY = BOSS_START_Y;
                float spd = 400.0f * dt;
                float dx = targetX - boss.x, dy = targetY - boss.y;
                float d  = sqrtf(dx * dx + dy * dy);
                if (d < spd)
                {
                    boss.x = targetX; boss.y = targetY;
                    boss.yTimer = 0.0f; // restart Y-bounce from top
                    boss.ragePhase = 0;
                }
                else { boss.x += (dx / d) * spd; boss.y += (dy / d) * spd; }
            }
            else // ragePhase == 0: normal movement
            {
                boss.x += boss.speed * boss.direction * dt;
                if (boss.x > WINDOW_WIDTH - BOSS_WIDTH - 20)
                    { boss.x = WINDOW_WIDTH - BOSS_WIDTH - 20; boss.direction = -1.0f; }
                if (boss.x < 20)
                    { boss.x = 20; boss.direction = 1.0f; }

                // Triangle-wave Y oscillation, ±30px from BOSS_START_Y
                boss.yTimer += dt;
                {
                    float period = 4.0f;
                    float t    = fmodf(boss.yTimer, period) / period;
                    float wave = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
                    boss.y = BOSS_START_Y + (wave - 0.5f) * 60.0f;
                }
            }

            // Decrement boss hit flash
            if (boss.hitFlashFrames > 0)
                boss.hitFlashFrames--;

            // --- Attack phase cycling ---
            boss.phaseTimer += dt;
            boss.shootTimer -= dt;
            // Bullets originate from center-X, 60% down the sprite (crab body/mouth)
            float bossCX = boss.x + BOSS_WIDTH / 2.0f;
            float bossCY = boss.y + BOSS_HEIGHT * BOSS_BULLET_Y_RATIO;

            // Phase 0: Rapid — tilted toward player, clamped to ±1/3 screen width
            if (boss.attackPhase == 0 && boss.phaseTimer < BOSS_PHASE_ATTACK)
            {
                if (boss.shootTimer <= 0.0f)
                {
                    float playerCX = player.position.x + player.width / 2.0f;
                    float rawDX    = playerCX - bossCX;
                    // Clamp horizontal tracking to 1/3 of screen width
                    float maxTrack = WINDOW_WIDTH / 3.0f;
                    float clampedDX = rawDX;
                    if (clampedDX >  maxTrack) clampedDX =  maxTrack;
                    if (clampedDX < -maxTrack) clampedDX = -maxTrack;
                    float pdy  = (float)PLAYER_Y - bossCY;
                    float dist = sqrtf(clampedDX * clampedDX + pdy * pdy);
                    if (dist > 0.0f)
                    {
                        for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                        {
                            if (!bossBullets[i].active)
                            {
                                bossBullets[i].x  = bossCX;
                                bossBullets[i].y  = bossCY;
                                bossBullets[i].vx = (clampedDX / dist) * BOSS_RAPID_SPEED;
                                bossBullets[i].vy = (pdy       / dist) * BOSS_RAPID_SPEED;
                                bossBullets[i].active = true;
                                break;
                            }
                        }
                    }
                    boss.shootTimer = BOSS_RAPID_COOLDOWN;
                }
            }
            // Phase 2: Star — 8 compass directions
            else if (boss.attackPhase == 2 && boss.phaseTimer < BOSS_PHASE_ATTACK)
            {
                if (boss.shootTimer <= 0.0f)
                {
                    for (int a = 0; a < 8; a++)
                    {
                        float angle = a * (PI / 4.0f);
                        int slot = -1;
                        for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                            if (!bossBullets[i].active)
                            {
                                slot = i;
                                break;
                            }
                        if (slot < 0)
                            break;
                        bossBullets[slot].x = bossCX;
                        bossBullets[slot].y = bossCY;
                        bossBullets[slot].vx = cosf(angle) * BOSS_STAR_SPEED;
                        bossBullets[slot].vy = sinf(angle) * BOSS_STAR_SPEED;
                        bossBullets[slot].active = true;
                    }
                    boss.shootTimer = BOSS_STAR_COOLDOWN;
                }
            }
            // Phase 4: Circle — 16 evenly-spaced bullets
            else if (boss.attackPhase == 4 && boss.phaseTimer < BOSS_PHASE_ATTACK)
            {
                if (boss.shootTimer <= 0.0f)
                {
                    for (int a = 0; a < 16; a++)
                    {
                        float angle = a * (PI / 8.0f);
                        int slot = -1;
                        for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                            if (!bossBullets[i].active)
                            {
                                slot = i;
                                break;
                            }
                        if (slot < 0)
                            break;
                        bossBullets[slot].x = bossCX;
                        bossBullets[slot].y = bossCY;
                        bossBullets[slot].vx = cosf(angle) * BOSS_CIRCLE_SPEED;
                        bossBullets[slot].vy = sinf(angle) * BOSS_CIRCLE_SPEED;
                        bossBullets[slot].active = true;
                    }
                    boss.shootTimer = BOSS_CIRCLE_COOLDOWN;
                }
            }

            // Advance to next phase when phase duration expires
            float phaseDur = (boss.attackPhase % 2 == 0) ? BOSS_PHASE_ATTACK : BOSS_PHASE_PAUSE;
            if (boss.phaseTimer >= phaseDur)
            {
                boss.attackPhase = (boss.attackPhase + 1) % 6;
                boss.phaseTimer = 0.0f;
                boss.shootTimer = 0.0f;
            }

            // --- Move all boss bullets ---
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active)
                    continue;
                bossBullets[i].x += bossBullets[i].vx * dt;
                bossBullets[i].y += bossBullets[i].vy * dt;
                // Deactivate when off screen
                if (bossBullets[i].x < -20 || bossBullets[i].x > WINDOW_WIDTH + 20 ||
                    bossBullets[i].y < -20 || bossBullets[i].y > WINDOW_HEIGHT + 20)
                    bossBullets[i].active = false;
            }

            // --- Player bullets vs Boss ---
            // Hitbox trimmed to the visible crab body (upper body only, with side margins)
            // so bullets must reach the shell rather than the transparent leg/claw area
            Rectangle bossRect = {
                boss.x + BOSS_HIT_X_MARGIN,
                boss.y + BOSS_HIT_Y_OFFSET,
                BOSS_HIT_WIDTH,
                BOSS_HIT_HEIGHT
            };
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;
                if (CheckCollisionPointRec(playerBullets[i].position, bossRect))
                {
                    playerBullets[i].active = false;
                    boss.health--;
                    boss.hitFlashFrames = 5;
                    score += 2;
                    PlaySound(sndBossHit); // ← deep thud

                    if (boss.health <= 0)
                    {
                        shakeTimer = 0.6f; // only shake on the final boss kill
                        SpawnParticles(particles,
                                       boss.x + BOSS_WIDTH / 2.0f,
                                       boss.y + BOSS_HEIGHT / 2.0f,
                                       15);
                        boss.active = false;
                        state = GAME_WON;
                    }

                    // No explosion circle on boss hit (misaligned with sprite) — particles handle the effect
                }
            }

            // --- Boss bullets vs Player ---
            Rectangle playerRect = {player.position.x, PLAYER_Y, player.width, player.height};
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active)
                    continue;
                Rectangle bRect = {bossBullets[i].x - 4, bossBullets[i].y - 4, 8, 8};
                if (CheckCollisionRecs(bRect, playerRect) && player.invincibleTimer <= 0.0f)
                {
                    bossBullets[i].active = false;
                    player.lives--;
                    player.invincibleTimer = PLAYER_INVINCIBLE_TIME;
                    shakeTimer = SHAKE_DURATION;

                    SpawnParticles(particles,
                                   player.position.x + player.width / 2.0f,
                                   PLAYER_Y + player.height / 2.0f,
                                   3);

                    explosion.position = (Vector2){player.position.x + player.width / 2.0f,
                                                   PLAYER_Y + player.height / 2.0f};
                    explosion.active = true;
                    explosion.timer = 0;
                }
            }

            // --- Minion spawn at health thresholds ---
            // Minions appear spread around the boss's current X position
            if (boss.health <= BOSS_MINION_WAVE1_HP && !boss.minionsSpawned1)
            {
                float spawnCenterX = boss.x + BOSS_WIDTH / 2.0f;
                float spawnY = boss.y + BOSS_HEIGHT + 20.0f; // just below the boss
                for (int j = 0; j < BOSS_MINION_COLS; j++)
                {
                    InitEnemy(&enemies[0][j], 0, j, ENEMY_BASIC, 1.0f);
                    // Override position: spread around boss centre
                    enemies[0][j].x = spawnCenterX - (BOSS_MINION_COLS / 2 - j) * (ENEMY_HITBOX + 5);
                    enemies[0][j].y = spawnY;
                    enemies[0][j].baseY = spawnY;
                }
                for (int j = BOSS_MINION_COLS; j < ENEMY_COLS; j++)
                    enemies[0][j].type = ENEMY_DEAD;
                if (numRows < 1)
                    numRows = 1;
                enemyCount += BOSS_MINION_COLS;
                boss.minionsSpawned1 = true;
            }
            if (boss.health <= BOSS_MINION_WAVE2_HP && !boss.minionsSpawned2)
            {
                float spawnCenterX = boss.x + BOSS_WIDTH / 2.0f;
                float spawnY = boss.y + BOSS_HEIGHT + 20.0f;
                for (int j = 0; j < BOSS_MINION_COLS; j++)
                {
                    InitEnemy(&enemies[1][j], 1, j, ENEMY_RAPID, 1.0f);
                    enemies[1][j].x = spawnCenterX - (BOSS_MINION_COLS / 2 - j) * (ENEMY_HITBOX + 5);
                    enemies[1][j].y = spawnY;
                    enemies[1][j].baseY = spawnY;
                }
                for (int j = BOSS_MINION_COLS; j < ENEMY_COLS; j++)
                    enemies[1][j].type = ENEMY_DEAD;
                if (numRows < 2)
                    numRows = 2;
                enemyCount += BOSS_MINION_COLS;
                boss.minionsSpawned2 = true;
            }

            // --- Boss win condition: boss death ends the level immediately ---
            if (boss.health <= 0)
            {
                boss.active = false;
                score += 100; // bonus for defeating boss
                state = GAME_WON;
            }
        }

        // (Win/Lose/Pause/Leaderboard/NameEntry handled above in the main else-if chain)

        // Draw everything
        BeginDrawing();
        ClearBackground(BLACK);

        int sx = shakeOffsetX;
        int sy = shakeOffsetY;

        // --- Draw starfield ---
        for (int i = 0; i < STAR_TOTAL; i++)
        {
            unsigned char b = stars[i].brightness;
            Color sc = {b, b, b, 255};
            float drawX = stars[i].x + sx;
            float drawY = stars[i].y + sy;
            if (stars[i].size <= 1.0f)
                DrawPixel((int)drawX, (int)drawY, sc);
            else
                DrawCircleV((Vector2){drawX, drawY}, stars[i].size, sc);
        }

                // --- Draw particles ---
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (!particles[i].active)
                continue;
            int sz = (int)particles[i].size;
            if (sz < 1)
                sz = 1;
            DrawRectangle((int)(particles[i].x + sx - sz / 2.0f),
                          (int)(particles[i].y + sy - sz / 2.0f),
                          sz, sz, particles[i].color);
        }

        // Draw HUD (score, lives, enemy count)
        DrawText(TextFormat("SCORE: %d", score), 20, 20, 25, WHITE);

        for (int i = 0; i < player.lives; i++)
        {
            DrawTextureEx(heartTex, (Vector2){20 + i * 60, 50}, 0, 0.5f, WHITE);
        }

        DrawText(TextFormat("ENEMIES: %d", enemyCount), 20, 80, 25, RED);
        // Pause icon in top-right corner (two ▐▐ bars on a dark pill background)
        if (state == PLAYING || state == BOSS_FIGHT)
        {
            // Icon geometry
            int iconX  = WINDOW_WIDTH - 52; // left edge of icon area
            int iconY  = 12;
            int barW   = 8;                  // width of each bar
            int barH   = 22;                 // height of each bar
            int barGap = 6;                  // gap between the two bars
            // Semi-transparent pill background
            DrawRectangleRounded((Rectangle){iconX - 6, iconY - 4,
                                             barW * 2 + barGap + 12, barH + 8},
                                 0.5f, 8, (Color){0, 0, 0, 130});
            // Left bar
            DrawRectangleRounded((Rectangle){(float)iconX, (float)iconY,
                                             (float)barW, (float)barH},
                                 0.3f, 4, (Color){200, 200, 200, 210});
            // Right bar
            DrawRectangleRounded((Rectangle){(float)(iconX + barW + barGap), (float)iconY,
                                             (float)barW, (float)barH},
                                 0.3f, 4, (Color){200, 200, 200, 210});
            // "P" key hint — tiny label just below the icon, only visible on hover
            Vector2 mouse = GetMousePosition();
            Rectangle iconArea = {(float)(iconX - 6), (float)(iconY - 4),
                                  (float)(barW * 2 + barGap + 12), (float)(barH + 8)};
            if (CheckCollisionPointRec(mouse, iconArea))
                DrawText("[P]", iconX - 2, iconY + barH + 6, 14, DARKGRAY);
        }

        // Draw all living enemies
        for (int i = 0; i < numRows; i++)
        {
            for (int j = 0; j < ENEMY_COLS; j++)
            {
                if (enemies[i][j].type == ENEMY_DEAD)
                    continue;

                Vector2 pos = {(float)(enemies[i][j].x + sx), (float)(enemies[i][j].y + sy)};
                Color tint = WHITE;

                // Hit flash override: RED tint for a few frames after taking damage
                bool flashing = (enemies[i][j].hitFlashFrames > 0);

                switch (enemies[i][j].type)
                {
                case ENEMY_DUMMY:
                    DrawTextureEx(dummy, pos, 0, .37f, flashing ? RED : WHITE);
                    break;
                case ENEMY_BASIC:
                    DrawTextureEx(basicTex, (Vector2){pos.x, pos.y}, 0, .37f, flashing ? RED : SKYBLUE);
                    break;
                case ENEMY_ZIGZAG:
                    DrawTextureEx(zigzag, (Vector2){pos.x - 5, pos.y}, 0, 0.35f, flashing ? RED : WHITE);
                    break;
                case ENEMY_TANK:
                    DrawTextureEx(tank, (Vector2){pos.x - 12, pos.y}, 0, 0.4f, flashing ? RED : WHITE);
                    // Draw health bar above tank
                    if (enemies[i][j].health < enemies[i][j].maxHealth)
                    {
                        float barWidth = 40.0f;
                        float barHeight = 4.0f;
                        float healthRatio = (float)enemies[i][j].health / (float)enemies[i][j].maxHealth;
                        DrawRectangle((int)pos.x + 5, (int)pos.y - 8, (int)barWidth, (int)barHeight, DARKGRAY);
                        DrawRectangle((int)pos.x + 5, (int)pos.y - 8, (int)(barWidth * healthRatio), (int)barHeight, RED);
                    }
                    break;
                case ENEMY_RAPID:
                    DrawTextureEx(rapidTex, (Vector2){pos.x - 5, pos.y}, 0, 0.35f, flashing ? RED : YELLOW);
                    break;
                }
            }
        }

        // Draw the player ship
        destination.x = player.position.x + sx;
        destination.y = player.position.y + sy;
        DrawTexturePro(spaceshipTex, source, destination, origin, 0, WHITE);

        // Draw player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        {
            if (playerBullets[i].active)
            {
                DrawRectangle(
                    (int)(playerBullets[i].position.x + sx),
                    (int)(playerBullets[i].position.y + sy),
                    BULLET_WIDTH, BULLET_HEIGHT, WHITE);
            }
        }

        // Draw enemy bullets
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        {
            if (enemyBullets[i].active)
            {
                DrawRectangle(
                    (int)(enemyBullets[i].position.x + sx),
                    (int)(enemyBullets[i].position.y + sy),
                    ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT, RED);
            }
        }

        // Draw explosion circle if active
        if (explosion.active)
        {
            float size = explosion.timer * EXPLOSION_SCALE;
            DrawCircle(
                (int)(explosion.position.x + EXPLOSION_OFFSET + sx),
                (int)(explosion.position.y + EXPLOSION_OFFSET + sy),
                size, WHITE);
        }

        // --- Loading screen draw ---
        if (state == LOADING)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 200});
            int tW = MeasureText("SPACE INVADERS", 50);
            DrawText("SPACE INVADERS", WINDOW_WIDTH / 2 - tW / 2, 280, 50, WHITE);
            int barW = 400, barH = 16;
            int barX = WINDOW_WIDTH / 2 - barW / 2, barY = 440;
            float prog = loadTimer / LOAD_DURATION;
            if (prog > 1.0f)
                prog = 1.0f;
            DrawRectangle(barX, barY, barW, barH, DARKGRAY);
            DrawRectangle(barX, barY, (int)(barW * prog), barH, GREEN);
            DrawRectangleLines(barX, barY, barW, barH, WHITE);
            int ldW = MeasureText("Loading...", 20);
            DrawText("Loading...", WINDOW_WIDTH / 2 - ldW / 2, barY + 25, 20, LIGHTGRAY);
        }

        // --- Main menu draw ---
        if (state == MAIN_MENU)
            DrawMenu(&mainMenu);

        // --- Level select draw ---
        if (state == LEVEL_SELECT)
            DrawMenu(&levelMenu);

        // --- Pause menu (frozen game scene underneath + overlay) ---
        if (state == PAUSED)
            DrawMenu(&pauseMenu);

        // --- Leaderboard draw ---
        if (state == LEADERBOARD)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 215});
            int lbW = MeasureText("LEADERBOARD", 44);
            DrawText("LEADERBOARD", WINDOW_WIDTH / 2 - lbW / 2, 155, 44, GOLD);
            DrawText("RANK   NAME             SCORE   LEVEL",
                     WINDOW_WIDTH / 2 - 200, 235, 22, LIGHTGRAY);
            DrawLine(WINDOW_WIDTH / 2 - 210, 265,
                     WINDOW_WIDTH / 2 + 210, 265, DARKGRAY);
            Color rankC[3] = {GOLD, LIGHTGRAY, WHITE};
            for (int i = 0; i < leaderCount; i++)
            {
                Color c = (i < 3) ? rankC[i] : WHITE;
                DrawText(TextFormat(" %d.   %-14s   %5d      %d",
                                    i + 1, leaderboard[i].name,
                                    leaderboard[i].score, leaderboard[i].level),
                         WINDOW_WIDTH / 2 - 200, 280 + i * 48, 24, c);
            }
            int escW = MeasureText("ESC / ENTER to go back", 18);
            DrawText("ESC / ENTER to go back",
                     WINDOW_WIDTH / 2 - escW / 2, 565, 18, LIGHTGRAY);
        }

        // --- Name entry draw ---
        if (state == NAME_ENTRY)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 215});
            int h1W = MeasureText("NEW HIGH SCORE!", 44);
            DrawText("NEW HIGH SCORE!", WINDOW_WIDTH / 2 - h1W / 2, 165, 44, GOLD);
            DrawText(TextFormat("Score: %d", score),
                     WINDOW_WIDTH / 2 - 60, 240, 30, WHITE);
            int h2W = MeasureText("Enter your name:", 26);
            DrawText("Enter your name:", WINDOW_WIDTH / 2 - h2W / 2, 325, 26, WHITE);
            int boxX = WINDOW_WIDTH / 2 - 150;
            DrawRectangle(boxX, 370, 300, 42, DARKGRAY);
            DrawRectangleLines(boxX, 370, 300, 42, WHITE);
            DrawText(nameBuffer, boxX + 10, 378, 26, YELLOW);
            if ((int)(GetTime() * 2) % 2 == 0)
                DrawText("_",
                         boxX + 10 + MeasureText(nameBuffer, 26), 378, 26, YELLOW);
            // Show a prompt that changes once a name has been typed
            if (nameLen == 0)
            {
                int h3W = MeasureText("Type your name, then press ENTER", 18);
                DrawText("Type your name, then press ENTER",
                         WINDOW_WIDTH / 2 - h3W / 2, 428, 18, LIGHTGRAY);
            }
            else
            {
                int h3W = MeasureText("Press ENTER to confirm", 18);
                DrawText("Press ENTER to confirm",
                         WINDOW_WIDTH / 2 - h3W / 2, 428, 18, GREEN);
            }
        }

        // --- Win/lose overlay ---
        if (state == GAME_WON)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("YOU WON!", WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 60, 40, GREEN);
            DrawText(TextFormat("Score: %d", score),
                     WINDOW_WIDTH / 2 - 70, WINDOW_HEIGHT / 2 - 5, 28, WHITE);
            if (IsHighScore(leaderboard, leaderCount, score))
                DrawText("New High Score!  Press ENTER to claim",
                         WINDOW_WIDTH / 2 - 215, WINDOW_HEIGHT / 2 + 50, 22, GOLD);
            else
                DrawText("Press ENTER to continue",
                         WINDOW_WIDTH / 2 - 140, WINDOW_HEIGHT / 2 + 50, 22, LIGHTGRAY);
        }
        else if (state == GAME_LOST)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("GAME OVER", WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2 - 60, 40, RED);
            DrawText(TextFormat("Score: %d", score),
                     WINDOW_WIDTH / 2 - 70, WINDOW_HEIGHT / 2 - 5, 28, WHITE);
            DrawText("Press ENTER to continue",
                     WINDOW_WIDTH / 2 - 140, WINDOW_HEIGHT / 2 + 50, 22, LIGHTGRAY);
        }
        // --- Boss fight draw ---
        if (state == BOSS_FIGHT && boss.active)
        {
            // Draw boss sprite (boss.png), scaled to BOSS_WIDTH x BOSS_HEIGHT
            // Apply RED tint on hit-flash frames, otherwise draw normally
            Color bossTint = (boss.hitFlashFrames > 0) ? RED : WHITE;
            bossTexDst.x = boss.x + sx;
            bossTexDst.y = boss.y + sy;
            DrawTexturePro(bossTex, bossTexSrc, bossTexDst, bossTexOrigin, 0.0f, bossTint);

            // Boss health bar — full width at screen top
            float barW = (float)(WINDOW_WIDTH - 40);
            float hpRatio = (float)boss.health / (float)boss.maxHealth;
            DrawRectangle(20, 12, (int)barW, 12, DARKGRAY);
            DrawRectangle(20, 12, (int)(barW * hpRatio), 12, RED);
            DrawRectangleLines(20, 12, (int)barW, 12, WHITE);
            DrawText(TextFormat("BOSS  %d / %d", boss.health, boss.maxHealth),
                     WINDOW_WIDTH / 2 - 70, 28, 18, WHITE);
        }

        // Draw boss bullets (always, so they fade out even after boss dies)
        if (state == BOSS_FIGHT)
        {
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (bossBullets[i].active)
                    DrawCircle((int)(bossBullets[i].x + sx), (int)(bossBullets[i].y + sy), 5, ORANGE);
            }
        }

        EndDrawing();
    }

    // --- Save on force-quit (X button / Alt+F4) ---
    // Only save if the player was actively in a game (not menus)
    if (state == PLAYING || state == BOSS_FIGHT || state == PAUSED)
    {
        int saveState = (state == PAUSED) ? (int)pausedFrom : (int)state;
        SaveGame("savegame.txt", saveState, currentLevel, score,
                 &player, shootCooldown, enemies, numRows, enemyCount,
                 enemyShootTimer, dropTimer, playerBullets, enemyBullets,
                 &boss, bossBullets);
    }

    // Clean up
    UnloadMusicStream(bgMusic);
    UnloadSound(sndShoot);
    UnloadSound(sndEnemyDie);
    UnloadSound(sndBossHit);
    UnloadSound(sndBossRage);
    UnloadSound(sndEnemyMove);
    CloseAudioDevice();
    UnloadTexture(spaceshipTex);
    UnloadTexture(dummy);
    UnloadTexture(basicTex);
    UnloadTexture(zigzag);
    UnloadTexture(rapidTex);
    UnloadTexture(tank);
    UnloadTexture(heartTex);
    UnloadTexture(bossTex);

    CloseWindow();
    return 0;
}
