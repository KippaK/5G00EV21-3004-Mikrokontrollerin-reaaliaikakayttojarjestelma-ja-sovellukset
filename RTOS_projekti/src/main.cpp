#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <vector>

#define KEYPAD_DEBOUNCE_MS 50
#define EXT0_WAKEUP_HIGH 1

#define KEYPAD_WAKEUP_INPUT_PIN GPIO_NUM_39
#define KEYPAD_WAKEUP_OUTPUT_PIN GPIO_NUM_26

#define DOOR_OPEN 1
#define DOOR_CLOSED 0

#define MINUTE_MS 60000

const char *WIFI_SSID = "SOURCE";
const char *WIFI_PASSWORD = "Pelle!23";

const String PASSCODE = "8437";

bool users_updated = false;

int8_t door_status = DOOR_CLOSED;

AsyncWebServer server(80);
QueueHandle_t keypadQueue;
TimerHandle_t inactivityTimer;
touch_pad_t touchPin;

TaskHandle_t *db_update_task_h = new TaskHandle_t;
SemaphoreHandle_t xSemaphore = NULL;

fs::File db_file;
std::vector<String> db_vec;

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

bool authenticateUser(AsyncWebServerRequest *request) {
  if (!request->hasHeader("username")) { return false; }
  if (!request->hasHeader("password")) { return false; }
  String user = request->getHeader("username")->value();
  user += ", ";
  user += request->getHeader("password")->value();
  for (int i = 0; i < db_vec.size(); i++) {
    if (db_vec.at(i) == user) {
      return true;
    }
  }
  return false;
}

void setupApiEndpoints() {
  // Door Status GET Endpoint
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest * request){
    String door_status_str;
    if (door_status == DOOR_OPEN) { door_status_str = "open"; }
    if (door_status == DOOR_CLOSED) { door_status_str = "closed"; }
    String message = "{\"message\": \"Door is " + door_status_str +"\", \"door_status\": \""+ String(door_status) + "\"}";
    request->send(200, "application/json", message);
  });

  // Open Door POST endpoint
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request){
    if (authenticateUser(request)) {
      door_status = DOOR_OPEN;
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0);
      tft.print("Open");
      request->send(200, "application/json");
    }
    else {
      request->send(401, "application/json");
    }
  });

  // Close Door POST endpoint
  server.on("/api/close", HTTP_POST, [](AsyncWebServerRequest *request){
    if (authenticateUser(request)) {
      door_status = DOOR_CLOSED;
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0);
      tft.print("Close");
      request->send(200, "application/json");
    }
    else {
      request->send(401, "application/json");
    }
  });

  // Add User POST endpoint
  server.on("/api/adduser", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("passcode") || !request->hasHeader("password") || !request->hasHeader("username")) {
      request->send(400, "application/json", "{\"error\": \"Missing headers\"}");
      return;
    }
    String received_passcode = request->getHeader("passcode")->value();
    if (received_passcode != PASSCODE) {
      request->send(401, "application/json", "{\"error\": \"Invalid passcode\"}");
      return;
    }
    String username = request->getHeader("username")->value();
    String password = request->getHeader("password")->value();
    String new_user = username + ", " + password;
    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
      db_vec.push_back(new_user);
      users_updated = true;
      request->send(200, "application/json", "{\"message\": \"User added successfully\"}");
      xSemaphoreGive(xSemaphore);
      return;
    }
    else {
      request->send(500, "application/json", "{\"error\": \"Internal server error during user creation\"}");
      return;
    }
  });
}

void db_init() {
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  } else {
    Serial.println("SPIFFS Mounted Successfully");
  }

  db_file = SPIFFS.open("/db.csv", "r+");
  if (!db_file) {
    db_file = SPIFFS.open("/db.csv", FILE_WRITE);
    db_file.println("admin, admin");
    db_vec.push_back("admin, admin");
  }
  if (!db_file) {
    Serial.println("Failed to open or create db file");
    return;
  }
  while (db_file.available()) {
    db_vec.push_back(db_file.readStringUntil('\n'));
  }
  db_file.close();
}

void update_db_file_task(void *params) {
  for (;;) {
    if (users_updated) {
      if (db_file) {
        db_file.close();
      }
      db_file = SPIFFS.open("/db.csv", FILE_WRITE);
      for (int i = 0; i < db_vec.size(); i++) {
        db_file.println(db_vec.at(i));
      }
    Serial.println("Updated database file from task");
    users_updated = false;
    }
    vTaskDelay(MINUTE_MS * 5 / portTICK_PERIOD_MS);
  }
}

void task_init() {
  xTaskCreate(
    update_db_file_task,
    "Update database file",
    2048,
    NULL,
    1,
    db_update_task_h
  );
}

void setup() {
  Serial.begin(115200);
  db_init();
  xSemaphore = xSemaphoreCreateMutex();
  if(xSemaphore == NULL) {
    Serial.println("Failed to create semaphore");
    return;
  }
  task_init();
  // port init for servo


  // port init for keypad
  for (int i = 0; i < 4; i++) {
    pinMode(KEYPAD_GPIO_IN[i], INPUT);
    pinMode(KEYPAD_GPIO_OUT[i], OUTPUT);
  }
  
  

  tft.init();
  tft.begin();
  tft.setRotation(3); 
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.fillScreen(TFT_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.println("Connecting to wifi");
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