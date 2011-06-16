// SM130 demo application
// Marc Boon <http://www.marcboon.com>
// June 2009

// Controls a SonMicro SM130/mini RFID reader or RFIDuino by I2C
// Arduino analog input 4 is I2C SDA (SM130/mini pin 10/6)
// Arduino analog input 5 is I2C SCL (SM130/mini pin 9/5)
// Arduino digital input 4 is DREADY (SM130/mini pin 21/18)
// Arduino digital output 3 is RESET (SM130/mini pin 18/14)

#include <Wire.h>
#include <SM130.h> // v1

// Actions
#define NONE 0
#define SEEK 1
#define READ 2
#define WRITE 3

// Create SM130 instance for RFIDuino
SM130 RFIDuino;

// Global vars
byte action = NONE;
boolean authenticated;
byte block;
byte numBlocks;
byte tagType;
char msg[16];

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  Serial.println("RFIDuino");

  // reset RFIDuino
  RFIDuino.reset();

  // read firmware version
  Serial.print("Version ");
  Serial.println(RFIDuino.getFirmwareVersion());
  
  // help
  Serial.println("Type ? for help");
}

void loop()
{
  // check for command from serial port
  if(Serial.available() > 0)
  {
    switch(Serial.read())
    {
    case '?':
      Serial.println("Commands:");
      Serial.println("D - Debug on/off");
      Serial.println("A - Antenna on/off");
      Serial.println("H - Halt tag");
      Serial.println("S - Seek tag");
      Serial.println("R - Read sector");
      Serial.println("W - Write string");
      Serial.println("V - Version");
      Serial.println("Q - Sleep");
      Serial.println("X - Reset");
      break;
    case 'd':
    case 'D':
      // debug on/off
      RFIDuino.debug = !RFIDuino.debug;
      Serial.print("Debug ");
      Serial.println(RFIDuino.debug ? "on" : "off");
      break;
    case 'a':
    case 'A':
      // antenna on/off
      RFIDuino.setAntennaPower(!RFIDuino.getAntennaPower());
      Serial.print("Antenna ");
      Serial.println(RFIDuino.getAntennaPower() ? "on" : "off");
      break;
    case 'h':
    case 'H':
      // halt
      Serial.println("Halt");
      RFIDuino.halt();
      action = NONE;
      break;
    case 's':
    case 'S':
      // enter seek mode
      Serial.println("Seek");
      RFIDuino.seekTag();
      action = SEEK;
      break;
    case 'r':
    case 'R':
      // read tag
      Serial.println("Read");
      action = READ;
      // specify what to read
      block = 0;
      numBlocks = 4;
      // tag has to be selected first
      RFIDuino.selectTag();
      break;
    case 'w':
    case 'W':
      // collect up to 15 characters from input, and terminate with zero
      if(readQuotedString(msg, sizeof(msg)) > 0)
      {
        // write string to tag
        Serial.print("Write '");
        Serial.print(msg);
        Serial.println("'");
        action = WRITE;
        block = 1;
        // tag has to be selected first
        RFIDuino.selectTag();
      }
      break;
    case 'v':
    case 'V':
      // read firmware version
      Serial.print("Version ");
      Serial.println(RFIDuino.getFirmwareVersion());
      break;
    case 'q':
    case 'Q':
      // enter sleep mode
      Serial.println("Sleep");
      RFIDuino.sleep();
      break;
    case 'x':
    case 'X':
      // reset
      Serial.println("Reset");
      RFIDuino.reset();
      action = NONE;
      break;
    }
  }

  // check for response from RFIDuino
  if(RFIDuino.available())
  {
    // check for errors, 0 means no error, L means logged in
    if(RFIDuino.getErrorCode() != 0 && RFIDuino.getErrorCode() != 'L')
    {
      Serial.println(RFIDuino.getErrorMessage());
      action = NONE;
    }
    else // deal with response
    {
      switch(RFIDuino.getCommand())
      {
      case SM130::CMD_SEEK_TAG:
        if(RFIDuino.getErrorCode() == 'L' || action == NONE)
        {
          // seek in progress, or auto-seek after reset, which we ignore
          break;
        }
      case SM130::CMD_SELECT_TAG:
        // store tag type
        tagType = RFIDuino.getTagType();
        // show tag name and serial number
        Serial.print(RFIDuino.getTagName());
        Serial.print(": ");
        Serial.println(RFIDuino.getTagString());
        // in case of read or write action, authenticate first
        if(action == READ || action == WRITE)
        {
          // mifare ultralight does not need authentication, and has 4-byte blocks
          if(tagType == SM130::MIFARE_ULTRALIGHT)
          {
            authenticated = true;
            if(action == READ)
            {
                RFIDuino.readBlock(block);
            }
            else
            {
              // write to last 4-byte block (because all others are write-protected on tikitags)
              RFIDuino.writeFourByteBlock(15, msg);
            }
          }
          else
          {
            authenticated = false;
            RFIDuino.authenticate(block);
          } 
        }
        else if(action == SEEK)
        {
          // keep seeking
          RFIDuino.seekTag();
        }
        else
        {
          // terminate seek
          action = NONE;
        }
        break;
      case SM130::CMD_AUTHENTICATE:
        authenticated = true;
        if(action == READ)
        {
            RFIDuino.readBlock(block);
        }
        else if(action == WRITE)
        {
            RFIDuino.writeBlock(block, msg);
        }
        break;
      case SM130::CMD_READ16:
        // print 16-byte block in hex and ascii
        Serial.print("Block ");
        printHex(RFIDuino.getBlockNumber());
        Serial.print(": ");
        printArrayHex(RFIDuino.getBlock(), 16);
        Serial.print("  ");
        printArrayAscii(RFIDuino.getBlock(), 16);
        Serial.println();
        // get next block
        if(++block < numBlocks)
        {
          // mifare ultralight does not need authentication, and has 4-byte blocks
          if(tagType == SM130::MIFARE_ULTRALIGHT)
          {
            RFIDuino.readBlock(block * 4);
          }
          else if(authenticated && (block & 0x03) != 0)
          {
            // blocks from same sector don't need further authentication
            RFIDuino.readBlock(block);
          }
          else
          {
            // authenticate next sector
            authenticated = false;
            RFIDuino.authenticate(block);
          } 
        }
        else // read completed
        {
          RFIDuino.haltTag();
          action = NONE;
        }
        break;
      case SM130::CMD_WRITE4:
      case SM130::CMD_WRITE16:
        // write completed
        Serial.println("OK");
        RFIDuino.haltTag();
        action = NONE;
        break;
      }
    }
  }
}

int readQuotedString(char *s, int len)
{
  int i = 0;
  char quote = 0;
  while(i < len)
  {
    delay(5);
    if(Serial.available() == 0)
    {
      break;
    }
    *s = Serial.read();
    if(quote == 0)
    {  
      if(*s == '"' || *s == '\'')
      {
        quote = *s;
      }
      else if(*s != ' ')
      {
        ++s;
        ++i;
      }
    }
    else if(*s == quote)
    {
      break;
    }
    else
    {
      ++s;
      ++i;
    }
  }
  *s = 0;
  return i;
}
