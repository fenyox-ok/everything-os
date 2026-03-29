#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>

// ═══════════════════════════════════════════════════════════════════════════
//  SHARED HARDWARE
// ═══════════════════════════════════════════════════════════════════════════

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VRX 34
#define VRY 35
#define SW  25

// ═══════════════════════════════════════════════════════════════════════════
//  HIGH SCORE (LittleFS)
// ═══════════════════════════════════════════════════════════════════════════

int hsSnake    = 0;
int hsTetris   = 0;
int hsFlappy   = 0;
int hsPlatform = 0;
int hsPong     = 0;  // best round duration in seconds

bool lfsReady = false;

void hsLoad() {
  lfsReady = LittleFS.begin(true);  // true = format if mount fails
  if (!lfsReady) return;
  auto readInt = [](const char* path, int def) -> int {
    File f = LittleFS.open(path, "r");
    if (!f) return def;
    int v = f.parseInt();
    f.close();
    return v;
  };
  hsSnake    = readInt("/hs_snake.txt",    0);
  hsTetris   = readInt("/hs_tetris.txt",   0);
  hsFlappy   = readInt("/hs_flappy.txt",   0);
  hsPlatform = readInt("/hs_platform.txt", 0);
  hsPong     = readInt("/hs_pong.txt",     0);
}

void hsSave(const char* path, int val) {
  if (!lfsReady) return;
  File f = LittleFS.open(path, "w");
  if (!f) return;
  f.print(val);
  f.close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

void snakeInit();
void pongInit();
void mazeInit();
void platformInit();
void wifiScanInit();
void btScanInit();
void cubeInit();
void tetrisInit();
void flappyInit();
void diceInit();
void laptopMonitorInit();
void shutdownInit();
void stopwatchInit();

// ═══════════════════════════════════════════════════════════════════════════
//  TRIPLE-CLICK TO MENU
// ═══════════════════════════════════════════════════════════════════════════

// Returns true once the button has been pressed 3 times within 600ms.
// Resets automatically after firing or after the window expires.
bool checkTripleClick() {
  static int           clicks       = 0;
  static unsigned long windowStart  = 0;
  static bool          lastPressed  = false;

  bool pressed = (digitalRead(SW) == LOW);

  if (pressed && !lastPressed) {           // rising edge
    unsigned long now = millis();
    if (clicks == 0 || now - windowStart < 600) {
      if (clicks == 0) windowStart = now;
      clicks++;
    } else {
      clicks      = 1;                     // missed window, restart
      windowStart = now;
    }
    if (clicks >= 3) {
      clicks = 0;
      lastPressed = pressed;
      return true;
    }
  }

  // Expire window
  if (clicks > 0 && millis() - windowStart >= 600) clicks = 0;

  lastPressed = pressed;
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENU
// ═══════════════════════════════════════════════════════════════════════════

enum GameState { MENU, GAME_SNAKE, GAME_PONG, GAME_MAZE, GAME_PLATFORM, GAME_WIFI, GAME_BT, GAME_CUBE, GAME_TETRIS, GAME_FLAPPY, GAME_DICE, GAME_LAPTOP, GAME_SHUTDOWN, GAME_STOPWATCH };
GameState currentState = MENU;

const char* menuItems[] = { "Flappy Bird", "Tetris", "Snake", "Pong", "Maze", "Platform", "Wifi Scan", "Bluetooth Scan", "3D Cube", "Dice", "Stopwatch", "Laptop Monitor", "Shutdown" };
const int   MENU_COUNT  = 13;
int         menuIndex   = 0;

unsigned long menuLastMove  = 0;
unsigned long lastFrame     = 0;
unsigned long menuLockUntil = 0;
const int     FRAME_MS      = 20;

void sharedDrawCenteredText(const char* text, int y, int size = 1) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

void drawMenuItems(int dotCountdown) {
  sharedDrawCenteredText("EVERYTHING-OS", 0, 1);
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  const int visible = 3;
  int startIdx = 0;
  if (menuIndex >= visible) startIdx = menuIndex - visible + 1;

  for (int i = 0; i < visible; i++) {
    int itemIdx = startIdx + i;
    if (itemIdx >= MENU_COUNT) break;
    int y = 14 + i * 15;
    if (itemIdx == menuIndex) {
      display.fillRect(0, y - 1, SCREEN_WIDTH - 8, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(menuItems[itemIdx], 0, y, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - 8 - w) / 2, y);
    display.print(menuItems[itemIdx]);
  }

  // Scroll indicator dots on right edge
  display.setTextColor(SSD1306_WHITE);
  for (int i = 0; i < MENU_COUNT; i++) {
    int dy = 14 + i * ((SCREEN_HEIGHT - 14) / MENU_COUNT);
    if (i == menuIndex) display.fillCircle(SCREEN_WIDTH - 3, dy + 3, 2, SSD1306_WHITE);
    else                display.drawPixel(SCREEN_WIDTH - 3, dy + 3, SSD1306_WHITE);
  }

  // Countdown dots at bottom
  if (dotCountdown > 0) {
    int totalW = dotCountdown * 6;
    int startX = (SCREEN_WIDTH - totalW) / 2;
    for (int d = 0; d < dotCountdown; d++)
      display.fillCircle(startX + d * 6 + 2, SCREEN_HEIGHT - 3, 2, SSD1306_WHITE);
  }

  display.display();
}

void drawMenu() {
  display.clearDisplay();
  drawMenuItems(0);
}

void runMenu() {
  int xVal = analogRead(VRX);
  const int lo = 800, hi = 3200;
  bool locked = millis() < menuLockUntil;

  if (locked) {
    display.clearDisplay();
    drawMenuItems((int)((menuLockUntil - millis()) / 75));
    return;
  }

  if (millis() - menuLastMove > 200) {
    if (xVal < lo) {
      menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
      menuLastMove = millis();
    } else if (xVal > hi) {
      menuIndex = (menuIndex + 1) % MENU_COUNT;
      menuLastMove = millis();
    }
  }

  if (digitalRead(SW) == LOW) {
    delay(200);
    if      (menuIndex == 0)  { currentState = GAME_FLAPPY;     flappyInit();        }
    else if (menuIndex == 1)  { currentState = GAME_TETRIS;     tetrisInit();        }
    else if (menuIndex == 2)  { currentState = GAME_SNAKE;      snakeInit();         }
    else if (menuIndex == 3)  { currentState = GAME_PONG;       pongInit();          }
    else if (menuIndex == 4)  { currentState = GAME_MAZE;       mazeInit();          }
    else if (menuIndex == 5)  { currentState = GAME_PLATFORM;   platformInit();      }
    else if (menuIndex == 6)  { currentState = GAME_WIFI;       wifiScanInit();      }
    else if (menuIndex == 7)  { currentState = GAME_BT;         btScanInit();        }
    else if (menuIndex == 8)  { currentState = GAME_CUBE;       cubeInit();          }
    else if (menuIndex == 9)  { currentState = GAME_DICE;       diceInit();          }
    else if (menuIndex == 10) { currentState = GAME_STOPWATCH;  stopwatchInit();     }
    else if (menuIndex == 11) { currentState = GAME_LAPTOP;     laptopMonitorInit(); }
    else if (menuIndex == 12) { currentState = GAME_SHUTDOWN;   shutdownInit();      }
  }

  drawMenu();
}

// ═══════════════════════════════════════════════════════════════════════════
//  SNAKE
// ═══════════════════════════════════════════════════════════════════════════

#define GRID       4
#define MAX_SNAKE  200

struct SnakePoint { int x, y; };

SnakePoint    snakeBody[MAX_SNAKE];
int           snakeLength;
SnakePoint    snakeFood;
int           snakeDirX, snakeDirY;
unsigned long snakeLastMove;
int           snakeSpeed;
unsigned long snakeLastResetTime;
bool          snakeWaitStart;
bool          snakeGameOver;
int           snakeScore;

void snakePlaceFood() {
  snakeFood.x = (random(0, SCREEN_WIDTH  / GRID)) * GRID;
  snakeFood.y = (random(0, SCREEN_HEIGHT / GRID)) * GRID;
}

void snakeInit() {
  snakeLength        = 3;
  snakeBody[0]       = {64, 32};
  snakeBody[1]       = {60, 32};
  snakeBody[2]       = {56, 32};
  snakeDirX          = 1;
  snakeDirY          = 0;
  snakeLastMove      = 0;
  snakeSpeed         = 150;
  snakeLastResetTime = 0;
  snakeWaitStart     = true;
  snakeGameOver      = false;
  snakeScore         = 0;
  snakePlaceFood();
}

void snakeReadJoystick() {
  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  const int minVal = 1000, maxVal = 3000, deadzone = 200;

  if      (xVal < minVal - deadzone && snakeDirY !=  1) { snakeDirX =  0; snakeDirY = -1; }
  else if (xVal > maxVal + deadzone && snakeDirY != -1) { snakeDirX =  0; snakeDirY =  1; }
  if      (yVal < minVal - deadzone && snakeDirX != -1) { snakeDirX =  1; snakeDirY =  0; }
  else if (yVal > maxVal + deadzone && snakeDirX !=  1) { snakeDirX = -1; snakeDirY =  0; }
}

void snakeMove() {
  for (int i = snakeLength - 1; i > 0; i--)
    snakeBody[i] = snakeBody[i - 1];

  snakeBody[0].x += snakeDirX * GRID;
  snakeBody[0].y += snakeDirY * GRID;

  if (snakeBody[0].x >= SCREEN_WIDTH)  snakeBody[0].x = 0;
  if (snakeBody[0].x < 0)              snakeBody[0].x = SCREEN_WIDTH - GRID;
  if (snakeBody[0].y >= SCREEN_HEIGHT) snakeBody[0].y = 0;
  if (snakeBody[0].y < 0)              snakeBody[0].y = SCREEN_HEIGHT - GRID;

  if (snakeBody[0].x == snakeFood.x && snakeBody[0].y == snakeFood.y) {
    if (snakeLength < MAX_SNAKE) snakeLength++;
    snakeScore++;
    snakePlaceFood();
  }

  if (millis() - snakeLastResetTime > 500) {
    for (int i = 1; i < snakeLength; i++) {
      if (snakeBody[0].x == snakeBody[i].x && snakeBody[0].y == snakeBody[i].y) {
        snakeGameOver = true;
        snakeLastResetTime = millis();
        break;
      }
    }
  }
}

void snakeDraw() {
  display.clearDisplay();
  // Food as small diamond
  display.drawPixel(snakeFood.x + 2, snakeFood.y,     SSD1306_WHITE);
  display.drawPixel(snakeFood.x,     snakeFood.y + 2, SSD1306_WHITE);
  display.drawPixel(snakeFood.x + 2, snakeFood.y + 4, SSD1306_WHITE);
  display.drawPixel(snakeFood.x + 4, snakeFood.y + 2, SSD1306_WHITE);
  display.drawPixel(snakeFood.x + 2, snakeFood.y + 2, SSD1306_WHITE);
  // Snake body
  for (int i = 0; i < snakeLength; i++)
    display.fillRect(snakeBody[i].x, snakeBody[i].y, GRID, GRID, SSD1306_WHITE);
  // Score
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 1);
  display.print(snakeScore);
  display.display();
}

void snakeLoop() {
  if (snakeWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("SNAKE", 6, 2);
    char hsBuf[16];
    snprintf(hsBuf, sizeof(hsBuf), "Best: %d", hsSnake);
    sharedDrawCenteredText(hsBuf, 28);
    sharedDrawCenteredText("Eat the food", 38);
    sharedDrawCenteredText("Press button", 52);
    display.display();
    if (digitalRead(SW) == LOW) { snakeWaitStart = false; delay(200); }
    return;
  }

  if (snakeGameOver) {
    if (snakeScore > hsSnake) { hsSnake = snakeScore; hsSave("/hs_snake.txt", hsSnake); }
    display.clearDisplay();
    sharedDrawCenteredText("GAME OVER",  10, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "Score: %d", snakeScore);
    sharedDrawCenteredText(buf, 30);
    snprintf(buf, sizeof(buf), "Best:  %d", hsSnake);
    sharedDrawCenteredText(buf, 40);
    sharedDrawCenteredText("3x: menu", 54);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (digitalRead(SW) == LOW) { delay(200); snakeInit(); snakeWaitStart = false; }
    return;
  }

  snakeReadJoystick();
  if (millis() - snakeLastMove > snakeSpeed) {
    snakeMove();
    snakeLastMove = millis();
  }
  snakeDraw();
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PONG
// ═══════════════════════════════════════════════════════════════════════════

#define PADDLE_W     4
#define PADDLE_H     16
#define PADDLE_SPEED 3
#define BALL_SIZE    4
#define PLAYER_X     4
#define BOT_X        (SCREEN_WIDTH - PADDLE_W - 4)

int  pongPlayerY, pongBotY;
int  pongBallX, pongBallY;
int  pongBallDX, pongBallDY;
int  pongPlayerScore, pongBotScore;
bool pongGameOver, pongWaitStart;
int  pongWinner;
unsigned long pongRoundStart = 0;
int  pongBestRound = 0;  // best round in seconds

void pongResetBall(int toward) {
  pongBallX  = SCREEN_WIDTH  / 2 - BALL_SIZE / 2;
  pongBallY  = SCREEN_HEIGHT / 2 - BALL_SIZE / 2;
  pongBallDX = (toward == 1) ? -3 : 3;
  pongBallDY = (random(0, 2) == 0) ? 2 : -2;
}

void pongInit() {
  pongPlayerY     = (SCREEN_HEIGHT - PADDLE_H) / 2;
  pongBotY        = (SCREEN_HEIGHT - PADDLE_H) / 2;
  pongPlayerScore = 0;
  pongBotScore    = 0;
  pongGameOver    = false;
  pongWaitStart   = true;
  pongWinner      = 0;
  pongBestRound   = 0;
  pongRoundStart  = 0;
  pongResetBall(1);
}

void pongReadJoystick() {
  int yVal = analogRead(VRX);
  const int lo = 800, hi = 3200;
  if      (yVal < lo) pongPlayerY -= PADDLE_SPEED;
  else if (yVal > hi) pongPlayerY += PADDLE_SPEED;
  pongPlayerY = constrain(pongPlayerY, 0, SCREEN_HEIGHT - PADDLE_H);
}

void pongMoveBot() {
  int botCenter  = pongBotY  + PADDLE_H / 2;
  int ballCenter = pongBallY + BALL_SIZE / 2;

  // Only track ball when it's heading toward the bot, adds reaction delay
  if (pongBallDX > 0) {
    // Slight deadzone so bot doesn't react to tiny offsets
    if      (ballCenter < botCenter - 3) pongBotY -= PADDLE_SPEED - 1;
    else if (ballCenter > botCenter + 3) pongBotY += PADDLE_SPEED - 1;
  } else {
    // Ball going away — drift slowly back to center
    int center = (SCREEN_HEIGHT - PADDLE_H) / 2;
    if      (pongBotY < center - 2) pongBotY += 1;
    else if (pongBotY > center + 2) pongBotY -= 1;
  }

  // Occasional random miss: every ~4 seconds nudge bot away from ball
  static unsigned long lastMiss = 0;
  static int missDir = 0;
  if (millis() - lastMiss > 4000) {
    lastMiss = millis();
    missDir  = (random(0, 3) == 0) ? (random(0, 2) == 0 ? 1 : -1) : 0;
  }
  pongBotY += missDir;

  pongBotY = constrain(pongBotY, 0, SCREEN_HEIGHT - PADDLE_H);
}

void pongMoveBall() {
  pongBallX += pongBallDX;
  pongBallY += pongBallDY;

  if (pongBallY <= 0)                         { pongBallY = 0;                     pongBallDY = -pongBallDY; }
  if (pongBallY + BALL_SIZE >= SCREEN_HEIGHT)  { pongBallY = SCREEN_HEIGHT - BALL_SIZE; pongBallDY = -pongBallDY; }

  // Player paddle
  if (pongBallDX < 0 &&
      pongBallX <= PLAYER_X + PADDLE_W && pongBallX >= PLAYER_X &&
      pongBallY + BALL_SIZE >= pongPlayerY && pongBallY <= pongPlayerY + PADDLE_H) {
    pongBallX  = PLAYER_X + PADDLE_W;
    pongBallDX = -pongBallDX;
    int rel = (pongBallY + BALL_SIZE/2) - (pongPlayerY + PADDLE_H/2);
    pongBallDY = rel / 4;
    if (pongBallDY == 0) pongBallDY = 1;
  }

  // Bot paddle
  if (pongBallDX > 0 &&
      pongBallX + BALL_SIZE >= BOT_X && pongBallX + BALL_SIZE <= BOT_X + PADDLE_W + 2 &&
      pongBallY + BALL_SIZE >= pongBotY && pongBallY <= pongBotY + PADDLE_H) {
    pongBallX  = BOT_X - BALL_SIZE;
    pongBallDX = -pongBallDX;
    int rel = (pongBallY + BALL_SIZE/2) - (pongBotY + PADDLE_H/2);
    pongBallDY = rel / 4;
    if (pongBallDY == 0) pongBallDY = -1;
  }

  if (pongBallX < 0) {
    int roundSec = (int)((millis() - pongRoundStart) / 1000);
    if (roundSec > pongBestRound) pongBestRound = roundSec;
    pongBotScore++;
    if (pongBotScore >= 10) { pongGameOver = true; pongWinner = 2; return; }
    pongResetBall(1); pongRoundStart = millis();
  }
  if (pongBallX > SCREEN_WIDTH) {
    int roundSec = (int)((millis() - pongRoundStart) / 1000);
    if (roundSec > pongBestRound) pongBestRound = roundSec;
    pongPlayerScore++;
    if (pongPlayerScore >= 10) { pongGameOver = true; pongWinner = 1; return; }
    pongResetBall(-1); pongRoundStart = millis();
  }
}

void pongDraw() {
  display.clearDisplay();
  for (int y = 0; y < SCREEN_HEIGHT; y += 6)
    display.drawFastVLine(SCREEN_WIDTH / 2, y, 3, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH/2 - 18, 0); display.print(pongPlayerScore);
  display.setCursor(SCREEN_WIDTH/2 + 10, 0); display.print(pongBotScore);
  display.fillRect(PLAYER_X,  pongPlayerY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
  display.fillRect(BOT_X,     pongBotY,    PADDLE_W, PADDLE_H, SSD1306_WHITE);
  display.fillRect(pongBallX, pongBallY,   BALL_SIZE, BALL_SIZE, SSD1306_WHITE);
  display.display();
}

void pongLoop() {
  if (pongWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("PONG", 6, 2);
    char hsBuf[20];
    snprintf(hsBuf, sizeof(hsBuf), "Best round: %ds", hsPong);
    sharedDrawCenteredText(hsBuf, 28);
    sharedDrawCenteredText("Press button", 42);
    display.display();
    if (digitalRead(SW) == LOW) { pongWaitStart = false; pongRoundStart = millis(); delay(200); }
    return;
  }

  if (pongGameOver) {
    if (pongBestRound > hsPong) { hsPong = pongBestRound; hsSave("/hs_pong.txt", hsPong); }
    display.clearDisplay();
    sharedDrawCenteredText(pongWinner == 1 ? "YOU WIN!" : "BOT WINS", 8, 2);
    char buf[20];
    snprintf(buf, sizeof(buf), "Best rnd: %ds", hsPong);
    sharedDrawCenteredText(buf, 32);
    sharedDrawCenteredText("Btn: restart", 44);
    sharedDrawCenteredText("3x: menu",     54);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (digitalRead(SW) == LOW) { delay(200); pongInit(); }
    return;
  }

  pongReadJoystick();
  pongMoveBot();
  pongMoveBall();
  pongDraw();
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAZE
// ═══════════════════════════════════════════════════════════════════════════

#define CELL     8
#define COLS     (SCREEN_WIDTH  / CELL)
#define ROWS     (SCREEN_HEIGHT / CELL)
#define MAX_ORBS 8

#define INVINCIBLE_MS  3000
#define COOLDOWN_MS   12000

struct MazePoint { int x, y; };

uint8_t   mazeGrid[ROWS][COLS];
bool      mazeVisited[ROWS][COLS];

MazePoint mazePlayer, mazeEnemy;
unsigned long mazeLastEnemyMove;
int           mazeEnemyInterval;

MazePoint mazeOrbs[MAX_ORBS];
bool      mazeOrbActive[MAX_ORBS];
int       mazeOrbsRemaining;

bool          mazeInvincible;
bool          mazeInvincibleReady;
unsigned long mazeInvincibleStart;
unsigned long mazeCooldownStart;

int  mazeScore;
bool mazeGameOver, mazeYouWin, mazeWaitStart;

// ── Maze gen ──

void mazeCave(int cx, int cy) {
  mazeVisited[cy][cx] = true;
  int dirs[4] = {0, 1, 2, 3};
  for (int i = 3; i > 0; i--) {
    int j = random(0, i + 1);
    int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
  }
  for (int d = 0; d < 4; d++) {
    int nx = cx, ny = cy;
    if      (dirs[d] == 0) nx++;
    else if (dirs[d] == 1) ny++;
    else if (dirs[d] == 2) nx--;
    else                   ny--;
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) continue;
    if (mazeVisited[ny][nx]) continue;
    if (dirs[d] == 0) mazeGrid[cy][cx]  &= ~0x01;
    if (dirs[d] == 1) mazeGrid[cy][cx]  &= ~0x02;
    if (dirs[d] == 2) mazeGrid[cy][nx]  &= ~0x01;
    if (dirs[d] == 3) mazeGrid[ny][cx]  &= ~0x02;
    mazeCave(nx, ny);
  }
}

void mazeGenerate() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      mazeGrid[r][c] = 0x03;
  memset(mazeVisited, false, sizeof(mazeVisited));
  mazeCave(0, 0);
}

bool mazeCanMove(int cx, int cy, int d) {
  if (d == 0) return !(mazeGrid[cy][cx]   & 0x01);
  if (d == 1) return !(mazeGrid[cy][cx]   & 0x02);
  if (d == 2) return cx > 0 && !(mazeGrid[cy][cx-1] & 0x01);
  if (d == 3) return cy > 0 && !(mazeGrid[cy-1][cx] & 0x02);
  return false;
}

void mazePlaceOrbs() {
  mazeOrbsRemaining = MAX_ORBS;
  int placed = 0;
  while (placed < MAX_ORBS) {
    int ox = random(1, COLS), oy = random(1, ROWS);
    if ((ox == mazePlayer.x && oy == mazePlayer.y) ||
        (ox == mazeEnemy.x  && oy == mazeEnemy.y))  continue;
    mazeOrbs[placed]      = {ox, oy};
    mazeOrbActive[placed] = true;
    placed++;
  }
}

void mazeInit() {
  mazeGenerate();
  mazePlayer          = {0, 0};
  mazeEnemy           = {COLS - 1, ROWS - 1};
  mazeScore           = 0;
  mazeGameOver        = false;
  mazeYouWin          = false;
  mazeWaitStart       = true;
  mazeInvincible      = false;
  mazeInvincibleReady = true;
  mazeEnemyInterval   = 400;
  mazePlaceOrbs();
}

// ── Enemy BFS ──

void mazeMoveEnemy() {
  int dx[] = {1, 0, -1,  0};
  int dy[] = {0, 1,  0, -1};

  if (mazeInvincible) {
    int dirs[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
      int j = random(0, i + 1);
      int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
    }
    for (int d = 0; d < 4; d++) {
      if (!mazeCanMove(mazeEnemy.x, mazeEnemy.y, dirs[d])) continue;
      mazeEnemy.x += dx[dirs[d]];
      mazeEnemy.y += dy[dirs[d]];
      return;
    }
    return;
  }

  int prev[ROWS][COLS];
  memset(prev, -1, sizeof(prev));
  int queue[ROWS * COLS][2];
  int head = 0, tail = 0;
  queue[tail][0] = mazeEnemy.x;
  queue[tail][1] = mazeEnemy.y;
  tail++;
  prev[mazeEnemy.y][mazeEnemy.x] = mazeEnemy.y * COLS + mazeEnemy.x;

  while (head < tail) {
    int cx = queue[head][0], cy = queue[head][1]; head++;
    if (cx == mazePlayer.x && cy == mazePlayer.y) break;
    for (int d = 0; d < 4; d++) {
      if (!mazeCanMove(cx, cy, d)) continue;
      int nx = cx + dx[d], ny = cy + dy[d];
      if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) continue;
      if (prev[ny][nx] != -1) continue;
      prev[ny][nx] = cy * COLS + cx;
      queue[tail][0] = nx; queue[tail][1] = ny; tail++;
    }
  }

  int tx = mazePlayer.x, ty = mazePlayer.y;
  while (true) {
    int p = prev[ty][tx]; if (p == -1) return;
    int px = p % COLS, py = p / COLS;
    if (px == mazeEnemy.x && py == mazeEnemy.y) { mazeEnemy.x = tx; mazeEnemy.y = ty; return; }
    tx = px; ty = py;
  }
}

// ── Maze input ──

void mazeReadJoystick() {
  int xVal = analogRead(VRX);
  const int lo = 800, hi = 3200;
  static unsigned long lastMove = 0;
  if (millis() - lastMove < 150) return;

  int nx = mazePlayer.x, ny = mazePlayer.y;
  if      (xVal < lo) ny--;
  else if (xVal > hi) ny++;
  else {
    int yVal = analogRead(VRY);
    if      (yVal > hi) nx--;
    else if (yVal < lo) nx++;
    else return;
  }

  if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) return;
  int d = -1;
  if      (nx == mazePlayer.x + 1) d = 0;
  else if (ny == mazePlayer.y + 1) d = 1;
  else if (nx == mazePlayer.x - 1) d = 2;
  else if (ny == mazePlayer.y - 1) d = 3;
  if (d == -1 || !mazeCanMove(mazePlayer.x, mazePlayer.y, d)) return;
  mazePlayer.x = nx; mazePlayer.y = ny;
  lastMove = millis();
}

void mazeReadButton() {
  static bool lastPressed = false;
  bool pressed = (digitalRead(SW) == LOW);
  if (pressed && !lastPressed && mazeInvincibleReady && !mazeInvincible) {
    mazeInvincible      = true;
    mazeInvincibleReady = false;
    mazeInvincibleStart = millis();
  }
  lastPressed = pressed;
}

void mazeUpdateInvincibility() {
  if (mazeInvincible && millis() - mazeInvincibleStart >= INVINCIBLE_MS) {
    mazeInvincible  = false;
    mazeCooldownStart = millis();
  }
  if (!mazeInvincibleReady && !mazeInvincible &&
      millis() - mazeCooldownStart >= COOLDOWN_MS) {
    mazeInvincibleReady = true;
  }
}

// ── Maze HUD ──

void mazeDrawHUD() {
  int bx = SCREEN_WIDTH - 14, by = SCREEN_HEIGHT - 8;
  if (mazeInvincible) {
    if ((millis() / 200) % 2 == 0) display.fillRect(bx, by, 12, 6, SSD1306_WHITE);
    else                            display.drawRect(bx, by, 12, 6, SSD1306_WHITE);
  } else if (!mazeInvincibleReady) {
    display.drawRect(bx, by, 12, 6, SSD1306_WHITE);
    int fill = constrain((int)map(millis() - mazeCooldownStart, 0, COOLDOWN_MS, 0, 10), 0, 10);
    display.fillRect(bx + 1, by + 1, fill, 4, SSD1306_WHITE);
  } else {
    display.fillRect(bx, by, 12, 6, SSD1306_WHITE);
    display.drawPixel(bx+7, by+1, SSD1306_BLACK);
    display.drawPixel(bx+6, by+2, SSD1306_BLACK);
    display.drawPixel(bx+5, by+3, SSD1306_BLACK);
    display.drawPixel(bx+6, by+3, SSD1306_BLACK);
    display.drawPixel(bx+5, by+4, SSD1306_BLACK);
  }
}

void mazeDraw() {
  display.clearDisplay();
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int px = c * CELL, py = r * CELL;
      if (mazeGrid[r][c] & 0x01) display.drawFastVLine(px + CELL, py, CELL, SSD1306_WHITE);
      if (mazeGrid[r][c] & 0x02) display.drawFastHLine(px, py + CELL, CELL, SSD1306_WHITE);
    }
  }
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

  for (int i = 0; i < MAX_ORBS; i++) {
    if (!mazeOrbActive[i]) continue;
    display.fillCircle(mazeOrbs[i].x * CELL + CELL/2,
                       mazeOrbs[i].y * CELL + CELL/2, 1, SSD1306_WHITE);
  }

  bool showEnemy = !(mazeInvincible && (millis() / 150) % 2 == 0);
  if (showEnemy) {
    int ex = mazeEnemy.x * CELL + 1, ey = mazeEnemy.y * CELL + 1;
    display.drawLine(ex, ey, ex + CELL-3, ey + CELL-3, SSD1306_WHITE);
    display.drawLine(ex + CELL-3, ey, ex, ey + CELL-3, SSD1306_WHITE);
  }

  if (mazeInvincible)
    display.drawRect(mazePlayer.x * CELL+2, mazePlayer.y * CELL+2, CELL-4, CELL-4, SSD1306_WHITE);
  else
    display.fillRect(mazePlayer.x * CELL+2, mazePlayer.y * CELL+2, CELL-4, CELL-4, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 1);
  display.print(mazeScore);

  mazeDrawHUD();
  display.display();
}

void mazeLoop() {
  if (mazeWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("MAZE",         8, 2);
    sharedDrawCenteredText("Collect orbs", 28);
    sharedDrawCenteredText("Btn = shield", 38);
    sharedDrawCenteredText("3s on/12s CD", 48);
    sharedDrawCenteredText("Press button", 56);
    display.display();
    if (digitalRead(SW) == LOW) { mazeWaitStart = false; delay(200); }
    return;
  }

  if (mazeGameOver) {
    display.clearDisplay();
    sharedDrawCenteredText(mazeYouWin ? "YOU WIN!" : "CAUGHT!", 14, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "Score: %d", mazeScore);
    sharedDrawCenteredText(buf, 36);
    sharedDrawCenteredText("Btn: restart", 46);
    sharedDrawCenteredText("3x: menu",     56);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (digitalRead(SW) == LOW) { delay(200); mazeInit(); mazeWaitStart = false; }
    return;
  }

  mazeReadJoystick();
  mazeReadButton();
  mazeUpdateInvincibility();

  for (int i = 0; i < MAX_ORBS; i++) {
    if (!mazeOrbActive[i]) continue;
    if (mazeOrbs[i].x == mazePlayer.x && mazeOrbs[i].y == mazePlayer.y) {
      mazeOrbActive[i] = false;
      mazeOrbsRemaining--;
      mazeScore += 10;
      mazeEnemyInterval = max(150, mazeEnemyInterval - 25);
    }
  }

  if (mazeOrbsRemaining <= 0) { mazeGameOver = true; mazeYouWin = true; return; }

  if (millis() - mazeLastEnemyMove > mazeEnemyInterval) {
    mazeMoveEnemy();
    mazeLastEnemyMove = millis();
  }

  if (!mazeInvincible && mazeEnemy.x == mazePlayer.x && mazeEnemy.y == mazePlayer.y) {
    mazeGameOver = true; mazeYouWin = false; return;
  }

  mazeDraw();
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PLATFORMER
// ═══════════════════════════════════════════════════════════════════════════

#define PLT_MAX_PLAT    10
#define PLT_PLAT_W_MIN  20
#define PLT_PLAT_W_MAX  40
#define PLT_PLAT_H      4
#define PLT_PLAYER_W    5
#define PLT_PLAYER_H    7
#define PLT_GRAVITY     0.28f
#define PLT_JUMP_VEL   -5.5f
#define PLT_MOVE_SPEED  2
#define PLT_SCROLL_SPD  1

struct Platform {
  int x, y, w;
};

// Player
float   pltPX, pltPY;       // position (float for smooth physics)
float   pltVY;              // vertical velocity
bool    pltOnGround;
int     pltScore;
bool    pltGameOver;
bool    pltWaitStart;

// Platforms
Platform pltPlats[PLT_MAX_PLAT];
int      pltPlatCount;

// Camera offset (world scrolls left)
int pltCamX;

// ── Helpers ──

void pltSpawnPlatform() {
  // Default to just off the right edge of the screen
  float rightmost = SCREEN_WIDTH;
  int   lastY     = SCREEN_HEIGHT - 14;

  // Find the actual rightmost platform edge, regardless of whether it's on screen
  for (int i = 0; i < pltPlatCount; i++) {
    float edge = pltPlats[i].x + pltPlats[i].w;
    if (edge > rightmost) {
      rightmost = edge;
      lastY     = pltPlats[i].y;
    }
  }
  // If there are platforms, always use the furthest one even if it's < SCREEN_WIDTH
  if (pltPlatCount > 0) {
    float edge = pltPlats[pltPlatCount - 1].x + pltPlats[pltPlatCount - 1].w;
    if (edge > pltPlats[0].x) {
      rightmost = edge;
      lastY     = pltPlats[pltPlatCount - 1].y;
    }
  }

  int gap = random(28, 42);
  int w   = random(PLT_PLAT_W_MIN, PLT_PLAT_W_MAX);
  int deltaY = random(-6, 7);
  int y      = constrain(lastY + deltaY, SCREEN_HEIGHT - 22, SCREEN_HEIGHT - 10);

  if (pltPlatCount < PLT_MAX_PLAT) {
    pltPlats[pltPlatCount++] = { (int)rightmost + gap, y, w };
  }
}

void pltRemoveFirstPlatform() {
  for (int i = 1; i < pltPlatCount; i++)
    pltPlats[i - 1] = pltPlats[i];
  pltPlatCount--;
}

void platformInit() {
  pltPX        = 20;
  pltPY        = 30;
  pltVY        = 0;
  pltOnGround  = false;
  pltScore     = 0;
  pltGameOver  = false;
  pltWaitStart = true;
  pltCamX      = 0;
  pltPlatCount = 0;

  // First platform: wide, centered on screen, player placed in the middle of it
  int firstPlatY = SCREEN_HEIGHT - 14;
  int firstPlatX = 0;
  int firstPlatW = 50;
  pltPlats[pltPlatCount++] = { firstPlatX, firstPlatY, firstPlatW };
  pltPX = firstPlatX + firstPlatW / 2 - PLT_PLAYER_W / 2;  // center of first platform
  pltPY = firstPlatY - PLT_PLAYER_H;
  pltPlatCount = 1;
  // Second platform: same height as first, short gap — guarantees first jump is easy
  pltPlats[pltPlatCount++] = { firstPlatX + firstPlatW + 6, firstPlatY, random(PLT_PLAT_W_MIN, PLT_PLAT_W_MAX) };
  // Fill rest randomly
  while (pltPlatCount < PLT_MAX_PLAT - 1)
    pltSpawnPlatform();
}

// ── Input ──

void pltReadInput() {
  int yVal = analogRead(VRY);
  int xVal = analogRead(VRX);
  const int lo = 800, hi = 3200;

  // Horizontal movement (inverted to match physical joystick)
  if (yVal > hi) pltPX -= PLT_MOVE_SPEED;   // joystick left  → move left
  if (yVal < lo) pltPX += PLT_MOVE_SPEED;   // joystick right → move right

  // Jump: joystick up, joystick down, or button — all jump
  bool jumpPressed = (xVal < lo) || (xVal > hi) || (digitalRead(SW) == LOW);
  static bool lastJump = false;
  if (jumpPressed && !lastJump && pltOnGround) {
    pltVY       = PLT_JUMP_VEL;
    pltOnGround = false;
  }
  lastJump = jumpPressed;

  // Clamp player horizontal to screen
  pltPX = constrain(pltPX, 0, SCREEN_WIDTH - PLT_PLAYER_W);
}

// ── Physics ──

void pltUpdate() {
  // Scroll world
  pltCamX += PLT_SCROLL_SPD;
  pltScore  = pltCamX / 10;

  // Scroll platforms AND player left together — player stays put when idle
  for (int i = 0; i < pltPlatCount; i++)
    pltPlats[i].x -= PLT_SCROLL_SPD;
  pltPX -= PLT_SCROLL_SPD;

  // Remove platforms that scrolled off left
  while (pltPlatCount > 0 && pltPlats[0].x + pltPlats[0].w < 0)
    pltRemoveFirstPlatform();

  // Spawn new platforms to keep screen full
  while (pltPlatCount < PLT_MAX_PLAT) {
    int rightmost = 0;
    for (int i = 0; i < pltPlatCount; i++)
      if (pltPlats[i].x + pltPlats[i].w > rightmost)
        rightmost = pltPlats[i].x + pltPlats[i].w;
    if (rightmost < SCREEN_WIDTH + 40)
      pltSpawnPlatform();
    else
      break;
  }

  // Gravity
  pltVY += PLT_GRAVITY;
  pltPY += pltVY;

  // Clamp to top of screen
  if (pltPY < 0) {
    pltPY = 0;
    pltVY = 0;
  }

  // Platform collision (only when falling)
  pltOnGround = false;
  if (pltVY >= 0) {
    for (int i = 0; i < pltPlatCount; i++) {
      Platform& p = pltPlats[i];
      float playerBottom = pltPY + PLT_PLAYER_H;
      float playerLeft   = pltPX;
      float playerRight  = pltPX + PLT_PLAYER_W;

      bool overlapX = playerRight > p.x && playerLeft < p.x + p.w;
      bool landing  = playerBottom >= p.y && playerBottom <= p.y + PLT_PLAT_H + pltVY + 1;

      if (overlapX && landing) {
        pltPY       = p.y - PLT_PLAYER_H;
        pltVY       = 0;
        pltOnGround = true;
        break;
      }
    }
  }

  // Fell off bottom or scrolled off left → game over
  if (pltPY > SCREEN_HEIGHT || pltPX + PLT_PLAYER_W < 0) {
    pltGameOver = true;
  }
}

// ── Draw ──

void pltDraw() {
  display.clearDisplay();

  // Platforms
  for (int i = 0; i < pltPlatCount; i++) {
    Platform& p = pltPlats[i];
    display.fillRect(p.x, p.y, p.w, PLT_PLAT_H, SSD1306_WHITE);
  }

  // Player (filled rect with eyes)
  int px = (int)pltPX, py = (int)pltPY;
  display.fillRect(px, py, PLT_PLAYER_W, PLT_PLAYER_H, SSD1306_WHITE);
  // Eyes (2 black pixels)
  display.drawPixel(px + 1, py + 2, SSD1306_BLACK);
  display.drawPixel(px + 3, py + 2, SSD1306_BLACK);

  // Score top-right
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH - 30, 0);
  display.print(pltScore);

  display.display();
}

// ── Loop ──

void platformLoop() {
  if (pltWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("PLATFORM", 6, 2);
    char hsBuf[16];
    snprintf(hsBuf, sizeof(hsBuf), "Best: %d", hsPlatform);
    sharedDrawCenteredText(hsBuf, 28);
    sharedDrawCenteredText("L/R:move Up:jump", 40);
    sharedDrawCenteredText("Press button", 52);
    display.display();
    if (digitalRead(SW) == LOW) { pltWaitStart = false; delay(200); }
    return;
  }

  if (pltGameOver) {
    if (pltScore > hsPlatform) { hsPlatform = pltScore; hsSave("/hs_platform.txt", hsPlatform); }
    display.clearDisplay();
    sharedDrawCenteredText("GAME OVER",  10, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "Score: %d", pltScore);
    sharedDrawCenteredText(buf, 30);
    snprintf(buf, sizeof(buf), "Best:  %d", hsPlatform);
    sharedDrawCenteredText(buf, 40);
    sharedDrawCenteredText("3x: menu", 54);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (digitalRead(SW) == LOW) { delay(200); platformInit(); pltWaitStart = false; }
    return;
  }

  pltReadInput();
  pltUpdate();
  pltDraw();
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  WIFI SCAN
// ═══════════════════════════════════════════════════════════════════════════

#include <WiFi.h>

#define WIFI_MAX_NETS  20

struct WifiNet {
  char     ssid[33];
  int32_t  rssi;
  uint8_t  enc;   // encryption type
};

WifiNet       wifiNets[WIFI_MAX_NETS];
int           wifiCount      = 0;
int           wifiIndex      = 0;   // currently viewed network
bool          wifiScanning   = false;
bool          wifiDone       = false;
unsigned long wifiScanStart  = 0;
unsigned long wifiLastMove   = 0;

void wifiScanInit() {
  wifiCount    = 0;
  wifiIndex    = 0;
  wifiDone     = false;
  wifiScanning = true;
  wifiScanStart = millis();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true);  // async scan
}

const char* wifiEncStr(uint8_t enc) {
  switch (enc) {
    case WIFI_AUTH_OPEN:           return "Open";
    case WIFI_AUTH_WEP:            return "WEP";
    case WIFI_AUTH_WPA_PSK:        return "WPA";
    case WIFI_AUTH_WPA2_PSK:       return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2E";
    case WIFI_AUTH_WPA3_PSK:       return "WPA3";
    default:                       return "?";
  }
}

bool wifiDetailView = false;

void wifiScanLoop() {
  // Triple-click exits at any time
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }

  const int lo = 800, hi = 3200;
  static bool lastBtn  = false;
  static bool lastLeft = false;
  static bool lastRight= false;

  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  bool btn   = digitalRead(SW) == LOW;
  bool left  = (yVal > hi);
  bool right = (yVal < lo);

  // ── Scanning screen ──────────────────────────────────────────────────────
  if (wifiScanning) {
    int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) {
      display.clearDisplay();
      sharedDrawCenteredText("WIFI SCAN", 10, 2);
      char dots[5] = "    ";
      int d = (millis() / 300) % 4;
      for (int i = 0; i < d; i++) dots[i] = '.';
      sharedDrawCenteredText("Scanning", 36);
      sharedDrawCenteredText(dots, 46);
      display.display();
      lastBtn = btn; lastLeft = left; lastRight = right;
      return;
    }
    wifiScanning   = false;
    wifiDone       = true;
    wifiDetailView = false;
    wifiCount      = 0;
    if (result > 0) {
      int n = min(result, WIFI_MAX_NETS);
      for (int i = 0; i < n; i++) {
        strncpy(wifiNets[wifiCount].ssid, WiFi.SSID(i).c_str(), 32);
        wifiNets[wifiCount].ssid[32] = '\0';
        wifiNets[wifiCount].rssi     = WiFi.RSSI(i);
        wifiNets[wifiCount].enc      = WiFi.encryptionType(i);
        wifiCount++;
      }
    }
    wifiIndex = 0;
    WiFi.scanDelete();
  }

  if (!wifiDone) { lastBtn = btn; lastLeft = left; lastRight = right; return; }

  // ── Detail view ──────────────────────────────────────────────────────────
  if (wifiDetailView) {
    // Any of: button, left, right → exit detail
    if ((btn && !lastBtn) || (left && !lastLeft) || (right && !lastRight)) {
      wifiDetailView = false;
      delay(150);
      lastBtn = btn; lastLeft = left; lastRight = right;
      return;
    }

    WifiNet& n = wifiNets[wifiIndex];
    display.clearDisplay();

    // SSID header
    char ssidBuf[17];
    strncpy(ssidBuf, n.ssid, 16);
    ssidBuf[16] = '\0';
    if (strlen(n.ssid) > 16) { ssidBuf[14] = '.'; ssidBuf[15] = '.'; }
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(ssidBuf);
    display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

    // RSSI bar (top right)
    int bars = map(constrain(n.rssi, -100, -40), -100, -40, 0, 5);
    for (int b = 0; b < 5; b++) {
      int bx = SCREEN_WIDTH - 32 + b * 6;
      int bh = 2 + b * 2;
      int by = 8 - bh;
      if (b < bars) display.fillRect(bx, by, 4, bh, SSD1306_WHITE);
      else          display.drawRect(bx, by, 4, bh, SSD1306_WHITE);
    }

    // Details
    char buf[24];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", n.rssi);
    display.setCursor(0, 12); display.print(buf);

    const char* qual;
    if      (n.rssi > -55) qual = "Excellent";
    else if (n.rssi > -67) qual = "Good";
    else if (n.rssi > -80) qual = "Fair";
    else                   qual = "Weak";
    snprintf(buf, sizeof(buf), "Sig:  %s", qual);
    display.setCursor(0, 22); display.print(buf);

    snprintf(buf, sizeof(buf), "Sec:  %s", wifiEncStr(n.enc));
    display.setCursor(0, 32); display.print(buf);

    display.setCursor(0, 46);
    display.print("Btn/L/R: back");

    display.display();
    lastBtn = btn; lastLeft = left; lastRight = right;
    return;
  }

  // ── List view ────────────────────────────────────────────────────────────

  // Navigate up/down
  if (millis() - wifiLastMove > 220) {
    if (xVal < lo && wifiIndex > 0)             { wifiIndex--; wifiLastMove = millis(); }
    if (xVal > hi && wifiIndex < wifiCount - 1) { wifiIndex++; wifiLastMove = millis(); }
  }

  // Enter detail: button, left or right
  if ((btn && !lastBtn) || (left && !lastLeft) || (right && !lastRight)) {
    if (wifiCount > 0) { wifiDetailView = true; delay(150); }
  }

  display.clearDisplay();

  if (wifiCount == 0) {
    sharedDrawCenteredText("No networks", 18);
    sharedDrawCenteredText("Btn: rescan", 34);
    sharedDrawCenteredText("3x: menu",    44);
    display.display();
    if (btn && !lastBtn) { lastBtn = btn; wifiScanInit(); return; }
    lastBtn = btn; lastLeft = left; lastRight = right;
    return;
  }

  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char hdr[20];
  snprintf(hdr, sizeof(hdr), "WiFi  %d/%d", wifiIndex + 1, wifiCount);
  display.setCursor(0, 0);
  display.print(hdr);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // Show 5 SSIDs, scroll window follows wifiIndex
  const int visRows = 5;
  int startIdx = 0;
  if (wifiIndex >= visRows) startIdx = wifiIndex - visRows + 1;

  for (int i = 0; i < visRows; i++) {
    int idx = startIdx + i;
    if (idx >= wifiCount) break;
    int y = 12 + i * 10;

    if (idx == wifiIndex) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    // Truncate SSID
    char ssidBuf[17];
    strncpy(ssidBuf, wifiNets[idx].ssid, 16);
    ssidBuf[16] = '\0';
    if (strlen(wifiNets[idx].ssid) > 16) { ssidBuf[14] = '.'; ssidBuf[15] = '.'; }

    display.setCursor(2, y);
    display.print(ssidBuf);

    // Mini signal bar (3 bars, right side)
    if (idx == wifiIndex) display.setTextColor(SSD1306_BLACK);
    else                  display.setTextColor(SSD1306_WHITE);
    int sig = map(constrain(wifiNets[idx].rssi, -100, -40), -100, -40, 0, 3);
    for (int b = 0; b < 3; b++) {
      int bx = SCREEN_WIDTH - 12 + b * 4;
      int bh = 2 + b * 2;
      int by = y + 6 - bh;
      if (b < sig) {
        if (idx == wifiIndex) display.fillRect(bx, by, 3, bh, SSD1306_BLACK);
        else                  display.fillRect(bx, by, 3, bh, SSD1306_WHITE);
      } else {
        if (idx == wifiIndex) display.drawRect(bx, by, 3, bh, SSD1306_BLACK);
        else                  display.drawRect(bx, by, 3, bh, SSD1306_WHITE);
      }
    }
  }

  display.setTextColor(SSD1306_WHITE);
  display.display();

  lastBtn = btn; lastLeft = left; lastRight = right;
}

// ═══════════════════════════════════════════════════════════════════════════
//  BLUETOOTH SCAN
// ═══════════════════════════════════════════════════════════════════════════

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define BT_MAX_DEVS  20

struct BTDev {
  char name[32];
  char addr[18];
  int  rssi;
};

BTDev         btDevs[BT_MAX_DEVS];
int           btCount       = 0;
int           btIndex       = 0;
bool          btScanning    = false;
bool          btScanDone    = false;
bool          btDone        = false;
bool          btDetailView  = false;
unsigned long btLastMove    = 0;
unsigned long btScanStart   = 0;
#define BT_SCAN_SECS 5
BLEScan*      btScanner     = nullptr;

class BTCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (btCount >= BT_MAX_DEVS) return;
    const char* name = dev.getName().c_str();
    const char* displayName = (strlen(name) > 0) ? name : "(unknown)";
    // Deduplicate by name — skip if already seen
    for (int i = 0; i < btCount; i++)
      if (strcmp(btDevs[i].name, displayName) == 0) return;
    strncpy(btDevs[btCount].name, displayName, 31);
    btDevs[btCount].name[31] = '\0';
    strncpy(btDevs[btCount].addr, dev.getAddress().toString().c_str(), 17);
    btDevs[btCount].addr[17] = '\0';
    btDevs[btCount].rssi = dev.getRSSI();
    btCount++;
  }
};

void btScanInit() {
  btCount      = 0;
  btIndex      = 0;
  btDone       = false;
  btScanDone   = false;
  btDetailView = false;
  btScanning   = true;
  btScanStart  = millis();

  BLEDevice::init("");
  btScanner = BLEDevice::getScan();
  btScanner->setAdvertisedDeviceCallbacks(new BTCallback(), true);
  btScanner->setActiveScan(true);
  btScanner->setInterval(100);
  btScanner->setWindow(99);
  btScanner->start(BT_SCAN_SECS, false);  // non-blocking
}

void btScanLoop() {
  if (checkTripleClick()) {
    if (btScanning) btScanner->stop();
    btScanning = false;
    currentState = MENU;
    menuLockUntil = millis() + 300;
    return;
  }

  const int lo = 800, hi = 3200;
  static bool lastBtn  = false;
  static bool lastLeft = false;
  static bool lastRight= false;

  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  bool btn   = digitalRead(SW) == LOW;
  bool left  = (yVal > hi);
  bool right = (yVal < lo);

  // ── Scanning screen ──
  if (btScanning) {
    if (millis() - btScanStart >= (BT_SCAN_SECS * 1000UL)) {
      btScanner->stop();
      btScanning = false;
      btDone     = true;
      btIndex    = 0;
    } else {
      display.clearDisplay();
      sharedDrawCenteredText("BT SCAN",  10, 2);
      sharedDrawCenteredText("Scanning", 34);
      char dots[5] = "    ";
      int d = (millis() / 300) % 4;
      for (int i = 0; i < d; i++) dots[i] = '.';
      sharedDrawCenteredText(dots, 46);
      display.display();
      lastBtn = btn; lastLeft = left; lastRight = right;
      return;
    }
  }

  if (!btDone) { lastBtn = btn; lastLeft = left; lastRight = right; return; }

  // ── Detail view ──
  if (btDetailView) {
    if ((btn && !lastBtn) || (left && !lastLeft) || (right && !lastRight)) {
      btDetailView = false;
      delay(150);
      lastBtn = btn; lastLeft = left; lastRight = right;
      return;
    }

    BTDev& d = btDevs[btIndex];
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Name header
    char nameBuf[17];
    strncpy(nameBuf, d.name, 16); nameBuf[16] = '\0';
    if (strlen(d.name) > 16) { nameBuf[14] = '.'; nameBuf[15] = '.'; }
    display.setCursor(0, 0);
    display.print(nameBuf);
    display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

    // RSSI bar
    int bars = map(constrain(d.rssi, -100, -40), -100, -40, 0, 5);
    for (int b = 0; b < 5; b++) {
      int bx = SCREEN_WIDTH - 32 + b * 6;
      int bh = 2 + b * 2;
      int by = 8 - bh;
      if (b < bars) display.fillRect(bx, by, 4, bh, SSD1306_WHITE);
      else          display.drawRect(bx, by, 4, bh, SSD1306_WHITE);
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", d.rssi);
    display.setCursor(0, 12); display.print(buf);

    const char* qual;
    if      (d.rssi > -55) qual = "Excellent";
    else if (d.rssi > -67) qual = "Good";
    else if (d.rssi > -80) qual = "Fair";
    else                   qual = "Weak";
    snprintf(buf, sizeof(buf), "Sig:  %s", qual);
    display.setCursor(0, 22); display.print(buf);

    display.setCursor(0, 32); display.print("Addr:");
    display.setCursor(0, 42); display.print(d.addr);
    display.setCursor(0, 54); display.print("Btn/L/R: back");
    display.display();

    lastBtn = btn; lastLeft = left; lastRight = right;
    return;
  }

  // ── List view ──
  if (millis() - btLastMove > 220) {
    if (xVal < lo && btIndex > 0)             { btIndex--; btLastMove = millis(); }
    if (xVal > hi && btIndex < btCount - 1)   { btIndex++; btLastMove = millis(); }
  }

  if ((btn && !lastBtn) || (left && !lastLeft) || (right && !lastRight)) {
    if (btCount > 0) { btDetailView = true; delay(150); }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (btCount == 0) {
    sharedDrawCenteredText("No devices", 18);
    sharedDrawCenteredText("Btn: rescan", 34);
    sharedDrawCenteredText("3x: menu",    44);
    display.display();
    if (btn && !lastBtn) { lastBtn = btn; btScanInit(); return; }
    lastBtn = btn; lastLeft = left; lastRight = right;
    return;
  }

  // Header
  char hdr[20];
  snprintf(hdr, sizeof(hdr), "BT  %d/%d", btIndex + 1, btCount);
  display.setCursor(0, 0);
  display.print(hdr);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  const int visRows = 5;
  int startIdx = 0;
  if (btIndex >= visRows) startIdx = btIndex - visRows + 1;

  for (int i = 0; i < visRows; i++) {
    int idx = startIdx + i;
    if (idx >= btCount) break;
    int y = 12 + i * 10;

    if (idx == btIndex) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    char nameBuf[17];
    strncpy(nameBuf, btDevs[idx].name, 16); nameBuf[16] = '\0';
    if (strlen(btDevs[idx].name) > 16) { nameBuf[14] = '.'; nameBuf[15] = '.'; }
    display.setCursor(2, y);
    display.print(nameBuf);

    // Mini signal bars
    int sig = map(constrain(btDevs[idx].rssi, -100, -40), -100, -40, 0, 3);
    for (int b = 0; b < 3; b++) {
      int bx = SCREEN_WIDTH - 12 + b * 4;
      int bh = 2 + b * 2;
      int by = y + 6 - bh;
      if (b < sig) {
        if (idx == btIndex) display.fillRect(bx, by, 3, bh, SSD1306_BLACK);
        else                display.fillRect(bx, by, 3, bh, SSD1306_WHITE);
      } else {
        if (idx == btIndex) display.drawRect(bx, by, 3, bh, SSD1306_BLACK);
        else                display.drawRect(bx, by, 3, bh, SSD1306_WHITE);
      }
    }
  }

  display.setTextColor(SSD1306_WHITE);
  display.display();

  lastBtn = btn; lastLeft = left; lastRight = right;
}

// ═══════════════════════════════════════════════════════════════════════════
//  3D CUBE
// ═══════════════════════════════════════════════════════════════════════════

#include <math.h>

// Rotation angles
float cubeAngleX = 0.0f;
float cubeAngleY = 0.0f;
float cubeAngleZ = 0.0f;

// Cube vertices (unit cube centered at origin)
const float cubeVerts[8][3] = {
  {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},  // back face
  {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}   // front face
};

// 12 edges (pairs of vertex indices)
const int cubeEdges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},  // back face
  {4,5},{5,6},{6,7},{7,4},  // front face
  {0,4},{1,5},{2,6},{3,7}   // connecting edges
};

void cubeInit() {
  cubeAngleX = 0.3f;
  cubeAngleY = 0.5f;
  cubeAngleZ = 0.0f;
}

// Project a 3D point to 2D screen coords with perspective
void cubeProject(float x, float y, float z, int& sx, int& sy) {
  float fov  = 3.5f;   // perspective strength
  float dist = 4.0f;   // camera distance
  float scale = fov / (dist + z);
  sx = (int)(SCREEN_WIDTH  / 2 + x * scale * 22);
  sy = (int)(SCREEN_HEIGHT / 2 - y * scale * 22);
}

// Rotate a point around all three axes
void cubeRotate(float ix, float iy, float iz,
                float ax, float ay, float az,
                float& ox, float& oy, float& oz) {
  // X axis
  float y1 = iy * cos(ax) - iz * sin(ax);
  float z1 = iy * sin(ax) + iz * cos(ax);
  // Y axis
  float x2 = ix * cos(ay) + z1 * sin(ay);
  float z2 = -ix * sin(ay) + z1 * cos(ay);
  // Z axis
  float x3 = x2 * cos(az) - y1 * sin(az);
  float y3 = x2 * sin(az) + y1 * cos(az);
  ox = x3; oy = y3; oz = z2;
}

void cubeLoop() {
  // Single button press exits — unique to this app
  static bool lastBtn = false;
  bool btn = digitalRead(SW) == LOW;
  if (btn && !lastBtn) {
    currentState = MENU;
    menuLockUntil = millis() + 300;
    lastBtn = btn;
    return;
  }
  lastBtn = btn;

  // Joystick controls rotation
  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  const int lo = 800, hi = 3200;
  const float rotSpeed = 0.04f;

  // VRX: tilt up/down → rotate around X
  if      (xVal > hi) cubeAngleX -= rotSpeed;
  else if (xVal < lo) cubeAngleX += rotSpeed;

  // VRY: tilt left/right → rotate around Y
  if      (yVal > hi) cubeAngleY -= rotSpeed;
  else if (yVal < lo) cubeAngleY += rotSpeed;
  else                cubeAngleY += 0.005f;  // slow auto-spin when idle

  // Project all 8 vertices
  int sx[8], sy[8];
  for (int i = 0; i < 8; i++) {
    float rx, ry, rz;
    cubeRotate(cubeVerts[i][0], cubeVerts[i][1], cubeVerts[i][2],
               cubeAngleX, cubeAngleY, cubeAngleZ,
               rx, ry, rz);
    cubeProject(rx, ry, rz, sx[i], sy[i]);
  }

  // Draw
  display.clearDisplay();

  for (int e = 0; e < 12; e++) {
    int a = cubeEdges[e][0];
    int b = cubeEdges[e][1];
    display.drawLine(sx[a], sy[a], sx[b], sy[b], SSD1306_WHITE);
  }

  display.display();
}

// ═══════════════════════════════════════════════════════════════════════════
//  TETRIS
// ═══════════════════════════════════════════════════════════════════════════

#define TET_COLS     16
#define TET_ROWS     21
#define TET_CELL      3
#define TET_BOARD_X   ((SCREEN_WIDTH - TET_COLS * TET_CELL) / 2)
#define TET_BOARD_Y   0
#define TET_BOARD_W  (TET_COLS * TET_CELL)

// 7 tetrominoes, each 4 rotations, each rotation 4 (x,y) cells
// Classic Gameboy-style shapes
const int8_t TET_PIECES[7][4][4][2] = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}},{{2,0},{2,1},{2,2},{2,3}},{{0,2},{1,2},{2,2},{3,2}},{{1,0},{1,1},{1,2},{1,3}}},
  // O
  {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{2,1},{1,2}},{{0,1},{1,1},{2,1},{1,2}},{{1,0},{0,1},{1,1},{1,2}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}},{{1,0},{1,1},{2,1},{2,2}},{{1,0},{2,0},{0,1},{1,1}},{{1,0},{1,1},{2,1},{2,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}},{{2,0},{1,1},{2,1},{1,2}},{{0,0},{1,0},{1,1},{2,1}},{{2,0},{1,1},{2,1},{1,2}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}},{{1,0},{2,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{2,2}},{{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,1},{0,2}},{{0,0},{1,0},{1,1},{1,2}}}
};

uint8_t tetBoard[TET_ROWS][TET_COLS];  // 0=empty, 1=filled
int     tetPieceType, tetNextType;
int     tetRot, tetPX, tetPY;
int     tetScore, tetLines, tetLevel;
bool    tetGameOver, tetWaitStart;
unsigned long tetLastDrop, tetLastInput;
int     tetDropInterval;

void tetSpawnPiece() {
  tetPieceType = tetNextType;
  tetNextType  = random(0, 7);
  tetRot = 0;
  tetPX  = TET_COLS / 2 - 2;
  tetPY  = 0;
}

bool tetCollides(int type, int rot, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int cx = px + TET_PIECES[type][rot][i][0];
    int cy = py + TET_PIECES[type][rot][i][1];
    if (cx < 0 || cx >= TET_COLS || cy >= TET_ROWS) return true;
    if (cy >= 0 && tetBoard[cy][cx]) return true;
  }
  return false;
}

void tetLockPiece() {
  for (int i = 0; i < 4; i++) {
    int cx = tetPX + TET_PIECES[tetPieceType][tetRot][i][0];
    int cy = tetPY + TET_PIECES[tetPieceType][tetRot][i][1];
    if (cy >= 0) tetBoard[cy][cx] = 1;
  }
  // Clear full lines
  int cleared = 0;
  for (int r = TET_ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < TET_COLS; c++) if (!tetBoard[r][c]) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr = r; rr > 0; rr--)
        memcpy(tetBoard[rr], tetBoard[rr-1], TET_COLS);
      memset(tetBoard[0], 0, TET_COLS);
      r++;  // recheck same row
    }
  }
  if (cleared > 0) {
    // Gameboy scoring
    const int pts[5] = {0, 40, 100, 300, 1200};
    tetScore += pts[min(cleared, 4)] * (tetLevel + 1);
    tetLines += cleared;
    tetLevel  = tetLines / 10;
    tetDropInterval = max(100, 800 - tetLevel * 70);
  }
}

void tetrisInit() {
  memset(tetBoard, 0, sizeof(tetBoard));
  tetScore       = 0;
  tetLines       = 0;
  tetLevel       = 0;
  tetDropInterval= 800;
  tetGameOver    = false;
  tetWaitStart   = true;
  tetLastDrop    = 0;
  tetLastInput   = 0;
  tetNextType    = random(0, 7);
  tetSpawnPiece();
}

void tetDraw() {
  display.clearDisplay();

  // Board border
  display.drawRect(TET_BOARD_X - 1, TET_BOARD_Y,
                   TET_BOARD_W + 2, TET_ROWS * TET_CELL + 1, SSD1306_WHITE);

  // Locked cells
  for (int r = 0; r < TET_ROWS; r++)
    for (int c = 0; c < TET_COLS; c++)
      if (tetBoard[r][c])
        display.fillRect(TET_BOARD_X + c * TET_CELL,
                         TET_BOARD_Y + r * TET_CELL,
                         TET_CELL - 1, TET_CELL - 1, SSD1306_WHITE);

  // Ghost piece
  int ghostY = tetPY;
  while (!tetCollides(tetPieceType, tetRot, tetPX, ghostY + 1)) ghostY++;
  if (ghostY != tetPY) {
    for (int i = 0; i < 4; i++) {
      int cx = tetPX + TET_PIECES[tetPieceType][tetRot][i][0];
      int cy = ghostY + TET_PIECES[tetPieceType][tetRot][i][1];
      if (cy >= 0)
        display.drawRect(TET_BOARD_X + cx * TET_CELL,
                         TET_BOARD_Y + cy * TET_CELL,
                         TET_CELL - 1, TET_CELL - 1, SSD1306_WHITE);
    }
  }

  // Active piece
  for (int i = 0; i < 4; i++) {
    int cx = tetPX + TET_PIECES[tetPieceType][tetRot][i][0];
    int cy = tetPY + TET_PIECES[tetPieceType][tetRot][i][1];
    if (cy >= 0)
      display.fillRect(TET_BOARD_X + cx * TET_CELL,
                       TET_BOARD_Y + cy * TET_CELL,
                       TET_CELL - 1, TET_CELL - 1, SSD1306_WHITE);
  }

  display.display();
}

void tetrisLoop() {
  if (tetWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("TETRIS", 6, 2);
    char hsBuf[16];
    snprintf(hsBuf, sizeof(hsBuf), "Best: %d", hsTetris);
    sharedDrawCenteredText(hsBuf, 28);
    sharedDrawCenteredText("L/R:move Dn:drop", 40);
    sharedDrawCenteredText("Press button", 52);
    display.display();
    if (digitalRead(SW) == LOW) { tetWaitStart = false; delay(200); }
    return;
  }

  if (tetGameOver) {
    if (tetScore > hsTetris) { hsTetris = tetScore; hsSave("/hs_tetris.txt", hsTetris); }
    display.clearDisplay();
    sharedDrawCenteredText("GAME OVER",  10, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "Score:%d", tetScore);
    sharedDrawCenteredText(buf, 28);
    snprintf(buf, sizeof(buf), "Best: %d", hsTetris);
    sharedDrawCenteredText(buf, 38);
    snprintf(buf, sizeof(buf), "Lines:%d", tetLines);
    sharedDrawCenteredText(buf, 48);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (digitalRead(SW) == LOW) { delay(200); tetrisInit(); tetWaitStart = false; }
    return;
  }

  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  const int lo = 800, hi = 3200;
  unsigned long now = millis();

  // Input with repeat delay
  if (now - tetLastInput > 120) {
    bool moved = false;

    // Left/right move
    if (yVal > hi) {
      if (!tetCollides(tetPieceType, tetRot, tetPX - 1, tetPY))
        tetPX--;
      moved = true;
    } else if (yVal < lo) {
      if (!tetCollides(tetPieceType, tetRot, tetPX + 1, tetPY))
        tetPX++;
      moved = true;
    }

    if (moved) tetLastInput = now;
  }

  // Rotate: button or up — edge triggered
  static bool lastBtn = false, lastUp = false;
  bool btn = digitalRead(SW) == LOW;
  bool up  = (xVal < lo);

  if ((btn && !lastBtn) || (up && !lastUp)) {
    int newRot = (tetRot + 1) % 4;
    // Wall kick: try plain, then nudge right, then left
    if      (!tetCollides(tetPieceType, newRot, tetPX,     tetPY)) tetRot = newRot;
    else if (!tetCollides(tetPieceType, newRot, tetPX + 1, tetPY)) { tetPX++; tetRot = newRot; }
    else if (!tetCollides(tetPieceType, newRot, tetPX - 1, tetPY)) { tetPX--; tetRot = newRot; }
  }
  lastBtn = btn; lastUp = up;

  // Gravity drop (soft drop: joystick down speeds it up)
  bool softDrop = (xVal > hi);
  int  interval = softDrop ? 40 : tetDropInterval;
  if (now - tetLastDrop > (unsigned long)interval) {
    tetLastDrop = now;
    if (!tetCollides(tetPieceType, tetRot, tetPX, tetPY + 1)) {
      tetPY++;
    } else {
      tetLockPiece();
      tetSpawnPiece();
      if (tetCollides(tetPieceType, tetRot, tetPX, tetPY))
        tetGameOver = true;
    }
  }

  tetDraw();
  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  FLAPPY BIRD
// ═══════════════════════════════════════════════════════════════════════════

#define FB_PIPE_W       8
#define FB_PIPE_GAP    22
#define FB_PIPE_COUNT   3
#define FB_PIPE_SPEED   1
#define FB_PIPE_SPACING 55
#define FB_BIRD_X       20
#define FB_BIRD_W       5
#define FB_BIRD_H       4
#define FB_GRAVITY      0.20f
#define FB_JUMP_VEL    -2.8f
#define FB_GROUND_Y    (SCREEN_HEIGHT - 6)

struct FBPipe {
  int x;
  int gapY;  // top of gap
};

float     fbBirdY, fbBirdVY;
FBPipe    fbPipes[FB_PIPE_COUNT];
int       fbScore;
bool      fbGameOver, fbWaitStart, fbStarted;
unsigned long fbLastFrame;

void fbSpawnPipe(int idx, int startX) {
  fbPipes[idx].x    = startX;
  fbPipes[idx].gapY = random(8, FB_GROUND_Y - FB_PIPE_GAP - 8);
}

void flappyInit() {
  fbBirdY    = SCREEN_HEIGHT / 2 - 4;
  fbBirdVY   = 0;
  fbScore    = 0;
  fbGameOver = false;
  fbWaitStart= true;
  fbStarted  = false;

  for (int i = 0; i < FB_PIPE_COUNT; i++)
    fbSpawnPipe(i, SCREEN_WIDTH + i * FB_PIPE_SPACING);
}

// Draw the bird as a small wing shape
void fbDrawBird(int x, int y) {
  // Body
  display.fillRect(x, y, FB_BIRD_W, FB_BIRD_H, SSD1306_WHITE);
  // Eye
  display.drawPixel(x + FB_BIRD_W - 1, y, SSD1306_BLACK);
  // Wing tip below
  display.drawPixel(x + 1, y + FB_BIRD_H, SSD1306_WHITE);
  display.drawPixel(x + 2, y + FB_BIRD_H, SSD1306_WHITE);
}

bool fbJustPressed() {
  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  bool btn = digitalRead(SW) == LOW;
  return btn || xVal < 800 || xVal > 3200 || yVal < 800 || yVal > 3200;
}

void flappyLoop() {
  // ── Wait screen ──
  if (fbWaitStart) {
    display.clearDisplay();
    sharedDrawCenteredText("FLAPPY", 4, 2);
    sharedDrawCenteredText("BIRD",  18, 2);
    char hsBuf[16];
    snprintf(hsBuf, sizeof(hsBuf), "Best: %d", hsFlappy);
    sharedDrawCenteredText(hsBuf, 36);
    sharedDrawCenteredText("Any key: start", 50);
    display.display();
    if (fbJustPressed()) { fbWaitStart = false; fbStarted = false; delay(200); }
    return;
  }

  // ── Game over screen ──
  if (fbGameOver) {
    if (fbScore > hsFlappy) { hsFlappy = fbScore; hsSave("/hs_flappy.txt", hsFlappy); }
    display.clearDisplay();
    sharedDrawCenteredText("GAME OVER",  10, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "Score: %d", fbScore);
    sharedDrawCenteredText(buf, 30);
    snprintf(buf, sizeof(buf), "Best:  %d", hsFlappy);
    sharedDrawCenteredText(buf, 40);
    sharedDrawCenteredText("3x: menu", 54);
    display.display();
    if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; return; }
    if (fbJustPressed()) { delay(200); flappyInit(); fbWaitStart = false; fbStarted = false; }
    return;
  }

  // ── Waiting for first flap ──
  static bool lastJump = false;
  bool jump = fbJustPressed();

  if (!fbStarted) {
    display.clearDisplay();
    // Draw ground
    display.drawFastHLine(0, FB_GROUND_Y, SCREEN_WIDTH, SSD1306_WHITE);
    // Draw bird hovering
    fbDrawBird(FB_BIRD_X, (int)fbBirdY);
    sharedDrawCenteredText("Flap!", 2);
    display.display();
    if (jump && !lastJump) {
      fbBirdVY = FB_JUMP_VEL;
      fbStarted = true;
    }
    lastJump = jump;
    return;
  }

  // ── Physics ──
  fbBirdVY += FB_GRAVITY;
  fbBirdY  += fbBirdVY;

  // Clamp to ceiling
  if (fbBirdY < 0) { fbBirdY = 0; fbBirdVY = 0; }

  // Jump input
  if (jump && !lastJump) fbBirdVY = FB_JUMP_VEL;
  lastJump = jump;

  // Move pipes
  for (int i = 0; i < FB_PIPE_COUNT; i++) {
    fbPipes[i].x -= FB_PIPE_SPEED;
    // Recycle pipe off left edge
    if (fbPipes[i].x + FB_PIPE_W < 0) {
      // Find rightmost pipe
      int rightmost = 0;
      for (int j = 0; j < FB_PIPE_COUNT; j++)
        if (fbPipes[j].x > rightmost) rightmost = fbPipes[j].x;
      fbSpawnPipe(i, rightmost + FB_PIPE_SPACING);
      fbScore++;
    }
  }

  // Collision detection
  int bx = FB_BIRD_X, by = (int)fbBirdY;
  int bx2 = bx + FB_BIRD_W - 1, by2 = by + FB_BIRD_H - 1;

  // Ground / ceiling
  if (by2 >= FB_GROUND_Y) { fbGameOver = true; return; }

  // Pipes
  for (int i = 0; i < FB_PIPE_COUNT; i++) {
    int px  = fbPipes[i].x;
    int px2 = px + FB_PIPE_W - 1;
    int gapTop = fbPipes[i].gapY;
    int gapBot = gapTop + FB_PIPE_GAP;

    bool overlapX = bx2 >= px && bx <= px2;
    if (overlapX && (by < gapTop || by2 > gapBot)) {
      fbGameOver = true;
      return;
    }
  }

  // ── Draw ──
  display.clearDisplay();

  // Ground with grass tufts
  display.drawFastHLine(0, FB_GROUND_Y, SCREEN_WIDTH, SSD1306_WHITE);
  display.fillRect(0, FB_GROUND_Y + 1, SCREEN_WIDTH, SCREEN_HEIGHT - FB_GROUND_Y - 1, SSD1306_WHITE);
  for (int x = 2; x < SCREEN_WIDTH; x += 7)
    display.drawPixel(x, FB_GROUND_Y - 1, SSD1306_WHITE);

  // Pipes
  for (int i = 0; i < FB_PIPE_COUNT; i++) {
    int px = fbPipes[i].x;
    int gapTop = fbPipes[i].gapY;
    int gapBot = gapTop + FB_PIPE_GAP;

    // Top pipe
    display.fillRect(px, 0, FB_PIPE_W, gapTop, SSD1306_WHITE);
    // Top pipe cap
    display.fillRect(px - 1, gapTop - 3, FB_PIPE_W + 2, 3, SSD1306_WHITE);

    // Bottom pipe
    display.fillRect(px, gapBot, FB_PIPE_W, FB_GROUND_Y - gapBot, SSD1306_WHITE);
    // Bottom pipe cap
    display.fillRect(px - 1, gapBot, FB_PIPE_W + 2, 3, SSD1306_WHITE);
  }

  // Bird
  fbDrawBird(FB_BIRD_X, (int)fbBirdY);

  // Score top center
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", fbScore);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 1);
  display.setTextColor(SSD1306_BLACK);  // black on pipe bg looks bad, use XOR trick
  display.setTextColor(SSD1306_WHITE);
  display.print(buf);

  display.display();

  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DICE
// ═══════════════════════════════════════════════════════════════════════════

int  diceValue = 1;
bool diceWaiting = false;

void diceInit() {
  diceValue   = random(1, 7);
  diceWaiting = true;
}

void diceLoop() {
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  char buf[2];
  snprintf(buf, sizeof(buf), "%d", diceValue);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(buf);
  display.display();

  // Any input exits
  static bool lastAny = false;
  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  bool any = digitalRead(SW) == LOW || xVal < 800 || xVal > 3200 || yVal < 800 || yVal > 3200;
  if (any && !lastAny) {
    currentState  = MENU;
    menuLockUntil = millis() + 300;
  }
  lastAny = any;
}

// ═══════════════════════════════════════════════════════════════════════════
//  LAPTOP MONITOR
// ═══════════════════════════════════════════════════════════════════════════

#include <lwip/sockets.h>
#include <lwip/netdb.h>

int          laptopSock      = -1;
#define      LAPTOP_PORT     5000
#define      LAPTOP_SECRET   "ev3ryth1ng-os-k3y-2025"

char   laptopTime[6]    = "--:--";
int    laptopCPU         = -1;
int    laptopMEM         = -1;
int    laptopBAT         = -1;
char   laptopUptime[12]  = "--:--:--";
bool   laptopConnected   = false;
unsigned long laptopLastPacket = 0;

// ── Tiny HMAC-SHA256 (first 4 bytes → 8 hex chars) ───────────────────────
// Matches exactly what monitor.c produces

static void sha256_transform_esp(uint32_t s[8], const uint8_t d[64]) {
  static const uint32_t K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t W[64],a,b,c,e,f,g,h,T1,T2,dd;
  #define RR(x,n)(((x)>>(n))|((x)<<(32-(n))))
  for(int i=0;i<16;i++) W[i]=((uint32_t)d[i*4]<<24)|((uint32_t)d[i*4+1]<<16)|((uint32_t)d[i*4+2]<<8)|d[i*4+3];
  for(int i=16;i<64;i++) W[i]=(RR(W[i-2],17)^RR(W[i-2],19)^(W[i-2]>>10))+W[i-7]+(RR(W[i-15],7)^RR(W[i-15],18)^(W[i-15]>>3))+W[i-16];
  a=s[0];b=s[1];c=s[2];dd=s[3];e=s[4];f=s[5];g=s[6];h=s[7];
  for(int i=0;i<64;i++){T1=h+(RR(e,6)^RR(e,11)^RR(e,25))+((e&f)^(~e&g))+K[i]+W[i];T2=(RR(a,2)^RR(a,13)^RR(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=dd+T1;dd=c;c=b;b=a;a=T1+T2;}
  s[0]+=a;s[1]+=b;s[2]+=c;s[3]+=dd;s[4]+=e;s[5]+=f;s[6]+=g;s[7]+=h;
  #undef RR
}

static bool laptopVerifyTag(const char* tag, const char* msg) {
  const char* key = LAPTOP_SECRET;
  size_t klen = strlen(key);
  uint8_t kbuf[64]={0}, ipad[64], opad[64];
  if (klen <= 64) memcpy(kbuf, key, klen);
  for (int i=0;i<64;i++){ipad[i]=kbuf[i]^0x36;opad[i]=kbuf[i]^0x5c;}

  // inner hash
  uint32_t s[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  uint8_t blk[64]; size_t mlen=strlen(msg);
  memcpy(blk,ipad,64); sha256_transform_esp(s,blk);
  // process msg bytes
  size_t idx=0; uint64_t cnt=64;
  memset(blk,0,64);
  for(size_t i=0;i<mlen;i++){blk[idx++]=((uint8_t*)msg)[i];if(idx==64){sha256_transform_esp(s,blk);idx=0;cnt+=64;}}
  cnt+=idx;
  blk[idx++]=0x80; if(idx>56){while(idx<64)blk[idx++]=0;sha256_transform_esp(s,blk);idx=0;}
  memset(blk+idx,0,56-idx);
  uint64_t bits=cnt*8;
  for(int i=7;i>=0;i--){blk[56+i]=(uint8_t)(bits&0xff);bits>>=8;}
  sha256_transform_esp(s,blk);
  uint8_t tmp[32];
  for(int i=0;i<8;i++){tmp[i*4]=(s[i]>>24)&0xff;tmp[i*4+1]=(s[i]>>16)&0xff;tmp[i*4+2]=(s[i]>>8)&0xff;tmp[i*4+3]=s[i]&0xff;}

  // outer hash
  uint32_t s2[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  memcpy(blk,opad,64); sha256_transform_esp(s2,blk);
  memset(blk,0,64); memcpy(blk,tmp,32); blk[32]=0x80;
  memset(blk+33,0,23); uint64_t bits2=(64+32)*8;
  for(int i=7;i>=0;i--){blk[56+i]=(uint8_t)(bits2&0xff);bits2>>=8;}
  sha256_transform_esp(s2,blk);
  char computed[9];
  snprintf(computed,9,"%02x%02x%02x%02x",(s2[0]>>24)&0xff,(s2[0]>>16)&0xff,(s2[0]>>8)&0xff,s2[0]&0xff);
  return strncmp(tag, computed, 8) == 0;
}

void laptopMonitorInit() {
  laptopConnected  = false;
  laptopLastPacket = 0;
  strncpy(laptopTime,   "--:--",  6);
  strncpy(laptopUptime, "--:--:--", 12);
  laptopCPU = laptopMEM = laptopBAT = -1;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
  }
}

// Parse "TAG|HH:MM,CPU,MEM,BAT,HH:MM:SS"
void laptopParsePacket(char* buf) {
  // Split on '|'
  char* pipe = strchr(buf, '|');
  if (!pipe) return;
  *pipe = '\0';
  char* tag = buf;
  char* payload = pipe + 1;

  if (!laptopVerifyTag(tag, payload)) return;  // drop invalid

  char* tok = strtok(payload, ",");
  if (tok) { strncpy(laptopTime, tok, 5); laptopTime[5] = '\0'; }
  tok = strtok(NULL, ",");
  if (tok) laptopCPU = atoi(tok);
  tok = strtok(NULL, ",");
  if (tok) laptopMEM = atoi(tok);
  tok = strtok(NULL, ",");
  if (tok) laptopBAT = atoi(tok);
  tok = strtok(NULL, ",\r\n");
  if (tok) { strncpy(laptopUptime, tok, 11); laptopUptime[11] = '\0'; }
}

void drawBar(int x, int y, int w, int h, int pct) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int fill = (int)((w - 2) * pct / 100.0f);
  if (fill > 0)
    display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void laptopMonitorLoop() {
  if (checkTripleClick()) {
    if (laptopSock >= 0) { lwip_close(laptopSock); laptopSock = -1; }
    WiFi.disconnect(true);
    currentState = MENU;
    menuLockUntil = millis() + 300;
    return;
  }

  // Start raw UDP socket once WiFi is connected
  static bool udpStarted = false;
  // Reset socket if we just entered (sock closed on exit)
  if (laptopSock < 0) udpStarted = false;
  if (WiFi.status() == WL_CONNECTED && !udpStarted) {
    Serial.printf("ESP32 IP: %s\n", WiFi.localIP().toString().c_str());
    laptopSock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (laptopSock >= 0) {
      int yes = 1;
      lwip_setsockopt(laptopSock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
      lwip_setsockopt(laptopSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
      // Non-blocking
      lwip_fcntl(laptopSock, F_SETFL, O_NONBLOCK);
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family      = AF_INET;
      addr.sin_port        = htons(LAPTOP_PORT);
      addr.sin_addr.s_addr = INADDR_ANY;  // accept broadcast
      lwip_bind(laptopSock, (struct sockaddr*)&addr, sizeof(addr));
      udpStarted = true;
    }
  }
  if (WiFi.status() != WL_CONNECTED && udpStarted) {
    lwip_close(laptopSock); laptopSock = -1;
    udpStarted = false;
  }

  // Check for incoming UDP packet
  char rxBuf[64];
  int pktSize = 0;
  if (laptopSock >= 0) {
    pktSize = lwip_recv(laptopSock, rxBuf, sizeof(rxBuf) - 1, MSG_DONTWAIT);
  }
  Serial.printf("wifi=%d udpStarted=%d pkt=%d\n", WiFi.status(), (int)udpStarted, pktSize);
  if (pktSize > 0) {
    rxBuf[pktSize] = '\0';
    laptopParsePacket(rxBuf);
    laptopConnected  = true;
    laptopLastPacket = millis();
  }

  // Timeout after 4 seconds of no data
  if (laptopConnected && millis() - laptopLastPacket > 4000)
    laptopConnected = false;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (WiFi.status() != WL_CONNECTED) {
    sharedDrawCenteredText("Connecting to", 18);
    sharedDrawCenteredText("monitor...", 28);
    sharedDrawCenteredText("3x: exit", 54);
    display.display();
    return;
  }

  if (!laptopConnected) {
    sharedDrawCenteredText("Connecting to", 18);
    sharedDrawCenteredText("monitor...", 28);
    sharedDrawCenteredText("3x: exit", 54);
    display.display();
    return;
  }

  // ── Main display ──
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Time left, uptime right on same row
  // "13:05|uptime:01:23:45"
  char topRow[32];
  snprintf(topRow, sizeof(topRow), "%s|uptime:%s", laptopTime, laptopUptime);
  display.setCursor(0, 0);
  display.print(topRow);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // CPU
  char pct[7];
  snprintf(pct, sizeof(pct), "%3d%%", laptopCPU);
  display.setCursor(0, 13);  display.print("CPU");
  display.setCursor(SCREEN_WIDTH - 25, 13); display.print(pct);
  drawBar(0, 23, SCREEN_WIDTH, 5, laptopCPU);

  // MEM
  snprintf(pct, sizeof(pct), "%3d%%", laptopMEM);
  display.setCursor(0, 31);  display.print("MEM");
  display.setCursor(SCREEN_WIDTH - 25, 31); display.print(pct);
  drawBar(0, 41, SCREEN_WIDTH, 5, laptopMEM);

  // BAT
  display.setCursor(0, 49);  display.print("BAT");
  if (laptopBAT < 0) {
    display.setCursor(SCREEN_WIDTH - 25, 49);
    display.print(" N/A");
  } else {
    snprintf(pct, sizeof(pct), "%3d%%", laptopBAT);
    display.setCursor(SCREEN_WIDTH - 25, 49);
    display.print(pct);
    drawBar(0, 59, SCREEN_WIDTH, 4, laptopBAT);
  }

  display.display();
}

// ═══════════════════════════════════════════════════════════════════════════
//  SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void shutdownInit() {
  // Immediately shut down — no confirmation
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  // Hold all GPIOs low to kill LED current draw
  gpio_hold_en((gpio_num_t)2);   // onboard LED pin on most ESP32 devboards
  gpio_deep_sleep_hold_en();

  // Deep sleep with no wakeup source — only RESET pin wakes it
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_deep_sleep_start();
}

void shutdownLoop() {
  // Never reached — shutdownInit deep sleeps immediately
}

// ═══════════════════════════════════════════════════════════════════════════
//  STOPWATCH
// ═══════════════════════════════════════════════════════════════════════════

unsigned long swStart    = 0;
unsigned long swElapsed  = 0;
bool          swRunning  = false;

void stopwatchInit() {
  swStart   = 0;
  swElapsed = 0;
  swRunning = false;
}

void stopwatchLoop() {
  int xVal = analogRead(VRX);
  int yVal = analogRead(VRY);
  static bool lastAny   = false;
  static unsigned long lastPress = 0;
  static int pressCount = 0;

  bool any = digitalRead(SW) == LOW || xVal < 800 || xVal > 3200 || yVal < 800 || yVal > 3200;

  if (any && !lastAny) {
    unsigned long now = millis();
    if (now - lastPress < 400) {
      pressCount++;
    } else {
      pressCount = 1;
    }
    lastPress = now;

    if (pressCount >= 2) {
      // Double press — reset
      swElapsed  = 0;
      swStart    = millis();
      swRunning  = false;
      pressCount = 0;
    } else {
      // Single press — toggle
      if (!swRunning) {
        swStart   = millis() - swElapsed;
        swRunning = true;
      } else {
        swElapsed = millis() - swStart;
        swRunning = false;
      }
    }
  }
  lastAny = any;

  if (swRunning) swElapsed = millis() - swStart;

  unsigned long ms  = swElapsed % 1000;
  unsigned long s   = swElapsed / 1000;
  unsigned long sec = s % 60;
  unsigned long m   = (s / 60) % 60;
  unsigned long h   = s / 3600;

  // "HH:MM:SS.mmm" — size 2 for HH:MM:SS, size 1 for .ms inline
  char main_buf[9];
  char ms_buf[5];
  snprintf(main_buf, sizeof(main_buf), "%02lu:%02lu:%02lu", h, m, sec);
  snprintf(ms_buf,   sizeof(ms_buf),   ".%03lu", ms);

  display.clearDisplay();

  // Measure sizes
  int16_t x1, y1; uint16_t mw, mh, sw2, sh;
  display.setTextSize(2);
  display.getTextBounds(main_buf, 0, 0, &x1, &y1, &mw, &mh);
  display.setTextSize(1);
  display.getTextBounds(ms_buf, 0, 0, &x1, &y1, &sw2, &sh);

  // Total width = mw + sw2, center both together
  int totalW = mw + sw2;
  int startX = (SCREEN_WIDTH - totalW) / 2;
  int centerY = (SCREEN_HEIGHT - mh) / 2;

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(startX, centerY);
  display.print(main_buf);

  // Ms aligned to baseline of size-2 text
  display.setTextSize(1);
  display.setCursor(startX + mw, centerY + mh - sh);
  display.print(ms_buf);

  display.display();

  if (checkTripleClick()) { currentState = MENU; menuLockUntil = millis() + 300; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SETUP / LOOP
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
  pinMode(VRX, INPUT);
  pinMode(VRY, INPUT);
  pinMode(SW, INPUT_PULLUP);
  Serial.begin(115200);
  randomSeed(analogRead(36));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed");
    for (;;);
  }
  display.clearDisplay();
  display.display();
  hsLoad();
}

void loop() {
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  switch (currentState) {
    case MENU:            runMenu();            break;
    case GAME_SNAKE:      snakeLoop();          break;
    case GAME_PONG:       pongLoop();           break;
    case GAME_MAZE:       mazeLoop();           break;
    case GAME_PLATFORM:   platformLoop();       break;
    case GAME_WIFI:       wifiScanLoop();       break;
    case GAME_BT:         btScanLoop();         break;
    case GAME_CUBE:       cubeLoop();           break;
    case GAME_TETRIS:     tetrisLoop();         break;
    case GAME_FLAPPY:     flappyLoop();         break;
    case GAME_DICE:       diceLoop();           break;
    case GAME_LAPTOP:     laptopMonitorLoop();  break;
    case GAME_SHUTDOWN:   shutdownLoop();       break;
    case GAME_STOPWATCH:  stopwatchLoop();      break;
  }
}
