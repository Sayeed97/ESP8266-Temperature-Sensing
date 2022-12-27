// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include "DHT.h"
#include "secrets.h"
#include "DHT11SensorDataTypes.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define UART_SIGNAL_PIN 15 // to enable USART mode

// Macro to convert different sensor values to USART compatible data
#define convertSensorDataToUsartFormat(stringBuff, sensorValue, sensorValueType)                        \ 
        sensorValueType != SENSOR_ERROR ? sprintf(stringBuff, "%d:%3.3f", sensorValueType, sensorValue) \
        : sprintf(stringBuff, "%d:Sensor_Error", sensorValue);

// Sensor macros
#define DHTPIN 10 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

// AWS IoT Core macros
#define TIME_ZONE -5 // EST TIME ZONE = -5 UTC
#define NET_WAIT_TIME_IN_SEC 1510592825
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

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

float humidity, temperature, heatIndex;
struct tm systemTimeInfo;

// AWS IoT core connection settings
WiFiClientSecure secureClient;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient awsIotClient(secureClient);

// a simple no operation function
void noop(void) {
  // arduino no operation: https://www.reddit.com/r/arduino/comments/3ynemw/is_there_a_nop_in_arduino/
  __asm__("nop\n\t");
}

void readAllSensorData(void) {
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

void usartSendSensorDataWithRetry(float sensorValue, int sensorValueType) {
  char data[20];
  int transmissionReturn = TRANSMISSION_FAILURE;

  convertSensorDataToUsartFormat(data, sensorValue, sensorValueType);   
  digitalWrite(UART_SIGNAL_PIN, HIGH);         
  // start transmission retry time                            
  unsigned long transmissionTimeStamp = millis();
  // retry transmitting sensor data for 350 ms
  while(millis() - transmissionTimeStamp <= 350) {
    char receivedCharacter;
    String readString = "";
    Serial.println(data); // send data via usart
    delay(100);
    while(Serial.available()) {
      delay(5); // delay to allow byte to arrive in input buffer
      receivedCharacter = Serial.read();
      readString += receivedCharacter;
      // read only complete data
      if (readString.length() > 0 && receivedCharacter == '\n') {
        transmissionReturn = atoi(readString.c_str());
        // check if receiver returns a transmission success
        if (transmissionReturn == TRANSMISSION_SUCCESS) {
          return; // exit on a transmission success
        }
      } 
    }
  }
  delay(20); // small buffer time for USART signal pin
  digitalWrite(UART_SIGNAL_PIN, LOW);
}                                 

// UART_SIGNAL_PIN goes high momentarily to provide synchronization with the receiver
// Sends sensor data type as a meta data
// Makes sure that we send only correct data with retry strategy
void usartSendAllSensorData(void) {
  usartSendSensorDataWithRetry(isnan(temperature) ? SENSOR_ERROR : temperature, isnan(temperature) ? SENSOR_ERROR : TEMPERATURE);  
  usartSendSensorDataWithRetry(isnan(humidity) ? SENSOR_ERROR : humidity, isnan(humidity) ? SENSOR_ERROR : HUMIDITY);
  usartSendSensorDataWithRetry((isnan(temperature) || isnan(humidity)) ? SENSOR_ERROR : heatIndex, isnan(temperature) || isnan(humidity) ? SENSOR_ERROR : HEAT_INDEX);
}

// Set system time using Simple Network Time Protocol (SNTP) to keep track of certificate expiry
// Refer: https://github.com/sborsay/Serverless-IoT-on-AWS/blob/1e7824dbe9591a26f32acac198e4b310ab9f8a91/ESP8266-to-AWS-IoT#L136
void setSystemTimeFromSNTP(void)
{
  Serial.println("Setting system time using SNTP");
  // Trys to get time from ntp or nist network time servers
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t netTimeInSeconds = time(nullptr); // This function returns the time since 00:00:00 UTC, January 1, 1970 (Unix timestamp) in seconds
  // Wait till we set network time, needed for correctness
  while (netTimeInSeconds < NET_WAIT_TIME_IN_SEC)
  {
    delay(500);
    Serial.print(".");
    netTimeInSeconds = time(nullptr);
  }  
  Serial.println("System time set");
  // Converts the calendar time pointed to by clock into Coordinated Universal Time (UTC)
  // Refer: https://www.ibm.com/jsonDocs/en/zos/2.4.0?topic=lf-gmtime-r-gmtime64-r-convert-time-value-broken-down-utc-time
  gmtime_r(&netTimeInSeconds, &systemTimeInfo);
}

String getCurrentTime(void) {
  char currentTime[10];
  setSystemTimeFromSNTP();
  sprintf(currentTime, "%d:%d:%d", systemTimeInfo.tm_hour + TIME_ZONE, systemTimeInfo.tm_min, systemTimeInfo.tm_sec);
  return String(currentTime);
}
 
void mqttMessageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void connectToWifi(void) {
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
}

// wrapper to return 0 for true and -1 for false
int awsIoTClientSubscribe(void) {
  return awsIotClient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC) ? 0 : -1;
}
 
// Any failure here is very likely that the issue is related to AWS IoT or the loading of certs
int connectToAWSIot(void)
{
  // re-connect to WiFi if seen disconnected
  WiFi.isConnected() ? noop() : connectToWifi();
  setSystemTimeFromSNTP();
 
  // AWS IoT Client Setup
  secureClient.setTrustAnchors(&cert);
  secureClient.setClientRSACert(&client_crt, &key);
  awsIotClient.setServer(MQTT_HOST, 8883);
  awsIotClient.setCallback(mqttMessageReceived);
 
  Serial.println("Connecting to AWS IOT");
 
  // Wait till we connect to aws IoT core
  while (!awsIotClient.connect(THINGNAME))
  {
    Serial.println(".");
    delay(500);
  }

  Serial.println("Connected to AWS IoT");
  
  // Return -1 for unable to connect to AWS IoT or Subscribe
  return awsIotClient.connected() ? awsIoTClientSubscribe() : -1;
}
 
int sendMqttMessage(void)
{
  StaticJsonDocument<200> jsonDoc;
  char jsonBuffer[512];
  jsonDoc["time"] = getCurrentTime();
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["heat"] = heatIndex;
  serializeJson(jsonDoc, jsonBuffer); // print to awsIotClient
 
  return awsIotClient.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer) ? 0 : -1;
}

void tryToSendMqttMessage(void) {
  // Check if we are connected to aws iot core else reconnect and try to send mqtt message
  awsIotClient.connected() ? ((sendMqttMessage() == 0) ? Serial.println("MQTT message sent to AWS IoT Core") : Serial.println("Failed to send MQTT message to AWS IoT Core")) 
  : ((connectToAWSIot() == 0) ? ((sendMqttMessage() == 0) ? Serial.println("MQTT message sent to AWS IoT Core") : Serial.println("Failed to send MQTT message to AWS IoT Core")) 
  : Serial.println("Cannot connect to AWS IoT Core"));
}

void setup() {
  pinMode(UART_SIGNAL_PIN, OUTPUT);
  Serial.begin(9600);
  // Unable to connect to AWS doesn't mean that we should stop the program
  connectToAWSIot() == 0 ? Serial.println("") : Serial.println("Unable to connect to AWS IoT Core");
  dht.begin();
}

void loop() {
  readAllSensorData();
  usartSendAllSensorData();
  // Send MQTT Message to AWS IoT Core
  tryToSendMqttMessage();
  // Need to refresh the mqtt connection, refer: https://ubidots.com/community/t/solved-what-is-the-function-of-client-loop/913
  awsIotClient.loop();
}