#include <Arduino.h>

#define PIN_LED 32
#define PIN_NTC 35
#define PIN_VIB 34 //Simulate vibration sensor via potentionmeter
#define LOG_MAX_BUFFER 30

const float BETA = 3950; // should match the Beta Coefficient of the thermistor

typedef struct {
  int sensor_type; //1 for NTC, 2 for vibration sensor
  float value;
  unsigned long timestamp;
} SensorData;

volatile bool debug_state = false; // control printing execution time and period; if ture, then print. else no.
SensorData log_buffer[LOG_MAX_BUFFER];
int write_index = 0;
int read_index = 0;
SemaphoreHandle_t data_mutex;
SemaphoreHandle_t countingLog_semaphore;

void readTemperature(void* param) {
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period = pdMS_TO_TICKS(200);
  while(1) {
    unsigned long start = millis();
    int analogValue = analogRead(PIN_NTC);
    float celsius = 1 / (log(1 / (4095. / analogValue - 1)) / BETA + 1.0 / 298.15) - 273.15;
    if(xSemaphoreTake(data_mutex , portMAX_DELAY) == pdTRUE) {
      log_buffer[write_index].sensor_type = 1;
      log_buffer[write_index].value = celsius;
      log_buffer[write_index].timestamp = millis();

      if(write_index >= 29) write_index = 0;
      else write_index = write_index + 1;

      xSemaphoreGive(data_mutex);
    }
    xSemaphoreGive(countingLog_semaphore);

    if((xTaskGetTickCount() - lastWake) > period) {
      Serial.printf("[CRITICAL] readTemperature missed deadlines!\n\r");
    }
    unsigned long end = millis();
    if(debug_state) printf("[DEBUG: EXEC] Execution time for read temperatre: %lu ms\n\r", end-start);
    vTaskDelayUntil(&lastWake, period);
    unsigned long end_period = millis();
    if(debug_state) printf("[DEBUG: PER] Period for read temperatre: %lu ms\n\r", end_period-start);
  }
}

void readVibration(void* param) {
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period = pdMS_TO_TICKS(500);
  while(1) {
    unsigned long start = millis();
    int analogValue = analogRead(PIN_VIB);
    if(xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE) {
      log_buffer[write_index].sensor_type = 2;
      log_buffer[write_index].value = (float) analogValue;
      log_buffer[write_index].timestamp = millis();

      if(write_index >= 29) write_index = 0;
      else write_index = write_index + 1;

      xSemaphoreGive(data_mutex);
    }
    xSemaphoreGive(countingLog_semaphore);
    if((xTaskGetTickCount() - lastWake) > period) {
      Serial.printf("[CRITICAL] readVibration missed deadlines!\n\r");
    }
    unsigned long end = millis();
    if(debug_state) printf("[DEBUG: EXEC] Execution time for read vibration: %lu ms\n\r", end-start);
    vTaskDelayUntil(&lastWake, period);
    unsigned long end_period = millis();
    if(debug_state) printf("[DEBUG: PER] Period for read vibration: %lu ms\n\r", end_period-start);
  }
}

void logSD(void* param) {
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period = pdMS_TO_TICKS(1000);
  SensorData temp_data;
  while(1) {
    unsigned long start = millis();
    if(uxSemaphoreGetCount(countingLog_semaphore) > 0) {
      int execution_time = random(10,300);
      xSemaphoreTake(countingLog_semaphore,0);

      if(xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE) {
        temp_data = log_buffer[read_index];

        if(read_index >= 29) read_index = 0;
        else read_index = read_index + 1;

        xSemaphoreGive(data_mutex);
      }

      switch(temp_data.sensor_type) {
        case 1:
          Serial.printf("[SD Logging] Temperature Reading: %.2f, Timestamp: %lu\n\r", temp_data.value, temp_data.timestamp);
          break;
        case 2:
          Serial.printf("[SD Logging] Vibration Reading: %.2f, Timestamp: %lu\n\r", temp_data.value, temp_data.timestamp);
          break;
      }
      
      //Simulate fluactuation logging delay
      vTaskDelay(pdMS_TO_TICKS(execution_time));
    }

    if((xTaskGetTickCount() - lastWake) > period) {
      Serial.printf("[CRITICAL] SD card logging missed deadlines!\n\r");
    }

    unsigned long end = millis();
    if(debug_state) printf("[DEBUG: EXEC] Execution time for logging SD card: %lu ms\n\r", end-start);
    vTaskDelayUntil(&lastWake, period);
    unsigned long end_period = millis();
    if(debug_state) printf("[DEBUG: PER] Period for logging SD card: %lu ms\n\r", end_period-start);
  }
}

void blinkLED(void* param) {
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period = pdMS_TO_TICKS(250);
  bool led_status = 0;
  while(1) {
    unsigned long start = millis();
    digitalWrite(PIN_LED, led_status=!led_status);
     if((xTaskGetTickCount() - lastWake) > period) {
      Serial.printf("[CRITICAL] LED blinking missed deadlines!\n\r");
    }

    unsigned long end = millis();
    if(debug_state) printf("[DEBUG: EXEC] Execution time for blinking LED: %lu ms\n\r", end-start);
    vTaskDelayUntil(&lastWake, period);
    unsigned long end_period = millis();
    if(debug_state) printf("[DEBUG: PER] Period for blinking LED: %lu ms\n\r", end_period-start);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_VIB, INPUT_PULLUP);

  data_mutex = xSemaphoreCreateMutex();
  if(data_mutex == NULL) {
    Serial.printf("Fail to create data mutex\n\r");
    while(1);
  }

  countingLog_semaphore = xSemaphoreCreateCounting(50, 0);
  if(countingLog_semaphore == NULL) {
    Serial.printf("Fail to create counting log semaphore\n\r");
    while(1);
  }

  xTaskCreate(readTemperature,"readTemp",2048,NULL,4,NULL);
  xTaskCreate(readVibration,"readVib",2048,NULL,3,NULL);
  xTaskCreate(logSD,"logSD",2048,NULL,2,NULL);
  xTaskCreate(blinkLED,"blinkLED",2048,NULL,1,NULL);

  Serial.printf("ESP32 is ready!\n\r");
}

void loop() {
  // nothing here
  vTaskDelay(pdMS_TO_TICKS(3000));
}

