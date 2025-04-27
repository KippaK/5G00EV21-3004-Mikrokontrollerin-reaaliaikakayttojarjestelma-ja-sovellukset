#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <vector>
#include <ESP32Servo.h>
#include "lock.h"
#include "unlock.h"

#define KEYPAD_DEBOUNCE_MS 50
#define EXT0_WAKEUP_HIGH 1

#define KEYPAD_WAKEUP_INPUT_PIN GPIO_NUM_39
#define KEYPAD_WAKEUP_OUTPUT_PIN GPIO_NUM_26

#define KEYPAD_NOT_PRESSED -1
#define DOOR_OPEN 1
#define DOOR_CLOSED 0

#define MINUTE_MS 60000

const char *WIFI_SSID = "SOURCE";
const char *WIFI_PASSWORD = "Pelle!23";

const String PASSCODE = "8437";

bool users_updated = false;

int8_t door_status = DOOR_CLOSED;

AsyncWebServer server(80);

QueueHandle_t displayQueue;
QueueHandle_t keypadQueue;
QueueHandle_t codeInputQueue;

TimerHandle_t inactivityTimer;

touch_pad_t touchPin;

TaskHandle_t *displayTaskHandle = new TaskHandle_t;
TaskHandle_t *codeTaskHandle = new TaskHandle_t;
TaskHandle_t *db_update_task_h = new TaskHandle_t;
SemaphoreHandle_t xSemaphore = NULL;

String DB_FILENAME = "/db.csv";
fs::File db_file;
std::vector<String> db_vec = {};

// === TFT Setup ===
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite backgroundSprite = TFT_eSprite(&tft);
TFT_eSprite unlockSprite = TFT_eSprite(&tft);
TFT_eSprite lockSprite = TFT_eSprite(&tft);

// === Servo & Buzzer ===
Servo myServo;
const int servoPin = 17 ;
const int buzzerPin = 13;

// === Password Setup ===
const char passwordCode[4] = {'1', '2', '3', '4'};
const int expectedLength = 4;
const int positions[4] = {42, 90, 138, 186};

const char KEYPAD[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

const int KEYPAD_GPIO_IN[4] = {GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38, GPIO_NUM_39};

const int KEYPAD_GPIO_OUT[4] = {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32};

enum DisplayState {
  DISPLAY_IDLE,
  DISPLAY_ENTER_CODE,
  DISPLAY_STAR,
  DISPLAY_UNLOCK,
  DISPLAY_WRONG
};

typedef struct {
  DisplayState state;
  int position;
} DisplayMessage;


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


void displayTask(void *pvParameters) {
  DisplayMessage msg;

  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(3);

  while (true) {
    if (xQueueReceive(displayQueue, &msg, portMAX_DELAY)) {
      switch (msg.state) {
        case DISPLAY_IDLE:
          tft.fillScreen(TFT_WHITE);
          tft.setTextColor(TFT_BLACK);
          tft.setTextSize(3);
          tft.drawString("Welcome", 60, 20);  // Or any idle UI
          break;
        case DISPLAY_ENTER_CODE:
          tft.fillScreen(TFT_WHITE);
          tft.drawString("Enter Code", 30, 30);
          break;
        case DISPLAY_STAR:
          tft.setTextColor(TFT_BLACK, TFT_WHITE);
          tft.drawString("*", msg.position, 84);
          break;
        case DISPLAY_UNLOCK:
          backgroundSprite.fillSprite(TFT_WHITE);
          unlockSprite.pushToSprite(&backgroundSprite, 88, 50, TFT_WHITE);
          backgroundSprite.pushSprite(0, 0);
          tft.drawString("Open!", 80, 20);
          break;
        case DISPLAY_WRONG:
          backgroundSprite.fillSprite(TFT_WHITE);
          lockSprite.pushToSprite(&backgroundSprite, 88, 50, TFT_WHITE);
          backgroundSprite.pushSprite(0, 0);
          tft.drawString("Wrong", 80, 20);
          break;
        default:
          break;
      }
    }
  }
}

void driveServo(){
  myServo.attach(servoPin);
  switch(door_status){
    case DOOR_OPEN:
      myServo.write(90);
      Serial.println("[driveServo] Door Opened");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
      break;
    case DOOR_CLOSED:
      myServo.write(0);
      Serial.println("[driveServo] Door Closed");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
      break;
    default:
      myServo.write(0);
      break;
  }
  myServo.detach();
}

void openDoor(){
  door_status = DOOR_OPEN;
  driveServo();
}
void closeDoor(){
  door_status = DOOR_CLOSED;
  driveServo();
}
void getCodeTask(void *pvParameters) {
  Serial.println("Code Task Started");
  int key;
  int inputCount = 0;
  int correctCount = 0;

  DisplayMessage msg;

  // Clear screen before starting
  msg.state = DISPLAY_ENTER_CODE;
  xQueueSend(displayQueue, &msg, portMAX_DELAY);

  while (true) {
    if (xQueueReceive(codeInputQueue, &key, portMAX_DELAY)) {
      if (key == '#') {
        break;
      }
      else if (key == '*') {
        // Reset input on '*'
        inputCount = 0;
        correctCount = 0;

        msg.state = DISPLAY_ENTER_CODE;
        xQueueSend(displayQueue, &msg, portMAX_DELAY);
        continue;
      }

      // Only process keys if within input range
      if (inputCount < expectedLength) {
        msg.state = DISPLAY_STAR;

        // Avoid accessing out-of-bounds position
        if (inputCount < sizeof(positions) / sizeof(positions[0])) {
          msg.position = positions[inputCount];
        } else {
          msg.position = 0; // fallback to safe position
        }

        xQueueSend(displayQueue, &msg, portMAX_DELAY);

        if (key == passwordCode[inputCount]) {
          correctCount++;
        }

        inputCount++;
      }
    }
  }

  if (correctCount == expectedLength) {
    msg.state = DISPLAY_UNLOCK;
    xQueueSend(displayQueue, &msg, portMAX_DELAY);
    openDoor();
    tone(buzzerPin, 75);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    noTone(buzzerPin);
  } else {
    msg.state = DISPLAY_WRONG;
    xQueueSend(displayQueue, &msg, portMAX_DELAY);
    for (int i = 0; i < 10; i++) {
      tone(buzzerPin, 262, 100);
      delay(150);
    }
  }

  codeTaskHandle = NULL;
  vTaskDelete(NULL); // Self-delete task
}

void inputManagerTask(void *pvParameters) {
  int key;

  while (true) {
    if (xQueueReceive(keypadQueue, &key, portMAX_DELAY)) {
      if (key == '*') {
        // Only create a new task if it's not running
        if (codeTaskHandle == NULL) {
          xTaskCreatePinnedToCore(getCodeTask, "CodeTask", 4096, NULL, 1, codeTaskHandle, 1);
        } else {
          // If task exists, reset input via special key
          xQueueSend(codeInputQueue, &key, portMAX_DELAY);
        }
      }
      else if (codeTaskHandle != NULL) {
        // Send other keys to the code task
        xQueueSend(codeInputQueue, &key, portMAX_DELAY);
      }
    }
  }
}



void inactivityTimeoutCallback(TimerHandle_t xTimer) {
  Serial.println("[Timeout] No activity detected for 10s.");

  if (door_status == DOOR_OPEN) {
    Serial.println("[Timeout] Closing door...");
    closeDoor();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait to reach position

    // Use non-blocking delay technique
    unsigned long closeStart = millis();
    while (millis() - closeStart < 1000) {
      vTaskDelay(10 / portTICK_PERIOD_MS); // Let other tasks run
    }
  }

  // Send IDLE screen update
  DisplayMessage msg;
  msg.state = DISPLAY_IDLE;
  msg.position = 0;

  if (xQueueSend(displayQueue, &msg, 100) != pdPASS) {
    Serial.println("[Timeout] Failed to send IDLE message to display queue.");
  } else {
    Serial.println("[Timeout] Display set to IDLE.");
  }
}


// Function to scan keypad and return pressed key
int checkKeypadStatus() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(KEYPAD_GPIO_OUT[i], HIGH);
    delay(1);
    for (int j = 0; j < 4; j++) {
      if (digitalRead(KEYPAD_GPIO_IN[j]) == HIGH) {
        delay(KEYPAD_DEBOUNCE_MS);
        while (digitalRead(KEYPAD_GPIO_IN[j]) == HIGH);  // Wait for release
        delay(KEYPAD_DEBOUNCE_MS);
        digitalWrite(KEYPAD_GPIO_OUT[i], LOW);
        return KEYPAD[j][i];
      }
    }
    digitalWrite(KEYPAD_GPIO_OUT[i], LOW);
  }
  return KEYPAD_NOT_PRESSED;
}

void keypadTask(void *pvParameters) {
  int key;
  for(;;) {
    key = checkKeypadStatus();
    if (key != KEYPAD_NOT_PRESSED) {
      xTimerReset(inactivityTimer, 0);
      xQueueSend(keypadQueue, &key, portMAX_DELAY);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
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

void task_init() {
  // Tasks
  xTaskCreate(update_db_file_task, "Update database file", 2048, NULL, 1, db_update_task_h);
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, displayTaskHandle, 1);
  xTaskCreatePinnedToCore(keypadTask, "KeypadTask", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(inputManagerTask, "InputManager", 4096, NULL, 1, NULL, 1);

  // Queues
  displayQueue = xQueueCreate(5, sizeof(DisplayMessage));
  keypadQueue = xQueueCreate(5, sizeof(int));
  codeInputQueue = xQueueCreate(5, sizeof(int));
}

void tft_init() {
  tft.init();
  tft.setRotation(3); 
  backgroundSprite.setColorDepth(8);
  backgroundSprite.createSprite(240, 135);

  unlockSprite.createSprite(64, 64); // You can pushImage here if you want to pre-fill
  unlockSprite.setSwapBytes(true);
  unlockSprite.pushImage(0,0,64,64, unlockIcon);

  lockSprite.createSprite(64, 64); // Same as above
  unlockSprite.setSwapBytes(true);
  lockSprite.pushImage(0,0,64,64, lockIcon);
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
  inactivityTimer = xTimerCreate("InactivityTimer", pdMS_TO_TICKS(10000), pdFALSE, (void *)0, inactivityTimeoutCallback);
  closeDoor();
  pinMode(buzzerPin, OUTPUT);
  Serial.println("Initializing WiFi");
  wifi_init();


  // port init for keypad
  for (int i = 0; i < 4; i++) {
    pinMode(KEYPAD_GPIO_IN[i], INPUT);
    pinMode(KEYPAD_GPIO_OUT[i], OUTPUT);
    digitalWrite(KEYPAD_GPIO_OUT[i], LOW);
  }
    
  tft_init();

  setupApiEndpoints();
  server.begin();

  DisplayMessage msg;
  msg.state = DISPLAY_IDLE;
  xQueueSend(displayQueue, &msg, portMAX_DELAY);
  xTimerStart(inactivityTimer, 0);

  Serial.println("Setup complete.");
}

void loop() {}