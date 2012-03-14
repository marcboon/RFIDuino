/**
 * 	@file	SM130.cpp
 * 	@brief	SM130 library for Arduino
 *	@author	Marc Boon <http://www.marcboon.com>
 *	@date	February 2012
 *
 *	<p>
 *	Controls a SonMicro SM130/mini RFID reader or RFIDuino by I2C
 *	</p>
 *	<p>
 *	Arduino analog input 4 is I2C SDA (SM130/mini pin 10/6)<br>
 *	Arduino analog input 5 is I2C SCL (SM130/mini pin 9/5)<br>
 *	Arduino digital input 4 is DREADY (SM130/mini pin 21/18)<br>
 *	Arduino digital output 3 is RESET (SM130/mini pin 18/14)
 *	</p>
 *
 *	@see	http://www.arduino.cc
 *	@see	http://www.sonmicro.com/1356/sm130.php
 *	@see	http://rfid.marcboon.com
 */

#include <Wire.h>
#include <string.h>

#include "SM130.h"

// local functions
void arrayToHex(char *s, byte array[], byte len);
char toHex(byte b);

/**	Constructor.
 *
 *	An instance of SM130 should be created as a global variable, outside of
 *	any function.
 *	The constructor sets data fields to default values for use with RFIDuino.
 *	These may be changed in setup() before SM130::reset() is called.
 */
SM130::SM130()
{
	address = 0x42;
	pinRESET = 3;
	pinDREADY = 4;
	debug = false;
	t = millis() + 10;
}

/* Public member functions ****************************************************/


/**	Reset the SM130 module
 *
 * 	This function should be called in setup(). It initializes the IO pins and
 *	issues a hardware or software reset, depending on the definition of pinRESET.
 *	After reset, a HALT_TAG command is issued to terminate the automatic SEEK mode.
 *
 *	Wire.begin() should also be called in setup(), and Wire.h should be included.
 *
 *	If pinRESET has the value 0xff (-1), software reset over I2C will be used.
 *	If pinDREADY has the value 0xff (-1), the SM130 will be polled over I2C while
 *	in SEEK mode, otherwise the DREADY pin will be polled in SEEK mode.
 *	For other commands, response polling is always over I2C.
 */
void SM130::reset()
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

	// Set antenna power
	setAntennaPower(1);

	// To cancel automatic seek mode after reset, we send a HALT_TAG command
	haltTag();
}

/**	Get the firmware version string.
 */
const char* SM130::getFirmwareVersion()
{
	// return immediately if version string already retrieved
	if (*versionString != 0)
		return versionString;

	// else send VERSION command and retry a few times if no response
	for (byte n = 0; n < 10; n++)
	{
		sendCommand(CMD_VERSION);
		if (available() && getCommand() == CMD_VERSION)
			return versionString;
		delay(100);
	}
	// time-out after 1s
	return 0;
}

/**	Checks for availability of a valid response packet.
 *
 *	This function should always be called and return true prior to using results
 *	of a command.
 *
 *	@returns	true if a valid response packet is available
 */
boolean SM130::available()
{
	// If in SEEK mode and using DREADY pin, check the status
	if (cmd == CMD_SEEK_TAG && pinDREADY != 0xff)
	{
		if (!digitalRead(pinDREADY))
			return false;
	}

	// Set the maximum length of the expected response packet
	byte len;
	switch(cmd)
	{
	case CMD_ANTENNA_POWER:
	case CMD_AUTHENTICATE:
	case CMD_DEC_VALUE:
	case CMD_INC_VALUE:
	case CMD_WRITE_KEY:
	case CMD_HALT_TAG:
	case CMD_SLEEP:
		len = 4;
		break;
	case CMD_WRITE4:
	case CMD_WRITE_VALUE:
	case CMD_READ_VALUE:
		len = 8;
	case CMD_SEEK_TAG:
	case CMD_SELECT_TAG:
		len = 11;
		break;
	default:
		len = SIZE_PACKET;
	}

	// If valid data received, process the response packet
	if (receiveData(len) > 0)
	{
		// Init response variables
		tagType = tagLength = *tagString = 0;

		// If packet length is 2, the command failed. Set error code.
		errorCode = getPacketLength() < 3 ? data[2] : 0;

		// Process command response
		switch (getCommand())
		{
		case CMD_RESET:
		case CMD_VERSION:
			// RESET and VERSION commands produce the firmware version
			len = min(getPacketLength(), sizeof(versionString)) - 1;
			memcpy(versionString, data + 2, len);
			versionString[len] = 0;
			break;

		case CMD_SEEK_TAG:
		case CMD_SELECT_TAG:
			// If no error, get tag number
			if(errorCode == 0 && getPacketLength() >= 6)
			{
				tagLength = getPacketLength() - 2;
				tagType = data[2];
				memcpy(tagNumber, data + 3, tagLength);
				arrayToHex(tagString, tagNumber, tagLength);
			}
			break;

		case CMD_AUTHENTICATE:
			break;

		case CMD_READ16:
			break;

		case CMD_WRITE16:
		case CMD_WRITE4:
			break;

		case CMD_ANTENNA_POWER:
			errorCode = 0;
			antennaPower = data[2];
			break;

		case CMD_SLEEP:
			// If in SLEEP mode, no data is available
			return false;
		}

		// Data available
		return true;
	}
	// No data available
	return false;
}

/**	Get error message for last command.
 *
 *	@return	Human-readable error message as a null-terminated string
 */
const char* SM130::getErrorMessage()
{
	switch(errorCode)
	{
	case 'L':
		if(getCommand() == CMD_SEEK_TAG) return "Seek in progress";
	case 0:
		return "OK";
	case 'N':
		if(getCommand() == CMD_WRITE_KEY) return "Write master key failed";
		if(getCommand() == CMD_SET_BAUD) return "Set baud rate failed";
		if(getCommand() == CMD_AUTHENTICATE) return "No tag present or login failed";
		return "No tag present";
	case 'U':
		if(getCommand() == CMD_AUTHENTICATE) return "Authentication failed";
		if(getCommand() == CMD_WRITE16 || getCommand() == CMD_WRITE4) return "Verification failed";
		return "Antenna off";
	case 'F':
		if(getCommand() == CMD_READ16) return "Read failed";
		return "Write failed";
	case 'I':
		return "Invalid value block";
	case 'X':
		return "Block is read-protected";
	case 'E':
		return "Invalid key format in EEPROM";
	default:
		return "Unknown error";
	}
}

/**	Turns on/off the RF field.
 *
 *	@param level 0 is off, anything else is on
 */
void SM130::setAntennaPower(byte level)
{
	antennaPower = level;
	data[0] = 2;
	data[1] = CMD_ANTENNA_POWER;
	data[2] = antennaPower;
	transmitData();
}

/** Authenticate with transport key (0xFFFFFFFFFFFF).
 *
 *	@param block Block number
 */
void SM130::authenticate(byte block)
{
	data[0] = 3;
	data[1] = CMD_AUTHENTICATE;
	data[2] = block;
	data[3] = 0xff;
	transmitData();
}

/** Authenticate with specified key A or key B.
 *
 *	@param block Block number
 *	@param keyType Which key to use: 0xAA for key A or 0xBB for key B
 *	@param key Key value (6 bytes)
 */
void SM130::authenticate(byte block, byte keyType, byte key[6])
{
	data[0] = 9;
	data[1] = CMD_AUTHENTICATE;
	data[2] = block;
	data[3] = keyType;
	memcpy(data + 4, key, 6);
	transmitData();
}

/**	Read 16-byte block.
 *
 *	@param block Block number
 */
void SM130::readBlock(byte block)
{
	data[0] = 2;
	data[1] = CMD_READ16;
	data[2] = block;
	transmitData();
}

/**	Write 16-byte block.
 *
 *	The block will be padded with zeroes if the message is shorter
 *	than 15 characters.
 *
 *	@param block Block number
 *	@param message Null-terminated string of up to 15 characters
 */
void SM130::writeBlock(byte block, const char* message)
{
	data[0] = 18;
	data[1] = CMD_WRITE16;
	data[2] = block;
	strncpy((char*)data + 3, message, 15);
	data[18] = 0;
	transmitData();
}

/**	Write 4-byte block.
 *
 *	This command is used for Mifare Ultralight tags which have 4 byte blocks.
 *
 *	@param block Block number
 *	@param message Null-terminated string of up to 3 characters
 */
void SM130::writeFourByteBlock(byte block, const char* message)
{
	data[0] = 6;
	data[1] = CMD_WRITE4;
	data[2] = block;
	strncpy((char*)data + 3, message, 3);
	data[6] = 0;
	transmitData();
}

/**	Send 1-byte command.
 *
 *	@param cmd Command
 */
void SM130::sendCommand(byte cmd)
{
	data[0] = 1;
	data[1] = cmd;
	transmitData();
}

/* Private member functions ****************************************************/


/**	Transmit a packet with checksum to the SM130.
 */
void SM130::transmitData()
{
	// wait until at least 20ms passed since last I2C transmission
	while(t > millis());
	t = millis() + 20;

	// init checksum and packet length
	byte sum = 0;
	byte len = data[0] + 1;

	// remember which command was sent
	cmd = data[1];

	// transmit packet with checksum
	Wire.beginTransmission(address);
	for (int i = 0; i < len; i++)
	{
#if defined(ARDUINO) && ARDUINO >= 100
		Wire.write(data[i]);
#else
		Wire.send(data[i]);
#endif
		sum += data[i];
	}
#if defined(ARDUINO) && ARDUINO >= 100
	Wire.write(sum);
#else
	Wire.send(sum);
#endif
	Wire.endTransmission();

	// show transmitted packet for debugging
	if (debug)
	{
		Serial.print("> ");
		printArrayHex(data, len);
		Serial.print(' ');
		printHex(sum);
		Serial.println();
	}
}

/**	Receives a packet from the SM130 and verifies the checksum.
 *
 *	@param length the number of bytes to receive
 *	@return the number of bytes in the payload, or -1 if bad checksum
 */
byte SM130::receiveData(byte length)
{
	// wait until at least 20ms passed since last I2C transmission
	while(t > millis());
	t = millis() + 20;

	// read response
	Wire.requestFrom(address, length);
	byte n = Wire.available();

	// get data if available
	if(n > 0)
	{
		for (byte i = 0; i < n;)
		{
#if defined(ARDUINO) && ARDUINO >= 100
			data[i++] = Wire.read();
#else
			data[i++] = Wire.receive();
#endif
		}

		// show received packet for debugging
		if (debug && data[0] > 0 )
		{
			Serial.print("< ");
			printArrayHex(data, n);
			Serial.println();
		}

		// verify checksum if length > 0 and <= SIZE_PAYLOAD
		if (data[0] > 0 && data[0] <= SIZE_PAYLOAD)
		{
			byte i, sum;
			for (i = 0, sum = 0; i <= data[0]; i++)
			{
				sum += data[i];
			}
			// return with length of response, or -1 if invalid checksum
			return sum == data[i] ? data[0] : -1;
		}
	}
	return 0;
}

/**	Maps tag types to names.
 *
 *	@param	type numeric tag type
 *	@return	Human-readable tag name as null-terminated string
 */
const char* SM130::tagName(byte type)
{
	switch(type)
	{
	case 1: return "Mifare UL";
	case 2: return "Mifare 1K";
	case 3:	return "Mifare 4K";
	default: return "Unknown Tag";
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
