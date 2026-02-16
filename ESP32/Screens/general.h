// ===== Standard 16-bit RGB565 Colors =====
#define COLOR_BACKGROUND  0x2945  // #2C2C2C
#define COLOR_ORANGE      0xFD20  // #FF7A00
#define COLOR_HEADER_BG   0xB595  // #DADADA
#define COLOR_BG          0x0841  // #111111
#define COLOR_PANEL       0x2965  // #2F2F2F
#define COLOR_TEXT        0xF7BE  // #F2F2F2
#define COLOR_DIVIDER     0xFD20  // #FF7A00
#define COLOR_WHITE       0xFFFF  // #FFFFFF
#define COLOR_GREY        0xAD75  // #959595

// ===== Layout Constants =====
#define HEADER_HEIGHT           70
#define SMALL_HEADER_HEIGHT     30

// ===== Drawing Functions =====
void drawSmallHeader(void) {
  tft.fillRect(0, 0, 320, SMALL_HEADER_HEIGHT, COLOR_HEADER_BG);
  tft.fillCircle(15, 15, 7, COLOR_ORANGE);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(30, 5);
  tft.print("auffline");

  tft.setTextSize(2);
  tft.setCursor(280, 5);
  tft.print("87%");
}

void drawHeader(void) {
  tft.fillRect(0, 0, 320, HEADER_HEIGHT, COLOR_HEADER_BG);
  tft.fillCircle(35, 35, 20, COLOR_ORANGE);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(3);
  tft.setCursor(65, 25);
  tft.print("auffline");

  tft.setTextSize(3);
  tft.setCursor(240, 25);
  tft.print("87%");
}

// ===== Icon Functions =====
void EQIcon(int x, int y) {
  // Horizontal bars
  tft.fillRoundRect(x, y, 3, 30, 10, COLOR_GREY);
  tft.fillRoundRect(x + 10, y, 3, 30, 10, COLOR_GREY);
  tft.fillRoundRect(x + 20, y, 3, 30, 10, COLOR_GREY);
  tft.fillRoundRect(x + 30, y, 3, 30, 10, COLOR_GREY);

  // Vertical bars
  tft.fillRoundRect(x - 3, y + 5, 10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 10, y + 10, 10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 20, y + 15, 10, 5, 2, COLOR_ORANGE);
  tft.fillRoundRect(x - 3 + 30, y + 7, 10, 5, 2, COLOR_ORANGE);
}
