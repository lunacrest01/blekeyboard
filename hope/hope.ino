#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include "image.h"  // For boot logo
#include "pic1.h"   // For memory images

// ================== CONFIG ==================
#define TFT_CS     10
#define TFT_RST     4
#define TFT_DC      3
#define TFT_SCLK    6
#define TFT_MOSI    7

#define SD_CS       9   // SD Card chip select
#define SD_MISO     8   // SD Card MISO

#define BUTTON_0    0  // UP (Password: 0)
#define BUTTON_1    1  // DOWN (Password: 1)
#define BUTTON_SELECT 2  // SELECT/CONFIRM (Password: 2)
#define BUTTON_BACK 5  // BACK button

#define RGB_LED_PIN  20  // RGB LED data pin
#define RGB_LED_COUNT 3  // Number of RGB LEDs

#define VIBRATION_MOTOR_PIN 21  // Vibration motor data pin

#define SCREEN_W 128
#define SCREEN_H 160
#define PASS_LENGTH 4
#define TIMEOUT_MS 5000
#define DEBOUNCE   60

// Vibration Patterns
#define VIB_SHORT 50     // Short vibration (ms)
#define VIB_MEDIUM 100   // Medium vibration (ms)
#define VIB_LONG 150     // Long vibration (ms)
#define VIB_PATTERN_GAP 40 // Gap between pattern vibrations

// Clean Color Palette
#define BG_COLOR 0xEF7D
#define CARD_BG 0xFFFF
#define SHADOW 0xCE79
#define TEXT_DARK 0x2945
#define TEXT_LIGHT 0x8C51
#define ACCENT_BLUE 0x4D7F
#define ACCENT_PURPLE 0x9A7F
#define ACCENT_PINK 0xFBDA
#define ACCENT_ORANGE 0xFC60
#define SUCCESS_GREEN 0x07E0
#define ERROR_RED 0xF800
#define CIRCLE_EMPTY 0xD69A
#define SETTING_BG 0xF7BE
#define SETTING_ITEM_BG 0xFFFF

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_NeoPixel rgbLeds(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// ================== STATE ==================
enum State { BOOTING, LOCKED, UNLOCKING, MENU, CONTENT };
State currentState = BOOTING;

// ================== SD CARD STATUS ==================
bool sdCardDetected = false;
uint64_t sdCardSize = 0;
uint8_t sdCardType = 0;

// ================== PASSCODE ==================
const uint8_t CORRECT_PASSCODE[PASS_LENGTH] = {0, 0, 0, 0};
uint8_t enteredPasscode[PASS_LENGTH] = {0, 0, 0, 0};

uint8_t passIndex = 0;
unsigned long lastPressTime = 0;
bool lastButton0 = HIGH;
bool lastButton1 = HIGH;
bool lastButtonSelect = HIGH;
bool lastButtonBack = HIGH;
uint8_t selectedMenuItem = 0;
uint8_t currentPage = 0;
uint8_t maxPages = 0;
uint8_t lastMemoryPage = 0; // Track last memory page for slide direction

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================
void initSDCard();
void setRGBLeds(uint32_t color);
void setRGBLed(uint8_t ledIndex, uint32_t color);
void rgbBreathEffect(uint32_t color, uint8_t cycles);
void rgbPulseEffect(uint32_t color);
void rgbRainbow();
void drawLockScreen();
void drawLockIcon(int x, int y, bool unlocked);
void drawPasscodeCircles();
void showUnlockAnimation();
void showLockedAnimation();
void drawMenu();
void drawMenuItem(int itemIndex, bool isSelected);
void showContent(int contentIndex);
void drawLetterContent(uint8_t page);
void updatePageContent(int contentIndex);
void drawMemoriesContent(uint8_t page);
void drawQuoteContent(uint8_t page);
void drawAboutContent(uint8_t page);
void drawPageIndicator();
void drawLoveLetter(int x, int y);
void drawLandscape(int x, int y);
void drawNote(int x, int y);
void drawProfile(int x, int y);
void drawSDCard(int x, int y, bool detected);
void drawSettingItem(int index, const char* title, const char* subtitle, bool hasIcon, uint16_t iconColor);
uint16_t RGB(uint8_t r, uint8_t g, uint8_t b);
bool checkPasscode();
void handleButtons();
void vibrate(uint16_t duration);
void vibratePattern(uint8_t count, uint16_t onTime, uint16_t offTime);
void successVibration();
void errorVibration();
void menuNavigationVibration();
void buttonPressVibration();
void unlockVibration();
void pageTurnVibration();
void drawFullScreenImage(const uint16_t* imageData);
void slideAnimation(bool direction); // true = right, false = left

// ======================================================
// VIBRATION FUNCTIONS
// ======================================================
void vibrate(uint16_t duration) {
  digitalWrite(VIBRATION_MOTOR_PIN, HIGH);
  delay(duration);
  digitalWrite(VIBRATION_MOTOR_PIN, LOW);
}

void vibratePattern(uint8_t count, uint16_t onTime, uint16_t offTime) {
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(VIBRATION_MOTOR_PIN, HIGH);
    delay(onTime);
    digitalWrite(VIBRATION_MOTOR_PIN, LOW);
    if (i < count - 1) {
      delay(offTime);
    }
  }
}

void successVibration() {
  vibrate(VIB_SHORT);
  delay(VIB_PATTERN_GAP);
  vibrate(VIB_SHORT);
  delay(VIB_PATTERN_GAP);
  vibrate(VIB_LONG);
}

void errorVibration() {
  vibrate(VIB_LONG);
  delay(VIB_PATTERN_GAP);
  vibrate(VIB_SHORT);
}

void menuNavigationVibration() {
  vibrate(VIB_SHORT);
}

void buttonPressVibration() {
  vibrate(30);
}

void unlockVibration() {
  vibratePattern(3, 60, 40);
}

void pageTurnVibration() {
  vibrate(VIB_SHORT);
}

// ======================================================
// PASSCODE CHECK
// ======================================================
bool checkPasscode() {
  for (int i = 0; i < PASS_LENGTH; i++) {
    if (enteredPasscode[i] != CORRECT_PASSCODE[i]) {
      return false;
    }
  }
  return true;
}

// ======================================================
// SD CARD INITIALIZATION
// ======================================================
void initSDCard() {
  Serial.println("Initializing SD card...");
  
  pinMode(SD_MISO, INPUT);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    sdCardDetected = false;
    return;
  }
  
  sdCardDetected = true;
  Serial.println("SD Card initialized successfully!");
  
  sdCardType = SD.cardType();
  
  if (sdCardType == CARD_NONE) {
    Serial.println("No SD card attached");
    sdCardDetected = false;
    return;
  }
  
  Serial.print("SD Card Type: ");
  if (sdCardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (sdCardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (sdCardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  sdCardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", sdCardSize);
  
  File testFile = SD.open("/test.txt", FILE_WRITE);
  if (testFile) {
    testFile.println("Hope Box - SD Card Test");
    testFile.close();
    Serial.println("Test file created successfully");
  } else {
    Serial.println("Failed to create test file");
  }
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  SPI.begin();
  SPI.setFrequency(80000000);
  
  pinMode(BUTTON_0, INPUT_PULLUP);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  pinMode(VIBRATION_MOTOR_PIN, OUTPUT);
  digitalWrite(VIBRATION_MOTOR_PIN, LOW);

  rgbLeds.begin();
  rgbLeds.setBrightness(255);
  setRGBLeds(rgbLeds.Color(100, 0, 150));
  rgbLeds.show();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);

  vibratePattern(2, 80, 40);
  
  showBootLogo();
  delay(600);
  
  initSDCard();
  
  setRGBLeds(rgbLeds.Color(0, 100, 255));

  drawLockScreen(); 
  currentState = LOCKED;
}

// ======================================================
// MAIN LOOP
// ======================================================
void loop() {
  handleButtons();

  if (currentState == LOCKED && millis() - lastPressTime > TIMEOUT_MS && passIndex > 0) {
    passIndex = 0;
    for (int i = 0; i < PASS_LENGTH; i++) {
      enteredPasscode[i] = 0;
    }
    drawPasscodeCircles();
  }
}

// ======================================================
// BOOT LOGO
// ======================================================
void showBootLogo() {
  tft.startWrite();
  tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
    tft.pushColor(unnamed_1_1_[i]);
  }
  tft.endWrite();
}

// ======================================================
// RGB565 HELPER
// ======================================================
uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ======================================================
// DRAW FULL SCREEN IMAGE
// ======================================================
void drawFullScreenImage(const uint16_t* imageData) {
  tft.startWrite();
  tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
    tft.pushColor(imageData[i]);
  }
  tft.endWrite();
}

// ======================================================
// SLIDE ANIMATION
// ======================================================
void slideAnimation(bool direction) {
  int steps = 16;
  int stepSize = SCREEN_W / steps;
  
  for (int i = 1; i <= steps; i++) {
    int offset = i * stepSize;
    
    if (direction) {
      tft.setAddrWindow(0, 0, offset, SCREEN_H);
      for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < offset; x++) {
          int srcX = SCREEN_W - offset + x;
          int pixelIndex = y * SCREEN_W + srcX;
          tft.pushColor(pic1[pixelIndex]);
        }
      }
    } else {
      tft.setAddrWindow(SCREEN_W - offset, 0, offset, SCREEN_H);
      for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < offset; x++) {
          int srcX = x;
          int pixelIndex = y * SCREEN_W + srcX;
          tft.pushColor(pic1[pixelIndex]);
        }
      }
    }
    delay(8);
  }
  
  drawFullScreenImage(pic1);
}

// ======================================================
// LOCK SCREEN
// ======================================================
void drawLockScreen() {
  for (int y = 0; y < SCREEN_H; y++) {
    uint8_t shade = 235 - (y / 15);
    tft.drawFastHLine(0, y, SCREEN_W, RGB(shade, shade + 5, shade + 10));
  }

  int cardY = 30;
  int cardH = 85;
  int cardW = 104;
  int cardX = (SCREEN_W - cardW) / 2;
  
  tft.fillRoundRect(cardX + 2, cardY + 2, cardW, cardH, 15, SHADOW);
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 15, CARD_BG);
  tft.drawRoundRect(cardX, cardY, cardW, cardH, 15, ACCENT_BLUE);
  
  drawLockIcon(SCREEN_W / 2, 65, false);
  
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DARK);
  int lockedTextW = 6 * 6;
  tft.setCursor((SCREEN_W - lockedTextW) / 2, 95);
  tft.print("LOCKED");
  
  tft.setTextColor(TEXT_LIGHT);
  int passcodeTextW = 14 * 6;
  tft.setCursor((SCREEN_W - passcodeTextW) / 2, 118);
  tft.print("Enter Passcode");
  
  drawPasscodeCircles();
}

// ======================================================
// LOCK ICON
// ======================================================
void drawLockIcon(int x, int y, bool unlocked) {
  if (unlocked) {
    tft.fillRoundRect(x - 10, y - 2, 20, 24, 4, RGB(230, 255, 230));
    tft.drawRoundRect(x - 10, y - 2, 20, 24, 4, SUCCESS_GREEN);
    tft.drawRoundRect(x - 9, y - 1, 18, 22, 3, SUCCESS_GREEN);
    
    tft.fillCircle(x, y + 8, 3, SUCCESS_GREEN);
    tft.fillRect(x - 1, y + 10, 2, 6, SUCCESS_GREEN);
    
    int shackleY = y - 24;
    tft.drawCircle(x, shackleY, 10, SUCCESS_GREEN);
    tft.drawCircle(x, shackleY, 9, SUCCESS_GREEN);
    tft.drawFastVLine(x - 10, shackleY, 8, SUCCESS_GREEN);
    tft.drawFastVLine(x - 9, shackleY, 8, SUCCESS_GREEN);
    
  } else {
    tft.fillRoundRect(x - 10, y - 2, 20, 24, 4, RGB(240, 245, 255));
    tft.drawRoundRect(x - 10, y - 2, 20, 24, 4, ACCENT_BLUE);
    tft.drawRoundRect(x - 9, y - 1, 18, 22, 3, ACCENT_BLUE);
    
    tft.fillCircle(x, y + 8, 3, ACCENT_BLUE);
    tft.fillRect(x - 1, y + 10, 2, 6, ACCENT_BLUE);
    tft.drawPixel(x - 1, y + 7, RGB(200, 220, 255));
    
    tft.drawCircle(x, y - 12, 10, ACCENT_BLUE);
    tft.drawCircle(x, y - 12, 9, ACCENT_BLUE);
    tft.drawCircle(x, y - 12, 8, ACCENT_BLUE);
    
    tft.drawFastVLine(x - 10, y - 12, 10, ACCENT_BLUE);
    tft.drawFastVLine(x - 9, y - 12, 10, ACCENT_BLUE);
    tft.drawFastVLine(x - 8, y - 12, 10, ACCENT_BLUE);
    tft.drawFastVLine(x + 8, y - 12, 10, ACCENT_BLUE);
    tft.drawFastVLine(x + 9, y - 12, 10, ACCENT_BLUE);
    tft.drawFastVLine(x + 10, y - 12, 10, ACCENT_BLUE);
  }
}

// ======================================================
// PASSCODE CIRCLES
// ======================================================
void drawPasscodeCircles() {
  int spacing = 24;
  int startX = (SCREEN_W - (PASS_LENGTH - 1) * spacing) / 2;
  int circleY = 138;
  
  for (int y = 133; y < 151; y++) {
    uint8_t shade = 235 - (y / 15);
    tft.drawFastHLine(15, y, 98, RGB(shade, shade + 5, shade + 10));
  }

  for (int i = 0; i < PASS_LENGTH; i++) {
    int x = startX + (i * spacing);
    
    if (i < passIndex) {
      uint16_t fillColor;
      if (enteredPasscode[i] == 0) {
        fillColor = ACCENT_BLUE;
      } else if (enteredPasscode[i] == 1) {
        fillColor = ACCENT_PURPLE;
      } else if (enteredPasscode[i] == 2) {
        fillColor = ACCENT_PINK;
      } else {
        fillColor = ACCENT_ORANGE;
      }
      
      tft.fillCircle(x, circleY, 7, fillColor);
      tft.drawCircle(x, circleY, 7, fillColor);
      
      tft.setTextSize(1);
      tft.setTextColor(CARD_BG);
      tft.setCursor(x - 3, circleY - 4);
      tft.print(enteredPasscode[i]);
      
    } else {
      tft.drawCircle(x, circleY, 7, CIRCLE_EMPTY);
      tft.drawCircle(x, circleY, 6, CIRCLE_EMPTY);
      tft.fillCircle(x, circleY, 2, CIRCLE_EMPTY);
    }
  }
}

// ======================================================
// UNLOCK ANIMATION
// ======================================================
void showUnlockAnimation() {
  rgbPulseEffect(rgbLeds.Color(0, 255, 0));
  successVibration();
  
  for (int blink = 0; blink < 2; blink++) {
    tft.fillRect(10, 60, 108, 35, BG_COLOR);
    delay(20);
    
    for (int y = 60; y < 95; y++) {
      uint8_t shade = 235 - (y / 15);
      tft.drawFastHLine(10, y, 108, RGB(shade, shade + 5, shade + 10));
    }
    delay(15);
    
    tft.setTextSize(2);
    tft.setTextColor(SUCCESS_GREEN);
    tft.setCursor(22, 70);
    tft.print("UNLOCKED");
    delay(300);
    
    tft.fillRect(10, 60, 108, 35, BG_COLOR);
    for (int y = 60; y < 95; y++) {
      uint8_t shade = 235 - (y / 15);
      tft.drawFastHLine(10, y, 108, RGB(shade, shade + 5, shade + 10));
    }
    delay(200);
  }
  
  tft.fillRect(10, 60, 108, 35, BG_COLOR);
  for (int y = 60; y < 95; y++) {
    uint8_t shade = 235 - (y / 15);
    tft.drawFastHLine(10, y, 108, RGB(shade, shade + 5, shade + 10));
  }
  delay(15);
  
  tft.setTextSize(2);
  tft.setTextColor(SUCCESS_GREEN);
  tft.setCursor(22, 70);
  tft.print("UNLOCKED");
  
  rgbRainbow();
  delay(300);
}

// ======================================================
// LOCKED ANIMATION
// ======================================================
void showLockedAnimation() {
  for (int i = 0; i < 3; i++) {
    setRGBLeds(rgbLeds.Color(255, 0, 0));
    vibrate(VIB_SHORT);
    setRGBLeds(rgbLeds.Color(0, 0, 0));
    delay(100);
  }
  
  errorVibration();
  
  for (int blink = 0; blink < 2; blink++) {
    tft.fillRect(10, 60, 108, 35, BG_COLOR);
    delay(20);
    
    for (int y = 60; y < 95; y++) {
      uint8_t shade = 235 - (y / 15);
      tft.drawFastHLine(10, y, 108, RGB(shade, shade + 5, shade + 10));
    }
    delay(15);
    
    tft.setTextSize(2);
    tft.setTextColor(ERROR_RED);
    tft.setCursor(28, 70);
    tft.print("LOCKED");
    delay(300);
    
    tft.fillRect(10, 60, 108, 35, BG_COLOR);
    for (int y = 60; y < 95; y++) {
      uint8_t shade = 235 - (y / 15);
      tft.drawFastHLine(10, y, 108, RGB(shade, shade + 5, shade + 10));
    }
    delay(200);
  }
  
  passIndex = 0;
  for (int i = 0; i < PASS_LENGTH; i++) {
    enteredPasscode[i] = 0;
  }
  
  setRGBLeds(rgbLeds.Color(0, 100, 255));
  drawLockScreen();
}

// ======================================================
// MENU
// ======================================================
void drawMenu() {
  setRGBLeds(rgbLeds.Color(150, 50, 200));
  unlockVibration();
  
  for (int y = 0; y < SCREEN_H; y++) {
    uint8_t shade = 245 - (y / 12);
    tft.drawFastHLine(0, y, SCREEN_W, RGB(shade - 5, shade, shade + 8));
  }
  
  int titleY = 8;
  tft.fillRoundRect(24, titleY + 3, 80, 28, 14, RGB(180, 185, 195));
  tft.fillRoundRect(23, titleY, 80, 28, 14, CARD_BG);
  
  for (int i = 0; i < 3; i++) {
    tft.drawRoundRect(23 + i, titleY + i, 80 - (i*2), 28 - (i*2), 14 - i, ACCENT_PURPLE);
  }
  
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(41, 15);
  tft.print("MENU");
  
  tft.fillCircle(15, 45, 2, ACCENT_PURPLE);
  tft.fillCircle(113, 45, 2, ACCENT_PURPLE);
  for (int i = 0; i < 2; i++) {
    tft.drawFastHLine(20, 45 + i, 88, RGB(210, 215, 225));
  }
  
  for (int i = 0; i < 4; i++) {
    drawMenuItem(i, (i == selectedMenuItem));
  }
}

// ======================================================
// DRAW MENU ITEM
// ======================================================
void drawMenuItem(int itemIndex, bool isSelected) {
  const char* items[] = {"Hope Letter", "Memories", "Quote", "About"};
  uint16_t colors[] = {ACCENT_PURPLE, ACCENT_PINK, ACCENT_BLUE, ACCENT_ORANGE};
  
  int startY = 55;
  int itemHeight = 20;
  int spacing = 3;
  int y = startY + (itemIndex * (itemHeight + spacing));
  
  tft.fillRoundRect(13, y + 2, 102, itemHeight, 10, RGB(190, 195, 205));
  tft.fillRoundRect(12, y + 1, 102, itemHeight, 10, SHADOW);
  
  if (isSelected) {
    tft.fillRoundRect(11, y, 106, itemHeight, 10, colors[itemIndex]);
    for (int b = 0; b < 3; b++) {
      tft.drawRoundRect(11 + b, y + b, 106 - (b*2), itemHeight - (b*2), 10 - b, colors[itemIndex]);
    }
  } else {
    tft.fillRoundRect(11, y, 106, itemHeight, 10, CARD_BG);
    for (int b = 0; b < 2; b++) {
      tft.drawRoundRect(11 + b, y + b, 106 - (b*2), itemHeight - (b*2), 10 - b, colors[itemIndex]);
    }
  }
  
  int badgeX = 19;
  int badgeY = y + 10;
  uint16_t badgeColor = isSelected ? CARD_BG : colors[itemIndex];
  uint16_t badgeTextColor = isSelected ? colors[itemIndex] : CARD_BG;
  
  tft.fillCircle(badgeX + 1, badgeY + 1, 7, RGB(180, 185, 195));
  tft.fillCircle(badgeX, badgeY, 8, badgeColor);
  tft.fillCircle(badgeX, badgeY, 7, badgeColor);
  
  tft.setTextSize(1);
  tft.setTextColor(badgeTextColor);
  tft.setCursor(badgeX - 3, badgeY - 4);
  tft.print(itemIndex + 1);
  
  int emojiBaseX = 32;
  int emojiBaseY = y + 4;
  
  if (isSelected) {
    tft.fillCircle(emojiBaseX + 6, emojiBaseY + 6, 8, RGB(255, 255, 255));
    tft.drawCircle(emojiBaseX + 6, emojiBaseY + 6, 8, RGB(240, 245, 250));
  } else {
    tft.fillCircle(emojiBaseX + 6, emojiBaseY + 6, 8, RGB(248, 250, 252));
    tft.drawCircle(emojiBaseX + 6, emojiBaseY + 6, 8, RGB(230, 235, 240));
  }
  
  switch(itemIndex) {
    case 0: drawLoveLetter(emojiBaseX + 1, emojiBaseY + 1); break;
    case 1: drawLandscape(emojiBaseX + 1, emojiBaseY + 2); break;
    case 2: drawNote(emojiBaseX + 1, emojiBaseY + 1); break;
    case 3: drawProfile(emojiBaseX + 1, emojiBaseY + 1); break;
  }
  
  tft.setTextSize(1);
  if (isSelected) {
    tft.setTextColor(CARD_BG);
  } else {
    tft.setTextColor(TEXT_DARK);
  }
  tft.setCursor(48, y + 6);
  tft.print(items[itemIndex]);
}

// ======================================================
// SHOW CONTENT SCREEN
// ======================================================
void showContent(int contentIndex) {
  const char* titles[] = {"Hope Letter", "Memories", "Quote", "About"};
  uint16_t colors[] = {ACCENT_PURPLE, ACCENT_PINK, ACCENT_BLUE, ACCENT_ORANGE};
  
  switch(contentIndex) {
    case 0: maxPages = 6; break;
    case 1: maxPages = 3; break;
    case 2: maxPages = 2; break;
    case 3: maxPages = 1; break;
  }
  
  currentPage = 0;
  lastMemoryPage = 0;
  
  vibrate(VIB_MEDIUM);
  
  // SPECIAL HANDLING FOR MEMORIES - SHOW IMAGE IMMEDIATELY
  if (contentIndex == 1) {
    drawFullScreenImage(pic1);
    
    if (maxPages > 1) {
      drawPageIndicator();
    }
    return;
  }
  
  // FOR ALL OTHER CONTENT - Show with title bar
  for (int y = 0; y < SCREEN_H; y++) {
    uint8_t shade = 245 - (y / 12);
    tft.drawFastHLine(0, y, SCREEN_W, RGB(shade - 5, shade, shade + 8));
  }
  
  tft.fillRoundRect(6, 7, 116, 22, 10, RGB(180, 185, 195));
  tft.fillRoundRect(5, 5, 116, 22, 10, colors[contentIndex]);
  
  tft.setTextSize(1);
  tft.setTextColor(CARD_BG);
  int titleW = strlen(titles[contentIndex]) * 6;
  tft.setCursor((SCREEN_W - titleW) / 2, 12);
  tft.print(titles[contentIndex]);
  
  if (contentIndex == 0 || contentIndex == 2) {
    switch(contentIndex) {
      case 0: drawLetterContent(currentPage); break;
      case 2: drawQuoteContent(currentPage); break;
    }
  } else if (contentIndex == 3) {
    drawAboutContent(currentPage);
  }
  
  if (maxPages > 1) {
    drawPageIndicator();
  }
}

// ======================================================
// UPDATE PAGE CONTENT
// ======================================================
void updatePageContent(int contentIndex) {
  pageTurnVibration();
  
  if (contentIndex == 1) {
    bool slideDirection = (currentPage > lastMemoryPage);
    lastMemoryPage = currentPage;
    
    slideAnimation(slideDirection);
    
    if (maxPages > 1) {
      drawPageIndicator();
    }
    return;
  }
  
  if (contentIndex == 0 || contentIndex == 2) {
    tft.fillRect(0, 31, SCREEN_W, 113, BG_COLOR);
    
    for (int y = 31; y < 144; y++) {
      uint8_t shade = 245 - (y / 12);
      tft.drawFastHLine(0, y, SCREEN_W, RGB(shade - 5, shade, shade + 8));
    }
    
    switch(contentIndex) {
      case 0: drawLetterContent(currentPage); break;
      case 2: drawQuoteContent(currentPage); break;
    }
  } else if (contentIndex == 3) {
    drawAboutContent(currentPage);
  }
  
  if (maxPages > 1) {
    drawPageIndicator();
  }
}

// ======================================================
// DRAW PAGE INDICATOR
// ======================================================
void drawPageIndicator() {
  if (maxPages <= 1) return;
  
  int dotSize = 4;
  int spacing = 9;
  int totalWidth = (maxPages * dotSize) + ((maxPages - 1) * spacing);
  int startX = (SCREEN_W - totalWidth) / 2;
  int dotY = 148;
  
  for (int i = 0; i < maxPages; i++) {
    int x = startX + (i * (dotSize + spacing)) + (dotSize / 2);
    
    if (i == currentPage) {
      tft.fillCircle(x, dotY, dotSize, ACCENT_PURPLE);
      tft.drawCircle(x, dotY, dotSize + 1, ACCENT_PURPLE);
    } else {
      tft.drawCircle(x, dotY, 2, CIRCLE_EMPTY);
      tft.fillCircle(x, dotY, 1, CIRCLE_EMPTY);
    }
  }
}

// ======================================================
// HOPE LETTER CONTENT
// ======================================================
void drawLetterContent(uint8_t page) {
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DARK);
  
  int leftMargin = 10;
  int topMargin = 36;
  int lineHeight = 9;
  
  switch(page) {
    case 0:
      tft.setTextColor(ACCENT_PURPLE);
      tft.setCursor(leftMargin, topMargin);
      tft.print("Dear Name,");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("I wanted to take a");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("moment to write you");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("this special letter.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 6);
      tft.print("Words sometimes");
      tft.setCursor(leftMargin, topMargin + lineHeight * 7);
      tft.print("feel inadequate to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("express what's in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("my heart, but I'll");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("try my best to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 11);
      tft.print("convey my feelings.");
      break;
      
    case 1:
      tft.setCursor(leftMargin, topMargin);
      tft.print("Every single");
      tft.setCursor(leftMargin, topMargin + lineHeight * 1);
      tft.print("moment we've shared");
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("together has been");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("a precious gift in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("my life.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 6);
      tft.print("You have this");
      tft.setCursor(leftMargin, topMargin + lineHeight * 7);
      tft.print("amazing ability to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("light up even the");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("darkest days with");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("your presence and");
      tft.setCursor(leftMargin, topMargin + lineHeight * 11);
      tft.print("smile.");
      break;
      
    case 2:
      tft.setTextColor(ACCENT_PURPLE);
      tft.setCursor(leftMargin, topMargin);
      tft.print("What I Admire:");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("Your strength in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("the face of");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("challenges inspires");
      tft.setCursor(leftMargin, topMargin + lineHeight * 5);
      tft.print("me daily.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 7);
      tft.print("Your compassion");
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("for others shows");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("the true beauty of");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("your soul.");
      break;
      
    case 3:
      tft.setCursor(leftMargin, topMargin);
      tft.print("I want you to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 1);
      tft.print("know that no");
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("matter what life");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("throws your way,");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("you never walk");
      tft.setCursor(leftMargin, topMargin + lineHeight * 5);
      tft.print("alone.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 7);
      tft.print("When doubts creep");
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("in, remember: you");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("are stronger than");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("you think!");
      break;
      
    case 4:
      tft.setTextColor(ACCENT_PURPLE);
      tft.setCursor(leftMargin, topMargin);
      tft.print("My Promise:");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("I promise to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("always believe in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("you, even when you");
      tft.setCursor(leftMargin, topMargin + lineHeight * 5);
      tft.print("don't believe in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 6);
      tft.print("yourself.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("I'll celebrate");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("your victories and");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("support you always.");
      break;
      
    case 5:
      tft.setCursor(leftMargin, topMargin);
      tft.print("Keep shining your");
      tft.setCursor(leftMargin, topMargin + lineHeight * 1);
      tft.print("beautiful light.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("Never forget how");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("special, unique,");
      tft.setCursor(leftMargin, topMargin + lineHeight * 5);
      tft.print("and amazing you");
      tft.setCursor(leftMargin, topMargin + lineHeight * 6);
      tft.print("truly are.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("This little device");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("holds my thoughts");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("for you always.");
      
      tft.setTextColor(ACCENT_PURPLE);
      tft.setCursor(leftMargin, topMargin + lineHeight * 12);
      tft.print("With endless love,");
      break;
  }
}

// ======================================================
// QUOTE CONTENT
// ======================================================
void drawQuoteContent(uint8_t page) {
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DARK);
  
  int leftMargin = 10;
  int topMargin = 36;
  int lineHeight = 9;
  
  switch(page) {
    case 0:
      tft.setTextColor(ACCENT_BLUE);
      tft.setCursor(leftMargin, topMargin);
      tft.print("Quote of Hope:");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("\"The best way to");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("predict the future");
      tft.setCursor(leftMargin, topMargin + lineHeight * 4);
      tft.print("is to create it.\"");
      
      tft.setTextColor(TEXT_LIGHT);
      tft.setCursor(leftMargin + 20, topMargin + lineHeight * 6);
      tft.print("- Peter Drucker");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("Never stop dreaming");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("and believing in");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("yourself!");
      break;
      
    case 1:
      tft.setTextColor(ACCENT_BLUE);
      tft.setCursor(leftMargin, topMargin);
      tft.print("Daily Reminder:");
      
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(leftMargin, topMargin + lineHeight * 2);
      tft.print("You are stronger");
      tft.setCursor(leftMargin, topMargin + lineHeight * 3);
      tft.print("than you think.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 5);
      tft.print("You are capable of");
      tft.setCursor(leftMargin, topMargin + lineHeight * 6);
      tft.print("amazing things.");
      
      tft.setCursor(leftMargin, topMargin + lineHeight * 8);
      tft.print("Believe in yourself");
      tft.setCursor(leftMargin, topMargin + lineHeight * 9);
      tft.print("always, and never");
      tft.setCursor(leftMargin, topMargin + lineHeight * 10);
      tft.print("give up on your");
      tft.setCursor(leftMargin, topMargin + lineHeight * 11);
      tft.print("dreams!");
      break;
  }
}

// ======================================================
// ABOUT CONTENT
// ======================================================
void drawAboutContent(uint8_t page) {
  tft.fillRect(0, 31, SCREEN_W, 113, SETTING_BG);
  
  tft.fillRect(5, 36, 118, 28, ACCENT_ORANGE);
  tft.drawRect(5, 36, 118, 28, ACCENT_ORANGE);
  
  tft.setTextSize(1);
  tft.setTextColor(CARD_BG);
  tft.setCursor(15, 42);
  tft.print("Hope Box Device");
  tft.setTextSize(1);
  tft.setCursor(15, 52);
  tft.print("Personal Digital Gift");
  
  drawSettingItem(0, "Device Info", "ESP32 + ST7735 + SD", true, ACCENT_BLUE);
  drawSettingItem(1, "Storage Status", sdCardDetected ? "SD Card Ready" : "No SD Card", true, sdCardDetected ? SUCCESS_GREEN : ERROR_RED);
  drawSettingItem(2, "Created By", "Bhavy", true, ACCENT_PURPLE);
  
  tft.fillRect(5, 136, 118, 8, RGB(240, 240, 245));
  tft.setTextSize(1);
  tft.setTextColor(TEXT_LIGHT);
  tft.setCursor(15, 137);
  tft.print("Hope Box v2.0 | Made with Love");
}

// ======================================================
// SETTING ITEM
// ======================================================
void drawSettingItem(int index, const char* title, const char* subtitle, bool hasIcon, uint16_t iconColor) {
  int y = 68 + (index * 22);
  
  tft.fillRoundRect(5, y, 118, 20, 6, SETTING_ITEM_BG);
  tft.drawRoundRect(5, y, 118, 20, 6, RGB(220, 220, 220));
  
  tft.fillCircle(15, y + 10, 6, iconColor);
  tft.drawCircle(15, y + 10, 6, iconColor);
  
  tft.setTextSize(1);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(25, y + 6);
  tft.print(title);
  
  tft.setTextColor(TEXT_LIGHT);
  tft.setCursor(25, y + 14);
  tft.print(subtitle);
  
  tft.fillTriangle(110, y + 10, 114, y + 7, 114, y + 13, TEXT_LIGHT);
}

// ======================================================
// BUTTON HANDLER
// ======================================================
void handleButtons() {
  bool currentButton0 = digitalRead(BUTTON_0);
  bool currentButton1 = digitalRead(BUTTON_1);
  bool currentButtonSelect = digitalRead(BUTTON_SELECT);
  bool currentButtonBack = digitalRead(BUTTON_BACK);

  if (currentButton0 == LOW && lastButton0 == HIGH) {
    delay(DEBOUNCE);
    currentButton0 = digitalRead(BUTTON_0);
    
    if (currentButton0 == LOW) {
      lastPressTime = millis();
      buttonPressVibration();
      
      if (currentState == LOCKED && passIndex < PASS_LENGTH) {
        enteredPasscode[passIndex] = 0;
        passIndex++;
        
        setRGBLeds(rgbLeds.Color(0, 100, 255));
        
        int spacing = 24;
        int startX = (SCREEN_W - (PASS_LENGTH - 1) * spacing) / 2;
        int x = startX + ((passIndex - 1) * spacing);
        int y = 138;
        
        for (int r = 7; r < 11; r += 2) {
          tft.drawCircle(x, y, r, ACCENT_BLUE);
          delay(8);
        }
        
        drawPasscodeCircles();
        
        if (passIndex >= PASS_LENGTH) {
          delay(150);
          
          if (checkPasscode()) {
            currentState = UNLOCKING;
            showUnlockAnimation();
            drawMenu();
            currentState = MENU;
          } else {
            showLockedAnimation();
          }
        }
        
      } else if (currentState == MENU) {
        uint8_t oldSelection = selectedMenuItem;
        selectedMenuItem = (selectedMenuItem == 0) ? 3 : selectedMenuItem - 1;
        
        menuNavigationVibration();
        
        switch(selectedMenuItem) {
          case 0: setRGBLeds(rgbLeds.Color(150, 50, 200)); break;
          case 1: setRGBLeds(rgbLeds.Color(255, 100, 180)); break;
          case 2: setRGBLeds(rgbLeds.Color(0, 100, 255)); break;
          case 3: setRGBLeds(rgbLeds.Color(255, 150, 0)); break;
        }
        
        drawMenuItem(oldSelection, false);
        drawMenuItem(selectedMenuItem, true);
        
      } else if (currentState == CONTENT) {
        if (currentPage > 0) {
          currentPage--;
          updatePageContent(selectedMenuItem);
        }
      }
    }
  }

  if (currentButton1 == LOW && lastButton1 == HIGH) {
    delay(DEBOUNCE);
    currentButton1 = digitalRead(BUTTON_1);
    
    if (currentButton1 == LOW) {
      lastPressTime = millis();
      buttonPressVibration();
      
      if (currentState == LOCKED && passIndex < PASS_LENGTH) {
        enteredPasscode[passIndex] = 1;
        passIndex++;
        
        setRGBLeds(rgbLeds.Color(150, 50, 200));
        
        int spacing = 24;
        int startX = (SCREEN_W - (PASS_LENGTH - 1) * spacing) / 2;
        int x = startX + ((passIndex - 1) * spacing);
        int y = 138;
        
        for (int r = 7; r < 11; r += 2) {
          tft.drawCircle(x, y, r, ACCENT_PURPLE);
          delay(8);
        }
        
        drawPasscodeCircles();
        
        if (passIndex >= PASS_LENGTH) {
          delay(150);
          
          if (checkPasscode()) {
            currentState = UNLOCKING;
            showUnlockAnimation();
            drawMenu();
            currentState = MENU;
          } else {
            showLockedAnimation();
          }
        }
        
      } else if (currentState == MENU) {
        uint8_t oldSelection = selectedMenuItem;
        selectedMenuItem = (selectedMenuItem + 1) % 4;
        
        menuNavigationVibration();
        
        switch(selectedMenuItem) {
          case 0: setRGBLeds(rgbLeds.Color(150, 50, 200)); break;
          case 1: setRGBLeds(rgbLeds.Color(255, 100, 180)); break;
          case 2: setRGBLeds(rgbLeds.Color(0, 100, 255)); break;
          case 3: setRGBLeds(rgbLeds.Color(255, 150, 0)); break;
        }
        
        drawMenuItem(oldSelection, false);
        drawMenuItem(selectedMenuItem, true);
        
      } else if (currentState == CONTENT) {
        if (currentPage < maxPages - 1) {
          currentPage++;
          updatePageContent(selectedMenuItem);
        }
      }
    }
  }

  if (currentButtonSelect == LOW && lastButtonSelect == HIGH) {
    delay(DEBOUNCE);
    currentButtonSelect = digitalRead(BUTTON_SELECT);
    
    if (currentButtonSelect == LOW) {
      lastPressTime = millis();
      buttonPressVibration();
      
      if (currentState == LOCKED && passIndex < PASS_LENGTH) {
        enteredPasscode[passIndex] = 2;
        passIndex++;
        
        setRGBLeds(rgbLeds.Color(255, 100, 180));
        
        int spacing = 24;
        int startX = (SCREEN_W - (PASS_LENGTH - 1) * spacing) / 2;
        int x = startX + ((passIndex - 1) * spacing);
        int y = 138;
        
        for (int r = 7; r < 11; r += 2) {
          tft.drawCircle(x, y, r, ACCENT_PINK);
          delay(8);
        }
        
        drawPasscodeCircles();
        
        if (passIndex >= PASS_LENGTH) {
          delay(150);
          
          if (checkPasscode()) {
            currentState = UNLOCKING;
            showUnlockAnimation();
            drawMenu();
            currentState = MENU;
          } else {
            showLockedAnimation();
          }
        }
        
      } else if (currentState == MENU) {
        showContent(selectedMenuItem);
        currentState = CONTENT;
        setRGBLeds(rgbLeds.Color(50, 50, 50));
      }
    }
  }

  if (currentButtonBack == LOW && lastButtonBack == HIGH) {
    delay(DEBOUNCE);
    currentButtonBack = digitalRead(BUTTON_BACK);
    
    if (currentButtonBack == LOW) {
      lastPressTime = millis();
      buttonPressVibration();
      
      if (currentState == LOCKED && passIndex < PASS_LENGTH) {
        enteredPasscode[passIndex] = 3;
        passIndex++;
        
        setRGBLeds(rgbLeds.Color(255, 150, 0));
        
        int spacing = 24;
        int startX = (SCREEN_W - (PASS_LENGTH - 1) * spacing) / 2;
        int x = startX + ((passIndex - 1) * spacing);
        int y = 138;
        
        for (int r = 7; r < 11; r += 2) {
          tft.drawCircle(x, y, r, ACCENT_ORANGE);
          delay(8);
        }
        
        drawPasscodeCircles();
        
        if (passIndex >= PASS_LENGTH) {
          delay(150);
          
          if (checkPasscode()) {
            currentState = UNLOCKING;
            showUnlockAnimation();
            drawMenu();
            currentState = MENU;
          } else {
            showLockedAnimation();
          }
        }
        
      } else if (currentState == CONTENT) {
        drawMenu();
        currentState = MENU;
      }
    }
  }

  lastButton0 = currentButton0;
  lastButton1 = currentButton1;
  lastButtonSelect = currentButtonSelect;
  lastButtonBack = currentButtonBack;
}

// ======================================================
// EMOJI DRAWINGS
// ======================================================

void drawLoveLetter(int x, int y) {
  tft.fillRect(x, y + 2, 10, 7, RGB(255, 240, 245));
  tft.drawRect(x, y + 2, 10, 7, RGB(255, 150, 180));
  tft.fillTriangle(x, y + 2, x + 10, y + 2, x + 5, y - 1, RGB(255, 200, 220));
  tft.drawLine(x, y + 2, x + 5, y - 1, RGB(255, 150, 180));
  tft.drawLine(x + 10, y + 2, x + 5, y - 1, RGB(255, 150, 180));
  tft.fillCircle(x + 3, y + 5, 1, RGB(255, 100, 140));
  tft.fillCircle(x + 5, y + 5, 1, RGB(255, 100, 140));
  tft.fillTriangle(x + 2, y + 5, x + 6, y + 5, x + 4, y + 7, RGB(255, 100, 140));
}

void drawLandscape(int x, int y) {
  tft.fillRect(x, y, 10, 4, RGB(135, 206, 250));
  tft.fillTriangle(x, y + 4, x + 5, y, x + 10, y + 4, RGB(139, 137, 137));
  tft.fillCircle(x + 8, y + 1, 2, RGB(255, 223, 0));
  tft.fillRect(x, y + 4, 10, 4, RGB(144, 238, 144));
}

void drawNote(int x, int y) {
  tft.fillRect(x + 1, y, 8, 10, RGB(255, 255, 240));
  tft.drawRect(x + 1, y, 8, 10, RGB(200, 200, 180));
  tft.drawFastHLine(x + 2, y + 2, 5, RGB(100, 150, 255));
  tft.drawFastHLine(x + 2, y + 4, 5, RGB(100, 150, 255));
  tft.drawFastHLine(x + 2, y + 6, 5, RGB(100, 150, 255));
  tft.drawFastHLine(x + 2, y + 8, 3, RGB(100, 150, 255));
}

void drawProfile(int x, int y) {
  tft.fillCircle(x + 5, y + 2, 3, RGB(100, 120, 140));
  tft.fillCircle(x + 5, y + 8, 4, RGB(100, 120, 140));
  tft.fillRect(x + 1, y + 8, 8, 2, RGB(240, 245, 255));
}

void drawSDCard(int x, int y, bool detected) {
  uint16_t cardColor = detected ? SUCCESS_GREEN : ERROR_RED;
  uint16_t fillColor = detected ? RGB(230, 255, 230) : RGB(255, 230, 230);
  
  tft.fillRoundRect(x, y + 2, 10, 8, 2, fillColor);
  tft.drawRoundRect(x, y + 2, 10, 8, 2, cardColor);
  
  tft.fillTriangle(x + 7, y, x + 10, y, x + 10, y + 3, fillColor);
  tft.drawLine(x + 7, y, x + 10, y + 3, cardColor);
  tft.drawLine(x + 7, y, x + 10, y, cardColor);
  tft.drawLine(x + 10, y, x + 10, y + 3, cardColor);
  
  for (int i = 0; i < 3; i++) {
    tft.drawFastVLine(x + 2 + (i * 2), y + 5, 3, cardColor);
  }
}
// ======================================================
// RGB LED CONTROL FUNCTIONS
// ======================================================

// Set all LEDs to the same color
void setRGBLeds(uint32_t color) {
  for (int i = 0; i < RGB_LED_COUNT; i++) {
    rgbLeds.setPixelColor(i, color);
  }
  rgbLeds.show();
}

// Set individual LED color
void setRGBLed(uint8_t ledIndex, uint32_t color) {
  if (ledIndex < RGB_LED_COUNT) {
    rgbLeds.setPixelColor(ledIndex, color);
    rgbLeds.show();
  }
}

// Breathing effect with specified color
void rgbBreathEffect(uint32_t color, uint8_t cycles) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  for (uint8_t c = 0; c < cycles; c++) {
    // Fade in
    for (int brightness = 0; brightness < 255; brightness += 5) {
      uint8_t r_scaled = (r * brightness) / 255;
      uint8_t g_scaled = (g * brightness) / 255;
      uint8_t b_scaled = (b * brightness) / 255;
      setRGBLeds(rgbLeds.Color(r_scaled, g_scaled, b_scaled));
      delay(10);
    }
    // Fade out
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
      uint8_t r_scaled = (r * brightness) / 255;
      uint8_t g_scaled = (g * brightness) / 255;
      uint8_t b_scaled = (b * brightness) / 255;
      setRGBLeds(rgbLeds.Color(r_scaled, g_scaled, b_scaled));
      delay(10);
    }
  }
}

// Quick pulse effect
void rgbPulseEffect(uint32_t color) {
  for (int i = 0; i < 3; i++) {
    setRGBLeds(color);
    delay(80);
    setRGBLeds(rgbLeds.Color(0, 0, 0));
    delay(80);
  }
  setRGBLeds(color);
} 

// Rainbow effect across 3 LEDs
void rgbRainbow() {
  rgbLeds.setPixelColor(0, rgbLeds.Color(150, 50, 200));  // Purple
  rgbLeds.setPixelColor(1, rgbLeds.Color(255, 100, 180)); // Pink
  rgbLeds.setPixelColor(2, rgbLeds.Color(0, 100, 255));   // Blue
  rgbLeds.show();
}

// ======================================================
// END OF CODE
// ======================================================