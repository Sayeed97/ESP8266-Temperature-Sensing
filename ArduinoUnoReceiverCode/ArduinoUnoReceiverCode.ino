#include <LiquidCrystal_I2C.h>

#define SENSOR_ERROR -1
#define TEMPERATURE 0
#define HUMIDITY 1
#define HEAT_INDEX 2
#define UART_SIGNAL_PIN 2

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,20,4); 

String readString;
char c;
int sensorDataDelimiter = -1; // indicates no index

void displaySensorData() {
  while(digitalRead(UART_SIGNAL_PIN) == HIGH && Serial.available()) {
    delay(5); // delay to allow byte to arrive in input buffer
    c = Serial.read();
    readString += c;
    if (readString.length() > 0 && c == '\n') {
      sensorDataDelimiter = readString.indexOf(':');
      if (sensorDataDelimiter > 0) {
        Serial.println(readString);
      } else {
        Serial.println("Sensor Data Error");
      }
      readString = "";
    } 
  } 
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
  lcd.setCursor(1,0);
  lcd.print("Reading Sensor..");
  delay(1000);
}

void loop() {
  displaySensorData();
}