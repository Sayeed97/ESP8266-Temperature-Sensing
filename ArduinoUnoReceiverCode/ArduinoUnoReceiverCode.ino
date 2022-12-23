#include <LiquidCrystal_I2C.h>

#define TEMPERATURE 0
#define HUMIDITY 1
#define HEAT_INDEX 2
#define UART_SIGNAL_PIN 2
#define Y_AXIS_ANALOG_STICK_PIN 15 // A2 pin on arduino

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,20,4); 

// LCD display that are shared between multiple methods
float tempData, humData, heatIndexData;
int floatFirstPart = 0;
int floatSecondPart = 0;
int sensorDataType = TEMPERATURE;
unsigned long lcdRefreshTimeout = 0;

// Analog Joystick states 
int yPosition = 0;
int yAxisMap = 0;
unsigned long joystickStateUpdateTimeout = millis();

// updates sensorDataType based on joystick movement every 800 ms
void senseJoyStickChange() {
  yAxisMap = map(analogRead(Y_AXIS_ANALOG_STICK_PIN), 0, 1023, 512, -512);
  if (yAxisMap >= -350 && yAxisMap <= 350) {
    return;
  } else if (yAxisMap > 350 && millis() - joystickStateUpdateTimeout > 800) {
    sensorDataType = sensorDataType < 2 ? (sensorDataType + 1) : 0;
    joystickStateUpdateTimeout = millis();
  } else if (yAxisMap < -350 && millis() - joystickStateUpdateTimeout > 800) {
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

void refreshSensorDataOnLCD() {
  char sensorDataBuff[20];
  lcd.clear();
  // we display sensor value based on the joystick state motion
  splitFloatValue();
  // Custom string building from float value as the LCD is no able to display float values correctly
  sprintf(sensorDataBuff, sensorDataType == TEMPERATURE ? "TEMP: %d.%d" : sensorDataType == HUMIDITY ? 
  "HUMI: %d.%d" : "HEAT: %d.%d", floatFirstPart, floatSecondPart);
  lcd.setCursor(0, 0);
  lcd.print(sensorDataBuff);
}

// refreshes LCD every 1 second
void refreshLCD() {
  if(millis() - lcdRefreshTimeout >= 1000) {
    refreshSensorDataOnLCD();
    lcdRefreshTimeout = millis();
  }
}

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
  senseJoyStickChange();
  refreshLCD();
}