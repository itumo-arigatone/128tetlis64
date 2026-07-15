#include <SPI.h>
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
#define FIELD_OX 42
#define FIELD_OY 14

// 落下間隔 (ms)
#define DROP_MS 250

// 壁・積みブロック用グリッド（1=占有）
uint8_t field[FIELD_H][FIELD_W];

// 現在落下中のミノ
int curMino;
int curX;  // field 座標（左上）。壁込みなのでプレイ開始は x=1
int curY;  // field 座標（上ほど小さい）
int curRot;

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

bool spawnPiece() {
  curMino = random(0, 7);
  curRot = random(0, 4);
  // ボタンなしでも積み方がバラけるよう、出現Xを少しランダムに
  curX = 1 + random(0, PLAY_W - 3);
  curY = 0;
  return !collides(curMino, curRot, curX, curY);
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
  Serial.begin(9600);
  randomSeed(analogRead(0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.display();
  delay(200);

  resetField();
  spawnPiece();
}

void loop() {
  // 1マス落下
  if (!collides(curMino, curRot, curX, curY + 1)) {
    curY++;
  } else {
    // 着地 → 固定 → ライン消し → 次ミノ
    lockPiece();
    clearLines();
    if (!spawnPiece()) {
      drawGameOver();
      resetField();
      spawnPiece();
    }
  }

  display.clearDisplay();
  drawWallFrame();
  drawField();
  drawPiece();
  display.display();

  delay(DROP_MS);
}
