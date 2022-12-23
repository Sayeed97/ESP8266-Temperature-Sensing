#include <LiquidCrystal_I2C.h>
#include "DHT11SensorDataTypes.h"

#define UART_SIGNAL_PIN 2 // signal pin for synchronizing with receiver
#define Y_AXIS_ANALOG_STICK_PIN 15 // A2 pin on arduino

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,20,4); 

// LCD display that are shared between multiple methods
float tempData, humData, heatIndexData;
int floatFirstPart = 0;
int floatSecondPart = 0;
int sensorDataType = TEMPERATURE; // this state is used by the lcd to display the sensor data by its type
unsigned long lcdRefreshTimeout = 0;

// Analog Joystick states 
int yPosition = 0;
int yAxisMap = 0;
unsigned long joystickStateUpdateTimeout = millis();

// updates sensorDataType based on joystick movement every 800 ms
// cyclic function to cycle between 0->2 state
// 0: Display Temperature data on the LCD
// 1: Display Humidity data on the LCD
// 2: Display Heat index data on the LCD
void changeLCDSensorDataByJoystickMovement() {
  // helps to map y axis movement between -512 to 512
  yAxisMap = map(analogRead(Y_AXIS_ANALOG_STICK_PIN), 0, 1023, 512, -512);
  if (yAxisMap >= -350 && yAxisMap <= 350) {
    return; // exit when no movement is detected
  } else if (yAxisMap > 350 && millis() - joystickStateUpdateTimeout > 500) {
    sensorDataType = sensorDataType < 2 ? (sensorDataType + 1) : 0;
    joystickStateUpdateTimeout = millis();
  } else if (yAxisMap < -350 && millis() - joystickStateUpdateTimeout > 500) {
    sensorDataType = sensorDataType > 0 ? (sensorDataType - 1) : 2;
    joystickStateUpdateTimeout = millis();
  }                                    
}

// 3 point decimal precision
void splitFloatValue() {
  // truncate anything after the decimal
  float value = sensorDataType == TEMPERATURE ? tempData : sensorDataType == HUMIDITY ? humData : heatIndexData;
  floatFirstPart = value; 
  floatSecondPart = value * 1000; 
  floatSecondPart = floatSecondPart - floatFirstPart * 1000; // we will get the decimal part alone
}

// refreshes the information on the LCD
void refreshInformationOnLCD() {
  char sensorDataBuff[20];
  lcd.clear();
  // we display sensor value based on the joystick state motion
  splitFloatValue();
  // Custom string building from float value as the LCD is not able to display float values correctly
  sprintf(sensorDataBuff, sensorDataType == TEMPERATURE ? "TEMP: %d.%d" : sensorDataType == HUMIDITY ? 
  "HUMI: %d.%d" : "HEAT: %d.%d", floatFirstPart, floatSecondPart);
  lcd.setCursor(0, 0);
  lcd.print(sensorDataBuff);
}

// refreshes LCD every 1 second
void refreshLCD() {
  if(millis() - lcdRefreshTimeout >= 800) {
    refreshInformationOnLCD();
    lcdRefreshTimeout = millis();
  }
}

// updates sensor data
void updateSensorData(int type, float data) {
  if (type == TEMPERATURE) {
    tempData = data;
  } else if (type == HUMIDITY) {  
      humData = data;
  } else if (type == HEAT_INDEX) {
      heatIndexData = data;
  }
}

// reads sensor data by its type based on the meta data provided by the transmitter
void uartReadSensorData() {
  char receivedCharacter;
  String readString;
  int sensorDataDelimiter = -1; // indicates no index
  // reads uart synchronously with the transmitter
  while(digitalRead(UART_SIGNAL_PIN) == HIGH && Serial.available()) {
    delay(5); // delay to allow byte to arrive in input buffer
    receivedCharacter = Serial.read();
    readString += receivedCharacter;
    // update only complete sensor data
    if (readString.length() > 0 && receivedCharacter == '\n') {
      sensorDataDelimiter = readString.indexOf(':');
      // sensorDataDelimiter should be greater than 0 for complete data
      if (sensorDataDelimiter > 0) {
        // update the sensor data and its type
        updateSensorData(atoi(readString.substring(0, sensorDataDelimiter).c_str()), 
        atof(readString.substring(sensorDataDelimiter + 1, readString.length()).c_str()));
      } else {
        // for debugging purpose
        Serial.println("Sensor Data Error");
      }
      // reset the sensor data
      readString = "";
    } 
  }
  // discard incomplete sensor data 
  if (readString.length() > 0) {
    readString = "";
  }
}

void setup() {
  pinMode(UART_SIGNAL_PIN, INPUT);
  pinMode(Y_AXIS_ANALOG_STICK_PIN, INPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB port only
  while (!Serial) {}
  // lcd initialization
  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  // start LCD refresh timer
  lcdRefreshTimeout = millis();
}

void loop() {
  uartReadSensorData();
  changeLCDSensorDataByJoystickMovement();
  refreshLCD();
}