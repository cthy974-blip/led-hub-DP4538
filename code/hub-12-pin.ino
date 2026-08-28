/*
 * P10 RG HUB12 (32x16) - XOAY LẠI THEO CHIỀU X
 * 
 * Sơ đồ chân ESP32 -> HUB12:
 *   A   -> GPIO 33  |  B   -> GPIO 25
 *   C   -> GPIO 26  |  D   -> GPIO 22
 *   S   -> GPIO 27 (Clock)
 *   L   -> GPIO 18 (Latch)
 *   R   -> GPIO 23 (Red Data)
 *   G   -> GPIO 19 (Green Data)
 *   OE  -> GPIO 15
 *   GND -> Nối chung GND
 */

#define PANEL_WIDTH  32
#define PANEL_HEIGHT 16

#define PIN_A   33
#define PIN_B   25
#define PIN_C   26
#define PIN_D   22
#define PIN_S   27
#define PIN_L   18
#define PIN_R   23
#define PIN_G   19
#define PIN_OE  15

#define DATA_ACTIVE_LEVEL HIGH
#define OE_ACTIVE_LEVEL   HIGH

// CẤU HÌNH HƯỚNG:
#define FLIP_X false  // Đã xoay lại theo chiều X (không lật X)
#define FLIP_Y true   // Giữ lật chiều Y

uint8_t frameBuffer[PANEL_HEIGHT][PANEL_WIDTH];
TaskHandle_t refreshTaskHandle = NULL;

// -----------------------------------------------------------------------
// Task quét màn hình chuẩn Mode 0
// -----------------------------------------------------------------------
void displayRefreshTask(void *parameter) {
  while (true) {
    for (int scan = 0; scan < 4; scan++) {
      digitalWrite(PIN_OE, !OE_ACTIVE_LEVEL);

      digitalWrite(PIN_A, (scan & 0x01) ? HIGH : LOW);
      digitalWrite(PIN_B, (scan & 0x02) ? HIGH : LOW);
      digitalWrite(PIN_C, LOW);
      digitalWrite(PIN_D, LOW);

      int r0 = scan;
      int r1 = scan + 4;
      int r2 = scan + 8;
      int r3 = scan + 12;
      int rows[4] = {r3, r2, r1, r0};

      for (int b = 3; b >= 0; b--) {
        int colStart = b * 8;
        int colEnd = colStart + 7;

        for (int rIdx = 0; rIdx < 4; rIdx++) {
          int r = rows[rIdx];
          for (int c = colEnd; c >= colStart; c--) {
            uint8_t color = frameBuffer[r][c];
            digitalWrite(PIN_R, (color & 0x01) ? DATA_ACTIVE_LEVEL : !DATA_ACTIVE_LEVEL);
            digitalWrite(PIN_G, (color & 0x02) ? DATA_ACTIVE_LEVEL : !DATA_ACTIVE_LEVEL);
            digitalWrite(PIN_S, HIGH); 
            digitalWrite(PIN_S, LOW);
          }
        }
      }

      digitalWrite(PIN_L, HIGH);
      digitalWrite(PIN_L, LOW);

      digitalWrite(PIN_OE, OE_ACTIVE_LEVEL);
      ets_delay_us(800);
      digitalWrite(PIN_OE, !OE_ACTIVE_LEVEL);
      ets_delay_us(800);
    }
    vTaskDelay(1);
  }
}

// -----------------------------------------------------------------------
// Hàm vẽ Pixel
// -----------------------------------------------------------------------
void drawPixel(int x, int y, uint8_t color) {
#if FLIP_X
  x = (PANEL_WIDTH - 1) - x;
#endif
#if FLIP_Y
  y = (PANEL_HEIGHT - 1) - y;
#endif

  if (x >= 0 && x < PANEL_WIDTH && y >= 0 && y < PANEL_HEIGHT) {
    frameBuffer[y][x] = color;
  }
}

void clearScreen() {
  memset(frameBuffer, 0, sizeof(frameBuffer));
}

// -----------------------------------------------------------------------
// FONT CHỮ 5x6
// -----------------------------------------------------------------------
const uint8_t font5x6[][6] = {
  // 0: 'C'
  { 0b01110,
    0b10001,
    0b10000,
    0b10000,
    0b10001,
    0b01110 },

  // 1: 'T'
  { 0b11111,
    0b00100,
    0b00100,
    0b00100,
    0b00100,
    0b00100 },

  // 2: 'H'
  { 0b10001,
    0b10001,
    0b11111,
    0b10001,
    0b10001,
    0b10001 },

  // 3: 'Y'
  { 0b10001,
    0b01010,
    0b00100,
    0b00100,
    0b00100,
    0b00100 }
};

void drawChar5x6(int x, int y, int charIdx, uint8_t color, int limitX) {
  if (charIdx < 0 || charIdx > 3) return;

  for (int row = 0; row < 6; row++) {
    uint8_t rowBits = font5x6[charIdx][row];
    for (int col = 0; col < 5; col++) {
      if ((rowBits >> (4 - col)) & 0x01) {
        int px = x + col;
        int py = y + row;
        if (px >= limitX) {
          drawPixel(px, py, color);
        }
      }
    }
  }
}

void drawTextCThy(uint8_t color, int limitX) {
  int sy = 5;
  drawChar5x6(1,  sy, 0, color, limitX); // C
  drawChar5x6(9,  sy, 1, color, limitX); // T
  drawChar5x6(17, sy, 2, color, limitX); // H
  drawChar5x6(25, sy, 3, color, limitX); // Y
}

// -----------------------------------------------------------------------
// Bitmap Pacman 9x9 CÓ MẮT
// -----------------------------------------------------------------------
const uint16_t pacmanOpen9x9[] = {
  0b001111100,
  0b011111110,
  0b111111000,
  0b111100000,
  0b110000000,
  0b111100000,
  0b111111000,
  0b011111110,
  0b001111100
};

const uint16_t pacmanClosed9x9[] = {
  0b001111100,
  0b011111110,
  0b111111110,
  0b111111111,
  0b111111111,
  0b111111111,
  0b111111110,
  0b011111110,
  0b001111100
};

void drawPacman(int cx, int cy, bool isOpen, uint8_t color) {
  const uint16_t* s = isOpen ? pacmanOpen9x9 : pacmanClosed9x9;
  for (int dy = -4; dy <= 4; dy++) {
    uint16_t rv = s[dy + 4];
    for (int dx = -4; dx <= 4; dx++) {
      if (dx == 0 && dy == -2) {
        continue;
      }
      if ((rv >> (8 - (dx + 4))) & 0x01) {
        drawPixel(cx + dx, cy + dy, color);
      }
    }
  }
}

// -----------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_A,  OUTPUT);
  pinMode(PIN_B,  OUTPUT);
  pinMode(PIN_C,  OUTPUT);
  pinMode(PIN_D,  OUTPUT);
  pinMode(PIN_S,  OUTPUT);
  pinMode(PIN_L,  OUTPUT);
  pinMode(PIN_R,  OUTPUT);
  pinMode(PIN_G,  OUTPUT);
  pinMode(PIN_OE, OUTPUT);

  digitalWrite(PIN_A,  LOW);
  digitalWrite(PIN_B,  LOW);
  digitalWrite(PIN_C,  LOW);
  digitalWrite(PIN_D,  LOW);
  digitalWrite(PIN_S,  LOW);
  digitalWrite(PIN_L,  LOW);
  digitalWrite(PIN_R,  !DATA_ACTIVE_LEVEL);
  digitalWrite(PIN_G,  !DATA_ACTIVE_LEVEL);
  digitalWrite(PIN_OE, !OE_ACTIVE_LEVEL);

  clearScreen();

  xTaskCreatePinnedToCore(
    displayRefreshTask, "LED_Refresh", 2048, NULL, 3, &refreshTaskHandle, 1
  );

  Serial.println("P10 Flip X Reversed Ready!");
}

void loop() {
  static uint8_t colorCycle = 0;
  uint8_t textColor = (colorCycle % 3) + 1;
  uint8_t pacmanColor = ((colorCycle + 1) % 3) + 1;
  colorCycle++;

  // 1. Chữ "C THY" to rõ đứng yên 2 giây
  uint32_t t = millis();
  while (millis() - t < 2000) {
    clearScreen();
    drawTextCThy(textColor, 0);
    delay(50);
  }

  // 2. Pacman có mắt chạy chậm từ trái qua phải ăn chữ
  for (int pacX = -6; pacX <= 38; pacX++) {
    clearScreen();
    drawTextCThy(textColor, pacX);
    bool isOpen = (pacX % 4 < 2);
    drawPacman(pacX, 7, isOpen, pacmanColor);
    delay(100);
  }

  // 3. Nghỉ 0.8 giây
  clearScreen();
  delay(800);
}
