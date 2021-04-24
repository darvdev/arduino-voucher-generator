#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

#define cspin 4
#define baud 9600
#define lengthFile "length.txt"
#define voucherFile "vouchers.txt"
#define countFile "count.txt"
#define timeFile "time.txt"
#define coinFile "coin.txt"

#define interval 150
#define coinpin 2
#define signalpin 3
#define ledpin 5
#define buttonpin 6

#define version "1.0"

LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t _length = 8;
uint8_t _count = 5;
uint8_t _format = 0;
uint8_t _time = 30;
String nextVoucher = "";
word nextLine = 0;
bool nextVoucherResult = false;
bool verifyVoucherResult = false;

uint8_t _coin = 0;
uint8_t _pulse = 0;
unsigned long _previousMillis = 0;
unsigned long _timerMillis = 0;
bool _inserted = false;
bool startTimer = false;
uint8_t timer = 0;
String err = "";

int ledState = LOW;
int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup()
{
  pinMode(buttonpin, INPUT);
  pinMode(signalpin, OUTPUT);
  pinMode(ledpin, OUTPUT);
  pinMode(coinpin, INPUT_PULLUP);
  digitalWrite(signalpin, LOW);

  Serial.begin(baud);
  Serial.print(F("Voucher Generator v"));
  Serial.println(version);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("VoucherGenerator"));
  lcd.setCursor(0, 1);
  lcd.print(F("Version "));
  lcd.print(version);
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Initializing,"));
  lcd.setCursor(0, 1);
  lcd.print(F("Please wait..."));

  Serial.print(F("Initializing SD card module... "));

  if (!SD.begin(cspin))
  {
    lcd.clear();
    lcd.print(F("ERR-100"));
    Serial.println(F("\nError: Cannot read SD card or SD card module."));
    return;
  }
  Serial.println(F("SUCCESS!"));

  _length = setConfig(lengthFile, _length);
  _count = setConfig(countFile, _count);
  _time = setConfig(timeFile, _time);
  _coin = setConfig(coinFile, _coin);
  displayConfig();

  verifyVoucherResult = verifyVouchers(voucherFile);
  if (!verifyVoucherResult) return;

  Serial.println(F("\nREADY!\n"));
}

void loop()
{
  unsigned long _millis = millis();
  uint8_t buttonReading = digitalRead(buttonpin);

  if (buttonReading != lastButtonState) {
    lastDebounceTime = _millis;
  }

  if ((_millis - lastDebounceTime) > debounceDelay) {
    if (buttonReading != buttonState) {
      buttonState = buttonReading;
      if (buttonState == HIGH) {

        if (startTimer) {
          startTimer = false;
          timer = 0;
          if (!nextVoucherResult) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(F("ERR-"));
            lcd.print(err);
            return;
          }
          verifyVoucherResult = verifyVouchers(voucherFile);
          return;
        }

        if (verifyVoucherResult) {
          verifyVoucherResult = false;
          attachInterrupt(digitalPinToInterrupt(coinpin), isr, FALLING);
          digitalWrite(signalpin, HIGH);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("Insert "));
          lcd.print(_count - _coin);
          lcd.print(F(" Peso"));
          digitalWrite(ledpin, HIGH);
        }
      }
    }
  }




  if (_millis - _timerMillis >= 1000) {
    _timerMillis = _millis;
    if (startTimer) {

      uint8_t index = 16 / 2 - (_time > 99 ? 2 : _time > 9 ? 1 : 0);
      if (timer > 99 && timer == 99) {
        lcd.setCursor(index + 2, 1);
        lcd.print(" ");
      }
      if (_time > 9 && timer == 9) {
        lcd.setCursor(index + 1, 1);
        lcd.print(" ");
      }

      lcd.setCursor(index, 1);
      lcd.print(timer);

      if (timer == 0) {
        startTimer = false;
        timer = 0;
        if (!nextVoucherResult) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("ERR-"));
          lcd.print(err);
          return;
        }
        verifyVoucherResult = verifyVouchers(voucherFile);
      }
      timer--;
    }
  }

  if (_millis - _previousMillis >= interval)
  {
    _previousMillis = _millis;

    if (_inserted && _pulse > 0)
    {
      _coin += _pulse;
      if (_coin >= _count)
      {
        _coin -= _count;
        setCoin(coinFile, _coin);
        lcd.clear();
        lcd.setCursor(16 / 2 - _length / 2, 0);
        lcd.print(nextVoucher);
        nextVoucher = "";
        timer = _time;
        startTimer = true;
        nextVoucherResult = initNextVoucher(voucherFile);
        digitalWrite(signalpin, LOW);
        detachInterrupt(digitalPinToInterrupt(coinpin));
        digitalWrite(ledpin, LOW);
      }
      else
      {
        setCoin(coinFile, _coin);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("Insert "));
        lcd.print(_count - _coin);
        lcd.print(F(" Peso"));
      }

      _pulse = 0;
    }

    _inserted = false;
  }

  lastButtonState = buttonReading;
}

uint8_t setConfig(String file, uint8_t data)
{
  uint8_t value = data;
  File _file = SD.open(file, FILE_READ);
  if (_file)
  {
    Serial.print(F("Reading "));
    Serial.print(file);
    Serial.print(F(" file... "));

    String text = "";
    while (_file.available())
    {
      char x = _file.read();
      if (x == '\n')
      {
        break;
      }
      else
      {
        text += x;
      }
    }

    text.trim();
    if (text != "")
    {
      uint8_t convert = text.toInt();
      if (convert > 0)
      {
        value = convert;
      }
      Serial.println(F("SUCCESS!"));
    }
  }

  _file.close();
  return value;
}

void displayConfig()
{
  Serial.println(F("\nCONFIGURATION"));
  Serial.print(F("Voucher length: "));
  Serial.println(_length);
  Serial.print(F("Coin count: "));
  Serial.println(_count);
  Serial.print(F("Waiting time: "));
  Serial.println(_time);
  Serial.print(F("Previous coin: "));
  Serial.println(_coin);
  Serial.println();
}

bool verifyVouchers(String file)
{
  lcd.clear();
  if (!SD.exists(file))
  {
    lcd.print(F("ERR-102"));
    Serial.println(F("Error: File not found."));
    return false;
  }

  File _file = SD.open(file, FILE_READ);

  if (!_file)
  {
    lcd.print(F("ERR-103"));
    Serial.println(F("Error: File failed to open."));
    _file.close();
    return false;
  }
  else
  {

    Serial.print(F("Reading "));
    Serial.print(file);
    Serial.print(F(" file... "));

    String _data = "";
    word _used = 0;
    word _available = 0;
    word _line = 0;

    while (_file.available())
    {
      char x = _file.read();
      if (x != '\n')
      {
        _data += x;
      }
      else
      {
        _line++;
        if (_data == "")
        {
          lcd.print(F("ERR-104"));
          Serial.println(F("\nError: Found empty line data."));
          _file.close();
          return false;
        }
        else
        {
          uint8_t index = _data.indexOf(',');
          if (index == -1)
          {
            lcd.print(F("ERR-105"));
            Serial.println(F("\nError: Found invalid data format. Comma is not found."));
            _file.close();
            return false;
          }
          else
          {
            String _text = _data.substring(0, index);
            if (_text.length() != _length)
            {
              lcd.print(F("ERR-106"));
              Serial.println(F("\nError: Found invalid voucher format length."));
              _file.close();
              return false;
            }
            else
            {
              String _status = _data.substring(index + 1);
              if (_status.length() > 1)
              {
                lcd.print(F("ERR-107"));
                Serial.println(F("\nError: Found invalid data format."));
                _file.close();
                return false;
              }
              else
              {
                if (_status == "0")
                {
                  _available++;
                  if (nextVoucher == "")
                  {
                    nextVoucher = _text;
                    nextLine = _line;
                  }
                }
                else
                {
                  _used++;
                }
              }
            }
          }
          _data = "";
        }
      }
    }

    if (_data != "") {

      uint8_t index = _data.indexOf(',');
      if (index != -1)
      {
        String _text = _data.substring(0, index);
        if (_text.length() == _length)
        {
          String _status = _data.substring(index + 1);
          if (_status.length() == 1)
          {
            _line++;
            if (_status == "0")
            {
              _available++;
              if (nextVoucher == "")
              {
                nextVoucher = _text;
                nextLine = _line;
              }
            }
            else
            {
              _used++;
            }
          }
        }
      }
      _data = "";

    }

    Serial.println(F("SUCCESS!"));

    if (_available == 0) {
      lcd.print(F("ERR-108"));
      Serial.println(F("\nError: No available vouchers found."));
      _file.close();
      return false;
    }

    lcd.setCursor(0, 0);
    lcd.print(F("Press the Button"));
  }

  _file.close();
  return true;
}

bool initNextVoucher(String file)
{
  if (!SD.exists(file))
  {
    err = F("102");
    Serial.print(F("Error: File not found."));
    return false;
  }

  File _file = SD.open(file, O_WRITE);
  if (!_file)
  {
    err = F("103");
    Serial.println(F("Error: File failed to open."));
    _file.close();
    return false;
  }
  else
  {
    int pos = _length * nextLine + nextLine * 3 - 2;
    _file.seek(pos);
    _file.write('1');
  }

  _file.close();
  return true;
}

void setCoin(String file, uint8_t coin) {

  File _file = SD.open(file, O_TRUNC | O_CREAT | O_WRITE);
  if (_file) {
    _file.print(coin);
  }
  _file.close();
}

void isr()
{
  delay(1);
  if (digitalRead(coinpin) == LOW)
  {
    _pulse++;
    _inserted = true;
  }
  else
  {
    _pulse = 0;
  }
}
