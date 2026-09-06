#include "display_ui.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "config.h"

namespace {

MatrixPanel_I2S_DMA *matrix = nullptr;

uint16_t COLOR_MTA_BLUE;
uint16_t COLOR_WHITE;
uint16_t COLOR_YELLOW;

// Small 8x8 "bullet" marker used next to the direction label: a filled
// blue circle with a white "C", echoing the real subway bullet.
void drawMiniBullet(int x, int y) {
  matrix->fillCircle(x + 3, y + 3, 4, COLOR_MTA_BLUE);
  matrix->setTextSize(1);
  matrix->setTextColor(COLOR_WHITE);
  matrix->setCursor(x + 1, y);
  matrix->print("C");
}

void printLabelRow(int y, const char *label) {
  drawMiniBullet(0, y);
  matrix->setTextSize(1);
  matrix->setTextColor(COLOR_WHITE);
  matrix->setCursor(10, y + 1);
  matrix->print(label);
}

}  // namespace

void displayInit() {
  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
                                   A_PIN,  B_PIN,  C_PIN,  D_PIN,  E_PIN,
                                   LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN, pins);

  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  // Keep brightness modest -- a fully lit 64x32 panel can pull several
  // amps, and the display doesn't need to be blinding indoors.
  matrix->setBrightness8(60);
  matrix->clearScreen();

  COLOR_MTA_BLUE = matrix->color565(0, 57, 166);  // official C train blue
  COLOR_WHITE = matrix->color565(255, 255, 255);
  COLOR_YELLOW = matrix->color565(255, 210, 0);
}

void displaySplash() {
  matrix->clearScreen();
  matrix->setTextSize(1);
  matrix->setTextColor(COLOR_WHITE);
  matrix->setCursor(2, 12);
  matrix->print("Connecting...");
}

void displayLogoScreen() {
  matrix->clearScreen();
  int cx = PANEL_WIDTH / 2;
  int cy = PANEL_HEIGHT / 2;
  int r = (PANEL_HEIGHT / 2) - 1;

  matrix->fillCircle(cx, cy, r, COLOR_MTA_BLUE);
  matrix->setTextSize(3);
  matrix->setTextColor(COLOR_WHITE);
  matrix->setCursor(cx - 9, cy - 12);
  matrix->print("C");
}

void displayTimesScreen(const TrainArrivals &arrivals) {
  matrix->clearScreen();

  if (!arrivals.valid) {
    matrix->setTextSize(1);
    matrix->setTextColor(COLOR_WHITE);
    matrix->setCursor(2, 12);
    matrix->print("No data yet");
    return;
  }

  printLabelRow(0, DIRECTION_LABEL);

  // Big, easy-to-read countdown to the very next train.
  matrix->setTextSize(2);
  matrix->setTextColor(COLOR_YELLOW);
  matrix->setCursor(1, 9);
  if (arrivals.count == 0) {
    matrix->print("--");
  } else {
    matrix->print(arrivals.minutes[0]);
    matrix->print("m");
  }

  // Smaller line underneath for the trains after that.
  matrix->setTextSize(1);
  matrix->setTextColor(COLOR_WHITE);
  matrix->setCursor(1, 25);
  if (arrivals.count > 1) {
    matrix->print("then ");
    for (int i = 1; i < arrivals.count; i++) {
      if (i > 1) matrix->print(" ");
      matrix->print(arrivals.minutes[i]);
      matrix->print("m");
    }
  }
}
