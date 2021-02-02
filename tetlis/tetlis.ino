/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

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
#define START_POSITION_Y 6

const unsigned char PROGMEM wall[] = {
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

const unsigned char PROGMEM minos[][8] =
    {
      { B00000000,
        B00000000,
        B00000000,
        B00000000,
        B00001100,
        B00001100,
        B00111111,
        B00111111 },

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

  for(;;) {
    startTetlis(minos[0], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[1], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[2], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[3], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[4], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[5], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
    startTetlis(minos[6], LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
  }
}

void loop() {
}

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2

void startTetlis(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  int8_t f, icons[NUMFLAKES][3];
  icons[0][XPOS]   = START_POSITION_X; // random(1 - LOGO_WIDTH, display.width());
  icons[0][YPOS]   = -LOGO_HEIGHT;
  icons[0][DELTAY] = 3;
  for(int i=0; i<20; i++) {
    display.clearDisplay(); // Clear the display buffer

    // Draw Wall
    display.drawBitmap(42, 8, wall, WALL_WIDTH, WALL_HEIGHT, SSD1306_WHITE);

    // Draw each snowflake:
    display.drawBitmap(START_POSITION_X, icons[0][YPOS], bitmap, w, h, SSD1306_WHITE);

    display.display(); // Show the display buffer on the screen
    delay(200);        // Pause for 1/10 second

    // Then update coordinates of each flake...
    for(f=0; f< NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS]   = START_POSITION_X;
        icons[f][YPOS]   = -LOGO_HEIGHT;
        icons[f][DELTAY] = 3;
      }
    }
  }
}
