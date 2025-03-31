#include <Arduino.h>
#include <TFT_eSPI.h>

#define KEYPAD_DEBOUNCE_MS 50

#define KEYPAD_NOT_PRESSED -1
#define KEYPAD_0 0
#define KEYPAD_1 1
#define KEYPAD_2 2
#define KEYPAD_3 3
#define KEYPAD_4 4
#define KEYPAD_5 5
#define KEYPAD_6 6
#define KEYPAD_7 7
#define KEYPAD_8 8
#define KEYPAD_9 9
#define KEYPAD_A 10
#define KEYPAD_B 11
#define KEYPAD_C 12
#define KEYPAD_D 13
#define KEYPAD_STAR 14
#define KEYPAD_HASH 15

const int KEYPAD[4][4] = {
  {KEYPAD_1, KEYPAD_2, KEYPAD_3, KEYPAD_A},
  {KEYPAD_4, KEYPAD_5, KEYPAD_6, KEYPAD_B},
  {KEYPAD_7, KEYPAD_8, KEYPAD_9, KEYPAD_C},
  {KEYPAD_STAR, KEYPAD_0, KEYPAD_HASH, KEYPAD_D}
};

const int KEYPAD_GPIO_IN[4] = {GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38, GPIO_NUM_39};

const int KEYPAD_GPIO_OUT[4] = {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32};

TFT_eSPI tft = TFT_eSPI();

int checkKeypadStatus() {
    for (int i = 0; i < 4; i++) {
        digitalWrite(KEYPAD_GPIO_OUT[i], HIGH);
        delay(1);
        for (int j = 0; j < 4; j++) {
            if (digitalRead(KEYPAD_GPIO_IN[j]) == HIGH) {
                delay(KEYPAD_DEBOUNCE_MS);
                while (digitalRead(KEYPAD_GPIO_IN[j]) == HIGH);
                delay(KEYPAD_DEBOUNCE_MS);
                digitalWrite(KEYPAD_GPIO_OUT[i], LOW);
                return KEYPAD[j][i];
            }
        }
        digitalWrite(KEYPAD_GPIO_OUT[i], LOW);
    }
    return KEYPAD_NOT_PRESSED;
}

int waitForKeyPress() {
  int keypadStatus = KEYPAD_NOT_PRESSED;
  for (;;) {
    keypadStatus = checkKeypadStatus();
    if (keypadStatus != KEYPAD_NOT_PRESSED) {
      return keypadStatus;
    }
  }
}

void setup() {
  // port init for servo


  // port init for keypad
  for (int i = 0; i < 4; i++) {
    pinMode(KEYPAD_GPIO_IN[i], INPUT);
    pinMode(KEYPAD_GPIO_OUT[i], OUTPUT);
  }
  
  Serial.begin(115200);
  tft.init();
  tft.begin();
  tft.setRotation(1); 
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
}

void loop() {
  int keyPress = waitForKeyPress();
  Serial.println(keyPress);
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(50, 50);
  tft.println(keyPress);
}