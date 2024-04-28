#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

void drawTextureResized(Texture2D texture, int x, int y, int width, int height, float rotation) {
    DrawTexturePro(
        texture,
        (Rectangle) { 0, 0, texture.width, texture.height },
        (Rectangle) { x, y, width, height },
        Vector2Zero(), rotation,
        WHITE
    );
}

typedef enum {
    GAME_RUNNING,
    GAME_WIN,
    GAME_LOSE
} GameState;

int gameState = GAME_RUNNING;

#define SCREEN_WIDTH 600
#define SCREEN_HEIGHT 800

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 16

time_t begunGameTime;
time_t currentGameTime;
int flagsLeft;
uint8_t **gameField = NULL;

typedef enum {
    CELL_CLEAR_0,
    CELL_CLEAR_1,
    CELL_CLEAR_2,
    CELL_CLEAR_3,
    CELL_CLEAR_4,
    CELL_CLEAR_5,
    CELL_CLEAR_6,
    CELL_CLEAR_7,
    CELL_CLEAR_8,
    CELL_CLEAR_BOMB,

    CELL_NORMAL,
    CELL_BOMB,
    CELL_NORMAL_FLAG,
    CELL_BOMB_FLAG
} CellType;

#define FIELD_CELL_SIZE 40
#define FIELD_CELL_INNER_SIZE 38

#define CELL_COLOR_NORMAL (Color){ 210, 210, 210, 255 }
#define CELL_COLOR_NORMAL_HOVER (Color){ 230, 230, 230, 255 }
#define CELL_COLOR_CLEAR (Color){ 160, 160, 160, 255 }

#define SMILEY_SIZE 80

Texture2D *textures = NULL;

#define TEXTURES_NUM 12
typedef enum {
    TEXTURE_CELL_1,
    TEXTURE_CELL_2,
    TEXTURE_CELL_3,
    TEXTURE_CELL_4,
    TEXTURE_CELL_5,
    TEXTURE_CELL_6,
    TEXTURE_CELL_7,
    TEXTURE_CELL_8,
    TEXTURE_BOMB,
    TEXTURE_FLAG,
    TEXTURE_SMILEY_HAPPY,
    TEXTURE_SMILEY_DEAD
} TextureID;

void loadTextures() {
    textures = (Texture2D *)malloc(TEXTURES_NUM * sizeof(Texture2D));

    textures[TEXTURE_CELL_1] = LoadTexture("../img/field_1.png");
    textures[TEXTURE_CELL_2] = LoadTexture("../img/field_2.png");
    textures[TEXTURE_CELL_3] = LoadTexture("../img/field_3.png");
    textures[TEXTURE_CELL_4] = LoadTexture("../img/field_4.png");
    textures[TEXTURE_CELL_5] = LoadTexture("../img/field_5.png");
    textures[TEXTURE_CELL_6] = LoadTexture("../img/field_6.png");
    textures[TEXTURE_CELL_7] = LoadTexture("../img/field_7.png");
    textures[TEXTURE_CELL_8] = LoadTexture("../img/field_8.png");
    textures[TEXTURE_BOMB] = LoadTexture("../img/bomb.png");
    textures[TEXTURE_FLAG] = LoadTexture("../img/flag.png");
    textures[TEXTURE_SMILEY_HAPPY] = LoadTexture("../img/smiley_happy.png");
    textures[TEXTURE_SMILEY_DEAD] = LoadTexture("../img/smiley_dead.png");
}

void generateGameField() {
    int rngValue;
    gameField = (uint8_t **)malloc(FIELD_WIDTH * sizeof(uint8_t *));

    for (size_t x = 0; x < FIELD_WIDTH; x++) {
        gameField[x] = (uint8_t *)malloc(FIELD_HEIGHT * sizeof(uint8_t));

        for (size_t y = 0; y < FIELD_HEIGHT; y++) {
            rngValue = GetRandomValue(0, 100);
            gameField[x][y] = (rngValue < 10 ? CELL_BOMB : CELL_NORMAL);
            if (gameField[x][y] == CELL_BOMB)
                flagsLeft++;
        }
    }
}

uint8_t tryGetFieldValue(int x, int y) {
    if (0 <= x && x < FIELD_WIDTH && 0 <= y && y < FIELD_HEIGHT)
        return gameField[x][y];
    return UINT8_MAX;
}

uint8_t countBombs(size_t x, size_t y) {
    uint8_t counter = 0;

    int xMax = x + 1;
    int yMax = y + 1;
    uint8_t val;
    for (int m_x = x - 1; m_x <= xMax; m_x++) {
        for (int m_y = y - 1; m_y <= yMax; m_y++) {
            val = tryGetFieldValue(m_x, m_y);
            if (val == CELL_BOMB || val == CELL_BOMB_FLAG)
                counter++;
        }
    }

    return counter;
}

bool canAutoClear(size_t x, size_t y) {
    if (CELL_CLEAR_8 < gameField[x][y] || gameField[x][y] == CELL_CLEAR_0)
        return false;
    
    uint8_t flagCounter = 0;
    bool hasNormalFields = false;

    int xMax = x + 1;
    int yMax = y + 1;
    uint8_t val;
    for (int m_x = x - 1; m_x <= xMax; m_x++) {
        for (int m_y = y - 1; m_y <= yMax; m_y++) {
            val = tryGetFieldValue(m_x, m_y);
            if (val == CELL_NORMAL_FLAG || val == CELL_BOMB_FLAG)
                flagCounter++;
            else if (val == CELL_NORMAL)
                hasNormalFields = true;
        }
    }

    return hasNormalFields && (flagCounter == gameField[x][y]);
}

const int drawPosX = (SCREEN_WIDTH - FIELD_WIDTH * FIELD_CELL_SIZE) / 2;
const int drawPosY = (SCREEN_HEIGHT - FIELD_HEIGHT * FIELD_CELL_SIZE) / 2 + 30;

void getSelectedCell(size_t *x, size_t *y) {
    Vector2 mousePos = GetMousePosition();

    mousePos = Vector2Subtract(mousePos, (Vector2){ drawPosX, drawPosY });
    mousePos = Vector2Scale(mousePos, 1.f / FIELD_CELL_SIZE);

    if (0 <= mousePos.x && (int)mousePos.x < FIELD_WIDTH &&
        0 <= mousePos.y && (int)mousePos.y < FIELD_HEIGHT) {
        *x = (size_t)mousePos.x;
        *y = (size_t)mousePos.y;
    } else {
        *x = SIZE_MAX;
        *y = SIZE_MAX;
    }
}

void drawField(size_t selectedX, size_t selectedY) {
    int xPos = drawPosX;
    int yPos = drawPosY;

    DrawRectangle(xPos - 1, yPos - 1, FIELD_CELL_SIZE * FIELD_WIDTH + 2, FIELD_CELL_SIZE * FIELD_HEIGHT + 2, DARKGRAY);
    for (size_t x = 0; x < FIELD_WIDTH; x++, xPos += FIELD_CELL_SIZE, yPos = drawPosY) {
        for (size_t y = 0; y < FIELD_HEIGHT; y++, yPos += FIELD_CELL_SIZE) {
            // Regular fields
            if (gameField[x][y] == CELL_NORMAL ||
                gameField[x][y] == CELL_BOMB)
                DrawRectangle(
                    xPos + 1, yPos + 1,
                    FIELD_CELL_INNER_SIZE, FIELD_CELL_INNER_SIZE,
                    (x == selectedX && y == selectedY ? CELL_COLOR_NORMAL_HOVER : CELL_COLOR_NORMAL)
                );
            // Flags
            else if (gameField[x][y] == CELL_NORMAL_FLAG || gameField[x][y] == CELL_BOMB_FLAG) {
                DrawRectangle(
                    xPos + 1, yPos + 1,
                    FIELD_CELL_INNER_SIZE, FIELD_CELL_INNER_SIZE,
                    (x == selectedX && y == selectedY ? CELL_COLOR_NORMAL_HOVER : CELL_COLOR_NORMAL)
                );
                drawTextureResized(textures[TEXTURE_FLAG], xPos, yPos, FIELD_CELL_SIZE, FIELD_CELL_SIZE, 0);
            // Cleared fields with numbers
            } else if (CELL_CLEAR_0 <= gameField[x][y] && gameField[x][y] <= CELL_CLEAR_8) {
                DrawRectangle(
                    xPos + 1, yPos + 1,
                    FIELD_CELL_INNER_SIZE, FIELD_CELL_INNER_SIZE,
                    (x == selectedX && y == selectedY && canAutoClear(x, y) ? CELL_COLOR_NORMAL_HOVER : CELL_COLOR_CLEAR)
                );
                if (gameField[x][y] != CELL_CLEAR_0)
                    drawTextureResized(textures[gameField[x][y] - CELL_CLEAR_1 + TEXTURE_CELL_1], xPos, yPos, FIELD_CELL_SIZE, FIELD_CELL_SIZE, 0);
            // Bombs
            } else if (gameField[x][y] == CELL_CLEAR_BOMB) {
                DrawRectangle(xPos + 1, yPos + 1, FIELD_CELL_INNER_SIZE, FIELD_CELL_INNER_SIZE, CELL_COLOR_CLEAR);
                drawTextureResized(textures[TEXTURE_BOMB], xPos, yPos, FIELD_CELL_SIZE, FIELD_CELL_SIZE, 0);
            }
        }
    }
}

void showBombs() {
    gameState = GAME_LOSE;
    for (size_t x = 0; x < FIELD_WIDTH; x++) {
        for (size_t y = 0; y < FIELD_HEIGHT; y++) {
            if (gameField[x][y] == CELL_BOMB || gameField[x][y] == CELL_BOMB_FLAG)
                gameField[x][y] = CELL_CLEAR_BOMB;
        }
    }
}

void revealCell(size_t x, size_t y) {
    if (gameField[x][y] == CELL_BOMB)
        showBombs();
    else if (gameField[x][y] == CELL_NORMAL) {
        gameField[x][y] = CELL_CLEAR_0 + countBombs(x, y);

        if (gameField[x][y] == CELL_CLEAR_0) {
            int xMax = x + 1;
            int yMax = y + 1;
            uint8_t val;
            for (int m_x = x - 1; m_x <= xMax; m_x++) {
                for (int m_y = y - 1; m_y <= yMax; m_y++) {
                    val = tryGetFieldValue(m_x, m_y);
                    if (val != UINT8_MAX && val != CELL_CLEAR_0)
                        revealCell(m_x, m_y);
                }
            }
        }
    } else if (canAutoClear(x, y)) {
        int xMax = x + 1;
        int yMax = y + 1;
        uint8_t val;
        for (int m_x = x - 1; m_x <= xMax; m_x++) {
            for (int m_y = y - 1; m_y <= yMax; m_y++) {
                val = tryGetFieldValue(m_x, m_y);
                if (val != UINT8_MAX && val == CELL_NORMAL)
                    revealCell(m_x, m_y);
            }
        }
    }
}

bool checkForWin() {
    bool win = true;
    for (size_t x = 0; x < FIELD_WIDTH; x++) {
        for (size_t y = 0; y < FIELD_HEIGHT; y++) {
            if (gameField[x][y] == CELL_NORMAL || gameField[x][y] == CELL_NORMAL_FLAG)
                win = false;
        }
    }
    if (win)
        gameState = GAME_WIN;
}

void freeMemory() {
    for (size_t x = 0; x < TEXTURES_NUM; x++)
        free(gameField[x]);
    free(gameField);

    for (size_t i = 0; i < TEXTURES_NUM; i++)
        UnloadTexture(textures[i]);
    free(textures);
}

int smileyPosX = (SCREEN_WIDTH - SMILEY_SIZE) / 2;
int smileyPosY = (drawPosY - SMILEY_SIZE) / 2;


int main(int argc, const char **argv) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Minesweeper");
    SetTargetFPS(60);

    begunGameTime = time(NULL);
    SetRandomSeed(begunGameTime * 100);

    loadTextures();

    size_t selectedX, selectedY;
    generateGameField();

    char flagText[4], timeText[4];
    while (!WindowShouldClose()) {
        getSelectedCell(&selectedX, &selectedY);

        if (gameState == GAME_RUNNING) {
            if (currentGameTime < 999)
                currentGameTime = time(NULL) - begunGameTime;
            if (selectedX != SIZE_MAX) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    revealCell(selectedX, selectedY);
                    checkForWin();
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    if (gameField[selectedX][selectedY] == CELL_NORMAL || gameField[selectedX][selectedY] == CELL_BOMB) {
                        gameField[selectedX][selectedY] += 2;
                        flagsLeft--;
                    } else if (gameField[selectedX][selectedY] == CELL_NORMAL_FLAG || gameField[selectedX][selectedY] == CELL_BOMB_FLAG) {
                        gameField[selectedX][selectedY] -= 2;
                        flagsLeft++;
                    }
                }
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            if (smileyPosX < mousePos.x && mousePos.x < smileyPosX + SMILEY_SIZE &&
                smileyPosY < mousePos.y && mousePos.y < smileyPosY + SMILEY_SIZE) {
                for (size_t x = 0; x < TEXTURES_NUM; x++)
                    free(gameField[x]);
                free(gameField);
                flagsLeft = 0;
                generateGameField();
                begunGameTime = time(NULL);
                gameState = GAME_RUNNING;
            }
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            drawField(selectedX, selectedY);
            drawTextureResized(
                textures[gameState == GAME_LOSE ? TEXTURE_SMILEY_DEAD : TEXTURE_SMILEY_HAPPY],
                smileyPosX, smileyPosY,
                SMILEY_SIZE, SMILEY_SIZE,
                0.f
            );
            if (currentGameTime < 10)
                snprintf(timeText, 4, "00%lld", currentGameTime);
            else if (currentGameTime < 100)
                snprintf(timeText, 4, "0%lld", currentGameTime);
            else
                snprintf(timeText, 4, "%lld", currentGameTime);
            DrawText(timeText, smileyPosX + SMILEY_SIZE + 90, smileyPosY + (SMILEY_SIZE - 30) / 2, 30, BLACK);

            if (flagsLeft < 10 && flagsLeft >= 0)
                snprintf(flagText, 4, "00%d", flagsLeft);
            else if (flagsLeft < 100 && flagsLeft > -10)
                snprintf(flagText, 4, "0%d", flagsLeft);
            else
                snprintf(flagText, 4, "%d", flagsLeft);
            DrawText(flagText, smileyPosX - 140, smileyPosY + (SMILEY_SIZE - 30) / 2, 30, BLACK);
        EndDrawing();
    }

    CloseWindow();
    freeMemory();
    
    return 0;
}
