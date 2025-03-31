#include <Arduino.h>

const int led1 = 2; // LED pin
const int led2 = 15;

void setup() {
  // put your setup code here, to run once:
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  xTaskCreatePinnedToCore(
    toggleLED1,    // Function that should be called
    "Toggle LED",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL,             // Task handle
    0
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
    // Turn the LED on
    digitalWrite(led1, HIGH);
    // Pause the task for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
    // Turn the LED off
    digitalWrite(led1, LOW);
    // Pause the task again for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void toggleLED2(void * parameter){
  for(;;){ // infinite loop
    Serial.println("Hello from toggleLED2");
    // Turn the LED on
    digitalWrite(led2, HIGH);
    // Pause the task for 500ms
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // Turn the LED off
    digitalWrite(led2, LOW);
    // Pause the task again for 500ms
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
