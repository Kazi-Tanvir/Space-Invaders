#include "raylib.h"
#include <math.h>

// Window settings
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 800

// Starfield settings
#define STAR_COUNT_FAR  150
#define STAR_COUNT_MID   80
#define STAR_COUNT_NEAR  30
#define STAR_TOTAL (STAR_COUNT_FAR + STAR_COUNT_MID + STAR_COUNT_NEAR)
#define STAR_SPEED_FAR   30.0f
#define STAR_SPEED_MID   70.0f
#define STAR_SPEED_NEAR 160.0f

// Player settings
#define PLAYER_START_X (WINDOW_WIDTH / 2 - 60) // Centered horizontally
#define PLAYER_Y (WINDOW_HEIGHT - 100) // Positioned near the bottom
#define PLAYER_SPEED 300.0f
#define PLAYER_WIDTH 120
#define PLAYER_HEIGHT 60
#define PLAYER_MAX_X (WINDOW_WIDTH - PLAYER_WIDTH)
#define PLAYER_LIVES 3

// Player bullet settings
#define MAX_PLAYER_BULLETS 25
#define BULLET_SPEED 1000.0f
#define BULLET_WIDTH 5
#define BULLET_HEIGHT 15
#define PLAYER_SHOOT_COOLDOWN 0.1f

// Enemy matrix layout
#define MAX_ENEMY_ROWS 8          // max rows any level can have (sized for level 2's 7 rows)
#define ENEMY_COLS 11
#define ENEMY_SPACING_X 55        // horizontal spacing (50px enemy + 5px gap)
#define ENEMY_SPACING_Y 60        // vertical spacing between rows
#define ENEMY_HITBOX 50           // hitbox matches visual size
#define ENEMY_START_X 90          // left edge of formation
#define ENEMY_START_Y 100         // top edge of formation
#define ENEMY_BOUND_LEFT 10
#define ENEMY_BOUND_RIGHT (WINDOW_WIDTH - ENEMY_HITBOX - 10)
#define ENEMY_DROP_STEP 20.0f
#define ENEMY_DROP_INTERVAL 8.0f  // seconds between Y-drops

// Level system
#define NUM_LEVELS 3

// Zigzag settings
#define ZIGZAG_PERIOD 2.0f        // seconds for one full triangle wave cycle
#define ZIGZAG_AMPLITUDE 15.0f    // vertical oscillation in pixels (peak to center)

// Enemy bullet settings
#define MAX_ENEMY_BULLETS 10
#define ENEMY_BULLET_SPEED 300.0f
#define ENEMY_BULLET_WIDTH 5
#define ENEMY_BULLET_HEIGHT 15
#define ENEMY_SHOOT_COOLDOWN 1.0f

// Explosion effect settings
#define EXPLOSION_GROW_RATE 6.0f
#define EXPLOSION_MAX_TIME 1.0f
#define EXPLOSION_SCALE 12.0f
#define EXPLOSION_OFFSET 15

// Boss settings
#define BOSS_WIDTH 140
#define BOSS_HEIGHT 90
#define BOSS_MAX_HEALTH 50
#define BOSS_SPEED 60.0f
#define BOSS_START_Y 60
#define MAX_BOSS_BULLETS 150
#define BOSS_RAPID_SPEED 350.0f
#define BOSS_STAR_SPEED 150.0f
#define BOSS_CIRCLE_SPEED 120.0f
#define BOSS_RAPID_COOLDOWN 0.5f
#define BOSS_STAR_COOLDOWN 0.6f
#define BOSS_CIRCLE_COOLDOWN 1.5f
#define BOSS_PHASE_ATTACK 3.0f    // seconds per attack phase
#define BOSS_PHASE_PAUSE 1.5f     // seconds pause between attacks
#define BOSS_MINION_WAVE1_HP 35   // spawn BASIC minions at this HP
#define BOSS_MINION_WAVE2_HP 20   // spawn RAPID minions at this HP
#define BOSS_MINION_COLS 5        // enemies per minion wave

// Score values per enemy type
#define SCORE_DUMMY   5
#define SCORE_BASIC  10
#define SCORE_ZIGZAG 15
#define SCORE_RAPID  20
#define SCORE_TANK   30



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
} Player;

typedef struct Bullet
{
    Vector2 position;
    float speed;
    bool active;
} Bullet;

typedef enum EnemyType{
    ENEMY_DEAD = 0,
    ENEMY_DUMMY,
    ENEMY_BASIC,
    ENEMY_ZIGZAG,
    ENEMY_TANK,
    ENEMY_RAPID,
}EnemyType;

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
    int hitFlashFrames;  // >0 means draw with RED tint
    bool active;
} Enemy;

typedef enum GameState
{
    MAIN_MENU,    // level selection screen
    PLAYING,
    GAME_WON,
    GAME_LOST,
    BOSS_FIGHT,   // level 3 placeholder
} GameState;

typedef struct Explosion
{
    Vector2 position;
    float timer;
    bool active;
} Explosion;

// Directional bullet for boss attacks (arbitrary vx,vy instead of just speed)
typedef struct BossBullet
{
    float x, y;
    float vx, vy;  // velocity components — allows any direction
    bool active;
} BossBullet;

typedef struct Boss
{
    float x, y;             // top-left of hitbox
    float speed;            // horizontal movement speed
    float direction;        // +1.0 right, -1.0 left
    float yDir;             // +1.0 down, -1.0 up (gentle Y drift)
    float yTimer;           // elapsed time for Y bounce period
    int health;
    int maxHealth;
    int hitFlashFrames;     // >0 = draw RED tint
    int attackPhase;        // 0=rapid,1=pause,2=star,3=pause,4=circle,5=pause (then loops)
    float phaseTimer;       // time spent in current phase
    float shootTimer;       // cooldown within current attack phase
    bool minionsSpawned1;   // true after wave 1 (BASIC) spawned
    bool minionsSpawned2;   // true after wave 2 (RAPID) spawned
    bool active;
} Boss;

typedef struct LevelConfig
{
    int numRows;                           // active enemy rows for this level
    EnemyType rowTypes[MAX_ENEMY_ROWS];    // enemy type per row (top to bottom)
    float speedMultiplier;                 // scales all enemy base speeds
} LevelConfig;

// Level definitions (index 0 = level 1, index 1 = level 2, index 2 = level 3)
static const LevelConfig levels[NUM_LEVELS] = {
    // Level 1: 4 rows, 3 enemy types, normal speed
    {
        .numRows = 4,
        .rowTypes = { ENEMY_TANK, ENEMY_RAPID, ENEMY_BASIC, ENEMY_BASIC },
        .speedMultiplier = 1.0f,
    },
    // Level 2: 7 rows, all 5 enemy types, 1.4x speed
    {
        .numRows = 7,
        .rowTypes = { ENEMY_TANK, ENEMY_RAPID, ENEMY_ZIGZAG, ENEMY_ZIGZAG,
                      ENEMY_BASIC, ENEMY_BASIC, ENEMY_DUMMY },
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
    e->direction = 1.0f;   // all start moving right (row-coherent)
    e->moveTimer = 0;
    e->shootTimer = (float)GetRandomValue(0, 200) / 100.0f; // stagger initial shots
    e->hitFlashFrames = 0;
    e->active = true;
    switch(type){
        case ENEMY_DUMMY:
            e->speed = 20 * speedMul;
            e->health = 2;           // spec: health 2
            e->shootCooldown = 999.0f; // dummy doesn't fire
            e->maxHealth = e->health;
            break;
        case ENEMY_BASIC:
            e->speed = 35 * speedMul;
            e->health = 1;
            e->shootCooldown = 1.5f;
            e->maxHealth = e->health;
            break;
        case ENEMY_ZIGZAG:
            e->speed = 50 * speedMul;
            e->health = 1;           // spec: health 1
            e->shootCooldown = 1.0f;
            e->maxHealth = e->health;
            break;
        case ENEMY_TANK:
            e->speed = 15 * speedMul;
            e->health = 3;           // spec: health 3
            e->shootCooldown = 3.0f;
            e->maxHealth = e->health;
            break;
        case ENEMY_RAPID:
            e->speed = 80 * speedMul;
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
}


//  Main

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Game");
    SetTargetFPS(60);

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
    Texture2D dummy        = LoadTexture("resources/dummy.png");    // ENEMY_DUMMY
    Texture2D basicTex     = LoadTexture("resources/dummy.png");    // ENEMY_BASIC (same sprite, cyan tint)
    Texture2D zigzag       = LoadTexture("resources/zigzag.png");   // ENEMY_ZIGZAG
    Texture2D rapidTex     = LoadTexture("resources/zigzag.png");   // ENEMY_RAPID (same sprite, yellow tint)
    Texture2D tank         = LoadTexture("resources/tank.png");     // ENEMY_TANK
    Texture2D heartTex     = LoadTexture("resources/heart.png");

    // Set up the player
    Player player = {
        .position = {PLAYER_START_X, PLAYER_Y},
        .speed = PLAYER_SPEED,
        .width = PLAYER_WIDTH,
        .height = PLAYER_HEIGHT,
        .lives = PLAYER_LIVES,
    };

    // Player bullets
    Bullet playerBullets[MAX_PLAYER_BULLETS] = {0};
    float shootCooldown = 0.0f;

    // Enemy matrix — sized for the largest possible level
    Enemy enemies[MAX_ENEMY_ROWS][ENEMY_COLS];
    int numRows = 0;       // set by ResetLevel() from the chosen level config
    int enemyCount = 0;    // set by ResetLevel()
    int currentLevel = 0;  // index into levels[] (0-based)

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

    // Score and game state — start on the main menu
    int score = 0;
    GameState state = MAIN_MENU;

    // Init enemy array to dead so nothing is drawn before a level is chosen
    for (int i = 0; i < MAX_ENEMY_ROWS; i++)
        for (int j = 0; j < ENEMY_COLS; j++)
            enemies[i][j].type = ENEMY_DEAD;


    // Rects for drawing the player ship texture
    Rectangle source = {0, 0, spaceshipTex.width, spaceshipTex.height};
    Rectangle destination = {player.position.x, player.position.y, player.width, player.height};
    Vector2 origin = {0, 0};

    // Game loop
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

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

        // --- Main menu: level selection ---
        if (state == MAIN_MENU)
        {
            if (IsKeyPressed(KEY_ONE))
            {
                currentLevel = 0;
                ResetLevel(enemies, &enemyCount, &numRows, &levels[currentLevel],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                state = PLAYING;
            }
            if (IsKeyPressed(KEY_TWO))
            {
                currentLevel = 1;
                ResetLevel(enemies, &enemyCount, &numRows, &levels[currentLevel],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                state = PLAYING;
            }
            if (IsKeyPressed(KEY_THREE))
            {
                currentLevel = 2;
                // Reset player/bullets via ResetLevel (0 rows = no formation)
                ResetLevel(enemies, &enemyCount, &numRows, &levels[2],
                           playerBullets, enemyBullets, &player,
                           &enemyShootTimer, &dropTimer, &score);
                // Clear boss bullets
                for (int i = 0; i < MAX_BOSS_BULLETS; i++) bossBullets[i].active = false;
                ResetBoss(&boss);
                state = BOSS_FIGHT;
            }
        }

        if (state == PLAYING || state == BOSS_FIGHT)
        {

            // Move player left/right
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

            if (IsKeyDown(KEY_SPACE) && shootCooldown <= 0.0f)
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

                if (!hasAlive) continue;

                // Find leftmost and rightmost alive enemy X positions in this row
                float leftmostX = (float)WINDOW_WIDTH;
                float rightmostX = 0.0f;
                for (int col = 0; col < ENEMY_COLS; col++)
                {
                    if (enemies[row][col].type != ENEMY_DEAD)
                    {
                        if (enemies[row][col].x < leftmostX) leftmostX = enemies[row][col].x;
                        if (enemies[row][col].x > rightmostX) rightmostX = enemies[row][col].x;
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
                            float wave = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0→1→0 triangle
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

                    if (CheckCollisionRecs(bulletRect, playerRect))
                    {
                        enemyBullets[i].active = false;
                        player.lives--;

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
            // Boss horizontal movement (bounces off screen edges)
            boss.x += boss.speed * boss.direction * dt;
            if (boss.x > WINDOW_WIDTH - BOSS_WIDTH - 20) { boss.x = WINDOW_WIDTH - BOSS_WIDTH - 20; boss.direction = -1.0f; }
            if (boss.x < 20)                              { boss.x = 20;                              boss.direction =  1.0f; }

            // Boss Y movement — triangle wave (sharp reversals like zigzag, not smooth sine)
            // Period: 4 seconds, amplitude: ±30px from BOSS_START_Y
            boss.yTimer += dt;
            {
                float period = 4.0f;
                float t = fmodf(boss.yTimer, period) / period;          // 0..1
                float wave = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0→1→0 triangle
                boss.y = BOSS_START_Y + (wave - 0.5f) * 60.0f;         // ±30px
            }

            // Decrement boss hit flash
            if (boss.hitFlashFrames > 0) boss.hitFlashFrames--;

            // --- Attack phase cycling ---
            boss.phaseTimer += dt;
            boss.shootTimer -= dt;
            float bossCX = boss.x + BOSS_WIDTH / 2.0f;   // boss centre X
            float bossCY = boss.y + BOSS_HEIGHT;          // bottom of boss (bullet origin)

            // Phase 0: Rapid — aimed at player
            if (boss.attackPhase == 0 && boss.phaseTimer < BOSS_PHASE_ATTACK)
            {
                if (boss.shootTimer <= 0.0f)
                {
                    float pdx = (player.position.x + player.width / 2.0f) - bossCX;
                    float pdy = (float)PLAYER_Y - bossCY;
                    float dist = sqrtf(pdx * pdx + pdy * pdy);
                    if (dist > 0.0f)
                    {
                        for (int i = 0; i < MAX_BOSS_BULLETS; i++)
                        {
                            if (!bossBullets[i].active)
                            {
                                bossBullets[i].x  = bossCX;
                                bossBullets[i].y  = bossCY;
                                bossBullets[i].vx = (pdx / dist) * BOSS_RAPID_SPEED;
                                bossBullets[i].vy = (pdy / dist) * BOSS_RAPID_SPEED;
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
                            if (!bossBullets[i].active) { slot = i; break; }
                        if (slot < 0) break;
                        bossBullets[slot].x  = bossCX;
                        bossBullets[slot].y  = bossCY;
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
                            if (!bossBullets[i].active) { slot = i; break; }
                        if (slot < 0) break;
                        bossBullets[slot].x  = bossCX;
                        bossBullets[slot].y  = bossCY;
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
                boss.phaseTimer  = 0.0f;
                boss.shootTimer  = 0.0f;
            }

            // --- Move all boss bullets ---
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active) continue;
                bossBullets[i].x += bossBullets[i].vx * dt;
                bossBullets[i].y += bossBullets[i].vy * dt;
                // Deactivate when off screen
                if (bossBullets[i].x < -20 || bossBullets[i].x > WINDOW_WIDTH + 20 ||
                    bossBullets[i].y < -20 || bossBullets[i].y > WINDOW_HEIGHT + 20)
                    bossBullets[i].active = false;
            }

            // --- Player bullets vs Boss ---
            Rectangle bossRect = {boss.x, boss.y, BOSS_WIDTH, BOSS_HEIGHT};
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (!playerBullets[i].active) continue;
                if (CheckCollisionPointRec(playerBullets[i].position, bossRect))
                {
                    playerBullets[i].active = false;
                    boss.health--;
                    boss.hitFlashFrames = 5;
                    score += 2;

                    // Trigger explosion at hit point
                    explosion.position = playerBullets[i].position;
                    explosion.active   = true;
                    explosion.timer    = 0;
                }
            }

            // --- Boss bullets vs Player ---
            Rectangle playerRect = {player.position.x, PLAYER_Y, player.width, player.height};
            for (int i = 0; i < MAX_BOSS_BULLETS; i++)
            {
                if (!bossBullets[i].active) continue;
                Rectangle bRect = {bossBullets[i].x - 4, bossBullets[i].y - 4, 8, 8};
                if (CheckCollisionRecs(bRect, playerRect))
                {
                    bossBullets[i].active = false;
                    player.lives--;
                    explosion.position = (Vector2){player.position.x + player.width / 2.0f,
                                                   PLAYER_Y + player.height / 2.0f};
                    explosion.active = true;
                    explosion.timer  = 0;
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
                if (numRows < 1) numRows = 1;
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
                if (numRows < 2) numRows = 2;
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

        // --- Win/lose: return to main menu ---
        if (state == GAME_WON || state == GAME_LOST)
        {
            if (IsKeyPressed(KEY_ENTER))
                state = MAIN_MENU;
        }

        // --- Boss fight placeholder ---
        if (state == BOSS_FIGHT)
        {
            if (IsKeyPressed(KEY_ENTER))
                state = MAIN_MENU;
        }

        // Draw everything
        BeginDrawing();
        ClearBackground(BLACK);

        // --- Draw starfield ---
        for (int i = 0; i < STAR_TOTAL; i++)
        {
            unsigned char b = stars[i].brightness;
            Color sc = {b, b, b, 255};
            if (stars[i].size <= 1.0f)
                DrawPixel((int)stars[i].x, (int)stars[i].y, sc);
            else
                DrawCircleV((Vector2){stars[i].x, stars[i].y}, stars[i].size, sc);
        }

        // Draw HUD (score, lives, enemy count)
        DrawText(TextFormat("SCORE: %d", score), 20, 20, 25, WHITE);

        for (int i = 0; i < player.lives; i++)
        {
            DrawTextureEx(heartTex, (Vector2){20 + i * 60, 50}, 0, 0.5f, WHITE);
        }

        DrawText(TextFormat("ENEMIES: %d", enemyCount), 20, 80, 25, RED);

        // Draw all living enemies
        for (int i = 0; i < numRows; i++)
        {
            for (int j = 0; j < ENEMY_COLS; j++)
            {
                if (enemies[i][j].type == ENEMY_DEAD)
                    continue;

                Vector2 pos = {enemies[i][j].x, enemies[i][j].y};
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
        destination.x = player.position.x;
        DrawTexturePro(spaceshipTex, source, destination, origin, 0, WHITE);

        // Draw player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        {
            if (playerBullets[i].active)
            {
                DrawRectangle(
                    (int)playerBullets[i].position.x,
                    (int)playerBullets[i].position.y,
                    BULLET_WIDTH, BULLET_HEIGHT, WHITE);
            }
        }

        // Draw enemy bullets
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        {
            if (enemyBullets[i].active)
            {
                DrawRectangle(
                    (int)enemyBullets[i].position.x,
                    (int)enemyBullets[i].position.y,
                    ENEMY_BULLET_WIDTH, ENEMY_BULLET_HEIGHT, RED);
            }
        }

        // Draw explosion circle if active
        if (explosion.active)
        {
            float size = explosion.timer * EXPLOSION_SCALE;
            DrawCircle(
                (int)(explosion.position.x + EXPLOSION_OFFSET),
                (int)(explosion.position.y + EXPLOSION_OFFSET),
                size, WHITE);
        }

        // --- Main menu draw ---
        if (state == MAIN_MENU)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 200});
            DrawText("SPACE INVADERS", WINDOW_WIDTH / 2 - 175, 180, 50, WHITE);
            DrawText("Select Level:", WINDOW_WIDTH / 2 - 100, 300, 30, WHITE);
            DrawText("1  -  Easy   [ BASIC | RAPID | TANK ]",          WINDOW_WIDTH / 2 - 215, 370, 25, GREEN);
            DrawText("2  -  Hard   [ All 5 types | 1.4x Speed ]",       WINDOW_WIDTH / 2 - 225, 415, 25, YELLOW);
            DrawText("3  -  Boss   [ Rapid | Star | Circle attacks ]",   WINDOW_WIDTH / 2 - 260, 460, 25, RED);
            DrawText(TextFormat("Last Score: %d", score), WINDOW_WIDTH / 2 - 80, 540, 20, LIGHTGRAY);
        }

        // --- Win/lose overlay ---
        if (state == GAME_WON)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("YOU WON!", WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 40, 40, GREEN);
            DrawText(TextFormat("Score: %d", score), WINDOW_WIDTH / 2 - 70, WINDOW_HEIGHT / 2 + 15, 28, WHITE);
            DrawText("Press ENTER for menu", WINDOW_WIDTH / 2 - 140, WINDOW_HEIGHT / 2 + 70, 22, LIGHTGRAY);
        }
        else if (state == GAME_LOST)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("GAME OVER", WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2 - 40, 40, RED);
            DrawText(TextFormat("Score: %d", score), WINDOW_WIDTH / 2 - 70, WINDOW_HEIGHT / 2 + 15, 28, WHITE);
            DrawText("Press ENTER for menu", WINDOW_WIDTH / 2 - 140, WINDOW_HEIGHT / 2 + 70, 22, LIGHTGRAY);
        }
        // --- Boss fight draw ---
        if (state == BOSS_FIGHT && boss.active)
        {
            // Draw boss (procedural — purple rectangle with inner detail)
            Color bossTint = (boss.hitFlashFrames > 0) ? RED : DARKPURPLE;
            Color bossInner = (boss.hitFlashFrames > 0) ? MAROON : PURPLE;
            DrawRectangleRounded((Rectangle){boss.x, boss.y, BOSS_WIDTH, BOSS_HEIGHT}, 0.2f, 8, bossTint);
            DrawRectangleRounded((Rectangle){boss.x + 15, boss.y + 10, BOSS_WIDTH - 30, BOSS_HEIGHT - 20}, 0.3f, 8, bossInner);
            // Core "eye"
            DrawCircle((int)(boss.x + BOSS_WIDTH / 2), (int)(boss.y + BOSS_HEIGHT / 2), 12, (boss.hitFlashFrames > 0) ? ORANGE : PINK);

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
                    DrawCircle((int)bossBullets[i].x, (int)bossBullets[i].y, 5, ORANGE);
            }
        }

        EndDrawing();
    }

    // Clean up
    UnloadTexture(spaceshipTex);
    UnloadTexture(dummy);
    UnloadTexture(basicTex);
    UnloadTexture(zigzag);
    UnloadTexture(rapidTex);
    UnloadTexture(tank);
    UnloadTexture(heartTex);

    CloseWindow();
    return 0;
}
