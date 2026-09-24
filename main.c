#include "raylib.h"

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
#define PLAYER_SHOOT_COOLDOWN 0.5f

// Enemy grid layout
#define ENEMY_ROWS 5
#define ENEMY_COLS 11
#define ENEMY_CELL_SIZE 50
#define ENEMY_HITBOX 40
#define ENEMY_GRID_X 90
#define ENEMY_GRID_Y 100
#define ENEMY_SPEED 25.0f
#define ENEMY_RIGHT_BOUND 140
#define ENEMY_LEFT_BOUND -60
#define ENEMY_DROP_STEP 20.0f


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

typedef struct EnemyGrid
{
    float offsetX;
    float offsetY;
    float speed;
    float shootTimer;
} EnemyGrid;

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

// Position of an enemy[row][col]

static Vector2 GetEnemyPosition(int row, int col, EnemyGrid grid)
{
    return (Vector2){
        ENEMY_GRID_X + col * ENEMY_CELL_SIZE + grid.offsetX,
        ENEMY_GRID_Y + row * ENEMY_CELL_SIZE + grid.offsetY};
}



// Reset single enemy into a fresh state
static void InitEnemy(Enemy *e, int row, int col, int type, EnemyGrid grid)
{
    e->type = type;
    Vector2 pos = GetEnemyPosition(row, col, grid);
    e->x = pos.x;
    e->y = pos.y;
    e->direction = GetRandomValue(0, 1) ? 1.0f : -1.0f;
    e->moveTimer = 0;
    e->shootTimer = 0;
   
    e->hitFlashFrames = 0;
    e->active = true;
    switch(type){
        case ENEMY_DUMMY:
            e->speed=20;
            e->health=1;
            e->shootCooldown=2;
            e->maxHealth=e->health;
            break;
        case ENEMY_BASIC:
            e->speed=35;
            e->health=1;
            e->shootCooldown=1.5f;
            e->maxHealth=e->health;
            break;
        case ENEMY_ZIGZAG:
            e->speed=50;
            e->health=2;
            e->shootCooldown=1;
            e->maxHealth=e->health;
            break;
        case ENEMY_TANK:
            e->speed=10;
            e->health=3;
            e->shootCooldown=3;
            e->maxHealth=e->health;
            break;
        case ENEMY_RAPID:
            e->speed=100;
            e->health=1;
            e->shootCooldown=0.5f;
            e->maxHealth=e->health;
            break;
        case ENEMY_DEAD:
            e->speed=0;
            break;
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

    // Enemy grid state
    Enemy enemies[ENEMY_ROWS][ENEMY_COLS];
    EnemyGrid grid = {
        .offsetX = 0,
        .offsetY = 0,
        .speed = ENEMY_SPEED,
        .shootTimer = 0,
    };
    int enemyCount = ENEMY_ROWS * ENEMY_COLS; // track alive enemies

    for (int i = 0; i < ENEMY_ROWS; i++)
    {
        for (int j = 0; j < ENEMY_COLS; j++)
        {
            if (i >= 3)
                enemies[i][j].type = ENEMY_DUMMY;
            else if (i >= 1)
                enemies[i][j].type = ENEMY_ZIGZAG;
            else
                enemies[i][j].type = ENEMY_TANK;
        }
    }

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

            // Enemy randomly shoots a bullet
            grid.shootTimer += dt;

            if (grid.shootTimer >= ENEMY_SHOOT_COOLDOWN)
            {
                int randCol;
                int randRow;
                do
                {
                    randCol = GetRandomValue(0, ENEMY_COLS - 1);
                    randRow = GetRandomValue(0, ENEMY_ROWS - 1);

                } while (enemies[randRow][randCol].type == ENEMY_DEAD);

                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                {
                    if (!enemyBullets[i].active)
                    {
                        Vector2 pos = GetEnemyPosition(randRow, randCol, grid);
                        enemyBullets[i].position.x = pos.x + 20;
                        enemyBullets[i].position.y = pos.y + 40;
                        enemyBullets[i].speed = ENEMY_BULLET_SPEED;
                        enemyBullets[i].active = true;
                        break;
                    }
                }
                grid.shootTimer = 0.0f;
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

            // Check if player bullets hit any enemies
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
                            Vector2 pos = GetEnemyPosition(j, k, grid);
                            Rectangle enemyRect = {pos.x, pos.y, ENEMY_HITBOX, ENEMY_HITBOX};

                            if (CheckCollisionPointRec(playerBullets[i].position, enemyRect))
                            {
                                score += GetEnemyScore(enemies[j][k].type);
                                enemies[j][k].type = ENEMY_DEAD;
                                playerBullets[i].active = false;
                                enemyCount--;

                                explosion.position = pos;
                                explosion.active = true;
                                explosion.timer = 0;
                            }
                        }
                    }
                }
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

            // Move the enemy grid side to side, drop on bounce
            grid.offsetX += grid.speed * dt;

            if (grid.offsetX > ENEMY_RIGHT_BOUND)
            {
                grid.offsetX = ENEMY_RIGHT_BOUND;
                grid.speed = -ENEMY_SPEED;
                grid.offsetY += ENEMY_DROP_STEP;
            }
            else if (grid.offsetX < ENEMY_LEFT_BOUND)
            {
                grid.offsetX = ENEMY_LEFT_BOUND;
                grid.speed = ENEMY_SPEED;
                grid.offsetY += ENEMY_DROP_STEP;
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
//RESTART GAME
        if (state == GAME_WON || state == GAME_LOST)
        {
        
            if (IsKeyPressed(KEY_ENTER))
            {
                player.lives = PLAYER_LIVES;
                score = 0;
                enemyCount = ENEMY_ROWS * ENEMY_COLS;
                grid.offsetX = 0;
                grid.offsetY = 0;
                grid.speed = ENEMY_SPEED;
                grid.shootTimer = 0;

                for (int i = 0; i < ENEMY_ROWS; i++)
                {
                    for (int j = 0; j < ENEMY_COLS; j++)
                    {
                        if (i >= 3)
                            enemies[i][j].type = ENEMY_DUMMY;
                        else if (i >= 1)
                            enemies[i][j].type = ENEMY_ZIGZAG;
                        else
                            enemies[i][j].type = ENEMY_TANK;
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

                Vector2 pos = GetEnemyPosition(i, j, grid);

                switch (enemies[i][j].type)
                {
                case ENEMY_DUMMY:
                    DrawTextureEx(dummy, pos, 0, .37f, WHITE);
                    break;
                case ENEMY_BASIC:
                    DrawTextureEx(basicTex, (Vector2){pos.x, pos.y}, 0, .37f, SKYBLUE);
                    break;
                case ENEMY_ZIGZAG:
                    DrawTextureEx(zigzag, (Vector2){pos.x - 5, pos.y}, 0, 0.35f, WHITE);
                    break;
                case ENEMY_TANK:
                    DrawTextureEx(tank, (Vector2){pos.x - 12, pos.y}, 0, 0.4f, WHITE);
                    break;
                case ENEMY_RAPID:
                    DrawTextureEx(rapidTex, (Vector2){pos.x - 5, pos.y}, 0, 0.35f, YELLOW);
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
