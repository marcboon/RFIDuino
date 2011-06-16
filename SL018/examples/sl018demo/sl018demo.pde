// SL018 demo application
// Marc Boon <http://www.marcboon.com>
// April 2010

// Controls a StrongLink SL018 or SL030 RFID reader by I2C
// Arduino to SL018/SL030 wiring:
// A3/TAG     1      5
// A4/SDA     2      3
// A5/SCL     3      4
// 5V         4      -
// GND        5      6
// 3V3        -      1

#include <Wire.h>
#include <SL018.h>

// TAG pin (low level when tag present)
#define TAG 17 // A3

// Actions
#define NONE 0
#define SEEK 1
#define READ 2
#define WRITE 3

// Create SL018 instance
SL018 rfid;

// Global vars
byte action = NONE;
boolean autoRead = false;
boolean tagPresent = false;
boolean authenticated;
byte block;
byte numBlocks;
byte tagType;
char msg[16];

void setup()
{
  pinMode(TAG, INPUT);
  Wire.begin();
  Serial.begin(19200);
  Serial.println("SL018 demo");

  // reset rfid module
//  rfid.reset();

  // help
  Serial.println("Type ? for help");
}

void loop()
{
  // check for tag presence when auto read is enabled
  if(autoRead && !tagPresent && !digitalRead(TAG) && action == NONE)
  {
    tagPresent = true;
    // read tag
    action = READ;
    // specify what to read
    block = 0;
    numBlocks = 16;
    // tag has to be selected first
    rfid.selectTag();
  }

  // check if tag has gone
  if(tagPresent && digitalRead(TAG))
  {
    tagPresent = false;
  }
  
  // check for command from serial port
  if(Serial.available() > 0)
  {
    switch(Serial.read())
    {
    case '?':
      Serial.println("Commands:");
      Serial.println("A - Auto read on/off");
      Serial.println("D - Debug on/off");
      Serial.println("S - Seek tag");
      Serial.println("R - Read sector");
      Serial.println("W - Write string");
      Serial.println("Q - Sleep");
      Serial.println("X - Reset");
      break;
    case 'a':
    case 'A':
      // auto read on/off
      autoRead = !autoRead;
      Serial.print("Auto read ");
      Serial.println(autoRead ? "on" : "off");
      break;
    case 'd':
    case 'D':
      // debug on/off
      rfid.debug = !rfid.debug;
      Serial.print("Debug ");
      Serial.println(rfid.debug ? "on" : "off");
      break;
    case 's':
    case 'S':
      // seek tag
      Serial.println("Seek");
      rfid.seekTag();
      action = SEEK;
      break;
    case 'r':
    case 'R':
      // read tag
      Serial.println("Read");
      action = READ;
      // specify what to read
      block = 0;
      numBlocks = 16;
      // tag has to be selected first
      rfid.selectTag();
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
        rfid.selectTag();
      }
      break;
    case 'q':
    case 'Q':
      // enter sleep mode
      Serial.println("Sleep");
      rfid.sleep();
      break;
    case 'x':
    case 'X':
      // reset
      Serial.println("Reset");
      rfid.reset();
      action = NONE;
      break;
    }
  }

  // check for response from rfid
  if(rfid.available())
  {
    // check for errors
    if(rfid.getErrorCode() != SL018::OK && rfid.getErrorCode() != SL018::LOGIN_OK)
    {
      if(action != SEEK) // ignore errors while in SEEK mode
      {
        Serial.println(rfid.getErrorMessage());
        rfid.haltTag();
        action = NONE;
      }
    }
    else // deal with response if no error
    {
      switch(rfid.getCommand())
      {
      case SL018::CMD_SEEK:
      case SL018::CMD_SELECT:
        // store tag type
        tagType = rfid.getTagType();
        // show tag name and serial number
        Serial.print(rfid.getTagName());
        Serial.print(' ');
        Serial.println(rfid.getTagString());
        // in case of read or write action, authenticate first
        if(action == READ || action == WRITE)
        {
          // mifare ultralight does not need authentication, and has 4-byte blocks
          if(tagType == SL018::MIFARE_ULTRALIGHT)
          {
            authenticated = true;
            if(action == READ)
            {
                rfid.readPage(block);
            }
            else
            {
              // write to last page (because all others are write-protected on tikitags)
              rfid.writePage(15, msg);
            }
          }
          else
          {
            authenticated = false;
            rfid.authenticate(block >> 2);
          } 
        }
        else
        {
          // terminate seek
          rfid.haltTag();
          action = NONE;
        }
        break;
      case SL018::CMD_LOGIN:
        authenticated = true;
        if(action == READ)
        {
            rfid.readBlock(block);
        }
        else if(action == WRITE)
        {
            rfid.writeBlock(block, msg);
        }
        break;
      case SL018::CMD_READ4:
      case SL018::CMD_READ16:
        if(rfid.getCommand() == SL018::CMD_READ4)
        {
          // print 4-byte page in hex and ascii
          Serial.print("Page ");
          printHex(block);
          Serial.print(": ");
          printArrayHex(rfid.getBlock(), 4);
          Serial.print("  ");
          printArrayAscii(rfid.getBlock(), 4);
          Serial.println();
        }
        else
        {
          // print 16-byte block in hex and ascii
          Serial.print("Block ");
          printHex(block);
          Serial.print(": ");
          printArrayHex(rfid.getBlock(), 16);
          Serial.print("  ");
          printArrayAscii(rfid.getBlock(), 16);
          Serial.println();
        }
        // get next block
        if(++block && --numBlocks)
        {
          // mifare ultralight does not need authentication, and has 4-byte pages
          if(tagType == SL018::MIFARE_ULTRALIGHT)
          {
            rfid.readPage(block);
          }
          else if(authenticated && (block & 0x03) != 0)
          {
            // blocks from same sector don't need further authentication
            rfid.readBlock(block);
          }
          else
          {
            // authenticate next sector (4 blocks per sector)
            authenticated = false;
            rfid.authenticate(block >> 2);
          } 
        }
        else // read completed
        {
          rfid.haltTag();
          action = NONE;
        }
        break;
      case SL018::CMD_WRITE4:
      case SL018::CMD_WRITE16:
        // write completed
        Serial.println("OK");
        rfid.haltTag();
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
