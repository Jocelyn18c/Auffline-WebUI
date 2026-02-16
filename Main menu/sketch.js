class TFT {
  constructor() {
    this.textSizeMultiplier = 1;
    this.cursorX = 0;
    this.cursorY = 0;
    this.textColor = "#FFFFFF";
  }

  fillScreen(color) {
    background(color);
  }

  fillRect(x, y, w, h, color) {
    noStroke();
    fill(color);
    rect(x, y, w, h);
  }

  fillRoundRect(x, y, w, h, r, color) {
    noStroke();
    fill(color);
    rect(x, y, w, h, r);
  }

  fillCircle(x, y, r, color) {
    noStroke();
    fill(color);
    circle(x, y, r * 2);
  }

  setTextSize(size) {
    this.textSizeMultiplier = size;
    textSize(8 * size);
  }

  setTextColor(color) {
    this.textColor = color;
    fill(color);
  }

  setCursor(x, y) {
    this.cursorX = x;
    this.cursorY = y;
  }

  print(str) {
    fill(this.textColor);
    textAlign(LEFT, TOP);
    text(str, this.cursorX, this.cursorY);
  }
}

// ===== Create TFT Object =====
let tft;

// ===== Colors =====
const COLOR_BACKGROUND = "#2C2C2C";
const COLOR_ORANGE     = "#FF7A00";
const COLOR_HEADER_BG  = "#DADADA";

const COLOR_BG      = "#111111";
const COLOR_PANEL   = "#2F2F2F";
const COLOR_TEXT    = "#F2F2F2";
const COLOR_DIVIDER = "#FF7A00";

// ===== Layout =====
const SMALL_HEADER_HEIGHT = 30;

const PANEL = { x: 20, w: 280, r: 14 };

const ROW = {
  h: 46,
  iconSize: 34,
  iconR: 8,
  leftPad: 22,
  topPad: 18,  // Top padding inside panel
  gap: 14,
};

// ===== Menu =====
const menuItems = ["Music", "Albums", "Playlists", "Recent"];
let selectedIndex = 0;

function setup() {
  createCanvas(320, 240);
  textFont("Arial");
  tft = new TFT();
  noLoop();
}

function draw() {
  tft.fillScreen(COLOR_BG);
  drawSmallHeader();
  drawMainMenu(menuItems, selectedIndex);
}

function drawSmallHeader() {
  tft.fillRect(0, 0, 320, SMALL_HEADER_HEIGHT, COLOR_HEADER_BG);
  tft.fillCircle(15, 15, 7, COLOR_ORANGE);

  tft.setTextColor(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(30, 5);
  tft.print("auffline");
}

function drawMainMenu(items, selected) {
  const topGap = 10;  // Gap between header and panel
  const bottomPadInsidePanel = 170 // Bottom breathing room inside panel

  const panelY = SMALL_HEADER_HEIGHT + topGap;

  // Calculate content height to fit all items perfectly
  const contentHeight =
    ROW.topPad +
    items.length + bottomPadInsidePanel;

  tft.fillRoundRect(
    PANEL.x,
    panelY,
    PANEL.w,
    contentHeight,
    PANEL.r,
    COLOR_PANEL
  );

  for (let i = 0; i < items.length; i++) {
    drawMenuRow(i, items[i], panelY);
    drawDividerLine(i, i === selected, panelY);
  }
}

function drawMenuRow(i, label, panelY) {
  const rowY = panelY + ROW.topPad + i * ROW.h;

  const iconX = PANEL.x + ROW.leftPad;
  const iconY = rowY - 5;


  tft.fillRoundRect(iconX, iconY, ROW.iconSize, ROW.iconSize, ROW.iconR, "#FFFFFF");
  drawIconPlaceholder(i, iconX, iconY, ROW.iconSize);

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2.4);
  tft.setCursor(iconX + ROW.iconSize + ROW.gap, iconY + 6);
  tft.print(label);
}

function drawDividerLine(i, isSelected, panelY) {
  // Do NOT draw divider after last item
  if (i === menuItems.length - 1) return;

  const lineY = panelY + ROW.topPad + (i + 1) * ROW.h - 11;
  const x1 = PANEL.x + 16;
  const x2 = PANEL.x + PANEL.w - 16;

  const thickness = isSelected ? 2 : 2;
  tft.fillRect(x1, lineY, x2 - x1, thickness, COLOR_DIVIDER);
}

function drawIconPlaceholder(i,x,y,s){
  const strokeCol = "#2B1A12"
  
  push();
  noFill();
  stroke(strokeCol);
  strokeWeight(2);
  
  const cx = x + s / 2;
  const cy = y + s / 2;
  
  if (i === 0) {
    line(cx - 6, cy - 6, cx - 6, cy + 8);
    line(cx + 2, cy - 10, cx + 2, cy + 4);
    ellipse(cx - 10, cy + 10, 8, 8);
    ellipse(cx - 2, cy + 6, 8, 8);
  } else if (i === 1) {
    // Folder with Bottom Notch and Disk
    fill(strokeCol);
    noStroke();
    
    // 1. Main Folder Body
    rect(x + 6, y + 5, 22, 22, 4); 
    
    // 2. The "Notch" (The white horizontal gap at the bottom)
    fill(255); // Background color
    rect(x + 6, y + 18, 12, 2); 
    
    // 3. The Disk "Knockout" (The white ring around the disk)
    ellipse(x + 24, y + 22, 16, 16);
    
    // 4. The Disk itself
    fill(strokeCol);
    ellipse(x + 24, y + 22, 12, 12);
    
    // 5. Center of Disk
    fill(255);
    ellipse(x + 24, y + 22, 4, 4);


} else if (i === 2) {
    strokeWeight(2);
    strokeJoin(ROUND); 
    line(x + 8, y + 12, x + 24, y + 12);
    line(x + 8, y + 18, x + 24, y + 18);
    line(x + 8, y + 24, x + 18, y + 24);
    triangle(x + 23, y + 22, x + 23, y + 28, x + 28, y + 25);
 } 
  else {
    // Settings icon
    strokeWeight(2.5);
    strokeJoin(ROUND); 
    
    const numTeeth = 6;
    const outerRadius = 14; // Height of the tooth flat
    const innerRadius = 10; // The base circle
    const toothBaseWidth = 0.40; // How wide the tooth is at the bottom
    const toothTipWidth = 0.18;  // How wide the tooth is at the top (smaller = more tapered)

    beginShape();
    for (let k = 0; k < numTeeth; k++) {
      let angle = (TWO_PI * k) / numTeeth;

      // 1. Bottom Left of tooth
      vertex(cx + cos(angle - toothBaseWidth) * innerRadius, 
             cy + sin(angle - toothBaseWidth) * innerRadius);
      
      // 2. Top Left of tooth
      vertex(cx + cos(angle - toothTipWidth) * outerRadius, 
             cy + sin(angle - toothTipWidth) * outerRadius);
      
      // 3. Top Right of tooth
      vertex(cx + cos(angle + toothTipWidth) * outerRadius, 
             cy + sin(angle + toothTipWidth) * outerRadius);
      
      // 4. Bottom Right of tooth
      vertex(cx + cos(angle + toothBaseWidth) * innerRadius, 
             cy + sin(angle + toothBaseWidth) * innerRadius);
    }
    endShape(CLOSE);

    // Center hole
    ellipse(cx, cy, 10, 10);
  }  
}   
  

