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

#define DB_UPDATE_MS 5000

#define SOURCE_VERKKO 1
#if SOURCE_VERKKO
const char *WIFI_SSID = "SOURCE";
const char *WIFI_PASSWORD = "Pelle!23";
#else
const char *WIFI_SSID = "Kekoaly";
const char *WIFI_PASSWORD = "aivosolu";
#endif

const String PASSCODE = "8437";

bool users_updated = false;

int8_t door_status = DOOR_CLOSED;

AsyncWebServer server(80);
QueueHandle_t keypadQueue;
TimerHandle_t inactivityTimer;
touch_pad_t touchPin;

TaskHandle_t *db_update_task_h = new TaskHandle_t;
SemaphoreHandle_t xSemaphore = NULL;

String DB_FILENAME = "/db.csv";
fs::File db_file;
std::vector<String> db_vec = {};

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

void drive_servo() {

}

void openDoor() {
  door_status = DOOR_OPEN;
  drive_servo();
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.print("Open");
}

void closeDoor() {
  door_status = DOOR_CLOSED;
  drive_servo();
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.print("Close");
}

void addCORS(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "*");
  response->addHeader("Access-Control-Allow-Credentials", "true");
}

void sendResponseWithCORS(AsyncWebServerRequest *request, int code, const String &contentType, const String &content) {
  AsyncWebServerResponse *response = request->beginResponse(code, contentType, content);
  addCORS(response);
  request->send(response);
}

void sendResponseWithCORS(AsyncWebServerRequest *request, int code, const String &contentType) {
  AsyncWebServerResponse *response = request->beginResponse(code, contentType);
  addCORS(response);
  request->send(response);
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

bool userExists(String username) {
  username.trim();
  if (xSemaphoreTake(xSemaphore, pdTICKS_TO_MS(5000)) == pdTRUE) {
    for (int i = 0; i < db_vec.size(); i++) {
      String db_user = db_vec.at(i);
      String db_username = db_user.substring(0, db_user.indexOf(','));
      if (db_username == username) {
        xSemaphoreGive(xSemaphore);
        return true;
      }
    }
    xSemaphoreGive(xSemaphore);
  }
  return false;
}

void setupApiEndpoints() {
  // Handler for OPTIONS method
  server.on("/api/*", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
    sendResponseWithCORS(request, 200, "application/json");
  });

  // Door Status GET Endpoint
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest * request){
    String door_status_str;
    if (door_status == DOOR_OPEN) { door_status_str = "open"; }
    if (door_status == DOOR_CLOSED) { door_status_str = "closed"; }
    String message = "{\"message\": \"Door is " + door_status_str +"\", \"door_status\": \""+ String(door_status) + "\"}";
    sendResponseWithCORS(request, 200, "application/json", message);
  });

  // Open Door POST endpoint
  server.on("/api/open", HTTP_POST, [](AsyncWebServerRequest *request){
    if (authenticateUser(request)) {
      openDoor();
      sendResponseWithCORS(request, 200, "application/json");
    }
    else {
      sendResponseWithCORS(request, 401, "application/json");
    }
  });

  // Close Door POST endpoint
  server.on("/api/close", HTTP_POST, [](AsyncWebServerRequest *request){
    if (authenticateUser(request)) {
      closeDoor();
      sendResponseWithCORS(request, 200, "application/json");
    }
    else {
      sendResponseWithCORS(request, 401, "application/json");
    }
  });

  // Add User POST endpoint
  server.on("/api/adduser", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasHeader("passcode") || !request->hasHeader("password") || !request->hasHeader("username")) {
      sendResponseWithCORS(request, 400, "application/json", "{\"error\": \"Missing headers\"}");
      return;
    }
    String received_passcode = request->getHeader("passcode")->value();
    if (received_passcode != PASSCODE) {
      sendResponseWithCORS(request, 401, "application/json", "{\"error\": \"Invalid passcode\"}");
      return;
    }
    String username = request->getHeader("username")->value();
    String password = request->getHeader("password")->value();
    String new_user = username + ", " + password;
    if (userExists(new_user)) {
      sendResponseWithCORS(request, 400, "application/json", "{\"error\": \"User Exists\"}");
      return;
    }
    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
      db_vec.push_back(new_user);
      users_updated = true;
      sendResponseWithCORS(request, 200, "application/json", "{\"message\": \"User added successfully\"}");
      xSemaphoreGive(xSemaphore);
      return;
    }
    else {
      sendResponseWithCORS(request, 500, "application/json", "{\"error\": \"Internal server error during user creation\"}");
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

  db_file = SPIFFS.open(DB_FILENAME, FILE_READ);
  if (!db_file) {
    Serial.println("Database file doesn't exist.");
    Serial.println("Upload SPIFFS image and try again.");
  } else {
    Serial.println("Found existing databse file");
  }
  if (!db_file) {
    Serial.println("Failed to open or create db file");
    return;
  }
  while (db_file.available()) {
    String user = db_file.readStringUntil('\n');
    user.trim();
    Serial.println("User in db_file: " + user);
    db_vec.push_back(user);
  }
  db_file.close();
}

void update_db_file_task(void *params) {
  for (;;) {
    if (users_updated) {
      xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(5000));
      db_file = SPIFFS.open(DB_FILENAME, FILE_WRITE);
      for (int i = 0; i < db_vec.size(); i++) {
        Serial.println("User in db_vec: " + db_vec.at(i));
        db_file.println(db_vec.at(i));
      }
      db_file.close();
      Serial.println("Updated database file from task");
      users_updated = false;
      xSemaphoreGive(xSemaphore);
    }
    vTaskDelay(DB_UPDATE_MS / portTICK_PERIOD_MS);
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

void wifi_init() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("Connected to WiFi network");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  db_init();
  xSemaphore = xSemaphoreCreateMutex();
  if(xSemaphore == NULL) {
    Serial.println("Failed to create semaphore");
    return;
  }
  Serial.println("Initializing tasks");
  task_init();
  // port init for servo
  Serial.println("Initializing WiFi");
  wifi_init();


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


  setupApiEndpoints();

  server.begin();

  Serial.println("Users in database:");
  for (int i = 0; i < db_vec.size(); i++) {
    Serial.println(db_vec.at(i));
  }
}

void loop() {
  char keyPress = waitForKeyPress();
  Serial.println(keyPress);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(50, 50);
  tft.println(keyPress);
}