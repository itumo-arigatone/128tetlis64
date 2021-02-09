#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     1 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   8
#define LOGO_WIDTH    8
#define WALL_HEIGHT   42
#define WALL_WIDTH    24

#define START_POSITION_X 50
#define START_POSITION_Y 14

// フィールドの座標
// topL(42,14) topR(66,14)
// btmL(42,56) btmR(66,56)
// 1ブロックが2*2pxのイメージ
const unsigned char PROGMEM wallImg[] = {
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11000000,B00000000,B00000011,
  B11111111,B11111111,B11111111,
  B11111111,B11111111,B11111111,
};

const unsigned char PROGMEM minosImg[][8] =
    {
      { B00000000,
        B00000000,
        B00001100,
        B00001100,
        B00111111,
        B00111111,
        B00000000,
        B00000000 },

      { B00110000,
        B00110000,
        B00110000,
        B00110000,
        B00110000,
        B00110000,
        B00110000,
        B00110000 },

      { B00000000,
        B00000000,
        B00110000,
        B00110000,
        B00110000,
        B00110000,
        B00111100,
        B00111100 },

      { B00000000,
        B00000000,
        B00110000,
        B00110000,
        B00110000,
        B00110000,
        B11110000,
        B11110000 },

      { B00000000,
        B00000000,
        B00000000,
        B00000000,
        B11110000,
        B11110000,
        B00111100,
        B00111100 },

      { B00000000,
        B00000000,
        B00000000,
        B00000000,
        B00001111,
        B00001111,
        B00111100,
        B00111100 },

      { B00000000,
        B00000000,
        B00111100,
        B00111100,
        B00111100,
        B00111100,
        B00000000,
        B00000000 }
    };

// index 5 = x座標の0
// フィールド簡略配列
unsigned char wall[][12] = {
  {1,0,0,0,0,0,0,0,0,0,0,1}, // 開始位置
  {1,0,0,0,0,0,0,0,0,0,0,1}, // 壁は描写されない
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1}, // ここまで
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1}
};

const unsigned char PROGMEM minos[7][4][4] = {
    {
      {0,0,0,0},
      {0,0,1,0},
      {0,1,1,1},
      {0,0,0,0}
    },
    {
      {0,1,0,0},
      {0,1,0,0},
      {0,1,0,0},
      {0,1,0,0}
    },
    {
      {0,0,0,0},
      {0,1,0,0},
      {0,1,0,0},
      {0,1,1,0}
    },
    {
      {0,0,0,0},
      {0,1,0,0},
      {0,1,0,0},
      {1,1,0,0}
    },
    {
      {0,0,0,0},
      {0,0,0,0},
      {1,1,0,0},
      {0,1,1,0}
    },
    {
      {0,0,0,0},
      {0,0,0,0},
      {0,0,1,1},
      {0,1,1,0}
    },
    {
      {0,0,0,0},
      {0,1,1,0},
      {0,1,1,0},
      {0,0,0,0}
    },
  };

void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  delay(2000);

  startTetlis(LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps

}

void loop() {
}

/**
 * hit check function
 * param; y
 * param; x
 * param: 現在のミノの種類
 */
bool hitCheck(int y, int nextX, int minoIndex) {
  // 落下中のブロックの次の位置情報を取得
  int nextY = y;
  int xIndex = nextX + 5;
  bool result = false;
  // 現在のフィールドと比較
  // minos の配列の数だけ回す
  for(int i=0; i<4; i++){ // y
    for(int j=0; j<4; j++) { //x
      if ( wall[nextY + i][xIndex + j] >= 1 && minos[minoIndex][i][j] >= 1 ) {
        result = true;
        break;
      }
    }
  }
  return result;
}

/**
 * stop mino function
 */
void stopMino() {

}

/**
 *  save field information
 */
void saveField(int x, int y, int minoIndex) {
  Serial.println(minos[minoIndex][0][1]);
  for(int i=0; i<4; i++){ // y
    for(int j=0; j<4; j++) { //x
      if(minos[minoIndex][i][j] >= 1){
        wall[y + i][x + j] = 1; // [i][j];
      }
    }
  }
}

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2

void startTetlis(uint8_t w, uint8_t h) {
  for(;;){
    int8_t f, icons[NUMFLAKES][3];
    icons[0][XPOS]   = START_POSITION_X; // random(1 - LOGO_WIDTH, display.width());
    icons[0][YPOS]   = START_POSITION_Y - 8;
    icons[0][DELTAY] = 2;
    // 表示するミノを設定
    int ran = rand() % 6 + 1;
    int *selectMino = &ran;
    for(int i=0; i<30; i++) {
      display.clearDisplay(); // Clear the display buffer

      // TODO ボタンからx座標の位置を常に把握しておく
      int positionX = 0;
      int positionY = i;
  
      // あたり判定
      bool hit = hitCheck(i, positionX, *selectMino);
      if( hit ) {
        // 着地だったら固定処理とフィールド登録処理
        Serial.println(*selectMino);
        Serial.println(minos[6][0][1]);
        saveField(positionX, positionY, *selectMino);
        break;
      }
      // Draw Wall
      display.drawBitmap(42, 14, wallImg, WALL_WIDTH, WALL_HEIGHT, SSD1306_WHITE);
  
      // Draw each snowflake:
      display.drawBitmap(START_POSITION_X, icons[0][YPOS], minosImg[*selectMino], w, h, SSD1306_WHITE);
      
      display.display(); // Show the display buffer on the screen
      delay(300);        // Pause for 1/10 second
  
      // Then update coordinates of each flake...
      for(f=0; f< NUMFLAKES; f++) {
        icons[f][YPOS] += icons[f][DELTAY];
      }
    }
  }
}
