/**
 * 	@file	SM130.h
 * 	@brief	Header file for SM130 library
 *	@author	Marc Boon <http://www.marcboon.com>
 *	@date	May 2009
 */

#ifndef SM130_h
#define SM130_h

#include "WProgram.h"

#define SIZE_PAYLOAD 18 // maximum payload size of I2C packet
#define SIZE_PACKET (SIZE_PAYLOAD + 2) // total I2C packet size, including length byte and checksum

#define halt haltTag // deprecated function halt() renamed to haltTag()

// Global functions
void printArrayAscii(byte array[], byte len);
void printArrayHex(byte array[], byte len);
void printHex(byte val);

/**	Class representing a <a href="http://www.sonmicro.com/en/index.php?option=com_content&view=article&id=57&Itemid=70">SonMicro SM130 RFID module</a>.
 *
 *	Nearly complete implementation of the <a href="http://www.sonmicro.com/en/downloads/Mifare/ds_SM130.pdf">SM130 datasheet</a>.<br>
 *	Functions dealing with value blocks and stored keys are not implemented.
 */
class SM130
{
	byte data[SIZE_PACKET]; //!< packet data
	char versionString[8]; //!< version string
	byte tagNumber[7]; //!< tag number as byte array
	byte tagLength; //!< length of tag number in bytes (4 or 7)
	char tagString[15]; //!< tag number as hex string
	byte tagType; //!< type of tag
	char errorCode; //!< error code from some commands
	byte antennaPower; //!< antenna power level
	byte cmd; //!< last sent command
	unsigned long t; //!< timer for sending I2C commands

public:
	static const int VERSION = 1;  //!< version of this library

	static const byte MIFARE_ULTRALIGHT  = 1;
	static const byte MIFARE_1K  = 2;
	static const byte MIFARE_4K  = 3;

	static const byte CMD_RESET = 0x80;
	static const byte CMD_VERSION = 0x81;
	static const byte CMD_SEEK_TAG = 0x82;
	static const byte CMD_SELECT_TAG = 0x83;
	static const byte CMD_AUTHENTICATE = 0x85;
	static const byte CMD_READ16 = 0x86;
	static const byte CMD_READ_VALUE = 0x87;
	static const byte CMD_WRITE16 = 0x89;
	static const byte CMD_WRITE_VALUE = 0x8a;
	static const byte CMD_WRITE4 = 0x8b;
	static const byte CMD_WRITE_KEY = 0x8c;
	static const byte CMD_INC_VALUE = 0x8d;
	static const byte CMD_DEC_VALUE = 0x8e;
	static const byte CMD_ANTENNA_POWER = 0x90;
	static const byte CMD_READ_PORT = 0x91;
	static const byte CMD_WRITE_PORT = 0x92;
	static const byte CMD_HALT_TAG = 0x93;
	static const byte CMD_SET_BAUD = 0x94;
	static const byte CMD_SLEEP = 0x96;

	boolean debug; //!< debug mode, prints all I2C communication to Serial port
	byte address; //!< I2C address (default 0x42)
	byte pinRESET; //!< RESET pin (default 3)
	byte pinDREADY; //!< DREADY pin (default 4)

	//! Constructor
	SM130();
	//! Hardware or software reset of the SM130 module
	void reset();
	//! Returns a null-terminated string with the firmware version of the SM130 module
	const char* getFirmwareVersion();
	//! Returns true if a response packet is available
	boolean available();
	//! Returns a pointer to the response packet
	byte* getRawData() { return data; };
	//! Returns the last executed command
	byte getCommand() { return data[1]; };
	//! Returns the packet length, excluding checksum
	byte getPacketLength() { return data[0]; };
	//! Returns the checksum
	byte getCheckSum() { return data[data[0]+1]; };
	//! Returns a pointer to the packet payload
	byte* getPayload() { return data+2; };
	//! Returns the block number for read/write commands
	byte getBlockNumber() { return data[2]; };
	//! Returns a pointer to the read block (with a length of 16 bytes)
	byte* getBlock() { return data+3; };
	//! Returns the tag's serial number as a byte array
	byte* getTagNumber() { return tagNumber; };
	//! Returns the length of the tag's serial number obtained by getTagNumer()
	byte getTagLength() { return tagLength; };
	//! Returns the tag's serial number as a hexadecimal null-terminated string
	const char* getTagString() { return tagString; };
	//! Returns the tag type (SM130::MIFARE_XX)
	byte getTagType() { return tagType; };
	//! Returns the tag type as a null-terminated string
	const char* getTagName() { return tagName(tagType); };
	//! Returns the error code of the last executed command
	char getErrorCode() { return errorCode; };
	//! Returns a human-readable error message corresponding to the error code
	const char* getErrorMessage();
	//! Returns the antenna power level (0 or 1)
	byte getAntennaPower() { return antennaPower; };
	//! Sends a SEEK_TAG command
	void seekTag() { sendCommand(CMD_SEEK_TAG); };
	//! Sends a SELECT_TAG command
	void selectTag() { sendCommand(CMD_SELECT_TAG); };
	//! Sends a HALT_TAG command
	void haltTag() { sendCommand(CMD_HALT_TAG); };
	//! Set antenna power (on/off)
	void setAntennaPower(byte level);
	//! Sends a SLEEP command (can only wake-up with hardware reset!)
	void sleep() { sendCommand(CMD_SLEEP); };
	//! Writes a null-terminated string of maximum 15 characters
	void writeBlock(byte block, const char* message);
	//! Writes a null-terminated string of maximum 3 characters to a Mifare Ultralight
	void writeFourByteBlock(byte block, const char* message);
	//! Sends a AUTHENTICATE command using the transport key
	void authenticate(byte block);
	//! Sends a AUTHENTICATE command using the specified key
	void authenticate(byte block, byte keyType, byte key[6]);
	//! Reads a 16-byte block
	void readBlock(byte block);

private:
	//! Send single-byte command
	void sendCommand(byte cmd);
	//! Transmit command packet over I2C
	void transmitData();
	//! Receive response packet over I2C
	byte receiveData(byte length);
	//! Returns human-readable tag name corresponding to tag type
	const char* tagName(byte type);
};

#endif // SM130_h
