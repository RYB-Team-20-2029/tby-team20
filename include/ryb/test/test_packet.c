/* Purpose: host test of the RYB Comms codec against the guideline's vectors and receive rules.
 * Owning module: management (mgmt/). */

#include <stdint.h>
#include <stdio.h>

#include "ryb/ryb_packet.h"

static int checks;
static int failures;

#define CHECK(cond)                                                                               \
    do {                                                                                          \
        checks++;                                                                                 \
        if (!(cond)) {                                                                            \
            failures++;                                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                \
        }                                                                                         \
    } while (0)

static int popcount13(uint32_t w)
{
    int n = 0;
    for (int i = 0; i < 13; i++) {
        n += (int) ((w >> i) & 1u);
    }
    return n;
}

static void test_guideline_vectors(void)
{
    uint16_t f = 0u;

    CHECK(ryb_pack_appended(2u, 0xA5u, RYB_TREND_INCREASING, &f) == RYB_OK && f == 0x1529u);
    CHECK(ryb_pack_appended(2u, 0xAAu, RYB_TREND_INCREASING, &f) == RYB_OK && f == 0x1551u);
    CHECK(ryb_pack_command(2u, 21u, 10u, &f) == RYB_OK && f == 0x1554u);
    CHECK(ryb_pack_appended(0u, 0x00u, RYB_TREND_NA, &f) == RYB_OK && f == 0x0005u);
}

static void test_pack_rejects(void)
{
    uint16_t f = 0xBEEFu;

    CHECK(ryb_pack_appended(4u, 0u, RYB_TREND_NA, &f) == RYB_ERR_PARAM);
    CHECK(ryb_pack_appended(0u, 0u, (ryb_trend_t) 3, &f) == RYB_ERR_PARAM);
    CHECK(ryb_pack_command(4u, 0u, 0u, &f) == RYB_ERR_PARAM);
    CHECK(ryb_pack_command(2u, 32u, 0u, &f) == RYB_ERR_PARAM);
    CHECK(ryb_pack_command(2u, 0u, 32u, &f) == RYB_ERR_PARAM);
    CHECK(ryb_pack_appended(0u, 0u, RYB_TREND_NA, NULL) == RYB_ERR_PARAM);
    /* A rejected pack must not leave a half-built frame for the caller to send. */
    CHECK(f == 0xBEEFu);
}

/* Every 13-bit word: the decoder must refuse exactly the odd-parity ones. That also covers the
 * guideline's idle-line argument, since all 1s is thirteen 1s and fails parity. */
static void test_parity_exhaustive(void)
{
    int wrong = 0;
    for (uint32_t w = 0u; w <= RYB_FRAME_MASK; w++) {
        ryb_packet_t p;
        const ryb_status_t rc = ryb_frame_decode((uint16_t) w, 0x0Fu, RYB_ADDR_DECISION, &p);
        const int odd = popcount13(w) % 2;
        if (odd != (rc == RYB_ERR_PARITY)) {
            wrong++;
        }
    }
    CHECK(wrong == 0);

    ryb_packet_t p;
    CHECK(ryb_frame_decode(0x1FFFu, 0x0Fu, RYB_ADDR_DECISION, &p) == RYB_ERR_PARITY);
    CHECK(ryb_frame_decode(0x2000u, 0x0Fu, RYB_ADDR_DECISION, &p) == RYB_ERR_PARAM);
}

static void test_round_trips(void)
{
    int wrong = 0;
    for (uint8_t addr = 0u; addr <= RYB_ADDR_MAX; addr++) {
        if (addr == RYB_ADDR_DECISION) {
            continue;
        }
        for (uint32_t data = 0u; data <= 255u; data++) {
            for (int t = 0; t <= (int) RYB_TREND_NA; t++) {
                uint16_t f = 0u;
                ryb_packet_t p;
                if (ryb_pack_appended(addr, (uint8_t) data, (ryb_trend_t) t, &f) != RYB_OK ||
                    ryb_frame_decode(f, 0x0Fu, RYB_ADDR_DECISION, &p) != RYB_OK ||
                    p.kind != RYB_PACKET_APPENDED || p.sender != addr ||
                    p.u.appended.data != data || (int) p.u.appended.trend != t) {
                    wrong++;
                }
            }
        }
    }
    for (uint16_t first = 0u; first <= RYB_SEGMENT_MAX; first++) {
        for (uint16_t second = 0u; second <= RYB_SEGMENT_MAX; second++) {
            uint16_t f = 0u;
            ryb_packet_t p;
            if (ryb_pack_command(RYB_ADDR_DECISION, first, second, &f) != RYB_OK ||
                ryb_frame_decode(f, 0x0Fu, RYB_ADDR_DECISION, &p) != RYB_OK ||
                p.kind != RYB_PACKET_COMMAND || p.u.command.first != first ||
                p.u.command.second != second) {
                wrong++;
            }
        }
    }
    CHECK(wrong == 0);
}

static void test_receive_checks(void)
{
    ryb_packet_t p;

    /* addr 0, data 0, trend 11: two bits set, so parity 0 and the frame is 0x0006. */
    CHECK(ryb_frame_decode(0x0006u, 0x0Fu, RYB_ADDR_DECISION, &p) == RYB_ERR_TREND);
    /* The same low bits from the decision module are a command's second segment, not a trend. */
    CHECK(ryb_frame_decode(0x1007u, 0x0Fu, RYB_ADDR_DECISION, &p) == RYB_OK &&
          p.kind == RYB_PACKET_COMMAND && p.u.command.first == 0u && p.u.command.second == 3u);

    /* A valid heartbeat frame on a link that only expects the sound module. */
    CHECK(ryb_frame_decode(0x0005u, 1u << RYB_ADDR_SOUND, RYB_ADDR_DECISION, &p) ==
          RYB_ERR_ADDRESS);
    /* Parity is checked before the address, as the guideline orders them. */
    CHECK(ryb_frame_decode(0x0004u, 1u << RYB_ADDR_SOUND, RYB_ADDR_DECISION, &p) ==
          RYB_ERR_PARITY);

    /* One receiver path decodes both types: the command example split 8 + 2 reads as data
     * 10101010 and trend 10, exactly as the guideline says. */
    CHECK(ryb_frame_decode(0x1554u, 0x0Fu, RYB_ADDR_MOTOR, &p) == RYB_OK &&
          p.kind == RYB_PACKET_APPENDED && p.u.appended.data == 0xAAu &&
          p.u.appended.trend == RYB_TREND_NA);
}

int main(void)
{
    test_guideline_vectors();
    test_pack_rejects();
    test_parity_exhaustive();
    test_round_trips();
    test_receive_checks();

    if (failures != 0) {
        printf("%d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("all %d checks passed\n", checks);
    return 0;
}
