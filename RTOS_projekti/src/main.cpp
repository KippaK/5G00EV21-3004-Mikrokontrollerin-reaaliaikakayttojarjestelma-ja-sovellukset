#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define KEYPAD_DEBOUNCE_MS 50
#define EXT0_WAKEUP_HIGH 1

#define KEYPAD_WAKEUP_INPUT_PIN GPIO_NUM_39
#define KEYPAD_WAKEUP_OUTPUT_PIN GPIO_NUM_26

const char *WIFI_SSID = "SOURCE";
const char *WIFI_PASSWORD = "Pelle!23";

AsyncWebServer server(80);

QueueHandle_t keypadQueue;

TimerHandle_t inactivityTimer;

touch_pad_t touchPin;

const char KEYPAD_NOT_PRESSED = 0;

const char KEYPAD[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

const int KEYPAD_GPIO_IN[4] = {GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38, GPIO_NUM_39};

const int KEYPAD_GPIO_OUT[4] = {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32};

TFT_eSPI tft = TFT_eSPI();

void IRAM_ATTR keypadISR() {

}

char checkKeypadStatus() {
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

char waitForKeyPress() {
  char keypadStatus = KEYPAD_NOT_PRESSED;
  for (;;) {
    keypadStatus = checkKeypadStatus();
    if (keypadStatus != KEYPAD_NOT_PRESSED) {
      return keypadStatus;
    }
  }
}

void setupApiEndpoints() {
  server.on("/api/open", HTTP_GET, [](AsyncWebServerRequest *request){
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.print("Open");
    request->send(200, "application/json");
  });
  server.on("/api/close", HTTP_GET, [](AsyncWebServerRequest *request){
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.print("close");
    request->send(200, "application/json");
  });
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
  tft.setRotation(3); 
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.fillScreen(TFT_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    tft.print('.');
    delay(500);
  }
  tft.setCursor(0, 0);
  tft.fillScreen(TFT_BLACK);
  tft.println("Connected to WiFi network");
  tft.print(WiFi.localIP());

  setupApiEndpoints();

  server.begin();
}

void loop() {
  char keyPress = waitForKeyPress();
  Serial.println(keyPress);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(50, 50);
  tft.println(keyPress);
}