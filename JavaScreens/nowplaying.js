// ====== Fake Adafruit_ILI9341 Simulator ======
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
    rect(x, y, w, h, r); // p5 supports rounded corners via the last arg
  }
  
    fillCircle(x, y, r, color) {
      noStroke();
      fill(color);
      circle(x, y, r * 2); // p5 uses diameter
    }
  
    setTextSize(size) {
      this.textSizeMultiplier = size;
      textSize(8 * size); // base font size scaling
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
  
  // ====== Create TFT Object ======
  let tft = new TFT();
  
  // ====== Colors (same as your sketch) ======
  const COLOR_BACKGROUND = "#2C2C2C";
  const COLOR_ORANGE     = "#FF7A00";
  const COLOR_WHITE      = "#FFFFFF";
  const COLOR_HEADER_BG  = "#DADADA";
  const COLOR_GREY  = "#959595";
  
  const HEADER_HEIGHT = 70;
  const SMALL_HEADER_HEIGHT = 30;
  
  
  let songProgressedSeconds = 110 //
  let songTotalSeconds = 180 //
  let songName = "Baby"     //
  let songArtist ="Justin Chalometh"     //
  
  
  const padding = 5;   // <---- MASTER X OFFSET CONTROL
  
  
  function setup() {
    createCanvas(320, 240);
    textFont("Arial");
    noLoop();
  }
  
  function draw() {
    drawUI();
    
  }
  
  function drawUI() {
    tft.fillScreen(COLOR_BACKGROUND);
    //drawHeader();
    
    //drawSmallHeader();
    playingUI();
    
  }
  
  
  function playingUI() {
    drawSmallHeader();
    
    tft.fillRoundRect(padding * 3, 45, 290, 180, 10, COLOR_HEADER_BG);//FRAME for all objects in this page
    
    tft.fillRoundRect(padding * 6, 65, 100, 100, 10, COLOR_ORANGE); // ALBUM COVER
    
  
    tft.fillRoundRect(padding * 14, 185, 170, 5, 10, COLOR_GREY);   // progress bar background
    
    //name and artist
    tft.setTextColor(COLOR_BACKGROUND);
    tft.setTextSize(2.2);
    tft.setCursor(padding * 28, 70);
    tft.print(songName);
  
    // ===== Artist Name =====
    tft.setTextColor(COLOR_GREY);
    tft.setTextSize(1.6);
    tft.setCursor(padding * 28, 95);
    tft.print(songArtist);
    
    EQIcon(150, 140);
    
    
    // ===== Progressed Time =====
    let minutes = Math.floor(songProgressedSeconds / 60);
    let seconds = songProgressedSeconds % 60;
    let formattedSeconds = seconds < 10 ? "0" + seconds : seconds;
  
    tft.setTextColor(COLOR_BACKGROUND);
    tft.setTextSize(1.8);
    tft.setCursor(padding * 6, 180);
    tft.print(minutes + ":" + formattedSeconds);
  
    // ===== Remaining Time =====
    let remainingSeconds = songTotalSeconds - songProgressedSeconds;
  
    // Clamp so it never goes negative
    if (remainingSeconds < 0) {
      remainingSeconds = 0;
    }
  
    let remainingMinutes = Math.floor(remainingSeconds / 60);
    let remainingSecs = remainingSeconds % 60;
    let formattedRemainingSecs = remainingSecs < 10 ? "0" + remainingSecs : remainingSecs;
  
    tft.setCursor(padding * 50, 180);
    tft.print("-" + remainingMinutes + ":" + formattedRemainingSecs);
    songUIprogress()
  }
  
  function songUIprogress(){
    let percentageSongComp = (songProgressedSeconds / songTotalSeconds);
    tft.fillRoundRect(
      padding * 14,
      185,
      170*percentageSongComp,
      5,
      10,
      COLOR_ORANGE
    );
  }
  
  
  function drawSmallHeader() {
     tft.fillRect(0, 0, 320, SMALL_HEADER_HEIGHT, COLOR_HEADER_BG);
    
    // Orange circle logo
    tft.fillCircle(15, 15, 7, COLOR_ORANGE);
    
    // "auffline" text
    tft.setTextColor(COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(30, 5);
    tft.print("auffline");
    
    // Battery percentage
    tft.setTextSize(2);
    tft.setCursor(280, 5);
    tft.print("87%");
  }
  function EQIcon(x, y) {
    //horizontal bars
    tft.fillRoundRect(x, y, 3.5, 30, 10, COLOR_GREY);
    tft.fillRoundRect(x + 10, y, 3.5, 30, 10, COLOR_GREY);
    tft.fillRoundRect(x + 20, y, 3.5, 30, 10,COLOR_GREY);
    tft.fillRoundRect(x + 30, y, 3.5, 30,10, COLOR_GREY);
    
    
    //vertical bars
    tft.fillRoundRect(x - 3, y+5, 10, 5, 2, COLOR_ORANGE);
    tft.fillRoundRect(x - 3 + 10, y+10, 10, 5, 2, COLOR_ORANGE);
    tft.fillRoundRect(x - 3 + 20, y+15, 10, 5, 2, COLOR_ORANGE);
    tft.fillRoundRect(x - 3 + 30, y+7, 10, 5, 2, COLOR_ORANGE);
  
  
    
  }
  
  function drawHeader() {
     tft.fillRect(0, 0, 320, HEADER_HEIGHT, COLOR_HEADER_BG);
    
    // Orange circle logo
    tft.fillCircle(35, 35, 20, COLOR_ORANGE);
    
    // "auffline" text
    tft.setTextColor(COLOR_BACKGROUND);
    tft.setTextSize(3);
    tft.setCursor(65, 25);
    tft.print("auffline");
    
    // Battery percentage
    tft.setTextSize(3);
    tft.setCursor(240, 25);
    tft.print("87%");
  }