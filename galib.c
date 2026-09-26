#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    InitWindow(800, 600, "Game");
    SetTargetFPS(60);
    srand(time(NULL));
    Texture2D spaceship=LoadTexture("spaceship.png");
    Texture2D noob=LoadTexture("noob.png");
    Texture2D medium=LoadTexture("medium.png");
    Texture2D strong=LoadTexture("strong.png");

     float spaceshipX=350;
     float spaceshipspeed=5;
     float spaceship_width=120;
     float spaceship_height=60;
     float bulletX=0;
     float bulletY=0;
     bool bulletactive=false;
     float enemyX=0;
     float enemyY=0;
     float enemySpeed=0.25;
     float enemyBulletX = 0;
     float enemyBulletY = 0;
     bool enemyBulletActive = false;
     float enemyShootTimer = 0;
     bool explosionActive = false;
     float explosionX = 0;
     float explosionY = 0;
     float explosionTimer = 0;
     int score=0;
     int enemy[5][11];
     int lives=3;

char bunker[14][23] = {
    "0000111111111111110000",
    "0011111111111111111100",
    "0111111111111111111110",
    "1111111111111111111111",
    "1111111111111111111111",
    "1111111111111111111111",
    "1111111111111111111111",
    "1111111111111111111111",
    "1111111111111111111111",
    "1111111000000001111111",
    "1111100000000000111111",
    "1111000000000000011111",
    "1111000000000000011111",
    "1111000000000000011111"
};

for(int i=0;i<5;i++){
    for(int j=0;j<11;j++){
    if(i >= 3){
           enemy[i][j] = 1;
    }
 else if(i >= 1){
    enemy[i][j] = 2;
}
else{
    enemy[i][j] = 3;
}
    }
}

           Rectangle source = {0,0, spaceship.width, spaceship.height};
           Rectangle destination = {spaceshipX, 500, spaceship_width, spaceship_height};
           Vector2 origin = {0, 0};

    while (!WindowShouldClose())
    {

        if (IsKeyDown(KEY_LEFT)){

    spaceshipX = spaceshipX - spaceshipspeed;
        }

if (IsKeyDown(KEY_RIGHT)){
    spaceshipX=spaceshipX+spaceshipspeed;
}
if (spaceshipX<0){
    spaceshipX=0;
}
if (spaceshipX>650){
    spaceshipX=650;
}
if(IsKeyPressed(KEY_SPACE) && ! bulletactive){
    bulletX=spaceshipX+57;
    bulletY=500;
    bulletactive=true;
}
if(bulletactive){
    bulletY-=20;
    if(bulletY<0){
        bulletactive=false;
    }
}
if(!enemyBulletActive && enemyShootTimer <= 0){

    int randomColumn = rand() % 11;
    int randomRow = rand() % 5;

    if(enemy[randomRow][randomColumn] != 0){

        enemyBulletX = 90 + randomColumn * 50 + enemyX + 20;
        enemyBulletY = 100 + randomRow * 50 + enemyY + 40;

        enemyBulletActive = true;
        enemyShootTimer = 2;
    }
}

if(enemyBulletActive){
    enemyBulletY += 5;

    if(enemyBulletY > 600){
        enemyBulletActive = false;
    }
}
if(enemyShootTimer > 0){
    enemyShootTimer -= 1.0 / 60.0;
}

for(int i=0;i<5;i++){
    for(int j=0;j<11;j++){
        if(enemy[i][j]!=0){

            float x=90+j*50+enemyX;
            float y=100+i*50+enemyY;

            if(bulletX >= x && bulletX <= x+40 &&
               bulletY >= y && bulletY <= y+40){

            if(enemy[i][j] == 1){
                score += 10;
               }
            else if(enemy[i][j] == 2){
               score += 20;
               }
           else if(enemy[i][j] == 3){
               score += 30;
               }

enemy[i][j] = 0;
bulletactive = false;


                explosionX = x;
                explosionY = y;
                explosionActive = true;
                explosionTimer = 0;
            }
        }
    }
}


//enemy bullet hit the player
for(int i=0;i<5;i++){
    for(int j=0;j<11;j++){
        if(enemyBulletActive){
        Rectangle enemyBulletRec={enemyBulletX,enemyBulletY,4,10};
        Rectangle playerRec={spaceshipX,500,spaceship_width,spaceship_height};
        if (CheckCollisionRecs(enemyBulletRec,playerRec)){
            enemyBulletActive=false;
            lives--;

        }
    }
    }
}

enemyX += enemySpeed;
if(enemyX > 170){
    enemySpeed = -0.25;
    enemyY+=20;
}
if(enemyX<-95){
    enemySpeed=0.25;
    enemyY+=20;
}
if(explosionActive){
    explosionTimer += 0.1;

    if(explosionTimer > 1){
        explosionActive = false;
    }
}

        BeginDrawing();
        ClearBackground((Color){8, 8, 12, 255});


  
int bunkerX[4] = {55, 265, 475, 685};

for(int b = 0; b < 4; b++){

    for(int i = 0; i < 14; i++){
        for(int j = 0; j < 22; j++){

            if(bunker[i][j] == '1'){
                DrawRectangle(bunkerX[b] + j * 4,430 + i * 4, 4, 4,GREEN );
            }

        }
    }

}
        DrawText(TextFormat("SCORE: %d", score), 20, 20, 25, WHITE);
        DrawText(TextFormat("LIVES :%d ",lives),20,50,25,GREEN);
        for(int i=0; i<5; i++){
        for(int j=0;j<11;j++){
        float x = 90+j*50+enemyX;
        float y= 100+i*50+enemyY;
 if(enemy[i][j] == 0){
    continue;
}
else if(enemy[i][j] == 1){
    DrawTextureEx(noob,(Vector2){x,y},0,0.37,WHITE);
}
else if(enemy[i][j] == 2){
    DrawTextureEx(medium,(Vector2){x-5,y},0,0.35,WHITE);
}
else{
    DrawTextureEx(strong,(Vector2){x-12,y},0,0.4,WHITE);
}
    }
}

        destination.x = spaceshipX;

        DrawTexturePro(spaceship, source, destination, origin, 0, WHITE);
        if(bulletactive){
            DrawRectangle(bulletX,bulletY,5,15,RED);
        }
        if(enemyBulletActive){
    DrawRectangle(enemyBulletX, enemyBulletY, 5, 15, RED);
}

if(explosionActive){
    float size =explosionTimer*12;
     Color explosionColor = WHITE;
    DrawCircle(explosionX + 15, explosionY + 15, size, explosionColor);
}
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
