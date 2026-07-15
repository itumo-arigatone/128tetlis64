#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 1ブロック = 2x2 px
#define BLOCK 2

// フィールド: 左右壁込み12列、見え始め〜底壁まで25行
// プレイアブルは x=1..10, y=0..23（底壁は y=24）
#define FIELD_W 12
#define FIELD_H 25
#define PLAY_W 10

// 画面上のフィールド左上（壁の外側含む）
// フィールド実寸: 幅 FIELD_W*BLOCK=24, 高さ FIELD_H*BLOCK=50
// 128x64 の中央寄せ → X=(128-24)/2=52, Y=(64-50)/2=7
#define FIELD_OX 52
#define FIELD_OY 7

// ネクスト表示（フィールド右）
#define NEXT_OX (FIELD_OX + FIELD_W * BLOCK + 4)
#define NEXT_OY (FIELD_OY + 2)

// ボタン（押下で GND、INPUT_PULLUP）
#define PIN_LEFT  2
#define PIN_RIGHT 3
#define PIN_A     5  // 右回転
#define PIN_B     6  // 左回転

// 落下間隔 (ms)
#define DROP_MS 250
// 左右: 初回リピート待ち / 連打間隔（狭いフィールドなので控えめに）
#define MOVE_DAS_MS 280
#define MOVE_ARR_MS 160
// 回転ボタンのチャタリング防止
#define BTN_DEBOUNCE_MS 40

// 壁・積みブロック用グリッド（1=占有）
uint8_t field[FIELD_H][FIELD_W];

// 現在落下中のミノ
int curMino;
int curX;  // field 座標（左上）。壁込みなのでプレイ開始は x=1
int curY;  // field 座標（上ほど小さい）
int curRot;

// 次に出るミノ
int nextMino;

unsigned long lastDropMs = 0;
bool needsDraw = true;

// 7種ミノ × 4回転 × 4x4
// 各回転は 4x4 の占有マスク
const uint8_t PROGMEM shapes[7][4][4][4] = {
  // T
  {
    {{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,1,0,0}},
    {{0,0,0,0},{0,0,0,0},{1,1,1,0},{0,1,0,0}},
    {{0,0,0,0},{0,1,0,0},{1,1,0,0},{0,1,0,0}},
  },
  // I
  {
    {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
    {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
    {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
    {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}},
  },
  // J
  {
    {{0,0,0,0},{1,0,0,0},{1,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,1,0},{0,1,0,0},{0,1,0,0}},
    {{0,0,0,0},{0,0,0,0},{1,1,1,0},{0,0,1,0}},
    {{0,0,0,0},{0,1,0,0},{0,1,0,0},{1,1,0,0}},
  },
  // L
  {
    {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,0,0},{0,1,0,0},{0,1,1,0}},
    {{0,0,0,0},{0,0,0,0},{1,1,1,0},{1,0,0,0}},
    {{0,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,0,0}},
  },
  // S
  {
    {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0}},
    {{0,0,0,0},{0,0,0,0},{0,1,1,0},{1,1,0,0}},
    {{0,0,0,0},{1,0,0,0},{1,1,0,0},{0,1,0,0}},
  },
  // Z
  {
    {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,0,1,0},{0,1,1,0},{0,1,0,0}},
    {{0,0,0,0},{0,0,0,0},{1,1,0,0},{0,1,1,0}},
    {{0,0,0,0},{0,1,0,0},{1,1,0,0},{1,0,0,0}},
  },
  // O
  {
    {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
  },
};

uint8_t cellAt(int mino, int rot, int row, int col) {
  return pgm_read_byte(&shapes[mino][rot][row][col]);
}

void resetField() {
  for (int y = 0; y < FIELD_H; y++) {
    for (int x = 0; x < FIELD_W; x++) {
      if (x == 0 || x == FIELD_W - 1 || y == FIELD_H - 1) {
        field[y][x] = 1;
      } else {
        field[y][x] = 0;
      }
    }
  }
}

bool collides(int mino, int rot, int ox, int oy) {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!cellAt(mino, rot, r, c)) continue;
      int fx = ox + c;
      int fy = oy + r;
      if (fx < 0 || fx >= FIELD_W || fy < 0 || fy >= FIELD_H) return true;
      if (field[fy][fx]) return true;
    }
  }
  return false;
}

void lockPiece() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!cellAt(curMino, curRot, r, c)) continue;
      int fx = curX + c;
      int fy = curY + r;
      if (fy >= 0 && fy < FIELD_H && fx >= 0 && fx < FIELD_W) {
        field[fy][fx] = 1;
      }
    }
  }
}

void clearLines() {
  for (int y = FIELD_H - 2; y >= 0; y--) {
    bool full = true;
    for (int x = 1; x <= PLAY_W; x++) {
      if (!field[y][x]) {
        full = false;
        break;
      }
    }
    if (!full) continue;

    // 上から詰める
    for (int yy = y; yy > 0; yy--) {
      for (int x = 1; x <= PLAY_W; x++) {
        field[yy][x] = field[yy - 1][x];
      }
    }
    for (int x = 1; x <= PLAY_W; x++) {
      field[0][x] = 0;
    }
    y++; // 同じ行を再チェック
  }
}

void pickNext() {
  nextMino = random(0, 7);
}

bool spawnPiece() {
  curMino = nextMino;
  pickNext();
  curRot = 0;
  // プレイ幅中央付近の固定位置から出現
  curX = 4;
  curY = 0;
  return !collides(curMino, curRot, curX, curY);
}

bool tryMove(int dx) {
  if (collides(curMino, curRot, curX + dx, curY)) return false;
  curX += dx;
  needsDraw = true;
  return true;
}

bool tryRotate(int dir) {
  // dir: +1 右回転, -1 左回転
  int newRot = (curRot + dir + 4) % 4;
  // 壁際でも回りやすいよう、左右に少しずらして再試行
  static const int8_t kicks[] = {0, -1, 1, -2, 2};
  for (uint8_t i = 0; i < sizeof(kicks); i++) {
    int nx = curX + kicks[i];
    if (!collides(curMino, newRot, nx, curY)) {
      curRot = newRot;
      curX = nx;
      needsDraw = true;
      return true;
    }
  }
  return false;
}

void handleButtons() {
  static bool leftWas = false;
  static bool rightWas = false;
  static bool aStable = false;
  static bool bStable = false;
  static unsigned long aChangeMs = 0;
  static unsigned long bChangeMs = 0;
  static unsigned long leftNext = 0;
  static unsigned long rightNext = 0;

  unsigned long now = millis();
  bool left = digitalRead(PIN_LEFT) == LOW;
  bool right = digitalRead(PIN_RIGHT) == LOW;
  bool aRaw = digitalRead(PIN_A) == LOW;
  bool bRaw = digitalRead(PIN_B) == LOW;

  if (left) {
    if (!leftWas) {
      tryMove(-1);
      leftNext = now + MOVE_DAS_MS;
    } else if (now >= leftNext) {
      tryMove(-1);
      leftNext = now + MOVE_ARR_MS;
    }
  }
  leftWas = left;

  if (right) {
    if (!rightWas) {
      tryMove(1);
      rightNext = now + MOVE_DAS_MS;
    } else if (now >= rightNext) {
      tryMove(1);
      rightNext = now + MOVE_ARR_MS;
    }
  }
  rightWas = right;

  // 回転: 信号が一定時間安定したあとの「押下」だけ反応（離し時チャタリング対策）
  static bool aRawLast = false;
  static bool bRawLast = false;
  if (aRaw != aRawLast) {
    aChangeMs = now;
    aRawLast = aRaw;
  }
  if ((now - aChangeMs) >= BTN_DEBOUNCE_MS && aRaw != aStable) {
    aStable = aRaw;
    if (aStable) tryRotate(1);
  }
  if (bRaw != bRawLast) {
    bChangeMs = now;
    bRawLast = bRaw;
  }
  if ((now - bChangeMs) >= BTN_DEBOUNCE_MS && bRaw != bStable) {
    bStable = bRaw;
    if (bStable) tryRotate(-1);
  }
}

void drawFrame() {
  display.clearDisplay();
  drawWallFrame();
  drawField();
  drawPiece();
  drawNext();
  display.display();
  needsDraw = false;
}

void drawBlock(int fx, int fy) {
  // 底壁行・左右壁は壁枠として描くのでスキップしてもよいが、
  // 積みブロックはプレイエリアのみ描画
  int px = FIELD_OX + fx * BLOCK;
  int py = FIELD_OY + fy * BLOCK;
  display.fillRect(px, py, BLOCK, BLOCK, SSD1306_WHITE);
}

void drawWallFrame() {
  // 左右壁 + 底壁（2px単位）
  for (int y = 0; y < FIELD_H; y++) {
    drawBlock(0, y);
    drawBlock(FIELD_W - 1, y);
  }
  for (int x = 0; x < FIELD_W; x++) {
    drawBlock(x, FIELD_H - 1);
  }
}

void drawField() {
  for (int y = 0; y < FIELD_H - 1; y++) {
    for (int x = 1; x <= PLAY_W; x++) {
      if (field[y][x]) drawBlock(x, y);
    }
  }
}

void drawPiece() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!cellAt(curMino, curRot, r, c)) continue;
      int fx = curX + c;
      int fy = curY + r;
      if (fy < 0) continue;
      drawBlock(fx, fy);
    }
  }
}

void drawNext() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!cellAt(nextMino, 0, r, c)) continue;
      int px = NEXT_OX + c * BLOCK;
      int py = NEXT_OY + r * BLOCK;
      display.fillRect(px, py, BLOCK, BLOCK, SSD1306_WHITE);
    }
  }
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 28);
  display.print(F("GAME OVER"));
  display.display();
  delay(1500);
}

void setup() {
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  randomSeed(analogRead(0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.clearDisplay();
  display.display();
  delay(200);

  resetField();
  pickNext();
  spawnPiece();
  lastDropMs = millis();
  needsDraw = true;
}

void loop() {
  handleButtons();

  unsigned long now = millis();
  if (now - lastDropMs >= DROP_MS) {
    lastDropMs = now;

    if (!collides(curMino, curRot, curX, curY + 1)) {
      curY++;
    } else {
      lockPiece();
      clearLines();
      if (!spawnPiece()) {
        drawGameOver();
        resetField();
        pickNext();
        spawnPiece();
      }
    }
    needsDraw = true;
  }

  if (needsDraw) {
    drawFrame();
  }
}
