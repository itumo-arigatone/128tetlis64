#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// 4線 I2C モジュールは RESET 無し → -1
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I2C 上に ACK があるか（Adafruit begin は未接続でも true を返すことがある）
uint8_t probeI2C(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission();  // 0 = ACK（端末あり）
}

// エラー点滅: n 回短点滅 → 長休止を繰り返す
void blinkError(uint8_t n) {
  for (;;) {
    for (uint8_t i = 0; i < n; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(120);
      digitalWrite(LED_BUILTIN, LOW);
      delay(120);
    }
    delay(800);
  }
}

// 1ブロック = 2x2 px
#define BLOCK 2

// フィールド: 左右壁込み12列、見え始め〜底壁まで25行
// プレイアブルは x=1..10, y=0..23（底壁は y=24）
#define FIELD_W 12
#define FIELD_H 25
#define PLAY_W 10

// 画面上のフィールド左上（壁の外側含む）
// フィールド実寸: 幅 FIELD_W*BLOCK=24, 高さ FIELD_H*BLOCK=50
// プレイ中は中央寄せ、ゲームオーバー時は右端
#define FIELD_OX_PLAY 52
#define FIELD_OX_OVER (SCREEN_WIDTH - FIELD_W * BLOCK)
#define FIELD_OY 7

// ネクスト表示（フィールド右）
#define NEXT_OX (FIELD_OX_PLAY + FIELD_W * BLOCK + 4)
#define NEXT_OY (FIELD_OY + 2)

// スコア表示（フィールド左）
#define HUD_OX 2
#define HUD_OY 10

// ボタン（押下で GND、INPUT_PULLUP）
// 十字: D2/D3/D5/D6（D4 は OLED_RESET のため空けている）
#define PIN_LEFT  2
#define PIN_RIGHT 3
#define PIN_UP    5  // ハードドロップ
#define PIN_DOWN  6  // ソフトドロップ
// AB: D7/D8
#define PIN_A     7  // 右回転
#define PIN_B     8  // 左回転

// 落下間隔 (ms)
#define DROP_MS 750
#define SOFT_DROP_MS 50
// 左右: 初回リピート待ち / 連打間隔（狭いフィールドなので控えめに）
#define MOVE_DAS_MS 280
#define MOVE_ARR_MS 160
// 回転・ハードドロップのチャタリング防止
#define BTN_DEBOUNCE_MS 40
// 着地ゴーストの点滅間隔 (ms)
#define GHOST_BLINK_MS 420
// ライン消去点滅: 3往復 × 80ms × 2相 ≒ 480ms
#define CLEAR_BLINK_MS 80
#define CLEAR_BLINK_TIMES 3
// 動作確認用 Lチカ（Uno/Nano 内蔵 LED = D13）
#define HEARTBEAT_MS 500

// バッテリー残量（TP4056 B+ を 100k/100k 分圧 → A1）
// 未配線でもゲームは動く。妥当電圧のときだけ右上に % 表示
#define PIN_BAT A1
#define BAT_DIV 2
#define BAT_MV_EMPTY 3300
#define BAT_MV_FULL 4200
#define BAT_MV_MIN_VALID 2800
#define BAT_MV_MAX_VALID 4500
#define BAT_POLL_MS 1000
#define BAT_ADC_SAMPLES 4

// 壁・積みブロック用グリッド（1=占有）
uint8_t field[FIELD_H][FIELD_W];

// 現在落下中のミノ
int curMino;
int curX;  // field 座標（左上）。壁込みなのでプレイ開始は x=1
int curY;  // field 座標（上ほど小さい）
int curRot;

// 次に出るミノ
int nextMino;

// スコア・消したライン数
uint16_t linesCleared = 0;
uint32_t score = 0;

unsigned long lastDropMs = 0;
bool needsDraw = true;
bool softDrop = false;
bool gameOver = false;
bool gameOverWaitRelease = false;

// ライン消去アニメ
bool clearing = false;
uint32_t clearRows = 0;  // bit y = 消去対象行
unsigned long clearAnimStart = 0;
uint8_t pendingClearCount = 0;
bool pendingTspin = false;

// バッテリー表示用キャッシュ（表示専用。未配線なら batValid=false）
bool batValid = false;
uint8_t batPercent = 0;
unsigned long lastBatPollMs = 0;

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

// SRS キック (x, y)。y は下方向が正（Guideline の上向き y を反転済み）
// [fromRot][0=CW/1=CCW][test][0=x,1=y]
const int8_t PROGMEM kicksJlstz[4][2][5][2] = {
  { // from 0
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},  // ->1 CW
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},    // ->3 CCW
  },
  { // from 1
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},     // ->2 CW
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // ->0 CCW
  },
  { // from 2
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
  },
  { // from 3
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
  },
};

const int8_t PROGMEM kicksI[4][2][5][2] = {
  {
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},
  },
  {
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
  },
  {
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
  },
  {
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
  },
};

// ミノ index: 0=T 1=I 2=J 3=L 4=S 5=Z 6=O
bool lastWasRotate = false;

bool fieldFilled(int fx, int fy) {
  if (fx < 0 || fx >= FIELD_W || fy >= FIELD_H) return true;
  if (fy < 0) return false;
  return field[fy][fx] != 0;
}

// T の中心は 4x4 内 (row=2,col=1)。角が3つ以上埋まっていれば Tスピン
bool isTSpin() {
  if (curMino != 0 || !lastWasRotate) return false;
  int cx = curX + 1;
  int cy = curY + 2;
  int corners = 0;
  if (fieldFilled(cx - 1, cy - 1)) corners++;
  if (fieldFilled(cx + 1, cy - 1)) corners++;
  if (fieldFilled(cx - 1, cy + 1)) corners++;
  if (fieldFilled(cx + 1, cy + 1)) corners++;
  return corners >= 3;
}

void addScore(int cleared, bool tspin) {
  if (cleared > 0) {
    linesCleared += cleared;
  }
  if (tspin) {
    // Tスピン: 0/1/2/3 ライン
    static const uint16_t tsp[] = {400, 800, 1200, 1600};
    int c = cleared;
    if (c > 3) c = 3;
    score += tsp[c];
  } else if (cleared > 0) {
    static const uint16_t points[] = {0, 100, 300, 500, 800};
    if (cleared > 4) cleared = 4;
    score += points[cleared];
  }
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

// 揃った行を検出して clearRows に載せ、消さずに件数を返す
int markFullLines() {
  clearRows = 0;
  int cleared = 0;
  for (int y = 0; y < FIELD_H - 1; y++) {
    bool full = true;
    for (int x = 1; x <= PLAY_W; x++) {
      if (!field[y][x]) {
        full = false;
        break;
      }
    }
    if (!full) continue;
    clearRows |= (1UL << y);
    cleared++;
  }
  return cleared;
}

int clearLines() {
  int cleared = 0;
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
    cleared++;
    y++; // 同じ行を再チェック
  }
  return cleared;
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
  lastWasRotate = false;
  return !collides(curMino, curRot, curX, curY);
}

bool tryMove(int dx) {
  if (collides(curMino, curRot, curX + dx, curY)) return false;
  curX += dx;
  lastWasRotate = false;
  needsDraw = true;
  return true;
}

bool tryRotate(int dir) {
  // dir: +1 右回転(CW), -1 左回転(CCW)
  int newRot = (curRot + dir + 4) % 4;
  int dirIdx = (dir > 0) ? 0 : 1;

  if (curMino == 6) {
    // O はキック不要
    if (!collides(curMino, newRot, curX, curY)) {
      curRot = newRot;
      lastWasRotate = true;
      needsDraw = true;
      return true;
    }
    return false;
  }

  for (uint8_t i = 0; i < 5; i++) {
    int8_t kx, ky;
    if (curMino == 1) {
      kx = (int8_t)pgm_read_byte(&kicksI[curRot][dirIdx][i][0]);
      ky = (int8_t)pgm_read_byte(&kicksI[curRot][dirIdx][i][1]);
    } else {
      kx = (int8_t)pgm_read_byte(&kicksJlstz[curRot][dirIdx][i][0]);
      ky = (int8_t)pgm_read_byte(&kicksJlstz[curRot][dirIdx][i][1]);
    }
    int nx = curX + kx;
    int ny = curY + ky;
    if (!collides(curMino, newRot, nx, ny)) {
      curRot = newRot;
      curX = nx;
      curY = ny;
      lastWasRotate = true;
      needsDraw = true;
      return true;
    }
  }
  return false;
}

void finishClearAndContinue() {
  clearLines();
  addScore(pendingClearCount, pendingTspin);
  clearing = false;
  clearRows = 0;
  pendingClearCount = 0;
  pendingTspin = false;
  if (!spawnPiece()) {
    gameOver = true;
    gameOverWaitRelease = true; // 押しっぱなしでの即リスタート防止
  }
  lastDropMs = millis();
  needsDraw = true;
}

void lockAndContinue() {
  bool tspin = isTSpin();
  lockPiece();
  int cleared = markFullLines();
  if (cleared > 0) {
    clearing = true;
    pendingClearCount = (uint8_t)cleared;
    pendingTspin = tspin;
    clearAnimStart = millis();
    needsDraw = true;
    return;
  }
  addScore(0, tspin);
  if (!spawnPiece()) {
    gameOver = true;
    gameOverWaitRelease = true; // 押しっぱなしでの即リスタート防止
  }
  lastDropMs = millis();
  needsDraw = true;
}

void restartGame() {
  resetField();
  pickNext();
  spawnPiece();
  linesCleared = 0;
  score = 0;
  gameOver = false;
  gameOverWaitRelease = false;
  softDrop = false;
  clearing = false;
  clearRows = 0;
  pendingClearCount = 0;
  pendingTspin = false;
  lastDropMs = millis();
  needsDraw = true;
}

bool anyButtonDown() {
  return digitalRead(PIN_LEFT) == LOW
      || digitalRead(PIN_RIGHT) == LOW
      || digitalRead(PIN_UP) == LOW
      || digitalRead(PIN_DOWN) == LOW
      || digitalRead(PIN_A) == LOW
      || digitalRead(PIN_B) == LOW;
}

void hardDrop() {
  while (!collides(curMino, curRot, curX, curY + 1)) {
    curY++;
  }
  lockAndContinue();
}

void handleButtons() {
  static bool leftWas = false;
  static bool rightWas = false;
  static bool aStable = false;
  static bool bStable = false;
  static bool upStable = false;
  static bool anyStable = false;
  static unsigned long aChangeMs = 0;
  static unsigned long bChangeMs = 0;
  static unsigned long upChangeMs = 0;
  static unsigned long anyChangeMs = 0;
  static unsigned long leftNext = 0;
  static unsigned long rightNext = 0;
  static bool anyRawLast = false;

  unsigned long now = millis();

  // ゲームオーバー中はどれか押すまで待つ
  if (gameOver) {
    bool anyRaw = anyButtonDown();
    if (gameOverWaitRelease) {
      if (!anyRaw) gameOverWaitRelease = false;
      softDrop = false;
      return;
    }
    if (anyRaw != anyRawLast) {
      anyChangeMs = now;
      anyRawLast = anyRaw;
    }
    if ((now - anyChangeMs) >= BTN_DEBOUNCE_MS && anyRaw != anyStable) {
      anyStable = anyRaw;
      if (anyStable) restartGame();
    }
    softDrop = false;
    return;
  }

  // ライン消去アニメ中は操作しない
  if (clearing) {
    softDrop = false;
    return;
  }

  bool left = digitalRead(PIN_LEFT) == LOW;
  bool right = digitalRead(PIN_RIGHT) == LOW;
  bool aRaw = digitalRead(PIN_A) == LOW;
  bool bRaw = digitalRead(PIN_B) == LOW;
  bool down = digitalRead(PIN_DOWN) == LOW;
  bool upRaw = digitalRead(PIN_UP) == LOW;

  softDrop = down;

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

  // 回転・ハードドロップ: 安定した「押下」だけ反応
  static bool aRawLast = false;
  static bool bRawLast = false;
  static bool upRawLast = false;
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
  if (upRaw != upRawLast) {
    upChangeMs = now;
    upRawLast = upRaw;
  }
  if ((now - upChangeMs) >= BTN_DEBOUNCE_MS && upRaw != upStable) {
    upStable = upRaw;
    if (upStable) hardDrop();
  }
}

// 分圧未接続でもゲーム処理には触れない（表示の有無だけ）
void updateBattery() {
  unsigned long now = millis();
  if (lastBatPollMs != 0 && (now - lastBatPollMs) < BAT_POLL_MS) return;
  lastBatPollMs = now;

  uint16_t sum = 0;
  for (uint8_t i = 0; i < BAT_ADC_SAMPLES; i++) {
    sum += analogRead(PIN_BAT);
  }
  uint16_t raw = sum / BAT_ADC_SAMPLES;
  // AVcc=5V 想定、分圧 1/2 を戻す
  uint16_t mV = (uint16_t)((raw * 5000UL * (uint32_t)BAT_DIV) / 1023UL);

  bool valid = (mV >= BAT_MV_MIN_VALID && mV <= BAT_MV_MAX_VALID);
  uint8_t pct = 0;
  if (valid) {
    if (mV <= BAT_MV_EMPTY) {
      pct = 0;
    } else if (mV >= BAT_MV_FULL) {
      pct = 100;
    } else {
      pct = (uint8_t)(((uint32_t)(mV - BAT_MV_EMPTY) * 100UL) /
                      (uint32_t)(BAT_MV_FULL - BAT_MV_EMPTY));
    }
  }

  if (valid != batValid || (valid && pct != batPercent)) {
    needsDraw = true;
  }
  batValid = valid;
  batPercent = pct;
}

void drawBattery() {
  if (!batValid) return;

  char buf[5];
  if (batPercent >= 100) {
    buf[0] = '1';
    buf[1] = '0';
    buf[2] = '0';
    buf[3] = '%';
    buf[4] = '\0';
  } else if (batPercent >= 10) {
    buf[0] = '0' + (batPercent / 10);
    buf[1] = '0' + (batPercent % 10);
    buf[2] = '%';
    buf[3] = '\0';
  } else {
    buf[0] = '0' + batPercent;
    buf[1] = '%';
    buf[2] = '\0';
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - (int16_t)w, 0);
  display.print(buf);
}

void drawFrame() {
  display.clearDisplay();
  drawWallFrame();
  drawField();
  if (gameOver) {
    drawGameOverScreen();
  } else {
    drawHud();
    drawBattery();
    // 消去アニメ中はロック済みなので落下ミノ／ゴーストは描かない
    if (!clearing) {
      drawGhost();
      drawPiece();
    }
    drawNext();
  }
  display.display();
  needsDraw = false;
}

void drawHud() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(HUD_OX, HUD_OY);
  display.print(F("L:"));
  display.print(linesCleared);
  display.setCursor(HUD_OX, HUD_OY + 12);
  display.print(F("S:"));
  display.print(score);
}

void drawCenteredText(const __FlashStringHelper *text, int y, uint8_t size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int16_t)w) / 2, y);
  display.print(text);
}

void drawCenteredNumber(uint32_t value, int y, uint8_t size) {
  char buf[11];
  ultoa(value, buf, 10);
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int16_t)w) / 2, y);
  display.print(buf);
}

void drawGameOverScreen() {
  // 上: GAME OVER / 中: スコア / 下: ライン数（いずれも中央寄せ・数値のみ）
  drawCenteredText(F("GAME OVER"), 6, 1);
  drawCenteredNumber(score, 26, 2);
  drawCenteredNumber(linesCleared, 50, 1);
}

int fieldDrawX() {
  return gameOver ? FIELD_OX_OVER : FIELD_OX_PLAY;
}

void drawBlock(int fx, int fy) {
  // 底壁行・左右壁は壁枠として描くのでスキップしてもよいが、
  // 積みブロックはプレイエリアのみ描画
  int px = fieldDrawX() + fx * BLOCK;
  int py = FIELD_OY + fy * BLOCK;
  display.fillRect(px, py, BLOCK, BLOCK, SSD1306_WHITE);
}

bool shapeOccupied(int r, int c) {
  if (r < 0 || r >= 4 || c < 0 || c >= 4) return false;
  return cellAt(curMino, curRot, r, c);
}

int ghostY() {
  int y = curY;
  while (!collides(curMino, curRot, curX, y + 1)) {
    y++;
  }
  return y;
}

// 着地位置のシルエット外周を 1px で描く（点滅で視認性を上げる）
void drawGhost() {
  if ((millis() / GHOST_BLINK_MS) & 1) return; // 点滅のオフ相

  int gy = ghostY();
  if (gy == curY) return; // 着地直前は本体と重なるので省略

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!shapeOccupied(r, c)) continue;
      int fx = curX + c;
      int fy = gy + r;
      if (fy < 0) continue;

      int px = fieldDrawX() + fx * BLOCK;
      int py = FIELD_OY + fy * BLOCK;

      if (!shapeOccupied(r - 1, c)) {
        display.drawFastHLine(px, py, BLOCK, SSD1306_WHITE);
      }
      if (!shapeOccupied(r + 1, c)) {
        display.drawFastHLine(px, py + BLOCK - 1, BLOCK, SSD1306_WHITE);
      }
      if (!shapeOccupied(r, c - 1)) {
        display.drawFastVLine(px, py, BLOCK, SSD1306_WHITE);
      }
      if (!shapeOccupied(r, c + 1)) {
        display.drawFastVLine(px + BLOCK - 1, py, BLOCK, SSD1306_WHITE);
      }
    }
  }
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
  // 消去対象行はオフ相で描画スキップ（点滅）
  bool hideClear = clearing && (((millis() - clearAnimStart) / CLEAR_BLINK_MS) & 1);
  for (int y = 0; y < FIELD_H - 1; y++) {
    if (hideClear && (clearRows & (1UL << y))) continue;
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

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);

  randomSeed(analogRead(0));

  Wire.begin();
  delay(50);

  uint8_t addr = 0;
  if (probeI2C(0x3C) == 0) {
    addr = 0x3C;
  } else if (probeI2C(0x3D) == 0) {
    addr = 0x3D;
  } else {
    // OLED 未検出（配線・電源・ピンを疑う）: 2回点滅
    blinkError(2);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
    // begin 失敗: 3回点滅
    blinkError(3);
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
  // 動作確認: 約0.5秒周期で D13 を点滅
  digitalWrite(LED_BUILTIN, (millis() / HEARTBEAT_MS) & 1);

  updateBattery();
  handleButtons();

  if (clearing) {
    static uint8_t clearBlinkPhase = 0;
    unsigned long elapsed = millis() - clearAnimStart;
    uint8_t phase = (elapsed / CLEAR_BLINK_MS) & 1;
    if (phase != clearBlinkPhase) {
      clearBlinkPhase = phase;
      needsDraw = true;
    }
    if (elapsed >= (unsigned long)CLEAR_BLINK_TIMES * 2 * CLEAR_BLINK_MS) {
      clearBlinkPhase = 0;
      finishClearAndContinue();
    }
  } else {
    // ゴースト点滅のために相が変わったら再描画
    static uint8_t ghostBlinkPhase = 0;
    uint8_t phase = (millis() / GHOST_BLINK_MS) & 1;
    if (!gameOver && phase != ghostBlinkPhase) {
      ghostBlinkPhase = phase;
      needsDraw = true;
    }

    if (!gameOver) {
      unsigned long now = millis();
      unsigned long interval = softDrop ? SOFT_DROP_MS : DROP_MS;
      if (now - lastDropMs >= interval) {
        lastDropMs = now;

        if (!collides(curMino, curRot, curX, curY + 1)) {
          curY++;
          needsDraw = true;
        } else {
          lockAndContinue();
        }
      }
    }
  }

  if (needsDraw) {
    drawFrame();
  }
}
