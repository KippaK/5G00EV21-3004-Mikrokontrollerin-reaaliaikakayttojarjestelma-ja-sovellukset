#include <TFT_eSPI.h>  // Include the TFT_eSPI library
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define BORDER_COLOR TFT_WHITE
#define BAR_COLOR TFT_WHITE

TFT_eSPI tft = TFT_eSPI(); // Create an instance of the TFT_eSPI class

const int adcPin = 15;    // ADC pin to read the value from
const int ledPin = 2;     // LED pin to blink

QueueHandle_t adcQueue;   // FreeRTOS queue to transfer ADC values between tasks

const int barX = 20;      // X position of the bar
const int barY = 100;     // Y position of the bar
const int barWidth = 200; // Width of the bar
const int barHeight = 20; // Height of the bar

void drawBorder() {
  // Draw a border around the bar
  tft.drawRect(barX - 2, barY - 2, barWidth + 4, barHeight + 4, BORDER_COLOR);
}

void readAdcTask(void *parameter) {
  for (;;) {
    int adcValue = analogRead(adcPin); // Read the ADC value
    xQueueSend(adcQueue, &adcValue, portMAX_DELAY); // Send value to the queue
    vTaskDelay(50 / portTICK_PERIOD_MS); // Delay for 50ms
  }
}

void updateDisplayTask(void *parameter) {
  static int prevBarLength = 0; // Store the previous bar length to update only when needed
  int adcValue;
  for (;;) {
    if (xQueueReceive(adcQueue, &adcValue, portMAX_DELAY) == pdTRUE) {
      int barLength = map(adcValue, 0, 4095, 0, barWidth); // Map ADC value to bar length

      // Update only the necessary portions of the bar
      if (barLength > prevBarLength) {
        // Extend the bar
        tft.fillRect(barX + prevBarLength, barY, barLength - prevBarLength, barHeight, BAR_COLOR);
      } else if (barLength < prevBarLength) {
        // Shrink the bar
        tft.fillRect(barX + barLength, barY, prevBarLength - barLength, barHeight, TFT_BLACK);
      }

      prevBarLength = barLength;

      // Update the ADC value text
      tft.fillRect(10, 10, 120, 20, TFT_BLACK); // Clear previous text
      tft.setCursor(10, 10);
      tft.print("ADC: ");
      tft.println(adcValue);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Update display every 100ms
  }
}

void blinkLedTask(void *parameter) {
  int adcValue;
  for (;;) {
    if (xQueueReceive(adcQueue, &adcValue, portMAX_DELAY) == pdTRUE) {
      int blinkRate = map(adcValue, 0, 4095, 5, 20); // Map ADC value to blink rate (5-20 Hz)
      int blinkDelay = 1000 / (blinkRate * 2); // Calculate delay based on blink rate

      digitalWrite(ledPin, HIGH);
      vTaskDelay(blinkDelay / portTICK_PERIOD_MS); // ON duration
      digitalWrite(ledPin, LOW);
      vTaskDelay(blinkDelay / portTICK_PERIOD_MS); // OFF duration
    }
  }
}

void setup() {
  tft.init();            // Initialize the display
  tft.setRotation(1);    // Set the screen rotation (0-3)
  tft.fillScreen(TFT_BLACK); // Clear the screen and set the background to black

  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to white on black background
  tft.setTextSize(2);    // Set text size multiplier

  pinMode(adcPin, INPUT); // Set ADC pin as input
  pinMode(ledPin, OUTPUT); // Set LED pin as output

  adcQueue = xQueueCreate(10, sizeof(int)); // Create a queue to hold ADC values
  if (adcQueue == NULL) {
    tft.setCursor(10, 30);
    tft.println("Queue creation failed!");
    while (true);
  }

  analogSetAttenuation(ADC_11db);

  drawBorder(); // Draw the border around the bar

  // Create tasks
  xTaskCreatePinnedToCore(readAdcTask, "Read ADC Task", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(updateDisplayTask, "Update Display Task", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(blinkLedTask, "Blink LED Task", 1024, NULL, 1, NULL, 1);
}

void loop() {
  // Nothing to do here, tasks handle the functionality
}
