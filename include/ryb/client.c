#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

// NOTE: This implementation expects even parity and host routing
// furthermore, the bus MUST BE PULLED HIGH AND LOW before transmission
// since the packet 00|0000|0000|00|0 is a possibility.

typedef union
{
    struct
    {
        uint8_t address: 2; // two bits for sender address
        uint16_t data:   8; // one uint16_t data packet
        uint8_t trend:   2; // trend
        uint8_t ext:     2; //padding
        uint8_t parity:  1;

    } fields;
    uint8_t bytes[2]; // define 16 bits, only use 13 for the struct + 2 for the control padding
} DataPacket;

// "memory"
void send(uint16_t a) {
    // REPLACE WITH VENDOR IMPLEMENTATION!!
    return;
}

uint8_t parity(uint16_t w) {
    w ^= w >> 8;
    w ^= w >> 4;
    w ^= w >> 2;
    w ^= w >> 1;
    // Even parity
    return (uint8_t)(w & 1u);
}


/**
 * Constructs, transmits, and returns a 13-bit data or control packet.
 * 
 * @param addr    2-bit sender address (0 to 3).
 * @param dat     8-bit payload for standard data packets. Ignored if control is true.
 * @param trnd    2-bit trend indicator (0 to 2).
 * @param dat1    Upper 4 bits of the payload, used only for control packets.
 * @param dat2    Lower 4 bits of the payload, used only for control packets.
 * @param control Toggles packet type (false for standard data, true for control).
 * @return        Fully constructed DataPacket union.
 */
DataPacket force_execute_send(uint8_t addr, uint8_t dat, uint8_t trnd, uint8_t dat1, uint8_t dat2, bool control) {

    DataPacket packet = {0};

    #define COMB()  ((uint16_t)packet.bytes[0] << 8 | packet.bytes[1] & 0xFF) // combines two bytes
    #define COMBS() ((dat1 & 0x1F) << 5 | (dat2 & 0x1F)) // combines two bytes into a 10-bit data package

    if (!control) { 
        // control defines if the packet is a control packet
        packet.fields.address = addr; //(0-4)
        packet.fields.data = dat; // uint8_t
        packet.fields.trend = trnd; // (0-2)
        packet.fields.parity = parity(COMB());
        
        send(COMB());
        return packet;
    } else {

        packet.fields.address = addr;
        packet.fields.data = COMBS() & 0xFF;
        packet.fields.ext =  COMBS() >> 8 & 0x3;
        packet.fields.parity = parity(COMB());

        send(COMB());
        return packet;
    }
}

DataPacket reconstruct_data_packet(uint16_t package, bool type) {
    DataPacket packet = {0};
    package = package << 0x3;
    // data packet is 13 bits, we need to shift it. type defines if it is a control signal or not
    if ((parity(COMB()) != package & 0x100) && !type) {
        packet.fields.trend = 11;
    } else {
        if (type) {
        } else {
            packet.fields.address = package & 0x180; // first two bits
            packet.fields.data = package & 0x780; // four bits after that
            packet.fields.trend = package & 0x600; // two bits
            packet.fields.parity = package & 0x100; // one bit
        };
    };


}

int main() {}