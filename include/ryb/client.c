#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

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
void send(int16_t a) {
    // REPLACE WITH VENDOR IMPLEMENTATION!!
    return;
}

uint8_t parity(int16_t) {
    //INOP
    return;
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

int main() {}