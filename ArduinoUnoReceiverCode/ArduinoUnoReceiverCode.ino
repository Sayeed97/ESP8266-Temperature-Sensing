#include <LiquidCrystal_I2C.h>

#define TEMPERATURE 0
#define HUMIDITY 1
#define HEAT_INDEX 2
#define UART_SIGNAL_PIN 2

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,20,4); 

float tempData, humData, heatIndexData;
char sensorDatabuff[20];
int floatFirstPart = 0;
int floatSecondPart = 0;

unsigned long lcdRefreshTimeout = 0;

void splitFloatValue(float value) {
  floatFirstPart = value; // truncate anything after the decimal
  floatSecondPart = value * 1000; 
  floatSecondPart = floatSecondPart - floatFirstPart * 1000; // we will get the decimal part alone
}

void refreshSensorDataOnLCD(float sensorValue) {
  lcd.clear();
  lcd.setCursor(0, 0);
  splitFloatValue(sensorValue);
  sprintf(sensorDatabuff, "Temp: %d.%d", floatFirstPart, floatSecondPart);
  lcd.print(sensorDatabuff);
}

// refreshes LCD every 1 second
void refreshLCD() {
  if(millis() - lcdRefreshTimeout >= 1000) {
    refreshSensorDataOnLCD(tempData);
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
      if (sensorDataDelimiter > 0) {
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
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB port only
  while (!Serial) {}
  // lcd initialization
  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  // start LCD refresh timer
  unsigned long lcdRefreshTimeout = millis();
}

void loop() {
  uartReadSensorData();
  refreshLCD();
}