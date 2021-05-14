// Meatloaf - A Commodore 64/128 multi-device emulator
// https://github.com/idolpx/meatloaf
// Copyright(C) 2020 James Johnston
//
// This file is part of Meatloaf but adapted for use in the FujiNet project
// https://github.com/FujiNetWIFI/fujinet-platformio
// 
// Meatloaf is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Meatloaf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Meatloaf. If not, see <http://www.gnu.org/licenses/>.

#ifndef IECBUS_H
#define IECBUS_H

/**
 * notes by jeffpiep 3/9/2021
 * 
 * i think we can make an iecBus class that listens for ATN
 * gets the command and then passes off control to the iecDevice
 * IIRC, we do this in SIO land: the sioDevice is a friend to the sioBus. 
 * The sioBus has a list of devices. The bus object (SIO) listens for commands.
 * When it sees a command, it finds out what device the command is directed
 * towards and then hands over control to that device object. Most of the SIO
 * commands and operation belong to the sioDevice class (e.g., ack, nak, 
 * _to_computer, _to_device). 
 * 
 * This file, iec.h, is partly a low-level driver for the IEC physical layer. 
 * We need this because we aren't using a UART. i think we might want to
 * break out the low level i/o into a differnt class - something to think about.
 * then there's the standard IEC protocol layer with commands and data state. 
 * 
 * The devices currently exist in the interface.h file. it is a disk device
 * and a realtime clock. it would great if we could port the SD2IEC devices
 * because they support JiffyDOS, TFC3 turbo, etc. There should be a minimum
 * set of capability that we can define in the base class iecDevice using
 * virual functions. I think that is the handlers for the ATN commands
 * and probably data input and output. maybe error reporting on channel 15?
 * maybe we want a process command to deal with incoming data, e.g., on the 
 * printer?
*/

#include <forward_list>

#include "../../include/global_defines.h"
#include "../../include/cbmdefines.h"

#include "device_db.h"
#include "helpers.h"
#include "MemoryInfo.h"

enum IECline
{
	pulled = true,
	released = false
};

enum IECState 
{
	noFlags   = 0,
	eoiFlag   = (1 << 0),   // might be set by iec_receive
	atnFlag   = (1 << 1),   // might be set by iec_receive
	errorFlag = (1 << 2)    // If this flag is set, something went wrong and
};

// Return values for checkATN:
enum ATNMode 
{
	ATN_IDLE = 0,           // Nothing recieved of our concern
	ATN_CMD = 1,            // A command is recieved
	ATN_LISTEN = 2,         // A command is recieved and data is coming to us
	ATN_TALK = 3,           // A command is recieved and we must talk now
	ATN_ERROR = 4,          // A problem occoured, reset communication
	ATN_RESET = 5		    // The IEC bus is in a reset state (RESET line).
};

// IEC ATN commands:
enum ATNCommand 
{
	ATN_COMMAND_GLOBAL = 0x00,     // 0x00 + cmd (global command)
	ATN_COMMAND_LISTEN = 0x20,     // 0x20 + device_id (LISTEN)
	ATN_COMMAND_UNLISTEN = 0x3F,   // 0x3F (UNLISTEN)
	ATN_COMMAND_TALK = 0x40,       // 0x40 + device_id (TALK)
	ATN_COMMAND_UNTALK = 0x5F,     // 0x5F (UNTALK)
	ATN_COMMAND_DATA = 0x60,       // 0x60 + channel (SECOND)
	ATN_COMMAND_CLOSE = 0xE0,  	   // 0xE0 + channel (CLOSE)
	ATN_COMMAND_OPEN = 0xF0	       // 0xF0 + channel (OPEN)
};

struct ATNData
{
	ATNMode mode;
	uint8_t code;
	uint8_t command;
	uint8_t channel;
	uint8_t device_id;
	char data[ATN_CMD_MAX_LENGTH];
};


enum OpenState 
{
	O_NOTHING,			// Nothing to send / File not found error
	O_INFO,				// User issued a reload sd card
	O_FILE,				// A program file is opened
	O_DIR,				// A listing is requested
	O_FILE_ERR,			// Incorrect file format opened
	O_SAVE_REPLACE,		// Save-with-replace is requested
	O_SYSTEM_INFO,
	O_DEVICE_STATUS
};

//helper functions
uint8_t iec_checksum(uint8_t *buf, unsigned short len);

// class def'ns
class iecBus;      // declare early so can be friend
class iecModem;    // declare here so can reference it, but define in modem.h
class iecNetwork;  // declare here so can reference it, but define in network.h
class iecCassette; // Cassette forward-declaration.
class iecCPM;      // CPM device.
class iecPrinter;  // Printer device

class iecDevice
{
protected:
	friend iecBus;

    uint8_t _device_id;

    virtual void _status(void) = 0;
    virtual void _process_command(void) = 0;

	// Reset device
	virtual void reset(void);

    // Optional shutdown/reboot cleanup routine
    virtual void shutdown(void);

	// This var is set after an open command and determines what to send next
	uint8_t _openState; // see OpenState
	uint8_t _queuedError;

	void sendStatus(void);
	void sendSystemInfo(void);
	void sendDeviceStatus(void);

	uint16_t sendHeader(uint16_t &basicPtr);
	uint16_t sendLine(uint16_t &basicPtr, uint16_t blocks, char* text);
	uint16_t sendLine(uint16_t &basicPtr, uint16_t blocks, const char* format, ...);

	// handler helpers.
	void _open(void) {};
	void _listen_data(void) {};
	void _talk_data(uint8_t chan) {};
	void _close(void) {};

public:
    /**
     * @brief get the IEC device Number (4-30)
     * @return The device number registered for this device
     */
    uint8_t device_id(void) { return _device_id; };


    /**
     * @brief is device active (turned on?)
     */
    bool device_active = true;

	iecDevice(void);
	virtual ~iecDevice(void) {}
};


class iecBus
{
private:
	std::forward_list<iecDevice *> _daisyChain;

    uint8_t _command_frame_counter = 0;

    iecDevice *_activeDev = nullptr;
    iecModem *_modemDev = nullptr;
    iecNetwork *_netDev[8] = {nullptr};
    iecCassette *_cassetteDev = nullptr;
    iecCPM *_cpmDev = nullptr;
    iecPrinter *_printerdev = nullptr;

    void _bus_process_command(void);

	uint8_t _iec_state;

	// IEC Bus Commands
//	void global(void) {};            // 0x00 + cmd          Global command to all devices, Not supported on CBM
	void listen(void);               // 0x20 + device_id 	Listen, device (0–30), Devices 0-3 are reserved
	void unlisten(void) {};          // 0x3F				Unlisten, all devices
	void talk(void);                 // 0x40 + device_id 	Talk, device (0-30)
	void untalk(void) {};            // 0x5F				Untalk, all devices 
	void data(void) {};              // 0x60 + channel		Open Channel/Data, Secondary Address / Channel (0–15)
	void close(void) {};             // 0xE0 + channel		Close, Secondary Address / Channel (0–15)
	void open(void) {};              // 0xF0 + channel		Open, Secondary Address / Channel (0–15)

	void receiveCommand(void);
	
	uint8_t receiveByte(void);
	bool sendByte(uint8_t data, bool signalEOI);
	bool turnAround(void);
	bool undoTurnAround(void);
	bool timeoutWait(uint8_t iecPIN, IECline lineStatus);	

	// true => PULL => DIGI_LOW
	inline void pull(uint8_t pin)
	{
		set_pin_mode(pin, OUTPUT);
		espDigitalWrite(pin, LOW);
	}

	// false => RELEASE => DIGI_HIGH
	inline void release(uint8_t pin)
	{
		set_pin_mode(pin, OUTPUT);
		espDigitalWrite(pin, HIGH);
	}

	inline IECline status(uint8_t pin)
	{
		#ifndef SPLIT_LINES
			// To be able to read line we must be set to input, not driving.
			set_pin_mode(pin, INPUT);
		#endif

		return espDigitalRead(pin) ? released : pulled;
	}

	inline uint8_t get_bit(uint8_t pin)
       {
		return espDigitalRead(pin);
	}

	inline void set_bit(uint8_t pin, uint8_t bit)
	{
		return espDigitalWrite(pin, bit);
	}

	inline void set_pin_mode(uint8_t pin, uint8_t mode) {
#if defined(ESP8266)		
		if(mode == OUTPUT){
			GPF(pin) = GPFFS(GPFFS_GPIO(pin));//Set mode to GPIO
			GPC(pin) = (GPC(pin) & (0xF << GPCI)); //SOURCE(GPIO) | DRIVER(NORMAL) | INT_TYPE(UNCHANGED) | WAKEUP_ENABLE(DISABLED)
			GPES = (1 << pin); //Enable
		} else if(mode == INPUT){
			GPF(pin) = GPFFS(GPFFS_GPIO(pin));//Set mode to GPIO
			GPEC = (1 << pin); //Disable
			GPC(pin) = (GPC(pin) & (0xF << GPCI)) | (1 << GPCD); //SOURCE(GPIO) | DRIVER(OPEN_DRAIN) | INT_TYPE(UNCHANGED) | WAKEUP_ENABLE(DISABLED)
		}
#elif defined(ESP32)
		pinMode( pin, mode );
#endif
	}

	inline void ICACHE_RAM_ATTR espDigitalWrite(uint8_t pin, uint8_t val) {
#if defined(ESP8266)
		if(val) GPOS = (1 << pin);
		else GPOC = (1 << pin);
#elif defined(ESP32)
		digitalWrite(pin, val);
#endif
	}

	inline uint8_t ICACHE_RAM_ATTR espDigitalRead(uint8_t pin) {
		uint8_t val = -1;
#if defined(ESP8266)
		val = GPIP(pin);
#elif defined(ESP32)
		val = digitalRead(pin);
#endif
		return val;
	}

public:
    void setup(void);
    void service(void);
	void reset(void);
    void shutdown(void);

    uint8_t numDevices(void);
    void addDevice(iecDevice *pDevice, uint8_t device_id);
    void remDevice(iecDevice *pDevice);
    iecDevice *deviceById(uint8_t device_id);
    void changeDeviceId(iecDevice *pDevice, uint8_t device_id);

    iecCassette *getCassette(void) { return _cassetteDev; }
    iecPrinter *getPrinter(void) { return _printerdev; }
    iecCPM *getCPM(void) { return _cpmDev; }


	ATNData ATN;

	// Sends a byte. The communication must be in the correct state: a load command
	// must just have been recieved. If something is not OK, FALSE is returned.
	bool send(uint8_t data);

	// Sends a string.
	bool send(uint8_t *data, uint16_t len);

	// Same as IEC_send, but indicating that this is the last byte.
	bool sendEOI(uint8_t data);

	// A special send command that informs file not found condition
	bool sendFNF(void);

	// Recieve a byte
	uint8_t receive(void);

	// Receive a string.
	bool receive(uint8_t *data, uint16_t len);

	// Enabled Device Bit Mask
	uint32_t enabledDevices;
	bool isDeviceEnabled(const uint8_t deviceNumber);
	void enableDevice(const uint8_t deviceNumber);
	void disableDevice(const uint8_t deviceNumber);

	IECState state(void) const;
};

extern iecBus IEC;

#endif // IECBUS_H