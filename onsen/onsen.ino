#include <OneWire.h>
#include <LiquidCrystal.h>

const int RELAY_PIN = 2; // relay
const int BACKLIGHT_PIN = 9; // baclight enable
const int BUTTON_PIN = 10; // rotary encoder button
const int ENCODER_DIR_A_PIN = 11; // rotary encoder dieection A
const int ENCODER_DIR_B_PIN = 12; // rotary encoder dieection B

const int SLOW_REGULATION_TEMP = 60;
const int LOW_TEMP = 64;
const int TARGET_TEMP = 66;
const int PANIC_TEMP = 67.5;

const int STATE_IDLE = 0;
const int STATE_HEATUP = 1;

const int SLOW_INTERVAL = 5;

// on pin A0 (a 4.7K resistor is necessary), won't work without
OneWire ds(A0); 
LiquidCrystal lcd(3, 4, 5, 6, 7, 8);

int state = STATE_IDLE;
int interval = 10; // s

void setup(void) {
  // setup relay control, init to off
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  
  // enable backlight
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  Serial.begin(9600);
}

void loop(void) {
  float celsius = readTemperature();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(celsius);
  lcd.print(" C");
  lcd.setCursor(0, 1);

  if (state == STATE_IDLE) {
    if (celsius < LOW_TEMP) {
        state = STATE_HEATUP;
        interval = 15;
    }
  }
  
  if (state == STATE_HEATUP) {   
    if (celsius < LOW_TEMP) {
      digitalWrite(RELAY_PIN, HIGH);
      lcd.print("Heating");      
    } else {
      digitalWrite(RELAY_PIN, LOW);
      lcd.print("Cooling");
    }
    
    if (celsius > SLOW_REGULATION_TEMP)
    {
      interval = SLOW_INTERVAL;
      lcd.print(" slow");
    }
  }

  for (int i = 0; i < interval; i++) {
    delay(1000);
  }
}

float readTemperature() 
{
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  
  if (!ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return  -1;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }
  
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return -2;
  }
  
  Serial.println();
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println(" Chip = DS18S20"); // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println(" Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println(" Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return -3;
  }
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end
  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad
  
  Serial.print(" Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();
  
  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  
  float celsius = (float)raw / 16.0;
  return celsius;
}
