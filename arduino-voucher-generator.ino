#include <SPI.h>
#include <SD.h>

#define cspin 4
#define baud 9600
#define lengthFile "length.txt"
#define voucherFile "vouchers.txt"
#define countFile "count.txt"
#define debugFile "debug.txt"

#define interval 150
#define coinpin 2
#define signal 3

uint8_t _length = 8;
uint8_t _count = 5;
uint8_t _format = 0;
uint8_t _debug = 0;
String nextVoucher = "";
word nextLine = 0;

uint8_t _coin = 0;
uint8_t _pulse = 0;
unsigned long _previousMillis = 0;
bool _inserted = false;

void debug(String text1 = "", String text2 = "", String text3 = "", bool show = true);

void setup()
{

  pinMode(signal, OUTPUT);
  digitalWrite(signal, LOW);

  Serial.begin(baud);
  Serial.print("Initializing SD card... ");

  if (!SD.begin(cspin))
  {
    Serial.println();
    Serial.println("Error: Please check the SD card module connection and try again.");
    return;
  }
  Serial.println("SUCCESS!");

  _length = setConfig(lengthFile, _length);
  _count = setConfig(countFile, _count);
  _debug = setConfig(debugFile, _debug);
  displayConfig();
  bool result = verifyVouchers(voucherFile);

  if (!result)
    return;

  pinMode(coinpin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(coinpin), isr, FALLING);
  digitalWrite(signal, 1);

  debug(String('\n'), "READY", "", _debug);
  debug("Please insert ", String(_count), " peso coin", _debug);
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

        debug("Voucher: ", String(nextVoucher), "", _debug);
        debug("Line: ", String(nextLine), "", _debug);
        debug("Coin: ", String(_coin), "", _debug);
        nextVoucher = "";
        initNextVoucher(voucherFile);
        verifyVouchers(voucherFile);
      }
      else
      {
        debug("Insert another ", String(_count - _coin), " peso coin to show the voucher", _debug);
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
    debug(file, " file not found. Default value will use.");
  }
  else
  {

    File _file = SD.open(file, FILE_READ);

    if (!_file)
    {
      debug(file, " file failed to open. Default value will use.");
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
        debug(String('\n'), file, " file is empty. Default value will use.");
      }
      else
      {

        uint8_t convert = text.toInt();
        if (convert == 0)
        {
          debug(String('\n'), file, " data format error. Default value will use.");
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
  debug(String('\n'), "CONFIGURATION", "", _debug);
  debug("Voucher length: ", String(_length), "", _debug);
  debug("Coin count: ", String(_count), "", _debug);
  debug("Show serial: ", String(_debug), String('\n'), _debug);
}

bool verifyVouchers(String file)
{

  if (!SD.exists(file))
  {
    debug("Error: ", file, " file not found.");
    return false;
  }

  File _file = SD.open(file, FILE_READ);

  if (!_file)
  {
    debug("Error: ", file, " file failed to open.");
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
        _data.trim();
        if (_data == "")
        {
          debug(String('\n'), "Error: empty data found in line ", String(_line));
          _file.close();
          return false;
        }
        else
        {
          uint8_t index = _data.indexOf(',');
          if (index != -1)
          {
            String _text = _data.substring(0, index);

            if (_text.length() != _length)
            {
              debug(String('\n'), "Error: voucher format error found in line ", String(_line), _debug);
              debug("Data: ", String(_text), "", _debug);
              _file.close();
              return false;
            }
            else
            {
              String _status = _data.substring(index + 1);
              if (_status.length() != 1)
              {
                debug(String('\n'), "Error: data format error found in line ", String(_line), _debug);
                debug("Data: ", String(_data), "", _debug);
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
          else
          {
            debug(String('\n'), "Error: data format error found in line ", String(_line), _debug);
            debug("Data: ", String(_data), "", _debug);
            _file.close();
            return false;
          }
          _data = "";
        }
      }
    }

    if (_data != "")
    {
      _data.trim();
      if (_data != "")
      {
        _line++;
        uint8_t index = _data.indexOf(',');
        if (index != -1)
        {
          String _text = _data.substring(0, index);

          if (_text.length() != _length)
          {
            debug(String('\n'), "Error: voucher format error found in line ", String(_line), _debug);
            debug("Data: ", String(_text), "", _debug);
            _file.close();
            return false;
          }
          else
          {
            String _status = _data.substring(index + 1);
            if (_status.length() != 1)
            {
              debug(String('\n'), "Error: data format error found in line ", String(_line), _debug);
              debug("Data: ", String(_data), "", _debug);
              _file.close();
              return false;
            }
            else
            {
              if (_status == "0")
              {
                _available++;
              }
              else
              {
                _used++;
              }
            }
          }
        }
        else
        {
          debug(String('\n'), "Error: data format error found in line ", String(_line), _debug);
          debug("Data: ", String(_data), "", _debug);
          _file.close();
          return false;
        }
        _data = "";
      }
    }

    Serial.println("SUCCESS!");
    debug(String('\n'), "Available: ", String(_available), _debug);
    debug("Used: ", String(_used), "", _debug);
    debug("Total: ", String(_line), "", _debug);
    debug("Next voucher: ", String(nextVoucher), "", _debug);
  }

  _file.close();
  return true;
}

void initNextVoucher(String file)
{
  if (!SD.exists(file))
  {
    Serial.print("Error: ");
    Serial.print(file);
    Serial.println(" file not found.");
    return;
  }

  File _file = SD.open(file, O_WRITE);

  if (!_file)
  {
    Serial.print("Error: ");
    Serial.print(file);
    Serial.println(" file failed to open.");
    _file.close();
    return;
  }
  else
  {

    int pos = (_length + 1) * nextLine;

    Serial.print("Position: ");
    Serial.println(pos);

    _file.seek(pos);

    Serial.print("File position was: ");
    Serial.println(_file.position());

    _file.write('1');
    Serial.println("Write done");
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

void debug(String text1 = "", String text2 = "", String text3 = "", bool show = true)
{
  if (show)
  {
    Serial.print(text1);
    Serial.print(text2);
    Serial.println(text3);
  }
}
