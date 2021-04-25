#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

#define CS_PIN 10
#define BAUD_RATE 9600
#define LENGTH_FILE "length.txt"
#define VOUCHERS_FILE "vouchers.txt"
#define VOUCHER_FILE "voucher.txt"
#define COUNT_FILE "count.txt"
#define TIMER_FILE "timer.txt"
#define COIN_FILE "coin.txt"

#define COIN_INTERVAL 150
#define COIN_PIN 2
#define SIGNAL_PIN 3
#define LED_PIN 4
#define BUTTON_PIN 5

#define VERSION "1.0"

LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t _length = 8;
uint8_t _count = 5;
uint8_t _format = 0;
uint8_t _timer = 30;
String _nextVoucher = "";
word _nextLine = 0;
bool _nextVoucherResult = false;
bool _verifyVouchersResult = false;

uint8_t _coin = 0;
uint8_t _pulse = 0;
unsigned long _previousMillis = 0;
unsigned long _timerMillis = 0;
bool _inserted = false;
bool _startTimer = false;
bool _insertCoin = false;
uint8_t _currentTimer = 0;
String _err = "";

uint8_t _buttonState;
uint8_t _lastButtonState = LOW;
unsigned long _lastDebounceTime = 0;
unsigned long _debounceDelay = 50;

void setup()
{
  pinMode(BUTTON_PIN, INPUT);
  pinMode(SIGNAL_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(COIN_PIN, INPUT_PULLUP);
  digitalWrite(SIGNAL_PIN, LOW);

  Serial.begin(BAUD_RATE);
  Serial.print(F("Voucher Generator v"));
  Serial.println(VERSION);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("VoucherGenerator"));
  lcd.setCursor(0, 1);
  lcd.print(F("Version "));
  lcd.print(VERSION);
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Initializing,"));
  lcd.setCursor(0, 1);
  lcd.print(F("Please wait..."));

  Serial.print(F("Initializing SD card module... "));

  if (!SD.begin(CS_PIN))
  {
    lcd.clear();
    lcd.print(F("ERR-100"));
    Serial.println(F("\nError: Cannot read SD card or SD card module."));
    return;
  }
  Serial.println(F("SUCCESS!"));

  _length = setConfig(LENGTH_FILE, _length);
  _count = setConfig(COUNT_FILE, _count);
  _timer = setConfig(TIMER_FILE, _timer);
  _coin = setConfig(COIN_FILE, _coin);
  _nextVoucher = getVoucher(VOUCHER_FILE);
  displayConfig();

  Serial.println(_nextVoucher);

  if (_nextVoucher != "") {
    lcd.clear();
    lcd.setCursor(16 / 2 - _length / 2, 0);
    lcd.print(_nextVoucher);
    _nextVoucher = "";
    _currentTimer = _timer;
    _startTimer = true;
    _nextVoucherResult = true;
  } else {
    _verifyVouchersResult = verifyVouchers(VOUCHERS_FILE);
    if (!_verifyVouchersResult) return;
  }

  Serial.println(F("\nREADY!\n"));
}

void loop()
{
  unsigned long milliseconds = millis();
  uint8_t buttonReading = digitalRead(BUTTON_PIN);

  if (buttonReading != _lastButtonState) {
    _lastDebounceTime = milliseconds;
  }

  if ((milliseconds - _lastDebounceTime) > _debounceDelay) {
    if (buttonReading != _buttonState) {
      _buttonState = buttonReading;
      if (_buttonState == HIGH) {

        if (_startTimer) {
          _startTimer = false;
          _currentTimer = 0;
          setVoucher(VOUCHER_FILE, "");
          if (!_nextVoucherResult) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(F("ERR-"));
            lcd.print(_err);
            return;
          }
          _verifyVouchersResult = verifyVouchers(VOUCHERS_FILE);
          return;
        }

        if (_verifyVouchersResult) {
          _verifyVouchersResult = false;
          attachInterrupt(digitalPinToInterrupt(COIN_PIN), isr, FALLING);
          digitalWrite(SIGNAL_PIN, HIGH);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("Insert "));
          lcd.print(_count - _coin);
          lcd.print(F(" Peso"));
          digitalWrite(LED_PIN, HIGH);
          _currentTimer = _timer;
          _insertCoin = true;
        }
      }
    }
  }

  if (milliseconds - _timerMillis >= 1000) {
    _timerMillis = milliseconds;
    if (_startTimer) {
      uint8_t index = 16 / 2 - (_timer > 99 ? 2 : _timer > 9 ? 1 : 0);
      if (_timer > 99 && _currentTimer == 99) {
        lcd.setCursor(index + 2, 1);
        lcd.print(" ");
      }
      if (_timer > 9 && _currentTimer == 9) {
        lcd.setCursor(index + 1, 1);
        lcd.print(" ");
      }

      lcd.setCursor(index, 1);
      lcd.print(_currentTimer);

      if (_currentTimer == 0) {
        _startTimer = false;
        setVoucher(VOUCHER_FILE, "");
        if (!_nextVoucherResult) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("ERR-"));
          lcd.print(_err);
          return;
        }
        _verifyVouchersResult = verifyVouchers(VOUCHERS_FILE);
      }
      _currentTimer--;
    }

    if (_insertCoin) {
      if (_currentTimer == 0) {
        _insertCoin = false;
        digitalWrite(SIGNAL_PIN, LOW);
        detachInterrupt(digitalPinToInterrupt(COIN_PIN));
        digitalWrite(LED_PIN, LOW);
        _verifyVouchersResult = verifyVouchers(VOUCHERS_FILE);
      }
      _currentTimer--;
    }

  }

  if (milliseconds - _previousMillis >= COIN_INTERVAL)
  {
    _previousMillis = milliseconds;

    if (_inserted && _pulse > 0)
    {
      _coin += _pulse;
      if (_coin >= _count)
      {
        _insertCoin = false;
        _coin -= _count;
        setCoin(COIN_FILE, _coin);
        setVoucher(VOUCHER_FILE, _nextVoucher);
        lcd.clear();
        lcd.setCursor(16 / 2 - _length / 2, 0);
        lcd.print(_nextVoucher);
        _nextVoucher = "";
        _currentTimer = _timer;
        _startTimer = true;
        _nextVoucherResult = initNextVoucher(VOUCHERS_FILE);
        digitalWrite(SIGNAL_PIN, LOW);
        detachInterrupt(digitalPinToInterrupt(COIN_PIN));
        digitalWrite(LED_PIN, LOW);
      }
      else
      {
        setCoin(COIN_FILE, _coin);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("Insert "));
        lcd.print(_count - _coin);
        lcd.print(F(" Peso"));
        _currentTimer = _timer;

      }

      _pulse = 0;
    }

    _inserted = false;
  }

  _lastButtonState = buttonReading;
}

uint8_t setConfig(String f, uint8_t data)
{
  uint8_t value = data;
  File file = SD.open(f, FILE_READ);
  if (file)
  {
    Serial.print(F("Reading "));
    Serial.print(f);
    Serial.print(F(" file... "));

    String text = "";
    while (file.available())
    {
      char x = file.read();
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

  file.close();
  return value;
}

String getVoucher(String f) {
  File file = SD.open(f, FILE_READ);
  if (file) {
    String text = "";
    while (file.available())
    {
      char x = file.read();
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
    if (text.length() == _length) {
      file.close();
      return text;
    }
  }
  file.close();
  return "";
}

void setVoucher(String f, String voucher) {
  File file = SD.open(f, O_TRUNC | O_CREAT | O_WRITE);
  if (file) {
    file.print(voucher);
  }
  file.close();
}

void displayConfig()
{
  Serial.println(F("\nCONFIGURATION"));
  Serial.print(F("Voucher length: "));
  Serial.println(_length);
  Serial.print(F("Coin count: "));
  Serial.println(_count);
  Serial.print(F("Waiting time: "));
  Serial.println(_timer);
  if (_coin > 0) {
    Serial.print(F("Previous coin: "));
    Serial.println(_coin);
  }
  if (_nextVoucher != "") {
    Serial.print(F("Previous voucher: "));
    Serial.println(_nextVoucher);
  }
  Serial.println();
}

bool verifyVouchers(String f)
{
  lcd.clear();
  lcd.setCursor(0,0);
  if (!SD.exists(f))
  {
    lcd.print(F("ERR-102"));
    Serial.println(F("Error: File not found."));
    return false;
  }

  File file = SD.open(f, FILE_READ);

  if (!file)
  {
    lcd.print(F("ERR-103"));
    Serial.println(F("Error: File failed to open."));
    file.close();
    return false;
  }
  else
  {    
    Serial.print(F("Reading "));
    Serial.print(f);
    Serial.print(F(" file... "));
    lcd.print(F("Reading vouchers"));
    lcd.setCursor(0,1);
    lcd.print(F("Please wait..."));

    String data = "";
    word used = 0;
    word ave = 0;
    word line = 0;

    while (file.available())
    {
      char x = file.read();
      if (x != '\n')
      {
        data += x;
      }
      else
      {
        line++;
        if (data == "")
        {
          lcd.print(F("ERR-104"));
          Serial.println(F("\nError: Found empty line data."));
          file.close();
          return false;
        }
        else
        {
          uint8_t index = data.indexOf(',');
          if (index == -1)
          {
            lcd.print(F("ERR-105"));
            Serial.println(F("\nError: Found invalid data format. Comma is not found."));
            file.close();
            return false;
          }
          else
          {
            String text = data.substring(0, index);
            if (text.length() != _length)
            {
              lcd.print(F("ERR-106"));
              Serial.println(F("\nError: Found invalid voucher format length."));
              file.close();
              return false;
            }
            else
            {
              String stat = data.substring(index + 1);
              if (stat.length() > 1)
              {
                lcd.print(F("ERR-107"));
                Serial.println(F("\nError: Found invalid data format."));
                file.close();
                return false;
              }
              else
              {
                if (stat == "0")
                {
                  ave++;
                  if (_nextVoucher == "")
                  {
                    _nextVoucher = text;
                    _nextLine = line;
                  }
                }
                else
                {
                  used++;
                }
              }
            }
          }
          data = "";
        }
      }
    }

    if (data != "") {

      uint8_t index = data.indexOf(',');
      if (index != -1)
      {
        String text = data.substring(0, index);
        if (text.length() == _length)
        {
          String stat = data.substring(index + 1);
          if (stat.length() == 1)
          {
            line++;
            if (stat == "0")
            {
              ave++;
              if (_nextVoucher == "")
              {
                _nextVoucher = text;
                _nextLine = line;
              }
            }
            else
            {
              used++;
            }
          }
        }
      }
      data = "";

    }

    Serial.println(F("SUCCESS!"));

    lcd.clear();
    lcd.setCursor(0, 0);
    if (ave == 0) {
      lcd.print(F("ERR-108"));
      Serial.println(F("\nError: No available vouchers found."));
      file.close();
      return false;
    }

    lcd.print(F("Press the Button"));
    if (_coin > 0) {
      lcd.setCursor(0, 1);
      lcd.print(F("Balance: "));
      lcd.print(_coin);
    }
  }

  file.close();
  return true;
}

bool initNextVoucher(String f)
{
  if (!SD.exists(f))
  {
    _err = F("102");
    Serial.print(F("Error: File not found."));
    return false;
  }

  File file = SD.open(f, O_WRITE);
  if (!file)
  {
    _err = F("103");
    Serial.println(F("Error: File failed to open."));
    file.close();
    return false;
  }
  else
  {
    int pos = _length * _nextLine + _nextLine * 3 - 2;
    file.seek(pos);
    file.write('1');
  }

  file.close();
  return true;
}

void setCoin(String f, uint8_t coin) {

  File file = SD.open(f, O_TRUNC | O_CREAT | O_WRITE);
  if (file) {
    file.print(coin);
  }
  file.close();
}

void isr()
{
  delay(1);
  if (digitalRead(COIN_PIN) == LOW)
  {
    _pulse++;
    _inserted = true;
  }
  else
  {
    _pulse = 0;
  }
}
