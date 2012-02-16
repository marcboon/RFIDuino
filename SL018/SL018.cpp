/**
 *  @title	StrongLink SL018/SL030 RFID reader library
 *
 * 	@file		SL018.cpp
 *  @author	marc@marcboon.com
 *  @modified fil@rezox.com (Filipe Laborde-Basto) [to make binary safe]
 *  @date		February 2012
 *
 *  @see		http://www.stronglink.cn/english/sl018.htm
 *  @see		http://www.stronglink.cn/english/sl030.htm
 */
 
#include <Wire.h>
#include <string.h>
#include "SL018.h"

// local prototypes
void arrayToHex(char *s, byte array[], byte len);
char toHex(byte b);

/**	Constructor.
 *
 *	An instance of SL018 should be created as a global variable, outside of
 *	any function.
 *	The constructor sets public data fields to default values.
 *	These may be changed in setup() before SL018::reset() is called.
 */
SL018::SL018()
{
	address = 0x50;
	pinRESET = -1;
	pinDREADY = -1;
	cmd = CMD_IDLE;
	debug = false;
	t = millis() + 10;
}

/* Public member functions ****************************************************/


/**	Reset the SL018 module
 *
 * 	This function should be called in setup(). It initializes the IO pins and
 *	issues a hardware or software reset, depending on the definition of pinRESET.
 *	After reset, antenna power is switched off to terminate the automatic SEEK mode.
 *
 *	Wire.begin() should also be called in setup(), and Wire.h should be included.
 *
 *	If pinRESET has the value -1 (default), software reset over I2C will be used.
 *	If pinDREADY has the value -1 (default), the SL018 will be polled over I2C for
 *	SEEK commands, otherwise the DREADY pin will be polled.
 *	For other commands, response polling is always over I2C.
 */
void SL018::reset()
{
	// Init DREADY pin
	if (pinDREADY != 0xff)
	{
		pinMode(pinDREADY, INPUT);
	}

	// Init RESET pin
	if (pinRESET != 0xff) // hardware reset
	{
		pinMode(pinRESET, OUTPUT);
		digitalWrite(pinRESET, HIGH);
		delay(10);
		digitalWrite(pinRESET, LOW);
	}
	else // software reset
	{
		sendCommand(CMD_RESET);
	}

	// Allow enough time for reset
	delay(200);
}

/**	Checks for availability of a valid response packet.
 *
 *	This function should always be called and return true prior to using results
 *	of a command.
 *
 *	@returns	true if a valid response packet is available
 */
boolean SL018::available()
{
	// Set the maximum length of the expected response packet
	byte len;
	switch(cmd)
	{
	case CMD_IDLE:
	case CMD_RESET:
		len = 0;
		break;
	case CMD_LOGIN:
	case CMD_SET_LED:
	case CMD_SLEEP:
		len = 3;
		break;
	case CMD_READ4:
	case CMD_WRITE4:
	case CMD_READ_VALUE:
	case CMD_WRITE_VALUE:
	case CMD_DEC_VALUE:
	case CMD_INC_VALUE:
	case CMD_COPY_VALUE:
		len = 7;
		break;
	case CMD_WRITE_KEY:
		len = 9;
		break;
	case CMD_SEEK:
	case CMD_SELECT:
		len = 11;
		break;
	default:
		len = SIZE_PACKET;
	}

	// If valid data received, process the response packet
	if (len && receiveData(len) > 0)
	{
		// Init response variables
		tagType = tagLength = *tagString = 0;
		errorCode = data[2];

		// Process command response
		switch (getCommand())
		{
		case CMD_SEEK:
		case CMD_SELECT:
			// If no error, get tag number
			if(errorCode == 0 && getPacketLength() >= 7)
			{
				tagLength = getPacketLength() - 3;
				tagType = data[getPacketLength()];
				memcpy(tagNumber, data + 3, tagLength);
				arrayToHex(tagString, tagNumber, tagLength);
			}
			else if(cmd == CMD_SEEK)
			{
				// Continue seek
				seekTag();
				return false;
			}
		}
		// Data is available
		return true;
	}
	// No data available
	return false;
}

/**	Get error message for last command.
 *
 *	@return	Human-readable error message as a null-terminated string
 */
const char* SL018::getErrorMessage()
{
	switch(errorCode)
	{
	case 0:
		return "OK";
	case 1:
		return "No tag present";
	case 2:
		return "Login OK";
	case 3:
	case 0x10:
		return "Login failed";
	case 4:
		return "Read failed";
	case 5:
		return "Write failed";
	case 6:
		return "Unable to read after write";
	case 0x0A:
		return "Collision detected";
	case 0x0C:
		return "Load key failed";
	case 0x0D:
		return "Not authenticated";
	case 0x0E:
		return "Not a value block";
	default:
		return "Unknown error";
	}
}

/** Authenticate with transport key (0xFFFFFFFFFFFF).
 *
 *	@param sector Sector number
 */
void SL018::authenticate(byte sector)
{
	data[0] = 9;
	data[1] = CMD_LOGIN;
	data[2] = sector;
	data[3] = 0xAA;
	memset(data + 4, 0xFF, 6);
	transmitData();
}

/** Authenticate with specified key A or key B.
 *
 *	@param sector Sector number
 *	@param keyType Which key to use: 0xAA for key A or 0xBB for key B
 *	@param key Key value (6 bytes)
 */
void SL018::authenticate(byte sector, byte keyType, byte key[6])
{
	data[0] = 9;
	data[1] = CMD_LOGIN;
	data[2] = sector;
	data[3] = keyType;
	memcpy(data + 4, key, 6);
	transmitData();
}

/**	Read 16-byte block.
 *
 *	@param block Block number
 */
void SL018::readBlock(byte block)
{
	data[0] = 2;
	data[1] = CMD_READ16;
	data[2] = block;
	transmitData();
}

/**	Read 4-byte page.
 *
 *	@param page	Page number
 */
void SL018::readPage(byte page)
{
	data[0] = 2;
	data[1] = CMD_READ4;
	data[2] = page;
	transmitData();
}

/**	Write 16-byte block.
 *
 *	The block will be padded with zeroes if the message is shorter
 *	than 15 characters.
 *
 *	@param block Block number
 *	@param message string of 16 characters (binary safe)
 */
void SL018::writeBlock(byte block, const char* message)
{
	data[0] = 18;
	data[1] = CMD_WRITE16;
	data[2] = block;
	//strncpy((char*)data + 3, message, 15); // not binary safe
	memcpy( (char*)data + 3, message, 16 );
	data[18] = 0;
	transmitData();
}

/**	Write 4-byte page.
 *
 *	This command is used for Mifare Ultralight tags which have 4 byte pages.
 *
 *	@param page Page number
 *	@param message String of 4 characters
 */
void SL018::writePage(byte page, const char* message)
{
	data[0] = 6;
	data[1] = CMD_WRITE4;
	data[2] = page;
	//strncpy((char*)data + 3, message, 3);  // not binary safe
	memcpy( (char*)data + 3, message, 4 );	
	data[6] = 0;
	transmitData();
}

/** Write master key (key A).
 *
 *	@param sector Sector number
 *	@param key Key value (6 bytes)
 */
void SL018::writeKey(byte sector, byte key[6])
{
	data[0] = 8;
	data[1] = CMD_WRITE_KEY;
	data[2] = sector;
	memcpy(data + 3, key, 6);
	transmitData();
}

/**	Control red LED on SL018 (not implemented on SL030).
 *
 *	@param on	true for on, false for off
 */
void SL018::led(boolean on)
{
	data[0] = 2;
	data[1] = CMD_SET_LED;
	data[2] = on;
	transmitData();
}

/**	Send 1-byte command.
 *
 *	@param cmd Command
 */
void SL018::sendCommand(byte cmd)
{
	data[0] = 1;
	data[1] = cmd;
	transmitData();
}

/* Private member functions ****************************************************/


/**	Transmit a packet to the SL018.
 */
 /*
 	data[0] = 18;
	data[1] = CMD_WRITE16;
	data[2] = block;
	strncpy((char*)data + 3, message, 15);
	data[18] = 0;
	*/
void SL018::transmitData()
{
	// wait until at least 20ms passed since last I2C transmission
	while(t > millis());
	t = millis() + 20;

	// remember which command was sent
	cmd = data[1];

	// transmit packet with checksum
	Wire.beginTransmission(address);
		
	for (int i = 0; i <= data[0]; i++)
	{
#if defined(ARDUINO) && ARDUINO >= 100
		Wire.write(data[i]);
#else
		Wire.send(data[i]);
#endif
	}
	Wire.endTransmission();

	// show transmitted packet for debugging
	if (debug)
	{
		Serial.print("> ");
		printArrayHex( data, data[0] + 1);
		Serial.println();
	}
}

/**	Receives a packet from the SL018.
 *
 *	@param length the number of bytes to receive
 *	@return the number of bytes in the payload, or -1 if bad checksum
 */
byte SL018::receiveData(byte length)
{
	// wait until at least 20ms passed since last I2C transmission
	while(t > millis());
	t = millis() + 20;

	// read response
	Wire.requestFrom(address, length);
	if(Wire.available())
	{
		// get length	of packet
#if defined(ARDUINO) && ARDUINO >= 100
		data[0] = Wire.read();
#else
		data[0] = Wire.receive();
#endif
		
		// get data
		for (byte i = 1; i <= data[0]; i++)
		{
#if defined(ARDUINO) && ARDUINO >= 100
			data[i] = Wire.read();
#else
			data[i] = Wire.receive();
#endif
		}

		// show received packet for debugging
		if (debug && data[0] > 0 )
		{
			Serial.print("< ");
			printArrayHex(data, data[0] + 1);
			Serial.println();
		}

		// return with length of response
		return data[0];
	}
	return 0;
}

/**	Maps tag types to names.
 *
 *	@param	type numeric tag type
 *	@return	Human-readable tag name as null-terminated string
 */
const char* SL018::tagName(byte type)
{
	switch(type)
	{
	case 1: return "Mifare 1K";
	case 2: return "Mifare Pro";
	case 3:	return "Mifare UltraLight";
	case 4:	return "Mifare 4K";
	case 5: return "Mifare ProX";
	case 6: return "Mifare DesFire";
	default: return "";
	}
}


// Global helper functions

/**	Convert byte array to null-terminated hexadecimal string.
 *
 *	@param	s	pointer to destination string
 *	@param	array	byte array to convert
 *	@param	len		length of byte array to convert
 */
void arrayToHex(char *s, byte array[], byte len)
{
	for (byte i = 0; i < len; i++)
	{
		*s++ = toHex(array[i] >> 4);
		*s++ = toHex(array[i]);
	}
	*s = 0;
}

/**	Convert low-nibble of byte to ASCII hex.
 *
 *	@param	b	byte to convert
 *	$return	uppercase hexadecimal character [0-9A-F]
 */
char toHex(byte b)
{
	b = b & 0x0f;
	return b < 10 ? b + '0' : b + 'A' - 10;
}

/**	Print byte array as ASCII string.
 *
 *	Non-printable characters (<0x20 or >0x7E) are printed as dot.
 *
 *	@param	array byte array
 *	@param	len length of byte array
 */
void printArrayAscii(byte array[], byte len)
{
  for (byte i = 0; i < len;)
  {
    char c = array[i++];
    if (c < 0x20 || c > 0x7e)
    {
      Serial.print('.');
    }
    else
    {
      Serial.print(char(c));
    }
  }
}

/**	Print byte array as hexadecimal character pairs.
 *
 *	@param	array byte array
 *	@param	len length of byte array
 */
void printArrayHex(byte array[], byte len)
{
  for (byte i = 0; i < len;)
  {
    printHex(array[i++]);
    if (i < len)
    {
      Serial.print(' ');
    }
  }
}
/** Print byte as two hexadecimal characters.
 *
 *	@param val	byte value
 */
void printHex(byte val)
{
  if (val < 0x10)
  {
    Serial.print('0');
  }
  Serial.print(val, HEX);
}
