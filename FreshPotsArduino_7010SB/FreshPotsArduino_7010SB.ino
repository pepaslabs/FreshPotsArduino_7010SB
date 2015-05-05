// FreshPotsArduino_7010SB: An Arduino sketch which interfaces to a 7010SB digital scale and displays the number of cups of coffee left in the carafe.
// See https://github.com/pepaslabs/FreshPotsArduino_7010SB
// Copyright Jason Pepas
// Released under the terms of the MIT License.  See http://opensource.org/licenses/MIT


#include <SoftwareSerial.h>
#define RX_PIN 4
#define TX_PIN 3
SoftwareSerial mySerial(RX_PIN, TX_PIN);

#include <LiquidCrystal.h>
#define LCD_RS_PIN 12
#define LCD_EN_PIN 11
#define LCD_D4_PIN 10
#define LCD_D5_PIN 9
#define LCD_D6_PIN 8
#define LCD_D7_PIN 7
LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

#define UPDATE_CARAFE_EMPTY_WEIGHT_GRAMS 1475
#define VONSHEF_CARAFE_EMPTY_WEIGHT_GRAMS 1655
#define COFFEE_GRAMS 1890
#define FRESH_POT_THRESHOLD (1800 + 1400)
#define EMPTY_SCALE_THRESHOLD 30

// by default, my LCD prints a Yen symbol when you send it an ASCII backslash.
// so we create a custom backslash character and use that instead.
// see http://omerk.github.io/lcdchargen/
byte customChar[8] = {
	0b00000,
	0b10000,
	0b01000,
	0b00100,
	0b00010,
	0b00001,
	0b00000,
	0b00000
};

void setup()
{
  Serial.begin(2400, SERIAL_8N1);

  mySerial.begin(2400);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  lcd.createChar(0, customChar);
  lcd.begin(16,2);
}


char buffer[16 + sizeof('\0')];

#define OZ_PER_GRAM 0.0338140
#define CUP_PER_OZ 0.125

int current_consistent_reading_grams = 0;
int previous_consistent_reading_grams = 0;
int full_carafe_grams = 0;
uint32_t last_refill_millis = 0;

void loop()
{
//  debug7010SB();
//  return;

  lcd.setCursor(0,0);
  int chars_written = 0;
  
  int remaining_grams = 0;
  
  int grams = readFrom7010SB();
  if (grams > 0)
  {  
    if (latestReadingIsUsable(grams) == true)
    {
      previous_consistent_reading_grams = current_consistent_reading_grams;
      current_consistent_reading_grams = grams;

      if (didGetRefilled(previous_consistent_reading_grams, current_consistent_reading_grams) == true)
      {
        full_carafe_grams = current_consistent_reading_grams;
        last_refill_millis = millis();
      }
  
      if (current_consistent_reading_grams > EMPTY_SCALE_THRESHOLD)
      {
        // account for when the scale "settles" and drifts up by 10 grams
        if (current_consistent_reading_grams > full_carafe_grams)
        {
          full_carafe_grams = current_consistent_reading_grams;
        }
      
        int carafe_grams = full_carafe_grams - COFFEE_GRAMS;
      
        remaining_grams = current_consistent_reading_grams - carafe_grams;
        if (remaining_grams < 0)
        {
          remaining_grams = 0;
        }
  
        float oz = remaining_grams * OZ_PER_GRAM;
        float cups = oz * CUP_PER_OZ;

        chars_written += printCups(cups);
        chars_written += lcd.print(" (");
        chars_written += printOz(oz);
        chars_written += lcd.print(")");
      }
    }
  }

  int remaining_spaces = 16 - chars_written;
  for (int i=remaining_spaces; i > 0; i--)
  {
    lcd.print(" ");
  }

// --- second LCD line

  lcd.setCursor(0,1);
  chars_written = 0;
  
  chars_written += printAge(last_refill_millis);
  
  remaining_spaces = 16 - chars_written - 1;
  for (int i=remaining_spaces; i > 0; i--)
  {
    lcd.print(" ");
  }
  printNextSpinnerChar();
  
    
  // debug
  /*
  lcd.setCursor(0,1);
  chars_written = 0;
  chars_written += lcd.print("c");
  chars_written += lcd.print(current_consistent_reading_grams);
  chars_written += lcd.print("f");
  chars_written += lcd.print(full_carafe_grams);
  chars_written += lcd.print("r");
  chars_written += lcd.print(remaining_grams);

  remaining_spaces = 16 - chars_written;
  for (int i=remaining_spaces; i > 0; i--)
  {
    lcd.print(" ");
  }  
  */
}


// ---


bool didGetRefilled(int previous, int current)
{
  return (previous < 20 && current > FRESH_POT_THRESHOLD);
}


void digitalToggle(int pin)
{
  int val = digitalRead(pin);
  if (val == 0)
  {
    digitalWrite(pin, HIGH);
  }
  else
  {
    digitalWrite(pin, LOW);
  }
}


// --- spinner


char spinner_chars[5] = "-\\|/";
int next_spinner_char_index = 0;

void printNextSpinnerChar()
{
  char ch = spinner_chars[next_spinner_char_index];

  if (next_spinner_char_index == 1)
  {
    lcd.write((uint8_t)0);
  }  
  else
  {
    lcd.print(ch);
  }
  
  next_spinner_char_index++;
  if (next_spinner_char_index == 4)
  {
    next_spinner_char_index = 0;
  }
}


// --- LCD printing


int printGrams(int grams)
{
  int chars_written = 0;
  snprintf(buffer, sizeof(buffer), "%i", grams);
  chars_written = lcd.print(buffer);
  chars_written += lcd.print(" grams");
  return chars_written;
}

int printOz(float oz)
{
  int chars_written = 0;
  chars_written = lcd.print(oz, 0);
  chars_written += lcd.print(" oz");
  return chars_written;
}

int printCups(float cups)
{
  int chars_written = 0;
  chars_written = lcd.print(cups, 1);
  if (cups > 1.0)
  {
    chars_written += lcd.print(" cups");
  }
  else
  {
    chars_written += lcd.print(" cup");
  }
  return chars_written;
}

int printAge(uint32_t last_reset_millis)
{
  int chars_written = 0;
  
  uint32_t age = (millis() - last_reset_millis) / 1000;
  
  if (age < 60)
  {
    chars_written += lcd.print("<1 minute old");
  }
  else if (age < 3600)
  {
    int minutes = age / 60;
    chars_written += lcd.print(minutes);
    if (minutes == 1)
    {
      chars_written += lcd.print(" minute old");
    }
    else
    {
      chars_written += lcd.print(" minutes old");
    }
  }
  else if (age < 86400)
  {
    float hours = age / 3600.0;
    if (hours <= 1.05)
    {
      chars_written += lcd.print(hours, 0);
      chars_written += lcd.print(" hour old");
    }
    if (hours < 10)
    {
      chars_written += lcd.print(hours, 1);
      chars_written += lcd.print(" hours old");
    }
    else
    {
      chars_written += lcd.print(hours, 0);
      chars_written += lcd.print(" hours old");
    }
  }
  else
  {
    float days = age / 86400.0;
    if (days <= 1.05)
    {
      chars_written += lcd.print(days, 0);
      chars_written += lcd.print(" day old");
    }
    if (days < 10)
    {
      chars_written += lcd.print(days, 1);
      chars_written += lcd.print(" days old");
    }
    else
    {
      chars_written += lcd.print(days, 0);
      chars_written += lcd.print(" days old");
    }
  }
  
  return chars_written;
}


// --- serial comm


uint8_t readSerialChar()
{
  return readSoftwareSerialChar();
}

uint8_t readSoftwareSerialChar()
{
  while (mySerial.available() == 0)
  {
    continue;
  }

  uint8_t ch = mySerial.read();
  return ch;
}

int readFrom7010SB()
{
  uint8_t ch;

  ch = readSerialChar();
  if (ch != 0x02)
  {
    return -1;
  }

  ch = readSerialChar();
  if (ch != 0xC0)
  {
    return -2;
  }

  ch = readSerialChar();
  if (ch != 0x80)
  {
    return -3;
  }

  ch = readSerialChar();
  if (ch != 0x80)
  {
    return -4;
  }

  char digits[5];
  digits[0] = readSerialChar();
  digits[1] = readSerialChar();
  digits[2] = readSerialChar();
  digits[3] = readSerialChar();
  digits[4] = readSerialChar();
  digits[5] = '\0';

  ch = readSerialChar();
  if (ch != 0x0D)
  {
    return -5;
  }

  int grams = atoi(digits);
  return grams;
}


int debug7010SB()
{
  uint8_t ch;
  int chars_written = 0;

  ch = readSerialChar();
  if (ch != 0x02)
  {
    return -1;
  }
  
  lcd.setCursor(0,0);
  chars_written += lcd.write(ch);

  while(true)
  {
    ch = readSerialChar();
    chars_written += lcd.write(ch);
    if (ch == 0x0D)
    {
      break;
    }
  }

  int remaining_spaces = 16 - chars_written;
  for (int i=remaining_spaces; i > 0; i--)
  {
    lcd.print(" ");
  }
}


// --- consistency engine


int num_identical_readings = 0;
int required_num_consistent_readings = 4;
int previous_reading_grams = 0;
int consistency_threshold_grams = 10;

bool latestReadingIsUsable(int grams)
{
  updateConsistencyCount(grams);
  return isConsistent();
}

bool isConsistent()
{
  return num_identical_readings >= required_num_consistent_readings;
}

void updateConsistencyCount(int grams)
{
  int difference = abs(grams - previous_reading_grams);
  if (difference > consistency_threshold_grams)
  {
    // inconsistent
    num_identical_readings = 0;
    previous_reading_grams = grams;
  }
  else
  {
    if (num_identical_readings < required_num_consistent_readings)
    {
      num_identical_readings++;
    }
  }
}

