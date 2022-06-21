
#include "Arduino.h"
#include "lmic.h"

/*  This EUI must be in little-endian format, so least-significant-byte
    first. When copying an EUI from ttnctl output, this means to reverse
    the bytes.
*/

// Copy the value from Device EUI from the TTN console in LSB mode.
static const u1_t PROGMEM DEVEUI[8]= { 0xA3, 0x24, 0x05, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 }; // Device EUI, hex, lsb

// Copy the value from Application EUI from the TTN console in LSB mode
static const u1_t PROGMEM APPEUI[8]= {  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Application EUI, hex, lsb

/*  This key should be in big endian format (or, since it is not really a
    number but a block of memory, endianness does not really apply). In
    practice, a key taken from ttnctl can be copied as-is. Anyway its in MSB mode.
*/
static const u1_t PROGMEM APPKEY[16] = { 0xC8, 0x7A, 0xB5, 0x7F, 0x84, 0x33, 0xA7, 0x15, 0x1F, 0x3B, 0x25, 0x43, 0x60, 0x84, 0x5C, 0xB3 }; // App Key, hex, msb

