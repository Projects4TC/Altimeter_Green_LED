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
#define RED       0xF800

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

// —— Trend‐icon params ——
#define ICON_W            55
#define ICON_H            55
#define ICON_X         (TAPE_X - ICON_W - 60)
#define ICON_Y         (TAPE_Y + (TAPE_H - ICON_H)/2)
#define TREND_INTERVAL_MS 1000

unsigned long lastTrendTime = 0;
int            lastTrendFt  = 0;

// —— Timer params ——
const unsigned long TIMER_DURATION = (5UL * 60 + 30) * 1000;  // 5 min 30 s
unsigned long       timerStartMs;
char                lastTimerStr[8] = "";

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

  // measure digit
  tft.setTextSize(6);
  {
    int16_t bx,by; uint16_t bw,bh;
    tft.getTextBounds("0", charX0, charY0, &bx,&by,&bw,&bh);
    charW = bw; charH = bh;
  }

  // compute clip bounds
  const int pad[3] = {6, 10, 20};
  for (int i=0; i<3; i++) {
    clipYMin[i] = charY0 - pad[i];
    clipYMax[i] = charY0 + charH + pad[i];
  }

  // static label
  tft.setTextColor(MIL_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10,10);
  tft.print("Altitude:");

  // init BMP280
  if(!bmp.begin(0x76)&&!bmp.begin(0x77)){
    tft.setTextSize(2);
    tft.setCursor(0,50);
    tft.println("BMP280 FAIL");
    while(1);
  }

  // calibration sweep
  testData();
  prev_alt_ft = 0;
  memset(prevAltStr,' ',3);

  // start timer
  timerStartMs = millis();
}

void loop() {
  float alt_m  = bmp.readAltitude(1013.25) - groundAltitude_m;
  int   alt_ft = (int)(alt_m * 3.28084);
  int   delta  = alt_ft - prev_alt_ft;
  unsigned long now = millis();

 // —— LOW ALT logic ——  
  if (alt_ft < 30 && alt_ft > 1) {  
    if (!lowAltActive) {  
      lowAltActive   = true;  
      lowAltStartMs  = now;  
    }  
  } else {  
    // instantly clear if out of range  
    lowAltActive = false;  
  }  
  // auto‐expire after duration  
  if (lowAltActive && now - lowAltStartMs >= LOWALT_DURATION_MS) {  
    lowAltActive = false;  
  }  

  // digits & outline
  updateAltitudeDigits(alt_ft, delta);
  drawDigitOutline();

  // trend once/sec
  if(now - lastTrendTime >= TREND_INTERVAL_MS){
    drawTrendIcon(delta);
    lastTrendFt   = alt_ft;
    lastTrendTime = now;
  }

  // draw tape before warnings
  drawTape();

  // countdown / BAT LOW
  drawTimer();

// always clear that exact box  
  const char* msg = "LOW ALT";  
  tft.setTextSize(2);  
  tft.setTextColor(ORANGE, BLACK);  
  int16_t bx, by; uint16_t bw, bh;  
  tft.getTextBounds(msg, 140, 20, &bx, &by, &bw, &bh);  
  tft.fillRect(140, 20, bw, bh, BLACK);  

  // only print if still active  
  if (lowAltActive) {  
    tft.setCursor(140, 20);  
    tft.print(msg);  
  }  


  prev_alt_ft = alt_ft;
  delay(200);
}

// Animate digits
void updateAltitudeDigits(int alt_ft, int delta_ft) {
  char newStr[4]; snprintf(newStr,4,"%3d",alt_ft);
  tft.setTextSize(6); tft.setTextColor(MIL_GREEN);

  int changed[3], n=0;
  for(int i=0;i<3;i++){
    if(newStr[i]!=prevAltStr[i]) changed[n++]=i;
  }
  if(!n) return;

  int anim = changed[0];
  for(int k=0;k<n;k++){
    int i=changed[k];
    if(i==anim) continue;
    int x=charX0 + i*charW;
    tft.fillRect(x,charY0-charH,charW,charH*3,BLACK);
    tft.setCursor(x,charY0);
    tft.print(newStr[i]);
    prevAltStr[i]=newStr[i];
  }

  int i=anim, x0=charX0+ i*charW, y0=charY0;
  char oldC=prevAltStr[i], newC=newStr[i];
  const int frames=8, clearH=charH*3;
  for(int f=0; f<=frames; f++){
    tft.fillRect(x0,y0-charH,charW,clearH,BLACK);
    float t=(float)f/frames; int off=(int)(t*charH);
    int y_old=(delta_ft>0)?y0+off:y0-off;
    int y_new=(delta_ft>0)?y0+off-charH:y0-off+charH;
    if(oldC!=' ' && y_old>=clipYMin[i] && y_old<=clipYMax[i]-charH){
      tft.setCursor(x0,y_old); tft.print(oldC);
    }
    if(newC!=' ' && y_new>=clipYMin[i] && y_new<=clipYMax[i]-charH){
      tft.setCursor(x0,y_new); tft.print(newC);
    }
    delay(20);
  }
  tft.fillRect(x0,y0-charH,charW,clearH,BLACK);
  tft.setCursor(x0,y0); tft.print(newC);
  prevAltStr[i]=newC;
}

// Draw outline
void drawDigitOutline(){
  const int pad1=4, pad2=20;
  int x0=charX0-1, x1=charX0+2*charW-1, x2=charX0+3*charW+1;
  int y1=charY0-pad1-1, y2=charY0-pad2-1;
  int y3=charY0+charH+pad1+1, y4=charY0+charH+pad2+1;
  tft.drawLine(x0,y1,x1,y1,MIL_GREEN);
  tft.drawLine(x1,y1,x1,y2,MIL_GREEN);
  tft.drawLine(x1,y2,x2,y2,MIL_GREEN);
  tft.drawLine(x2,y2,x2,y4,MIL_GREEN);
  tft.drawLine(x2,y4,x1,y4,MIL_GREEN);
  tft.drawLine(x1,y4,x1,y3,MIL_GREEN);
  tft.drawLine(x1,y3,x0,y3,MIL_GREEN);
  tft.drawLine(x0,y3,x0,y1,MIL_GREEN);
}

void drawTrendIcon(int delta) {
  // 1) clear inside of icon
  tft.fillRect(ICON_X, ICON_Y, ICON_W, ICON_H, BLACK);

  // 2) draw the arrow or bar
  int cx = ICON_X + ICON_W/2, cy = ICON_Y + ICON_H/2, r = ICON_W/2 - 6;
  if      (delta >  1) tft.fillTriangle(cx,cy-r, cx-r,cy+r, cx+r,cy+r, MIL_GREEN);
  else if (delta < -1) tft.fillTriangle(cx,cy+r, cx-r,cy-r, cx+r,cy-r, MIL_GREEN);
  else                  tft.fillRect   (cx-r,cy-r/4, r*2,  r/2,   MIL_GREEN);

  // 3) draw the static border *after* painting the icon
  tft.drawRect(ICON_X, ICON_Y-7, ICON_W, ICON_H+14, MIL_GREEN);
}


// Draw tape
void drawTape(){
  tft.fillRect(TAPE_X-2,TAPE_Y,TAPE_WIDTH+4,TAPE_H,BLACK);
  int alt=prev_alt_ft;
  int offset=PX_PER_TICK-((alt%FT_PER_TICK)*PX_PER_TICK)/FT_PER_TICK;
  int cY=TAPE_Y+TAPE_H/2, half=(TAPE_H/2)/PX_PER_TICK+1;
  int base=(alt/FT_PER_TICK)*FT_PER_TICK;
  tft.setTextSize(1); tft.setTextColor(MIL_GREEN);
  for(int i=-half;i<=half;i++){
    int y=cY+i*PX_PER_TICK+offset;
    if(y<TAPE_Y||y>TAPE_Y+TAPE_H) continue;
    tft.drawFastHLine(TAPE_X+8,y,TAPE_WIDTH-8,MIL_GREEN);
    char b[6]; snprintf(b,6,"%d",base - i*FT_PER_TICK);
    int16_t bx,by;uint16_t bw,bh;
    tft.getTextBounds(b,0,0,&bx,&by,&bw,&bh);
    tft.setCursor((TAPE_X+8)-4-bw,y-bh/2); tft.print(b);
    for(int s=1;s<FT_PER_TICK;s++){
      int ys=y+s*SUB_PX;
      if(ys<TAPE_Y||ys>TAPE_Y+TAPE_H) continue;
      tft.drawFastHLine(TAPE_X+TAPE_WIDTH-(TAPE_WIDTH-8)/2, ys, (TAPE_WIDTH-8)/2, MIL_GREEN);
    }
  }
  int wY=cY-(PX_PER_TICK+6), wH=PX_PER_TICK*2+12;
  tft.drawRect(TAPE_X-12,wY,TAPE_WIDTH+14,wH,MIL_GREEN);
}

void drawTimer(){
  const int TX = 150, TY = 145;
  unsigned long elapsed = millis() - timerStartMs;
  if (elapsed > TIMER_DURATION) elapsed = TIMER_DURATION;

  // Remaining time
  unsigned long rem = TIMER_DURATION - elapsed;
  unsigned int m = rem / 60000;
  unsigned int s = (rem % 60000) / 1000;

  // Build “MM:SS” string
  char buf[6];
  sprintf(buf, "%02u:%02u", m, s);

  tft.setTextSize(3);
  tft.setTextColor(MIL_GREEN, BLACK);

  // Walk each character, only update if changed
  int16_t bx, by; uint16_t bw, bh;
  int x = TX;
  for (int i = 0; i < 5; i++) {
    char cNew = buf[i];
    char cOld = lastTimerStr[i];

    // measure this glyph
    tft.getTextBounds(String(cNew), 0, 0, &bx, &by, &bw, &bh);

    if (cNew != cOld) {
      // clear just this character’s box
      tft.fillRect(x, TY, bw, bh, BLACK);
      // draw new character
      tft.setCursor(x, TY);
      tft.print(cNew);
      lastTimerStr[i] = cNew;
    }
    // advance x by this glyph’s width
    x += bw;
  }

  // when time’s up, overwrite with BAT LOW in red
  if (millis() - timerStartMs >= TIMER_DURATION) {
    const char *msg = "BAT LOW";
    tft.setTextSize(3);
    tft.setTextColor(RED, BLACK);
    // measure full string
    tft.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
    // clear its area
    tft.fillRect(TX, TY, bw, bh, BLACK);
    // draw it
    tft.setCursor(TX, TY);
    tft.print(msg);
    // prevent further MM:SS updates
    for(int i=0;i<5;i++) lastTimerStr[i] = 0;
  }
}


// Calibration sweep 20→0
void testData(){
  float sum=0; int cnt=0;
  tft.fillRect(charX0,charY0-charH,charW*3,charH*3,BLACK);
  memset(prevAltStr,' ',3);
  for(int alt=20;alt>=0;alt--){
    updateAltitudeDigits(alt,-1);
    drawDigitOutline();
    delay(30);
    sum+=bmp.readAltitude(1013.25);
    cnt++;
  }
  groundAltitude_m = sum/cnt;
  tft.fillRect(charX0,charY0-charH,charW*3,charH*3,BLACK);
  memset(prevAltStr,' ',3);
}
