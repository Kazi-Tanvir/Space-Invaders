/* =========================================================================
 *  SPACE SHOOTER — A Raylib game for CSE University Lab Showcase
 * =========================================================================
 *  Features:
 *    - 3 Levels with escalating difficulty
 *    - 5 Enemy types: Dummy, Basic, Zigzag, Tank, Rapid
 *    - Level 3 Boss with 4 attack patterns + minion spawning
 *    - Parallax starfield, screen shake, hit flash, death particles
 *
 *  Controls:
 *    LEFT / RIGHT  — Move ship
 *    SPACE         — Shoot
 *    ENTER         — Start / Restart
 *
 *  Build: compile with Raylib linked (same as before).
 * ========================================================================= */

#include "raylib.h"
#include <math.h>
#include "raymath.h"

/* =========================================================================
 *  CONSTANTS
 * ========================================================================= */

// --- Window ---
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 900

// --- Colors ---
#define SABER_GREEN (Color){255, 20, 147, 255} // Player bullet color (hot pink)
#define BG_COLOR (Color){8, 8, 12, 255}        // Deep space background

// --- Player ---
#define PLAYER_WIDTH 120
#define PLAYER_HEIGHT 60
#define PLAYER_START_X ((WINDOW_WIDTH - PLAYER_WIDTH) / 2)
#define PLAYER_Y (WINDOW_HEIGHT - PLAYER_HEIGHT - 50)
#define PLAYER_SPEED 350.0f
#define PLAYER_MAX_X (WINDOW_WIDTH - PLAYER_WIDTH)
#define PLAYER_LIVES 3
#define PLAYER_INVINCIBLE_TIME 1.5f // seconds of invincibility after being hit

// --- Player Bullets ---
#define MAX_PLAYER_BULLETS 30
#define BULLET_SPEED 1000.0f
#define BULLET_WIDTH 5
#define BULLET_HEIGHT 15
#define PLAYER_SHOOT_COOLDOWN 0.20f

// --- Enemies ---
#define MAX_ENEMIES 50
#define ENEMY_HITBOX 40

// --- Enemy Bullets ---
#define MAX_ENEMY_BULLETS 40
#define ENEMY_BULLET_SPEED 300.0f
#define ENEMY_BULLET_WIDTH 5
#define ENEMY_BULLET_HEIGHT 15

// --- Boss ---
#define BOSS_WIDTH 200
#define BOSS_HEIGHT 80
#define BOSS_MAX_HP 50
#define BOSS_BULLET_SPEED 400.0f
#define MAX_BOSS_BULLETS 50
#define BOSS_MINION_INTERVAL 10.0f // seconds between minion spawns

// --- Particles ---
#define MAX_PARTICLES 100
#define PARTICLE_LIFETIME 0.5f

// --- Screen Shake ---
#define SHAKE_DURATION 0.3f

// --- Starfield ---
#define STAR_COUNT_FAR 150
#define STAR_COUNT_MID 80
#define STAR_COUNT_NEAR 30
#define STAR_TOTAL (STAR_COUNT_FAR + STAR_COUNT_MID + STAR_COUNT_NEAR)
#define STAR_SPEED_FAR 30.0f
#define STAR_SPEED_MID 70.0f
#define STAR_SPEED_NEAR 160.0f

// --- Score Values ---
#define SCORE_DUMMY 5
#define SCORE_BASIC 10
#define SCORE_ZIGZAG 15
#define SCORE_RAPID 20
#define SCORE_TANK 30
#define SCORE_BOSS 500

// --- Level Transition ---
#define LEVEL_TRANSITION_TIME 2.5f // seconds to show "LEVEL X" screen

// --- Explosion (legacy single-circle, kept for small effects) ---
#define EXPLOSION_GROW_RATE 6.0f
#define EXPLOSION_MAX_TIME 1.0f
#define EXPLOSION_SCALE 12.0f
#define EXPLOSION_OFFSET 15

/* =========================================================================
 *  ENUMS
 * ========================================================================= */

// Enemy types — each drives different movement & shooting behavior
typedef enum EnemyType
{
    ENEMY_DUMMY = 0, // Drifts straight down, never shoots
    ENEMY_BASIC,     // Horizontal bounce + slow drift down, occasional shots
    ENEMY_ZIGZAG,    // Sine-wave horizontal + fast drift down, rare shots
    ENEMY_TANK,      // Very slow, 3 HP, health bar + hit flash
    ENEMY_RAPID,     // Stays near top, fast horizontal bounce, shoots often
    ENEMY_DEAD       // Destroyed — slot is empty
} EnemyType;

// Game states — the main state machine
typedef enum GameState
{
    STATE_TITLE,            // Title screen
    STATE_LEVEL_TRANSITION, // "LEVEL X" countdown
    STATE_PLAYING,          // Normal enemy wave
    STATE_BOSS_FIGHT,       // Level 3 boss battle
    STATE_GAME_WON,         // Victory screen
    STATE_GAME_LOST         // Game over screen
} GameState;

// Boss attack phases — cycles through these
typedef enum BossPhase
{
    BOSS_IDLE,   // Sways left-right, no shooting
    BOSS_DIRECT, // Fires aimed bullets at player X
    BOSS_SPREAD, // Fires 3-bullet cone
    BOSS_RAIN    // Drops slow bullets across the screen
} BossPhase;

/* =========================================================================
 *  STRUCTS
 * ========================================================================= */

// A single star in the parallax background
typedef struct Star
{
    float x, y;
    float speed;
    float size;
    unsigned char brightness;
} Star;

// The player ship
typedef struct Player
{
    Vector2 position;
    float speed;
    int width, height;
    int lives;
    float invincibleTimer; // >0 means player cannot be hit (blinks)
} Player;

// A bullet (used for player, enemy, and boss bullets)
typedef struct Bullet
{
    float x, y;   // Position
    float vx, vy; // Velocity components (allows angled boss shots)
    bool active;
} Bullet;

// An individual enemy with its own position and behavior
typedef struct Enemy
{
    EnemyType type;
    float x, y;          // World position
    float speed;         // Movement speed
    float direction;     // +1.0 = moving right, -1.0 = moving left
    float moveTimer;     // Elapsed time — used for sine wave, etc.
    float shootTimer;    // Time until next shot
    float shootCooldown; // How often this enemy fires (randomized at spawn)
    int health;
    int maxHealth;
    int hitFlashFrames; // >0 means draw with RED tint
    bool active;
} Enemy;

// The Level 3 boss
typedef struct Boss
{
    float x, y;
    float width, height;
    int health, maxHealth;
    BossPhase phase;
    float phaseTimer;  // Time spent in current phase
    float attackTimer; // Cooldown between shots within a phase
    float minionTimer; // (unused — kept for ABI compat)
    float direction;   // +1 right, -1 left (horizontal sway)
    int hitFlashFrames;
    bool active;
    bool minionSpawnFlags[3]; // tracks HP threshold spawns: 75%, 50%, 25%
} Boss;

// A small particle for death explosions
typedef struct Particle
{
    float x, y;
    float vx, vy;
    float size;
    float life; // Remaining lifetime in seconds
    Color color;
    bool active;
} Particle;

/* =========================================================================
 *  HELPER FUNCTIONS
 * ========================================================================= */

// ------------------------------------------------------------------
// GetEnemyScore: returns score value for destroying an enemy type
// ------------------------------------------------------------------
static int GetEnemyScore(EnemyType t)
{
    switch (t)
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

// ------------------------------------------------------------------
// InitEnemy: configure a single enemy's stats based on its type.
//   x, y = spawn position on screen.
// ------------------------------------------------------------------
static void InitEnemy(Enemy *e, EnemyType type, float x, float y)
{
    e->type = type;
    e->x = x;
    e->y = y;
    e->active = true;
    e->direction = (GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f;
    e->moveTimer = (float)GetRandomValue(0, 314) / 100.0f; // random phase offset for sine
    e->hitFlashFrames = 0;

    switch (type)
    {
    case ENEMY_DUMMY:
        e->speed = 40.0f;
        e->health = 1;
        e->maxHealth = 1;
        e->shootCooldown = 999.0f; // never shoots
        break;
    case ENEMY_BASIC:
        e->speed = 80.0f;
        e->health = 1;
        e->maxHealth = 1;
        e->shootCooldown = (float)GetRandomValue(20, 40) / 10.0f; // 2.0–4.0s
        break;
    case ENEMY_ZIGZAG:
        e->speed = 70.0f;
        e->health = 1;
        e->maxHealth = 1;
        e->shootCooldown = (float)GetRandomValue(30, 50) / 10.0f; // 3.0–5.0s
        break;
    case ENEMY_TANK:
        e->speed = 10.0f;
        e->health = 3;
        e->maxHealth = 3;
        e->shootCooldown = (float)GetRandomValue(20, 30) / 10.0f; // 2.0–3.0s
        break;
    case ENEMY_RAPID:
        e->speed = 120.0f;
        e->health = 1;
        e->maxHealth = 1;
        e->shootCooldown = (float)GetRandomValue(5, 10) / 10.0f; // 0.5–1.0s
        break;
    default:
        e->speed = 0;
        e->health = 0;
        e->maxHealth = 0;
        e->shootCooldown = 999.0f;
        break;
    }
    e->shootTimer = e->shootCooldown; // start with full cooldown
}

// ------------------------------------------------------------------
// CountActiveEnemies: count how many enemies are still alive
// ------------------------------------------------------------------
static int CountActiveEnemies(Enemy enemies[], int maxCount)
{
    int count = 0;
    for (int i = 0; i < maxCount; i++)
    {
        if (enemies[i].active && enemies[i].type != ENEMY_DEAD)
            count++;
    }
    return count;
}

// ------------------------------------------------------------------
// SpawnLevelEnemies: fill the enemies array for the given level.
//   Returns the number of enemies spawned.
// ------------------------------------------------------------------
static int SpawnLevelEnemies(Enemy enemies[], int level)
{
    // Clear all enemy slots first
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].active = false;
        enemies[i].type = ENEMY_DEAD;
    }

    int idx = 0; // index into the flat enemies array

    if (level == 1)
    {
        // Level 1: 4 rows x 6 cols = 24 enemies
        // Row 0: DUMMY, Row 1: DUMMY, Row 2: BASIC, Row 3: RAPID
        EnemyType rowTypes[4] = {ENEMY_DUMMY, ENEMY_DUMMY, ENEMY_BASIC, ENEMY_RAPID};
        int cols = 6;
        int rows = 4;
        float startX = 150.0f;
        float startY = 80.0f;
        float spacingX = 200.0f;
        float spacingY = 80.0f;

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                if (idx < MAX_ENEMIES)
                {
                    float x = startX + c * spacingX;
                    float y = startY + r * spacingY;
                    InitEnemy(&enemies[idx], rowTypes[r], x, y);
                    idx++;
                }
            }
        }
    }
    else if (level == 2)
    {
        // Level 2: 4 rows x 9 cols = 36 enemies
        // Row 0: TANK, Row 1: ZIGZAG, Row 2: ZIGZAG, Row 3: BASIC
        EnemyType rowTypes[4] = {ENEMY_TANK, ENEMY_ZIGZAG, ENEMY_ZIGZAG, ENEMY_BASIC};
        int cols = 7;
        int rows = 4;
        float startX = 120.0f;
        float startY = 60.0f;
        float spacingX = 180.0f;
        float spacingY = 80.0f;

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                if (idx < MAX_ENEMIES)
                {
                    float x = startX + c * spacingX;
                    float y = startY + r * spacingY;
                    InitEnemy(&enemies[idx], rowTypes[r], x, y);
                    idx++;
                }
            }
        }
    }
    else if (level == 3)
    {
        // Level 3: 2 rows x 6 cols = 12 support enemies
        // Row 0: RAPID, Row 1: DUMMY
        EnemyType rowTypes[2] = {ENEMY_RAPID, ENEMY_DUMMY};
        int cols = 6;
        int rows = 2;
        float startX = 200.0f;
        float startY = 60.0f;
        float spacingX = 180.0f;
        float spacingY = 80.0f;

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                if (idx < MAX_ENEMIES)
                {
                    float x = startX + c * spacingX;
                    float y = startY + r * spacingY;
                    InitEnemy(&enemies[idx], rowTypes[r], x, y);
                    idx++;
                }
            }
        }
    }

    return idx;
}

// ------------------------------------------------------------------
// InitBoss: set up the Level 3 boss
// ------------------------------------------------------------------
static void InitBoss(Boss *boss)
{
    boss->x = (WINDOW_WIDTH - BOSS_WIDTH) / 2.0f;
    boss->y = 50.0f;
    boss->width = BOSS_WIDTH;
    boss->height = BOSS_HEIGHT;
    boss->health = BOSS_MAX_HP;
    boss->maxHealth = BOSS_MAX_HP;
    boss->phase = BOSS_IDLE;
    boss->phaseTimer = 0.0f;
    boss->attackTimer = 1.5f; // initial delay — no instant bullets on spawn
    boss->minionTimer = 0.0f;
    boss->direction = 1.0f;
    boss->hitFlashFrames = 0;
    boss->active = true;
    boss->minionSpawnFlags[0] = false; // 75% HP threshold
    boss->minionSpawnFlags[1] = false; // 50% HP threshold
    boss->minionSpawnFlags[2] = false; // 25% HP threshold
}

// ------------------------------------------------------------------
// SpawnParticles: create a burst of particles at position (px, py).
//   Used when an enemy or the boss dies.
// ------------------------------------------------------------------
static void SpawnParticles(Particle particles[], float px, float py, int count)
{
    // Colors for explosion particles
    Color particleColors[4] = {RED, ORANGE, YELLOW, WHITE};
    int spawned = 0;

    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++)
    {
        if (!particles[i].active)
        {
            particles[i].active = true;
            particles[i].x = px;
            particles[i].y = py;
            // Random outward velocity
            particles[i].vx = (float)GetRandomValue(-150, 150);
            particles[i].vy = (float)GetRandomValue(-200, 50);
            particles[i].size = (float)GetRandomValue(3, 8);
            particles[i].life = PARTICLE_LIFETIME;
            particles[i].color = particleColors[GetRandomValue(0, 3)];
            spawned++;
        }
    }
}

// ------------------------------------------------------------------
// FireEnemyBullet: try to place a bullet into the enemy bullet pool.
//   Bullet goes straight down at ENEMY_BULLET_SPEED.
// ------------------------------------------------------------------
static void FireEnemyBullet(Bullet bullets[], float x, float y)
{
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].active = true;
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = 0;
            bullets[i].vy = ENEMY_BULLET_SPEED;
            break;
        }
    }
}

// ------------------------------------------------------------------
// FireBossBullet: place a bullet into the boss bullet pool with
//   custom velocity (allows angled shots for spread pattern).
// ------------------------------------------------------------------
static void FireBossBullet(Bullet bullets[], float x, float y, float vx, float vy)
{
    for (int i = 0; i < MAX_BOSS_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].active = true;
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = vx;
            bullets[i].vy = vy;
            break;
        }
    }
}

/* =========================================================================
 *  MAIN
 * ========================================================================= */
int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Shooter");
    SetTargetFPS(60);

    /* ------------------------------------------------------------------
     *  STARFIELD INITIALIZATION — 3 layers for parallax depth
     * ------------------------------------------------------------------ */
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
    // Mid layer: medium stars
    for (int i = STAR_COUNT_FAR; i < STAR_COUNT_FAR + STAR_COUNT_MID; i++)
    {
        stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
        stars[i].y = (float)GetRandomValue(0, WINDOW_HEIGHT);
        stars[i].speed = STAR_SPEED_MID;
        stars[i].size = GetRandomValue(2, 3) * 0.5f;
        stars[i].brightness = (unsigned char)GetRandomValue(140, 200);
    }
    // Near layer: fewer bright stars (closest to camera)
    for (int i = STAR_COUNT_FAR + STAR_COUNT_MID; i < STAR_TOTAL; i++)
    {
        stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
        stars[i].y = (float)GetRandomValue(0, WINDOW_HEIGHT);
        stars[i].speed = STAR_SPEED_NEAR;
        stars[i].size = GetRandomValue(2, 4) * 0.5f;
        stars[i].brightness = (unsigned char)GetRandomValue(200, 255);
    }

    /* ------------------------------------------------------------------
     *  LOAD TEXTURES — use existing assets; boss uses rectangles
     * ------------------------------------------------------------------ */
    Texture2D spaceshipTex = LoadTexture("resources/spaceship.png");
    Texture2D dummyTex = LoadTexture("resources/dummy.png");
    Texture2D basicTex = LoadTexture("resources/dummy.png"); // BASIC reuses dummy
    Texture2D zigzagTex = LoadTexture("resources/zigzag.png");
    Texture2D rapidTex = LoadTexture("resources/zigzag.png"); // RAPID reuses zigzag
    Texture2D tankTex = LoadTexture("resources/tank.png");
    Texture2D heartTex = LoadTexture("resources/heart.png");

    // Source & destination rects for DrawTexturePro (player ship)
    Rectangle playerSrcRect = {0, 0, (float)spaceshipTex.width, (float)spaceshipTex.height};
    Rectangle playerDestRect = {0, 0, PLAYER_WIDTH, PLAYER_HEIGHT};
    Vector2 playerOrigin = {0, 0};

    /* ------------------------------------------------------------------
     *  GAME STATE VARIABLES
     * ------------------------------------------------------------------ */
    GameState state = STATE_TITLE;
    int currentLevel = 1;
    int score = 0;
    float transitionTimer = 0.0f; // countdown for level transition screen
    int menuSelection = 0;        // 0=Level1, 1=Level2, 2=Level3/Boss

    /* ------------------------------------------------------------------
     *  PLAYER
     * ------------------------------------------------------------------ */
    Player player = {
        .position = {PLAYER_START_X, PLAYER_Y},
        .speed = PLAYER_SPEED,
        .width = PLAYER_WIDTH,
        .height = PLAYER_HEIGHT,
        .lives = PLAYER_LIVES,
        .invincibleTimer = 0.0f,
    };
    Bullet playerBullets[MAX_PLAYER_BULLETS] = {0};
    float shootCooldown = 0.0f;

    /* ------------------------------------------------------------------
     *  ENEMIES — flat array, each with individual position & behavior
     * ------------------------------------------------------------------ */
    Enemy enemies[MAX_ENEMIES] = {0};

    /* ------------------------------------------------------------------
     *  ENEMY BULLETS
     * ------------------------------------------------------------------ */
    Bullet enemyBullets[MAX_ENEMY_BULLETS] = {0};

    /* ------------------------------------------------------------------
     *  BOSS (Level 3)
     * ------------------------------------------------------------------ */
    Boss boss = {0};
    Bullet bossBullets[MAX_BOSS_BULLETS] = {0};

    /* ------------------------------------------------------------------
     *  PARTICLES — small colored squares for death explosions
     * ------------------------------------------------------------------ */
    Particle particles[MAX_PARTICLES] = {0};

    /* ------------------------------------------------------------------
     *  SCREEN SHAKE
     * ------------------------------------------------------------------ */
    float shakeTimer = 0.0f;
    int shakeOffsetX = 0;
    int shakeOffsetY = 0;

    /* ================================================================
     *  MAIN GAME LOOP
     * ================================================================ */
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        /* ==============================================================
         *  UPDATE: STARFIELD (always runs, even on title/menus)
         * ============================================================== */
        for (int i = 0; i < STAR_TOTAL; i++)
        {
            stars[i].y += stars[i].speed * dt;
            if (stars[i].y > WINDOW_HEIGHT)
            {
                stars[i].y = 0;
                stars[i].x = (float)GetRandomValue(0, WINDOW_WIDTH);
            }
        }

        /* ==============================================================
         *  UPDATE: SCREEN SHAKE (decrement timer, compute offset)
         * ============================================================== */
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

        /* ==============================================================
         *  UPDATE: PARTICLES (always tick so they fade out across states)
         * ============================================================== */
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
            // Shrink over lifetime
            float ratio = particles[i].life / PARTICLE_LIFETIME;
            particles[i].size = particles[i].size * ratio + 0.5f;
        }

        /* ==============================================================
         *  STATE MACHINE
         * ============================================================== */
        switch (state)
        {
        /* ---------------------------------------------------------------
         *  TITLE SCREEN
         * --------------------------------------------------------------- */
        case STATE_TITLE:
        {
            // Navigate menu with UP / DOWN
            if (IsKeyPressed(KEY_UP))
                menuSelection = (menuSelection + 2) % 3; // wrap upward
            if (IsKeyPressed(KEY_DOWN))
                menuSelection = (menuSelection + 1) % 3;

            if (IsKeyPressed(KEY_ENTER))
            {
                // Map selection → starting level
                currentLevel = menuSelection + 1; // 1, 2, or 3
                score = 0;
                player.lives = PLAYER_LIVES;
                player.position = (Vector2){PLAYER_START_X, PLAYER_Y};
                player.invincibleTimer = 0.0f;
                boss.active = false;
                // Clear all bullet pools
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                    playerBullets[i].active = false;
                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                    enemyBullets[i].active = false;
                for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                    bossBullets[i].active = false;
                for (int i = 0; i < MAX_PARTICLES; i++)
                    particles[i].active = false;

                if (menuSelection == 2) // Level 3: boss fight only — no support wave
                {
                    // Clear all enemies — boss fight starts clean
                    for (int i = 0; i < MAX_ENEMIES; i++)
                    {
                        enemies[i].active = false;
                        enemies[i].type = ENEMY_DEAD;
                    }
                    InitBoss(&boss);
                }
                else
                {
                    SpawnLevelEnemies(enemies, currentLevel);
                }
                transitionTimer = LEVEL_TRANSITION_TIME;
                state = STATE_LEVEL_TRANSITION;
            }
            break;
        }

        /* ---------------------------------------------------------------
         *  LEVEL TRANSITION — shows "LEVEL X" for a few seconds
         * --------------------------------------------------------------- */
        case STATE_LEVEL_TRANSITION:
        {
            transitionTimer -= dt;
            if (transitionTimer <= 0.0f)
            {
                if (currentLevel == 3 && boss.active)
                    state = STATE_BOSS_FIGHT;
                else
                    state = STATE_PLAYING;
            }
            break;
        }

        /* ---------------------------------------------------------------
         *  PLAYING — normal enemy waves (Levels 1, 2, 3 support enemies)
         * --------------------------------------------------------------- */
        case STATE_PLAYING:
        {
            // --- Player Movement ---
            if (IsKeyDown(KEY_LEFT))
                player.position.x -= player.speed * dt;
            if (IsKeyDown(KEY_RIGHT))
                player.position.x += player.speed * dt;
            if (player.position.x < 0)
                player.position.x = 0;
            if (player.position.x > PLAYER_MAX_X)
                player.position.x = PLAYER_MAX_X;

            // --- Player Invincibility Timer ---
            if (player.invincibleTimer > 0.0f)
                player.invincibleTimer -= dt;

            // --- Player Shooting ---
            shootCooldown -= dt;
            if (IsKeyDown(KEY_SPACE) && shootCooldown <= 0.0f)
            {
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    if (!playerBullets[i].active)
                    {
                        playerBullets[i].active = true;
                        playerBullets[i].x = player.position.x + player.width / 2.0f - BULLET_WIDTH / 2.0f;
                        playerBullets[i].y = PLAYER_Y;
                        playerBullets[i].vx = 0;
                        playerBullets[i].vy = -BULLET_SPEED;
                        shootCooldown = PLAYER_SHOOT_COOLDOWN;
                        break;
                    }
                }
            }

            // --- Move Player Bullets ---
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;
                playerBullets[i].y += playerBullets[i].vy * dt;
                if (playerBullets[i].y < -20)
                    playerBullets[i].active = false;
            }

            // --- Update Enemies (per-type behavior) ---
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!enemies[i].active || enemies[i].type == ENEMY_DEAD)
                    continue;

                Enemy *e = &enemies[i];
                e->moveTimer += dt;

                // Decrement hit flash
                if (e->hitFlashFrames > 0)
                    e->hitFlashFrames--;

                // --- Movement based on type ---
                switch (e->type)
                {
                case ENEMY_DUMMY:
                    // Drifts straight down slowly
                    e->y += e->speed * dt;
                    break;

                case ENEMY_BASIC:
                    // Horizontal bounce + slow drift down
                    e->x += e->speed * e->direction * dt;
                    e->y += 15.0f * dt; // slow downward drift

                    // Bounce off screen edges
                    if (e->x < 20.0f)
                    {
                        e->x = 20.0f;
                        e->direction = 1.0f;
                    }
                    if (e->x > WINDOW_WIDTH - ENEMY_HITBOX - 20.0f)
                    {
                        e->x = WINDOW_WIDTH - ENEMY_HITBOX - 20.0f;
                        e->direction = -1.0f;
                    }
                    break;

                case ENEMY_ZIGZAG:
                    // Sine-wave horizontal + fast drift down
                    e->x += sinf(e->moveTimer * 3.0f) * 120.0f * e->direction * dt;
                    e->y += 60.0f * dt; // fast downward drift

                    // Keep within screen bounds
                    if (e->x < 10.0f)
                        e->x = 10.0f;
                    if (e->x > WINDOW_WIDTH - ENEMY_HITBOX - 10.0f)
                        e->x = WINDOW_WIDTH - ENEMY_HITBOX - 10.0f;
                    break;

                case ENEMY_TANK:
                    // Very slow drift down + slight horizontal sway
                    e->x += sinf(e->moveTimer * 0.5f) * 30.0f * e->direction * dt;
                    e->y += e->speed * dt; // 10 px/s

                    if (e->x < 10.0f)
                        e->x = 10.0f;
                    if (e->x > WINDOW_WIDTH - ENEMY_HITBOX - 10.0f)
                        e->x = WINDOW_WIDTH - ENEMY_HITBOX - 10.0f;
                    break;

                case ENEMY_RAPID:
                    // Stays near top half, fast horizontal bounce, NO downward drift
                    e->x += e->speed * e->direction * dt;

                    if (e->x < 20.0f)
                    {
                        e->x = 20.0f;
                        e->direction = 1.0f;
                    }
                    if (e->x > WINDOW_WIDTH - ENEMY_HITBOX - 20.0f)
                    {
                        e->x = WINDOW_WIDTH - ENEMY_HITBOX - 20.0f;
                        e->direction = -1.0f;
                    }

                    // Clamp to top 40% of screen
                    if (e->y > WINDOW_HEIGHT * 0.4f)
                        e->y = WINDOW_HEIGHT * 0.4f;
                    break;

                default:
                    break;
                }

                // Deactivate enemies that go off the bottom of the screen
                if (e->y > WINDOW_HEIGHT + 50.0f)
                {
                    e->active = false;
                    e->type = ENEMY_DEAD;
                    continue;
                }

                // --- Enemy Shooting ---
                e->shootTimer -= dt;
                if (e->shootTimer <= 0.0f && e->type != ENEMY_DUMMY)
                {
                    FireEnemyBullet(enemyBullets, e->x + ENEMY_HITBOX / 2.0f, e->y + ENEMY_HITBOX);
                    e->shootTimer = e->shootCooldown;
                }
            }

            for (int a = 0; a < MAX_ENEMIES; a++)
            {
                if (!enemies[a].active || enemies[a].type == ENEMY_DEAD)
                    continue;

                Rectangle aRect = {
                    enemies[a].x, enemies[a].y,
                    ENEMY_HITBOX, ENEMY_HITBOX};

                for (int b = a + 1; b < MAX_ENEMIES; b++)
                {
                    if (!enemies[b].active || enemies[b].type == ENEMY_DEAD)
                        continue;

                    Rectangle bRect = {
                        enemies[b].x, enemies[b].y,
                        ENEMY_HITBOX, ENEMY_HITBOX};

                    if (!CheckCollisionRecs(aRect, bRect))
                        continue;

                    float centersA = enemies[a].x + ENEMY_HITBOX / 2.0f;
                    float centersB = enemies[b].x + ENEMY_HITBOX / 2.0f;
                    float overlap = (ENEMY_HITBOX - fabsf(centersA - centersB)) / 2.0f;

                    if (centersA <= centersB)
                    {
                        enemies[a].x -= overlap;
                        enemies[b].x += overlap;
                    }
                    else
                    {
                        enemies[a].x += overlap;
                        enemies[b].x -= overlap;
                    }

                    enemies[a].direction *= -1.0f;
                    enemies[b].direction *= -1.0f;
                }
            }

            // --- Move Enemy Bullets ---
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (!enemyBullets[i].active)
                    continue;
                enemyBullets[i].x += enemyBullets[i].vx * dt;
                enemyBullets[i].y += enemyBullets[i].vy * dt;
                if (enemyBullets[i].y > WINDOW_HEIGHT + 10)
                    enemyBullets[i].active = false;
            }

            // --- Collision: Player Bullets vs Enemies ---
            for (int b = 0; b < MAX_PLAYER_BULLETS; b++)
            {
                if (!playerBullets[b].active)
                    continue;

                Rectangle bRect = {
                    playerBullets[b].x, playerBullets[b].y,
                    BULLET_WIDTH, BULLET_HEIGHT};

                for (int e = 0; e < MAX_ENEMIES; e++)
                {
                    if (!enemies[e].active || enemies[e].type == ENEMY_DEAD)
                        continue;

                    Rectangle eRect = {
                        enemies[e].x, enemies[e].y,
                        ENEMY_HITBOX, ENEMY_HITBOX};

                    if (CheckCollisionRecs(bRect, eRect))
                    {
                        playerBullets[b].active = false;
                        enemies[e].health--;

                        if (enemies[e].health <= 0)
                        {
                            // Enemy destroyed
                            score += GetEnemyScore(enemies[e].type);
                            SpawnParticles(particles, enemies[e].x + ENEMY_HITBOX / 2.0f,
                                           enemies[e].y + ENEMY_HITBOX / 2.0f, 5);
                            enemies[e].type = ENEMY_DEAD;
                            enemies[e].active = false;
                        }
                        else
                        {
                            // Non-lethal hit — flash red
                            enemies[e].hitFlashFrames = 8;
                        }
                        break; // This bullet is consumed
                    }
                }
            }

            // --- Collision: Enemy Bullets vs Player ---
            if (player.invincibleTimer <= 0.0f)
            {
                Rectangle pRect = {
                    player.position.x, PLAYER_Y,
                    (float)player.width, (float)player.height};

                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                {
                    if (!enemyBullets[i].active)
                        continue;

                    Rectangle bRect = {
                        enemyBullets[i].x, enemyBullets[i].y,
                        ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT};

                    if (CheckCollisionRecs(bRect, pRect))
                    {
                        enemyBullets[i].active = false;
                        player.lives--;
                        player.invincibleTimer = PLAYER_INVINCIBLE_TIME;
                        shakeTimer = SHAKE_DURATION; // Screen shake on hit
                        SpawnParticles(particles,
                                       player.position.x + player.width / 2.0f,
                                       PLAYER_Y + player.height / 2.0f, 3);
                    }
                }
            }

            // --- Check: Level Complete or Game Over ---
            if (player.lives <= 0)
            {
                state = STATE_GAME_LOST;
            }
            else if (CountActiveEnemies(enemies, MAX_ENEMIES) <= 0)
            {
                // All enemies destroyed — advance level
                if (currentLevel < 3)
                {
                    currentLevel++;
                    SpawnLevelEnemies(enemies, currentLevel);
                    // Clear leftover bullets
                    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                        enemyBullets[i].active = false;
                    transitionTimer = LEVEL_TRANSITION_TIME;
                    state = STATE_LEVEL_TRANSITION;
                }
                else
                {
                    // Level 3 enemies cleared — start boss fight
                    InitBoss(&boss);
                    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                        enemyBullets[i].active = false;
                    transitionTimer = LEVEL_TRANSITION_TIME;
                    state = STATE_LEVEL_TRANSITION;
                }
            }
            break;
        }

        /* ---------------------------------------------------------------
         *  BOSS FIGHT — Level 3 boss with attack patterns
         * --------------------------------------------------------------- */
        case STATE_BOSS_FIGHT:
        {
            // --- Player Movement (same as PLAYING) ---
            if (IsKeyDown(KEY_LEFT))
                player.position.x -= player.speed * dt;
            if (IsKeyDown(KEY_RIGHT))
                player.position.x += player.speed * dt;
            if (player.position.x < 0)
                player.position.x = 0;
            if (player.position.x > PLAYER_MAX_X)
                player.position.x = PLAYER_MAX_X;

            if (player.invincibleTimer > 0.0f)
                player.invincibleTimer -= dt;

            // --- Player Shooting (same as PLAYING) ---
            shootCooldown -= dt;
            if (IsKeyDown(KEY_SPACE) && shootCooldown <= 0.0f)
            {
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    if (!playerBullets[i].active)
                    {
                        playerBullets[i].active = true;
                        playerBullets[i].x = player.position.x + player.width / 2.0f - BULLET_WIDTH / 2.0f;
                        playerBullets[i].y = PLAYER_Y;
                        playerBullets[i].vx = 0;
                        playerBullets[i].vy = -BULLET_SPEED;
                        shootCooldown = PLAYER_SHOOT_COOLDOWN;
                        break;
                    }
                }
            }

            // --- Move Player Bullets ---
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;
                playerBullets[i].y += playerBullets[i].vy * dt;
                if (playerBullets[i].y < -20)
                    playerBullets[i].active = false;
            }

            // --- Boss Update ---
            if (boss.active)
            {
                // Decrement hit flash
                if (boss.hitFlashFrames > 0)
                    boss.hitFlashFrames--;

                boss.phaseTimer += dt;
                boss.attackTimer -= dt;
                boss.minionTimer += dt;

                // Horizontal sway (always active)
                boss.x += 80.0f * boss.direction * dt;
                if (boss.x < 50.0f)
                {
                    boss.x = 50.0f;
                    boss.direction = 1.0f;
                }
                if (boss.x > WINDOW_WIDTH - boss.width - 50.0f)
                {
                    boss.x = WINDOW_WIDTH - boss.width - 50.0f;
                    boss.direction = -1.0f;
                }

                // --- Boss Attack State Machine ---
                float bossCenter = boss.x + boss.width / 2.0f;
                float bossBottom = boss.y + boss.height;

                switch (boss.phase)
                {
                case BOSS_IDLE:
                    // Idle for 2 seconds, then go to DIRECT
                    if (boss.phaseTimer >= 2.0f)
                    {
                        boss.phase = BOSS_DIRECT;
                        boss.phaseTimer = 0.0f;
                        boss.attackTimer = 0.5f; // initial delay before first shot
                    }
                    break;

                case BOSS_DIRECT:
                    // Fire aimed bullet at player's X every 0.3s for 3s
                    if (boss.attackTimer <= 0.0f)
                    {
                        float playerCenter = player.position.x + player.width / 2.0f;
                        float dx = playerCenter - bossCenter;
                        // Fire toward player's X position
                        FireBossBullet(bossBullets, bossCenter, bossBottom,
                                       dx * 0.8f, BOSS_BULLET_SPEED);
                        boss.attackTimer = 0.3f;
                    }
                    if (boss.phaseTimer >= 3.0f)
                    {
                        boss.phase = BOSS_SPREAD;
                        boss.phaseTimer = 0.0f;
                        boss.attackTimer = 0.6f; // initial delay before first spread
                    }
                    break;

                case BOSS_SPREAD:
                    // Fire 3-bullet cone every 0.6s for 2s
                    if (boss.attackTimer <= 0.0f)
                    {
                        // Center bullet goes straight down
                        FireBossBullet(bossBullets, bossCenter, bossBottom,
                                       0, BOSS_BULLET_SPEED);
                        // Left bullet angled left
                        FireBossBullet(bossBullets, bossCenter - 20.0f, bossBottom,
                                       -120.0f, BOSS_BULLET_SPEED);
                        // Right bullet angled right
                        FireBossBullet(bossBullets, bossCenter + 20.0f, bossBottom,
                                       120.0f, BOSS_BULLET_SPEED);
                        boss.attackTimer = 0.6f;
                    }
                    if (boss.phaseTimer >= 2.0f)
                    {
                        boss.phase = BOSS_RAIN;
                        boss.phaseTimer = 0.0f;
                        boss.attackTimer = 0.4f; // initial delay before first rain drop
                    }
                    break;

                case BOSS_RAIN:
                    // Circular burst — fires 16 bullets evenly in all directions
                    if (boss.attackTimer <= 0.0f)
                    {
                        int numBullets = 16;
                        float angleStep = (2.0f * 3.14159265f) / numBullets;
                        for (int b = 0; b < numBullets; b++)
                        {
                            float angle = angleStep * b;
                            float vx = sinf(angle) * BOSS_BULLET_SPEED;
                            float vy = cosf(angle) * BOSS_BULLET_SPEED;
                            FireBossBullet(bossBullets,
                                           bossCenter,
                                           boss.y + boss.height / 2.0f,
                                           vx, vy);
                        }
                        boss.attackTimer = 1.5f; // delay between each burst
                    }
                    if (boss.phaseTimer >= 4.5f) // 3 bursts then cycle
                    {
                        boss.phase = BOSS_IDLE;
                        boss.phaseTimer = 0.0f;
                        boss.attackTimer = 1.0f;
                    }
                    break;
                }

                // --- Boss HP-Threshold Minion Spawning ---
                // Thresholds: 75%, 50%, 25% of max HP
                int hpThresholds[3] = {
                    (int)(boss.maxHealth * 0.75f),
                    (int)(boss.maxHealth * 0.50f),
                    (int)(boss.maxHealth * 0.25f)
                };
                for (int t = 0; t < 3; t++)
                {
                    if (!boss.minionSpawnFlags[t] && boss.health <= hpThresholds[t])
                    {
                        boss.minionSpawnFlags[t] = true;
                        int minionsToSpawn = GetRandomValue(1, 2);
                        int spawned = 0;
                        for (int e = 0; e < MAX_ENEMIES && spawned < minionsToSpawn; e++)
                        {
                            if (!enemies[e].active)
                            {
                                // Spread minions evenly from boss position
                                float offsetX = (spawned == 0) ? -60.0f : 60.0f;
                                float spawnX = boss.x + boss.width / 2.0f + offsetX;
                                InitEnemy(&enemies[e], ENEMY_DUMMY,
                                          spawnX, boss.y + boss.height + 10.0f);
                                spawned++;
                            }
                        }
                    }
                }
            }

            // --- Move Boss Bullets ---
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active)
                    continue;
                bossBullets[i].x += bossBullets[i].vx * dt;
                bossBullets[i].y += bossBullets[i].vy * dt;
                if (bossBullets[i].y > WINDOW_HEIGHT + 10 ||
                    bossBullets[i].y < -40 ||
                    bossBullets[i].x < -40 || bossBullets[i].x > WINDOW_WIDTH + 40)
                    bossBullets[i].active = false;
            }

            // --- Update Minion Enemies (same logic as PLAYING) ---
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!enemies[i].active || enemies[i].type == ENEMY_DEAD)
                    continue;
                Enemy *e = &enemies[i];
                e->moveTimer += dt;
                if (e->hitFlashFrames > 0)
                    e->hitFlashFrames--;

                // Minions spawned during boss fight are DUMMYs — just drift down
                e->y += e->speed * dt;
                if (e->y > WINDOW_HEIGHT + 50.0f)
                {
                    e->active = false;
                    e->type = ENEMY_DEAD;
                }
            }

            // --- Move Enemy Bullets (from minions) ---
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (!enemyBullets[i].active)
                    continue;
                enemyBullets[i].y += enemyBullets[i].vy * dt;
                if (enemyBullets[i].y > WINDOW_HEIGHT + 10)
                    enemyBullets[i].active = false;
            }

            // --- Collision: Player Bullets vs Boss ---
            if (boss.active)
            {
                Rectangle bossRect = {boss.x, boss.y, boss.width, boss.height};
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    if (!playerBullets[i].active)
                        continue;
                    Rectangle bRect = {
                        playerBullets[i].x, playerBullets[i].y,
                        BULLET_WIDTH, BULLET_HEIGHT};
                    if (CheckCollisionRecs(bRect, bossRect))
                    {
                        playerBullets[i].active = false;
                        boss.health--;
                        boss.hitFlashFrames = 6;

                        if (boss.health <= 0)
                        {
                            boss.active = false;
                            score += SCORE_BOSS;
                            shakeTimer = 0.6f; // Big shake on boss death
                            // Massive particle burst
                            SpawnParticles(particles, boss.x + boss.width / 2.0f,
                                           boss.y + boss.height / 2.0f, 15);
                            state = STATE_GAME_WON;
                        }
                    }
                }
            }

            // --- Collision: Player Bullets vs Minion Enemies ---
            for (int b = 0; b < MAX_PLAYER_BULLETS; b++)
            {
                if (!playerBullets[b].active)
                    continue;
                Rectangle bRect = {
                    playerBullets[b].x, playerBullets[b].y,
                    BULLET_WIDTH, BULLET_HEIGHT};
                for (int e = 0; e < MAX_ENEMIES; e++)
                {
                    if (!enemies[e].active || enemies[e].type == ENEMY_DEAD)
                        continue;
                    Rectangle eRect = {enemies[e].x, enemies[e].y, ENEMY_HITBOX, ENEMY_HITBOX};
                    if (CheckCollisionRecs(bRect, eRect))
                    {
                        playerBullets[b].active = false;
                        enemies[e].health--;
                        if (enemies[e].health <= 0)
                        {
                            score += GetEnemyScore(enemies[e].type);
                            SpawnParticles(particles, enemies[e].x + ENEMY_HITBOX / 2.0f,
                                           enemies[e].y + ENEMY_HITBOX / 2.0f, 5);
                            enemies[e].type = ENEMY_DEAD;
                            enemies[e].active = false;
                        }
                        break;
                    }
                }
            }

            // --- Collision: Boss Bullets vs Player ---
            if (player.invincibleTimer <= 0.0f)
            {
                Rectangle pRect = {
                    player.position.x, PLAYER_Y,
                    (float)player.width, (float)player.height};

                for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                {
                    if (!bossBullets[i].active)
                        continue;
                    Rectangle bRect = {
                        bossBullets[i].x, bossBullets[i].y,
                        ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT};
                    if (CheckCollisionRecs(bRect, pRect))
                    {
                        bossBullets[i].active = false;
                        player.lives--;
                        player.invincibleTimer = PLAYER_INVINCIBLE_TIME;
                        shakeTimer = SHAKE_DURATION;
                        SpawnParticles(particles,
                                       player.position.x + player.width / 2.0f,
                                       PLAYER_Y + player.height / 2.0f, 3);
                    }
                }

                // Also check enemy bullets (from minions)
                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                {
                    if (!enemyBullets[i].active)
                        continue;
                    Rectangle bRect = {
                        enemyBullets[i].x, enemyBullets[i].y,
                        ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT};
                    if (CheckCollisionRecs(bRect, pRect))
                    {
                        enemyBullets[i].active = false;
                        player.lives--;
                        player.invincibleTimer = PLAYER_INVINCIBLE_TIME;
                        shakeTimer = SHAKE_DURATION;
                    }
                }
            }

            // --- Check Game Over ---
            if (player.lives <= 0)
                state = STATE_GAME_LOST;

            break;
        }

        /* ---------------------------------------------------------------
         *  GAME WON / GAME LOST — wait for ENTER to go back to title
         * --------------------------------------------------------------- */
        case STATE_GAME_WON:
        case STATE_GAME_LOST:
        {
            if (IsKeyPressed(KEY_ENTER))
                state = STATE_TITLE;
            break;
        }
        } // end switch(state)

        /* ==============================================================
         *  DRAW
         * ============================================================== */
        BeginDrawing();
        ClearBackground(BG_COLOR);

        // Apply screen shake offset to all drawing via a translate trick:
        // We simply add the offset to all draw positions below.
        int sx = shakeOffsetX;
        int sy = shakeOffsetY;

        /* --- Starfield (always visible) --- */
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

        /* --- Particles (always visible) --- */
        for (int i = 0; i < MAX_PARTICLES; i++)
        {
            if (!particles[i].active)
                continue;
            int sz = (int)particles[i].size;
            if (sz < 1)
                sz = 1;
            DrawRectangle((int)particles[i].x + sx - sz / 2,
                          (int)particles[i].y + sy - sz / 2,
                          sz, sz, particles[i].color);
        }

        /* ---------------------------------------------------------------
         *  Draw per-state content
         * --------------------------------------------------------------- */
        switch (state)
        {
        /* --- TITLE SCREEN --- */
        case STATE_TITLE:
        {
            float time = (float)GetTime();

            // Title
            const char *title = "SPACE SHOOTER";
            int titleWidth = MeasureText(title, 60);
            DrawText(title, (WINDOW_WIDTH - titleWidth) / 2, WINDOW_HEIGHT / 2 - 180, 60, WHITE);

            // Subtitle
            const char *sub = "SELECT LEVEL";
            int subWidth = MeasureText(sub, 22);
            DrawText(sub, (WINDOW_WIDTH - subWidth) / 2, WINDOW_HEIGHT / 2 - 100, 22, LIGHTGRAY);

            // Separator line
            DrawRectangle(WINDOW_WIDTH / 2 - 160, WINDOW_HEIGHT / 2 - 78, 320, 2, (Color){80, 80, 80, 255});

            // Menu options
            const char *menuItems[3] = {
                "Level 1  —  Dummies & Basics",
                "Level 2  —  Tanks & Zigzags",
                "Level 3  —  Boss Fight"
            };
            Color menuColors[3] = {SKYBLUE, GREEN, ORANGE};

            for (int m = 0; m < 3; m++)
            {
                int itemY = WINDOW_HEIGHT / 2 - 55 + m * 55;
                bool selected = (m == menuSelection);

                // Highlight background for selected item
                if (selected)
                {
                    DrawRectangle(WINDOW_WIDTH / 2 - 175, itemY - 8,
                                  350, 42, (Color){255, 255, 255, 20});
                    DrawRectangleLines(WINDOW_WIDTH / 2 - 175, itemY - 8,
                                       350, 42, (Color){menuColors[m].r, menuColors[m].g, menuColors[m].b, 180});
                }

                // Arrow indicator
                Color c = selected ? menuColors[m] : (Color){100, 100, 100, 255};
                int itemWidth = MeasureText(menuItems[m], 24);
                if (selected)
                    DrawText("\xbb", WINDOW_WIDTH / 2 - itemWidth / 2 - 28, itemY, 24, c);
                DrawText(menuItems[m], WINDOW_WIDTH / 2 - itemWidth / 2, itemY, 24, c);
            }

            // Controls hint (blinking)
            if (fmodf(time, 1.2f) < 0.85f)
            {
                const char *hint = "UP / DOWN to navigate    ENTER to start";
                int hintW = MeasureText(hint, 18);
                DrawText(hint, (WINDOW_WIDTH - hintW) / 2, WINDOW_HEIGHT / 2 + 130, 18, (Color){160, 160, 160, 255});
            }

            // Credits
            const char *credits = "A BUET CSE Lab Project";
            int credWidth = MeasureText(credits, 18);
            DrawText(credits, (WINDOW_WIDTH - credWidth) / 2, WINDOW_HEIGHT - 40, 18, (Color){80, 80, 80, 255});
            break;
        }

        /* --- LEVEL TRANSITION --- */
        case STATE_LEVEL_TRANSITION:
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 150});

            const char *levelText = TextFormat("LEVEL %d", currentLevel);
            int tw = MeasureText(levelText, 60);
            DrawText(levelText, (WINDOW_WIDTH - tw) / 2 + sx, WINDOW_HEIGHT / 2 - 40 + sy, 60, WHITE);

            const char *hint = "";
            switch (currentLevel)
            {
            case 1:
                hint = "Clear the dummies!";
                break;
            case 2:
                hint = "Watch out for Tanks and Zigzags!";
                break;
            case 3:
                hint = "Boss incoming... Destroy the support first!";
                break;
            }
            int hw = MeasureText(hint, 22);
            DrawText(hint, (WINDOW_WIDTH - hw) / 2 + sx, WINDOW_HEIGHT / 2 + 30 + sy, 22, YELLOW);
            break;
        }

        /* --- PLAYING --- */
        case STATE_PLAYING:
        {
            // Draw enemies
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!enemies[i].active || enemies[i].type == ENEMY_DEAD)
                    continue;

                float ex = enemies[i].x + sx;
                float ey = enemies[i].y + sy;
                Color tint = (enemies[i].hitFlashFrames > 0) ? RED : WHITE;

                switch (enemies[i].type)
                {
                case ENEMY_DUMMY:
                    DrawTextureEx(dummyTex, (Vector2){ex, ey}, 0, 0.37f, tint);
                    break;
                case ENEMY_BASIC:
                    DrawTextureEx(basicTex, (Vector2){ex, ey}, 0, 0.37f, tint);
                    break;
                case ENEMY_ZIGZAG:
                    DrawTextureEx(zigzagTex, (Vector2){ex, ey}, 0, 0.35f, tint);
                    break;
                case ENEMY_RAPID:
                    DrawTextureEx(rapidTex, (Vector2){ex, ey}, 0, 0.35f, tint);
                    break;
                case ENEMY_TANK:
                    DrawTextureEx(tankTex, (Vector2){ex, ey}, 0, 0.40f, tint);
                    // Health bar above Tank
                    {
                        float barW = (float)ENEMY_HITBOX * enemies[i].health / enemies[i].maxHealth;
                        DrawRectangle((int)ex, (int)ey - 8, ENEMY_HITBOX, 5, DARKGRAY);
                        DrawRectangle((int)ex, (int)ey - 8, (int)barW, 5, RED);
                    }
                    break;
                default:
                    break;
                }
            }

            // Draw player ship (blink when invincible)
            {
                bool drawPlayer = true;
                if (player.invincibleTimer > 0.0f)
                {
                    // Blink: visible every other 0.1s
                    int frame = (int)(player.invincibleTimer * 10.0f);
                    drawPlayer = (frame % 2 == 0);
                }
                if (drawPlayer)
                {
                    playerDestRect.x = player.position.x + sx;
                    playerDestRect.y = PLAYER_Y + sy;
                    DrawTexturePro(spaceshipTex, playerSrcRect, playerDestRect, playerOrigin, 0, WHITE);
                }
            }

            // Draw player bullets
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;
                DrawRectangle((int)playerBullets[i].x + sx, (int)playerBullets[i].y + sy,
                              BULLET_WIDTH, BULLET_HEIGHT, SABER_GREEN);
            }

            // Draw enemy bullets
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (!enemyBullets[i].active)
                    continue;
                DrawRectangle((int)enemyBullets[i].x + sx, (int)enemyBullets[i].y + sy,
                              ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT, RED);
            }

            // HUD — Score, Lives, Level
            DrawText(TextFormat("SCORE: %d", score), 20, 20, 25, WHITE);
            for (int i = 0; i < player.lives; i++)
                DrawTextureEx(heartTex, (Vector2){20.0f + i * 40.0f, 50.0f}, 0, 0.35f, WHITE);
            DrawText(TextFormat("LEVEL %d", currentLevel), WINDOW_WIDTH - 150, 20, 25, YELLOW);
            break;
        }

        /* --- BOSS FIGHT --- */
        case STATE_BOSS_FIGHT:
        {
            // Draw boss (colored rectangle — no texture)
            if (boss.active)
            {
                Color bossColor = (boss.hitFlashFrames > 0) ? RED : (Color){100, 20, 150, 255};
                // Main body
                DrawRectangle((int)(boss.x + sx), (int)(boss.y + sy),
                              (int)boss.width, (int)boss.height, bossColor);
                // Accent stripe
                DrawRectangle((int)(boss.x + sx + 10), (int)(boss.y + sy + boss.height / 2 - 5),
                              (int)boss.width - 20, 10, (Color){200, 50, 50, 255});
                // "Cockpit" detail
                DrawRectangle((int)(boss.x + sx + boss.width / 2 - 15),
                              (int)(boss.y + sy + boss.height - 20),
                              30, 20, (Color){200, 200, 50, 255});

                // Boss health bar at top of screen
                float hpRatio = (float)boss.health / (float)boss.maxHealth;
                int barWidth = WINDOW_WIDTH - 100;
                DrawRectangle(50, 15, barWidth, 14, DARKGRAY);
                DrawRectangle(50, 15, (int)(barWidth * hpRatio), 14, RED);
                DrawRectangleLines(50, 15, barWidth, 14, WHITE);
                DrawText("BOSS", 50, 2, 12, WHITE);
                DrawText(TextFormat("%d / %d", boss.health, boss.maxHealth),
                         50 + barWidth / 2 - 30, 16, 12, WHITE);
            }

            // Draw minion enemies (same as PLAYING draw, but simpler — only DUMMYs)
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!enemies[i].active || enemies[i].type == ENEMY_DEAD)
                    continue;
                float ex = enemies[i].x + sx;
                float ey = enemies[i].y + sy;
                Color tint = (enemies[i].hitFlashFrames > 0) ? RED : WHITE;
                DrawTextureEx(dummyTex, (Vector2){ex, ey}, 0, 0.37f, tint);
            }

            // Draw player ship (with invincibility blink)
            {
                bool drawPlayer = true;
                if (player.invincibleTimer > 0.0f)
                {
                    int frame = (int)(player.invincibleTimer * 10.0f);
                    drawPlayer = (frame % 2 == 0);
                }
                if (drawPlayer)
                {
                    playerDestRect.x = player.position.x + sx;
                    playerDestRect.y = PLAYER_Y + sy;
                    DrawTexturePro(spaceshipTex, playerSrcRect, playerDestRect, playerOrigin, 0, WHITE);
                }
            }

            // Draw player bullets
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active)
                    continue;
                DrawRectangle((int)playerBullets[i].x + sx, (int)playerBullets[i].y + sy,
                              BULLET_WIDTH, BULLET_HEIGHT, SABER_GREEN);
            }

            // Draw boss bullets (orange to distinguish from enemy bullets)
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active)
                    continue;
                DrawRectangle((int)bossBullets[i].x + sx, (int)bossBullets[i].y + sy,
                              ENEMY_BULLET_WIDTH + 2, ENEMY_BULLET_HEIGHT + 2, ORANGE);
            }

            // Draw enemy bullets (from minions)
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
            {
                if (!enemyBullets[i].active)
                    continue;
                DrawRectangle((int)enemyBullets[i].x + sx, (int)enemyBullets[i].y + sy,
                              ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT, RED);
            }

            // HUD
            DrawText(TextFormat("SCORE: %d", score), 20, 35, 25, WHITE);
            for (int i = 0; i < player.lives; i++)
                DrawTextureEx(heartTex, (Vector2){20.0f + i * 40.0f, 65.0f}, 0, 0.35f, WHITE);
            DrawText("LEVEL 3 — BOSS", WINDOW_WIDTH - 220, 35, 25, (Color){255, 100, 100, 255});
            break;
        }

        /* --- GAME WON --- */
        case STATE_GAME_WON:
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            const char *winText = "YOU WON!";
            int ww = MeasureText(winText, 50);
            DrawText(winText, (WINDOW_WIDTH - ww) / 2 + sx, WINDOW_HEIGHT / 2 - 60 + sy, 50, GREEN);
            DrawText(TextFormat("Final Score: %d", score),
                     WINDOW_WIDTH / 2 - 100 + sx, WINDOW_HEIGHT / 2 + sy, 30, WHITE);
            const char *restartText = "Press ENTER to return to title";
            int rw = MeasureText(restartText, 20);
            DrawText(restartText, (WINDOW_WIDTH - rw) / 2, WINDOW_HEIGHT / 2 + 60, 20, LIGHTGRAY);
            break;
        }

        /* --- GAME LOST --- */
        case STATE_GAME_LOST:
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            const char *loseText = "GAME OVER";
            int lw = MeasureText(loseText, 50);
            DrawText(loseText, (WINDOW_WIDTH - lw) / 2 + sx, WINDOW_HEIGHT / 2 - 60 + sy, 50, RED);
            DrawText(TextFormat("Final Score: %d", score),
                     WINDOW_WIDTH / 2 - 100 + sx, WINDOW_HEIGHT / 2 + sy, 30, WHITE);
            const char *restartText = "Press ENTER to return to title";
            int rw = MeasureText(restartText, 20);
            DrawText(restartText, (WINDOW_WIDTH - rw) / 2, WINDOW_HEIGHT / 2 + 60, 20, LIGHTGRAY);
            break;
        }
        } // end draw switch

        EndDrawing();
    }

    /* ==================================================================
     *  CLEANUP — unload textures and close window
     * ================================================================== */
    UnloadTexture(spaceshipTex);
    UnloadTexture(dummyTex);
    UnloadTexture(basicTex);
    UnloadTexture(zigzagTex);
    UnloadTexture(rapidTex);
    UnloadTexture(tankTex);
    UnloadTexture(heartTex);
    CloseWindow();

    return 0;
}
