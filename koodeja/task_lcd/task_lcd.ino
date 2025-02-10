#include <Arduino.h>
#include "TFT_eSPI.h"

TFT_eSPI tft;

const int led1 = 2; // LED pin
const int led2 = 15;

int led1_state = 0;
int led2_state = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  xTaskCreatePinnedToCore (
    updateLCD,
    "Update LCD",
    1000,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    toggleLED1,    // Function that should be called
    "Toggle LED",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL,             // Task handle
    1
  );
  xTaskCreatePinnedToCore(
    toggleLED2,
    "Toggle LED 2",
    1000,
    NULL,
    1,
    NULL,
    1
  );
  Serial.begin(115200);
}

void toggleLED1(void * parameter){
  for(;;){ // infinite loop
    Serial.println("Hello from toggleLED1");
    if (led1_state == 0) {
      led1_state = 1;
    } else {
      led1_state = 0;
    }
    digitalWrite(led1, led1_state);
    // Pause the task for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void toggleLED2(void * parameter){
  for(;;){ // infinite loop
    Serial.println("Hello from toggleLED2");
    if (led2_state == 0) {
      led2_state = 1;
    } else {
      led2_state = 0;
    }
    digitalWrite(led2, led2_state);
    // Pause the task for 500ms
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void updateLCD(void * parameter) {
  for (;;) {
    tft.setCursor(10, 30);
    tft.print("LED 1 STATE : ");
    tft.println(led1_state);

    tft.setCursor(10, 60);
    tft.print("LED 2 STATE : ");
    tft.println(led2_state);
    
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
