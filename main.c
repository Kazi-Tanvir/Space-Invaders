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
#define ENEMY_ROWS 6
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
    PLAYING,
    GAME_WON,
    GAME_LOST
} GameState;

typedef struct Explosion
{
    Vector2 position;
    float timer;
    bool active;
} Explosion;


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
static void InitEnemy(Enemy *e, int row, int col, int type)
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
            e->speed = 20;
            e->health = 2;           // spec: health 2
            e->shootCooldown = 999.0f; // dummy doesn't fire
            e->maxHealth = e->health;
            break;
        case ENEMY_BASIC:
            e->speed = 35;
            e->health = 1;
            e->shootCooldown = 1.5f;
            e->maxHealth = e->health;
            break;
        case ENEMY_ZIGZAG:
            e->speed = 50;
            e->health = 1;           // spec: health 1
            e->shootCooldown = 1.0f;
            e->maxHealth = e->health;
            break;
        case ENEMY_TANK:
            e->speed = 15;
            e->health = 3;           // spec: health 3
            e->shootCooldown = 3.0f;
            e->maxHealth = e->health;
            break;
        case ENEMY_RAPID:
            e->speed = 80;
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

// Get the enemy type for a given row in the formation layout
static EnemyType GetRowEnemyType(int row)
{
    switch (row)
    {
        case 0: return ENEMY_TANK;    // top row: slow, 3 HP
        case 1: return ENEMY_RAPID;   // fast movers
        case 2: return ENEMY_ZIGZAG;  // zigzag row 1 of 2
        case 3: return ENEMY_ZIGZAG;  // zigzag row 2 of 2
        case 4: return ENEMY_BASIC;   // standard enemies
        case 5: return ENEMY_DUMMY;   // front row: shield, no fire, 2 HP
        default: return ENEMY_BASIC;
    }
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

    // Enemy matrix — each enemy has its own absolute position
    Enemy enemies[ENEMY_ROWS][ENEMY_COLS];
    int enemyCount = ENEMY_ROWS * ENEMY_COLS;

    for (int i = 0; i < ENEMY_ROWS; i++)
    {
        EnemyType rowType = GetRowEnemyType(i);
        for (int j = 0; j < ENEMY_COLS; j++)
        {
            InitEnemy(&enemies[i][j], i, j, rowType);
        }
    }

    // Global timers (replaced EnemyGrid)
    float enemyShootTimer = 0.0f;
    float dropTimer = 0.0f;

    // Enemy bullets pool
    Bullet enemyBullets[MAX_ENEMY_BULLETS] = {0};

    // Explosion state
    Explosion explosion = {.position = {0, 0}, .timer = 0, .active = false};

    // Score and game state
    int score = 0;
    GameState state = PLAYING;


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

        if (state == PLAYING)
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
            for (int row = 0; row < ENEMY_ROWS; row++)
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
                for (int i = 0; i < ENEMY_ROWS; i++)
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
                    randRow = GetRandomValue(0, ENEMY_ROWS - 1);
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

                for (int j = 0; j < ENEMY_ROWS; j++)
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

            // Check win/lose conditions
            if (enemyCount <= 0)
                state = GAME_WON;
            if (player.lives <= 0)
                state = GAME_LOST;
        }

        // RESTART GAME
        if (state == GAME_WON || state == GAME_LOST)
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                player.lives = PLAYER_LIVES;
                player.position.x = PLAYER_START_X;
                score = 0;
                enemyCount = ENEMY_ROWS * ENEMY_COLS;
                enemyShootTimer = 0.0f;
                dropTimer = 0.0f;

                for (int i = 0; i < ENEMY_ROWS; i++)
                {
                    EnemyType rowType = GetRowEnemyType(i);
                    for (int j = 0; j < ENEMY_COLS; j++)
                    {
                        InitEnemy(&enemies[i][j], i, j, rowType);
                    }
                }

                // Reset bullets
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                    playerBullets[i].active = false;
                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                    enemyBullets[i].active = false;

                state = PLAYING;
            }
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
        for (int i = 0; i < ENEMY_ROWS; i++)
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

        // Draw win/lose screen overlay
        if (state == GAME_WON)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("YOU WON!", WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 30, 40, GREEN);
            DrawText(TextFormat("Final Score: %d", score), WINDOW_WIDTH / 2 - 110, WINDOW_HEIGHT / 2 + 20, 25, WHITE);
            DrawText("Press ENTER to play again", WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 + 80, 20, WHITE);
        }
        else if (state == GAME_LOST)
        {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (Color){0, 0, 0, 180});
            DrawText("GAME OVER", WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2 - 30, 40, RED);
            DrawText(TextFormat("Final Score: %d", score), WINDOW_WIDTH / 2 - 110, WINDOW_HEIGHT / 2 + 20, 25, WHITE);
            DrawText("Press ENTER to play again", WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 + 80, 20, WHITE);
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
