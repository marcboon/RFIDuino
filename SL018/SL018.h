/**
 *	@file		SL018.h
 *	@brief	Header file for	SL018 library
 *	@author	Marc Boon <http://www.marcboon.com>
 *	@date		February 2012
 */

#ifndef	SL018_h
#define	SL018_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define SIZE_PACKET 19

// Global functions
void printArrayAscii(byte array[], byte len);
void printArrayHex(byte array[], byte len);
void printHex(byte val);

class SL018
{
	public:
		static const int VERSION = 1;  //!< version of this library
	
		static const byte MIFARE_1K					= 1;
		static const byte MIFARE_PRO				= 2;
		static const byte MIFARE_ULTRALIGHT	= 3;
		static const byte MIFARE_4K					= 4;
		static const byte MIFARE_PROX				= 5;
		static const byte MIFARE_DESFIRE		= 6;

		static const byte CMD_IDLE				= 0x00;
		static const byte CMD_SELECT			= 0x01;
		static const byte CMD_LOGIN				= 0x02;
		static const byte	CMD_READ16			= 0x03;
		static const byte	CMD_WRITE16			= 0x04;
		static const byte	CMD_READ_VALUE	= 0x05;
		static const byte	CMD_WRITE_VALUE	= 0x06;
		static const byte	CMD_WRITE_KEY		= 0x07;
		static const byte	CMD_INC_VALUE		= 0x08;
		static const byte	CMD_DEC_VALUE		= 0x09;
		static const byte	CMD_COPY_VALUE	= 0x0A;
		static const byte	CMD_READ4				= 0x10;
		static const byte	CMD_WRITE4			= 0x11;
		static const byte CMD_SEEK				= 0x20;
		static const byte CMD_SET_LED			= 0x40;
		static const byte	CMD_SLEEP				= 0x50;
		static const byte	CMD_RESET				= 0xFF;

		static const byte	OK							= 0x00;
		static const byte	NO_TAG					= 0x01;
		static const byte	LOGIN_OK				= 0x02;
		static const byte	LOGIN_FAIL			= 0x03;
		static const byte	READ_FAIL				= 0x04;
		static const byte	WRITE_FAIL			= 0x05;
		static const byte	CANT_VERIFY			= 0x06;
		static const byte	COLLISION				= 0x0A;
		static const byte	KEY_FAIL				= 0x0C;
		static const byte	NO_LOGIN				= 0x0D;
		static const byte	NO_VALUE				= 0x0E;

		boolean debug; //!< debug mode, prints all I2C communication to Serial port
		byte address; //!< I2C address (default 0x50)
		byte pinRESET; //!< RESET pin (default -1)
		byte pinDREADY; //!< DREADY pin (default -1)

	private:
		byte data[SIZE_PACKET]; //!< packet data
		byte tagNumber[7]; //!< tag number as byte array
		byte tagLength; //!< length of tag number in bytes (4 or 7)
		char tagString[15]; //!< tag number as hex string
		byte tagType; //!< type of tag
		char errorCode; //!< error code from some commands
		byte cmd; //!< last sent command
		unsigned long t; //!< timer for sending I2C commands

	public:
		//! Constructor
		SL018();

		//! Hardware or software reset of the SL018 module
		void reset();

		//! Returns true if a response packet is available
		boolean available();

		//! Returns a pointer to the response packet
		byte* getRawData() { return data; };

		//! Returns the last executed command
		byte getCommand() { return cmd; };

		//! Returns the packet length, excluding checksum
		byte getPacketLength() { return data[0]; };

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

		//! Returns the tag type (SL018::MIFARE_XX)
		byte getTagType() { return tagType; };

		//! Returns the tag type as a null-terminated string
		const char* getTagName() { return tagName(tagType); };

		//! Returns the error code of the last executed command
		char getErrorCode() { return errorCode; };

		//! Returns a human-readable error message corresponding to the error code
		const char* getErrorMessage();

		//! Starts SEEK mode
		void seekTag() { selectTag(); cmd = CMD_SEEK; };

		//! Sends a SELECT_TAG command
		void selectTag() { sendCommand(CMD_SELECT); };

		//! Sends a HALT_TAG command
		void haltTag() { cmd = CMD_IDLE; };
		
		//! Sends a SLEEP command (can only wake-up with hardware reset!)
		void sleep() { sendCommand(CMD_SLEEP); };

		//! Writes a null-terminated string of maximum 15 characters to a block
		void writeBlock(byte block, const char* message);

		//! Writes a null-terminated string of maximum 3 characters to a Mifare Ultralight page
		void writePage(byte page, const char* message);

		//! Authenticate a sector using the transport key
		void authenticate(byte sector);

		//! Authenticate a sector using the specified key
		void authenticate(byte sector, byte keyType, byte key[6]);

		//! Reads a 16-byte block
		void readBlock(byte block);

		//! Reads a 4-byte page
		void readPage(byte page);

		//! Write master key (key A)
		void writeKey(byte sector, byte key[6]);

		//! LED control (SL018 only)
		void led(boolean on);

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

#endif
