// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include "DHT.h"

#define SENSOR_ERROR -1
#define TEMPERATURE 0
#define HUMIDITY 1
#define HEAT_INDEX 2
#define UART_SIGNAL_PIN 15 // to enable USART mode

// Macro to send different sensor values
#define uartSendSensorData(buff, sensorValue, sensorValueType)                                    \ 
        sensorValueType != SENSOR_ERROR ? sprintf(buff, "%d:%3.2f", sensorValueType, sensorValue) \
        : sprintf(buff, "%d:Sensor_Error", sensorValue);                                          \
        Serial.println(buff);

#define DHTPIN 10 // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11 // DHT 11

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 3 (on the right) of the sensor to GROUND (if your sensor has 3 pins)
// Connect pin 4 (on the right) of the sensor to GROUND and leave the pin 3 EMPTY (if your sensor has 4 pins)
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

// 10 characters = 8 char sensor value + \0 null char + buffer char
char stringBuffer[10];
float humidity, temperature, heatIndex;

// UART_SIGNAL_PIN goes high momentarily to provide synchronization with the receiver
// Makes sure that we send only correct data
void uartSendAllSensorData() {
  digitalWrite(UART_SIGNAL_PIN, HIGH);
  uartSendSensorData(stringBuffer, isnan(temperature) ? SENSOR_ERROR : temperature, isnan(temperature) ? SENSOR_ERROR : TEMPERATURE);
  delay(200);
  digitalWrite(UART_SIGNAL_PIN, LOW);
  delay(100);
  digitalWrite(UART_SIGNAL_PIN, HIGH);
  uartSendSensorData(stringBuffer, isnan(humidity) ? SENSOR_ERROR : humidity, isnan(humidity) ? SENSOR_ERROR : HUMIDITY);
  delay(200);
  digitalWrite(UART_SIGNAL_PIN, LOW);
  delay(100);
  digitalWrite(UART_SIGNAL_PIN, HIGH);
  uartSendSensorData(stringBuffer, (isnan(temperature) || isnan(humidity)) ? SENSOR_ERROR : heatIndex, isnan(temperature) || isnan(humidity) ? SENSOR_ERROR : HEAT_INDEX);
  delay(200);
  digitalWrite(UART_SIGNAL_PIN, LOW);
}

void readAllSensorData() {
  // Wait a few seconds between measurements.
  delay(250);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    return;
  } else {
    // Compute heat index in Celsius (isFahreheit = false)
    heatIndex = dht.computeHeatIndex(temperature, humidity, false);
  }
}

void setup() {
  pinMode(UART_SIGNAL_PIN, OUTPUT);
  Serial.begin(9600);
  dht.begin();
}

void loop() {
  readAllSensorData();
  uartSendAllSensorData();
}