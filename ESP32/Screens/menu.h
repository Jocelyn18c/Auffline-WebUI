#include "general.h"

// ===== Menu Configuration =====
typedef struct {
  int x, w, r;
} Panel_t;

typedef struct {
  int topPad;
  int h;
  int leftPad;
  int iconSize;
  int iconR;
  int gap;
} Row_t;

static const Panel_t PANEL = { 20, 280, 14 };
static const Row_t ROW = { 10, 50, 15, 30, 5, 10 };

static const char *menuItems[] = { "Music", "Albums", "Playlists", "Recent" };
static const int menuCount = 4;
int selectedIndex = 0;

// ===== Drawing Functions =====
void drawMainMenu(const char *items[], int selected) {
  int topGap = 10;
  int bottomPadInsidePanel = 170;
  int panelY = SMALL_HEADER_HEIGHT + topGap;

  int contentHeight = ROW.topPad + (menuCount * ROW.h) + bottomPadInsidePanel;

  tft.fillRoundRect(PANEL.x, panelY, PANEL.w, contentHeight, PANEL.r, COLOR_PANEL);

  for (int i = 0; i < menuCount; i++) {
    drawMenuRow(i, items[i], panelY);
    drawDividerLine(i, i == selected, panelY);
  }
}

void drawMenuRow(int i, const char *label, int panelY) {
  int rowY = panelY + ROW.topPad + i * ROW.h;
  int iconX = PANEL.x + ROW.leftPad;
  int iconY = rowY - 5;

  tft.fillRoundRect(iconX, iconY, ROW.iconSize, ROW.iconSize, ROW.iconR, COLOR_WHITE);
  drawIconPlaceholder(i, iconX, iconY, ROW.iconSize);

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(iconX + ROW.iconSize + ROW.gap, iconY + 6);
  tft.print(label);
}

void drawDividerLine(int i, int isSelected, int panelY) {
  if (i == menuCount - 1) return;

  int lineY = panelY + ROW.topPad + (i + 1) * ROW.h - 11;
  int x1 = PANEL.x + 16;
  int x2 = PANEL.x + PANEL.w - 16;

  tft.fillRect(x1, lineY, x2 - x1, 2, COLOR_DIVIDER);
}

void drawIconPlaceholder(int i, int x, int y, int s) {
  int cx = x + s / 2;
  int cy = y + s / 2;

  // Music icon
  if (i == 0) {
    tft.drawLine(cx - 6, cy - 6, cx - 6, cy + 8, COLOR_BACKGROUND);
    tft.drawLine(cx + 2, cy - 10, cx + 2, cy + 4, COLOR_BACKGROUND);
    tft.drawCircle(cx - 10, cy + 10, 4, COLOR_BACKGROUND);
    tft.drawCircle(cx - 2, cy + 6, 4, COLOR_BACKGROUND);
  }
  // Folder with disk icon
  else if (i == 1) {
    tft.fillRoundRect(x + 6, y + 5, 22, 22, 4, COLOR_BACKGROUND);
    tft.fillRect(x + 6, y + 18, 12, 2, COLOR_WHITE);
    tft.drawCircle(x + 24, y + 22, 8, COLOR_BACKGROUND);
    tft.fillCircle(x + 24, y + 22, 6, COLOR_BACKGROUND);
    tft.fillCircle(x + 24, y + 22, 2, COLOR_WHITE);
  }
  // List/Playlist icon
  else if (i == 2) {
    tft.drawLine(x + 8, y + 12, x + 24, y + 12, COLOR_BACKGROUND);
    tft.drawLine(x + 8, y + 18, x + 24, y + 18, COLOR_BACKGROUND);
    tft.drawLine(x + 8, y + 24, x + 18, y + 24, COLOR_BACKGROUND);
  }
  // Settings gear icon
  else {
    tft.drawCircle(cx, cy, 10, COLOR_BACKGROUND);
    tft.drawCircle(cx, cy, 5, COLOR_BACKGROUND);
  }
}
