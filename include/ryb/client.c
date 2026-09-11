#include <stdio.h>
#include <stdint.h>
#include <unistd.h>


// data package
typedef union
{
    struct
    {
        uint8_t address: 2; // two bits for sender address
        uint8_t data:    8; // one uint8_t data packet
        uint8_t trend:   2; // trend
        uint8_t parity:  1;

    } fields;
    uint8_t raw_bytes[2]; // define 16 bits, only use 13 for the struct
} DataPacket;

// "memory"
void send(int8_t) {
    // REPLACE WITH VENDOR IMPLEMENTATION!!
    return;
}

void force_execute_send(uint8_t addr, uint8_t dat, uint8_t trnd, uint8_t prty, bool control) {
    DataPacket packet = {0};
    if (!control) { // control defines if the packet is a control packet
        packet.fields.address = addr; //00, 01, 10, 11 (0-4)
        packet.fields.data = dat; // full uint8_t (0-255)
        packet.fields.trend = trnd; // 00, 01, 10, (11 is not valid, 0-3)
        packet.fields.parity = prty; // 1, 0 (0-1)

        send(packet.raw_bytes[1]);
        send(packet.raw_bytes[2]);
    } else {
        
    }
}

int main() {}