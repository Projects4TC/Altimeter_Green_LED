#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP280.h>

// —— PIN CONFIG ——
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7

// —— COLOR CONSTANTS ——
#define MIL_GREEN 0x07E0
#define BLACK     ST77XX_BLACK
#define ORANGE    0xFD20

// —— DISPLAY & SENSOR ——
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BMP280  bmp;

// —— CALIBRATION & STATE ——
float groundAltitude_m = 0;
int   prev_alt_ft      = 0;

// —— LOW ALT WARNING ——
const unsigned long LOWALT_DURATION_MS = 3000;
bool           lowAltActive = false;
unsigned long  lowAltStartMs = 0;

// —— Digit‐refresh globals ——
char    prevAltStr[4] = "   ";
int     charW, charH;
const int charX0 = 10, charY0 = 80;

// —— Clipping bounds ——
int clipYMin[3], clipYMax[3];

// —— Tape (right) params ——
#define FT_PER_TICK      5
#define PX_PER_TICK     20
#define SUB_PX          (PX_PER_TICK/FT_PER_TICK)
#define TAPE_WIDTH      40
#define TAPE_X         (tft.width() - TAPE_WIDTH - 5)
#define TAPE_Y          10
#define TAPE_H         (tft.height() - 20)
#define TAPE_CLEAR_LEFT 60
#define TAPE_CLEAR_W    (TAPE_WIDTH + TAPE_CLEAR_LEFT)

// —— Trend‐icon params ——
#define ICON_W            55
#define ICON_H            55
#define ICON_X         (TAPE_X - ICON_W - 60)
#define ICON_Y         (TAPE_Y + (TAPE_H - ICON_H)/2)
#define TREND_INTERVAL_MS 1000

unsigned long lastTrendTime = 0;
int            lastTrendFt  = 0;

// —— Timer params ——
const unsigned long TIMER_DURATION = 5UL * 60 * 1000;  // 5 minutes
unsigned long       timerStartMs;
char                lastTimerStr[6] = "";

// —— Forward declarations ——
void updateAltitudeDigits(int alt_ft, int delta_ft);
void drawTrendIcon(int delta);
void drawDigitOutline();
void testData();
void drawTape();
void drawTimer();

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Wire.begin();

  tft.init(170, 320);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  // measure digit size
  tft.setTextSize(6);
  { int16_t bx,by; uint16_t bw,bh;
    tft.getTextBounds("0", charX0, charY0, &bx,&by,&bw,&bh);
    charW = bw; charH = bh;
  }

  // compute clip bounds
  const int pad[3] = {6, 10, 20};
  for (int i = 0; i < 3; i++) {
    clipYMin[i] = charY0 - pad[i];
    clipYMax[i] = charY0 + charH + pad[i];
  }

  tft.setTextColor(MIL_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10,10);
  tft.print("Altitude:");

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    tft.setTextSize(2);
    tft.setCursor(0,50);
    tft.println("BMP280 FAIL");
    while(1);
  }

  testData();
  prev_alt_ft = 0;
  lastTrendFt = 0;
  memset(prevAltStr,' ',3);

  timerStartMs = millis();
}

void loop() {
  float alt_m  = bmp.readAltitude(1013.25) - groundAltitude_m;
  int   alt_ft = (int)(alt_m * 3.28084);
  int   delta  = alt_ft - prev_alt_ft;
  unsigned long now = millis();

  if (alt_ft < 20 && alt_ft > 2) {  //if between altitude 5 and 20, LOW ALT warning comes on
    if (!lowAltActive) {
      lowAltActive  = true;
      lowAltStartMs = now;
    }
  }
  if (lowAltActive && now - lowAltStartMs >= LOWALT_DURATION_MS) {
    lowAltActive = false;
  }

  updateAltitudeDigits(alt_ft, delta);
  drawDigitOutline();

  if (now - lastTrendTime >= TREND_INTERVAL_MS) {
    drawTrendIcon(delta);
    lastTrendFt   = alt_ft;
    lastTrendTime = now;
  }

    // —— draw the tape BEFORE the LOW ALT text ——  
  drawTape();

  drawTimer();    //timer drawn aftter tape as it is located close and would get clipped if not

  // —— always clear the LOW ALT area ——  
  const char* msg = "LOW ALT";  
  tft.setTextSize(2);  
  tft.setTextColor(ORANGE, BLACK);  
  int16_t bx, by; uint16_t bw, bh;  
  tft.getTextBounds(msg, 140, 20, &bx, &by, &bw, &bh);  
  tft.fillRect(140, 20, bw, bh, BLACK);

  // —— only re-draw it if still active ——  
  if (lowAltActive) {  
    tft.setCursor(140, 20);  
    tft.print(msg);  
  }


  prev_alt_ft = alt_ft;
  delay(200);
}

// Animate digits
void updateAltitudeDigits(int alt_ft, int delta_ft) {
  char newStr[4];
  snprintf(newStr, sizeof(newStr), "%3d", alt_ft);
  tft.setTextSize(6);
  tft.setTextColor(MIL_GREEN);

  int changed[3], n = 0;
  for (int i = 0; i < 3; i++) {
    if (newStr[i] != prevAltStr[i]) changed[n++] = i;
  }
  if (n == 0) return;

  int anim = changed[0];
  for (int k = 0; k < n; k++) {
    int i = changed[k];
    if (i == anim) continue;
    int x = charX0 + i*charW;
    tft.fillRect(x, charY0-charH, charW, charH*3, BLACK);
    tft.setCursor(x, charY0);
    tft.print(newStr[i]);
    prevAltStr[i] = newStr[i];
  }

  int i = anim;
  int x0 = charX0 + i*charW, y0 = charY0;
  char oldC = prevAltStr[i], newC = newStr[i];
  const int frames = 8, clearH = charH*3;

  for (int f = 0; f <= frames; f++) {
    tft.fillRect(x0, y0-charH, charW, clearH, BLACK);
    float t = (float)f / frames;
    int off = (int)(t*charH);
    int y_old = (delta_ft>0) ? y0+off : y0-off;
    int y_new = (delta_ft>0) ? y0+off-charH : y0-off+charH;

    if (oldC!=' ' && y_old>=clipYMin[i] && y_old<=clipYMax[i]-charH) {
      tft.setCursor(x0, y_old);
      tft.print(oldC);
    }
    if (newC!=' ' && y_new>=clipYMin[i] && y_new<=clipYMax[i]-charH) {
      tft.setCursor(x0, y_new);
      tft.print(newC);
    }
    delay(20);
  }

  tft.fillRect(x0, y0-charH, charW, clearH, BLACK);
  tft.setCursor(x0, y0);
  tft.print(newC);
  prevAltStr[i] = newC;
}

// Draw polygon outline
void drawDigitOutline() {
  const int pad1 = 4, pad2 = 20;
  int x0 = charX0 - 1, x1 = charX0 + 2*charW - 1, x2 = charX0 + 3*charW + 1;
  int y1 = charY0 - pad1 - 1, y2 = charY0 - pad2 - 1;
  int y3 = charY0 + charH + pad1 + 1, y4 = charY0 + charH + pad2 + 1;
  tft.drawLine(x0,y1,x1,y1,MIL_GREEN);
  tft.drawLine(x1,y1,x1,y2,MIL_GREEN);
  tft.drawLine(x1,y2,x2,y2,MIL_GREEN);
  tft.drawLine(x2,y2,x2,y4,MIL_GREEN);
  tft.drawLine(x2,y4,x1,y4,MIL_GREEN);
  tft.drawLine(x1,y4,x1,y3,MIL_GREEN);
  tft.drawLine(x1,y3,x0,y3,MIL_GREEN);
  tft.drawLine(x0,y3,x0,y1,MIL_GREEN);
}

// Draw trend icon
void drawTrendIcon(int delta) {
  tft.fillRect(ICON_X, ICON_Y, ICON_W, ICON_H, BLACK);
  int cx = ICON_X + ICON_W/2, cy = ICON_Y + ICON_H/2, r = ICON_W/2 - 6;
  if      (delta >  1) tft.fillTriangle(cx,cy-r, cx-r,cy+r, cx+r,cy+r, MIL_GREEN);
  else if (delta < -1) tft.fillTriangle(cx,cy+r, cx-r,cy-r, cx+r,cy-r, MIL_GREEN);
  else                  tft.fillRect   (cx-r,cy-r/4, r*2,   r/2,    MIL_GREEN);
}

// Draw altitude tape
void drawTape(){
  int alt = prev_alt_ft;
  tft.fillRect(TAPE_X-TAPE_CLEAR_LEFT, TAPE_Y, TAPE_CLEAR_W, TAPE_H, BLACK);
  int tickOffset = PX_PER_TICK - ((alt%FT_PER_TICK)*PX_PER_TICK)/FT_PER_TICK;
  int centerY = TAPE_Y+TAPE_H/2;
  int halfMaj = (TAPE_H/2)/PX_PER_TICK + 1;
  int baseVal = (alt/FT_PER_TICK)*FT_PER_TICK;
  tft.setTextSize(1);
  tft.setTextColor(MIL_GREEN);
  for(int i=-halfMaj;i<=halfMaj;i++){
    int y = centerY + i*PX_PER_TICK + tickOffset;
    if(y<TAPE_Y||y>TAPE_Y+TAPE_H) continue;
    tft.drawFastHLine(TAPE_X+8,y,TAPE_WIDTH-8,MIL_GREEN);
    char buf[6]; snprintf(buf,6,"%d",baseVal - i*FT_PER_TICK);
    int16_t bx,by; uint16_t bw,bh;
    tft.getTextBounds(buf,0,0,&bx,&by,&bw,&bh);
    tft.setCursor((TAPE_X+8)-4-bw,y - bh/2);
    tft.print(buf);
    for(int s=1;s<FT_PER_TICK;s++){
      int ys=y+s*SUB_PX;
      if(ys<TAPE_Y||ys>TAPE_Y+TAPE_H) continue;
      tft.drawFastHLine(
        TAPE_X+TAPE_WIDTH-(TAPE_WIDTH-8)/2,
        ys,(TAPE_WIDTH-8)/2,MIL_GREEN);
    }
  }
  int winY = centerY - (PX_PER_TICK+6);
  int winH = PX_PER_TICK*2 + 12;
  tft.drawRect(TAPE_X-12,winY,TAPE_WIDTH+14,winH,MIL_GREEN);
}

// Draw countdown timer
void drawTimer(){
  const int TX=150, TY=145;    //manually drawn below Altitude-change indicator
  unsigned long elapsed = millis() - timerStartMs;
  if(elapsed > TIMER_DURATION) elapsed = TIMER_DURATION;
  unsigned int m = elapsed/60000, s = (elapsed%60000)/1000;
  char buf[6]; sprintf(buf,"%02u:%02u",m,s);
  if(strcmp(buf, lastTimerStr)){
    strcpy(lastTimerStr, buf);
    tft.setTextSize(3);
    int16_t bx,by; uint16_t bw,bh;
    tft.getTextBounds(buf,0,0,&bx,&by,&bw,&bh);
    tft.fillRect(TX,TY,bw,bh,BLACK);
    tft.setTextColor(MIL_GREEN,BLACK);
    tft.setCursor(TX,TY);
    tft.print(buf);
  }
}

// Startup sweep 30→0 for calibration
void testData(){
  float sum=0; int cnt=0;
  tft.fillRect(charX0, charY0-charH, charW*3, charH*3, BLACK);
  memset(prevAltStr,' ',3);
  for(int alt=20; alt>=0; alt--){
    updateAltitudeDigits(alt, -1);
    drawDigitOutline();
    delay(30);
    sum += bmp.readAltitude(1013.25);
    cnt++;
  }
  groundAltitude_m = sum / cnt;
  tft.fillRect(charX0, charY0-charH, charW*3, charH*3, BLACK);
  memset(prevAltStr,' ',3);
}
