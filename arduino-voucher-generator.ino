#include <SPI.h>
#include <SD.h>

#define cspin 4
#define baud 9600
#define lengthFile "length.txt"
#define voucherFile "vouchers.txt"
#define countFile "count.txt"
#define timeFile "time.txt"
#define coinFile "coin.txt"
#define debugFile "debug.txt"

#define interval 150
#define coinpin 2
#define signalpin 3

uint8_t _length = 8;
uint8_t _count = 5;
uint8_t _format = 0;
uint8_t _time = 30;
uint8_t _debug = 0;
String nextVoucher = "";
word nextLine = 0;
String err = "000";

uint8_t _coin = 0;
uint8_t _pulse = 0;
unsigned long _previousMillis = 0;
bool _inserted = false;

void setup()
{

  pinMode(signalpin, OUTPUT);
  digitalWrite(signalpin, LOW);

  Serial.begin(baud);
  Serial.print("Initializing SD card... ");

  if (!SD.begin(cspin))
  {
    err = "100";
    Serial.println("\nError: ERR-100 Cannot read SD card or module.");
    return;
  }
  Serial.println("SUCCESS!");

  _length = setConfig(lengthFile, _length);
  _count = setConfig(countFile, _count);
  _time = setConfig(timeFile, _time);
  _coin = setConfig(coinFile, _coin);
  _debug = setConfig(debugFile, _debug);
  displayConfig();
  if (!verifyVouchers(voucherFile)) return;

  pinMode(coinpin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(coinpin), isr, FALLING);
  digitalWrite(signalpin, HIGH);
}

void loop()
{

  unsigned long _millis = millis();

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
        Serial.print("Voucher: ");
        Serial.println(nextVoucher);
        Serial.print("Line: ");
        Serial.println(nextLine);
        Serial.print("Coin: ");
        Serial.println(_coin);
        Serial.println();
        nextVoucher = "";
        if (!initNextVoucher(voucherFile)) {
          digitalWrite(signalpin, LOW);
          return;
        }
        if (!verifyVouchers(voucherFile)) {
          digitalWrite(signalpin, LOW);
          return;
        }
      }
      else
      {
        setCoin(coinFile, _coin);
        Serial.print("Insert ");
        Serial.print(_count - _coin);
        Serial.println(" peso coin");
      }

      _pulse = 0;
    }

    _inserted = false;
  }
}

uint8_t setConfig(String file, uint8_t data)
{
  uint8_t value = data;

  if (!SD.exists(file))
  {
    Serial.println("File not found.");
  }
  else
  {

    File _file = SD.open(file, FILE_READ);

    if (!_file)
    {
      Serial.println("File failed to open.");
    }
    else
    {

      Serial.print("Reading ");
      Serial.print(file);
      Serial.print(" file... ");

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

      if (text == "")
      {
        Serial.println("\nFile is empty.");
      }
      else
      {
        uint8_t convert = text.toInt();
        if (convert == 0)
        {
          Serial.println("\nData format invalid.");
        }
        else
        {
          value = convert;
          Serial.println("SUCCESS!");
        }
      }
    }

    _file.close();
  }

  return value;
}

void displayConfig()
{
  Serial.println("\nCONFIGURATION");
  Serial.print("Voucher length: ");
  Serial.println(_length);
  Serial.print("Coin count: ");
  Serial.println(_count);
  Serial.print("Waiting time: ");
  Serial.println(_time);
  Serial.print("Previous coin: ");
  Serial.println(_coin);
  Serial.print("Debug mode: ");
  Serial.println(_debug);
  Serial.println();
}

bool verifyVouchers(String file)
{

  if (!SD.exists(file))
  {
    err = "102";
    Serial.println("Error: File not found.");
    return false;
  }

  File _file = SD.open(file, FILE_READ);

  if (!_file)
  {
    err = "103";
    Serial.println("Error: File failed to open.");
    _file.close();
    return false;
  }
  else
  {

    Serial.print("Reading ");
    Serial.print(file);
    Serial.print(" file... ");

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
          err = "104";
          Serial.println("\nError: Found empty line data.");
          _file.close();
          return false;
        }
        else
        {

          uint8_t index = _data.indexOf(',');
          if (index == -1)
          {
            err = "105";
            Serial.println("\nError: Found invalid data format. Comma is not found.");
            _file.close();
            return false;
          }
          else
          {
            String _text = _data.substring(0, index);

            if (_text.length() != _length)
            {
              err = "106";
              Serial.println("\nError: Found invalid voucher format length.");
              _file.close();
              return false;
            }
            else
            {
              String _status = _data.substring(index + 1);

              if (_status.length() > 1)
              {
                err = "107";
                Serial.println("\nError: Found invalid data format.");
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

    //    if (_line == nextLine && nextVoucher == "") {
    //      err = "108";
    //      Serial.println("\nError: Empty vouchers.");
    //      return false;
    //    }

    Serial.println("SUCCESS!");
    Serial.print("Available: ");
    Serial.println(_available);
    Serial.print("Used: ");
    Serial.println(_used);
    Serial.print("Total: ");
    Serial.println(_line);

    if (_available == 0) {
      err = "108";
      Serial.println("\nError: No available vouchers found.");
      _file.close();
      return false;
    }

    Serial.print("Next voucher: ");
    Serial.println(nextVoucher);
    Serial.print("Next line: ");
    Serial.println(nextLine);
    Serial.print("\nInsert ");
    Serial.print(_count - _coin);
    Serial.println(" peso coin");
  }

  _file.close();
  return true;
}

bool initNextVoucher(String file)
{
  if (!SD.exists(file))
  {
    err = "102";
    Serial.print("Error: File not found.");
    return false;
  }

  File _file = SD.open(file, O_WRITE);

  if (!_file)
  {
    err = "103";
    Serial.println("Error: File failed to open.");
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
