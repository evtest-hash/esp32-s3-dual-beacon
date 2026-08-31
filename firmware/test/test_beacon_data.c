#include "beacon_data.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, msg)                                           \
    do {                                                           \
        if (cond) { printf("  ok   %s\n", (msg)); }                \
        else { printf("  FAIL %s  (%s:%d)\n", (msg), __FILE__, __LINE__); g_fail++; } \
    } while (0)

static void test_derive_typical(void)
{
    printf("test_derive_typical\n");
    const uint8_t mac[6] = { 0x84, 0xF7, 0x03, 0xA1, 0xB2, 0xC3 };
    device_id_t id;
    device_id_derive(mac, "S3-BEACON", &id);

    CHECK(strcmp(id.ssid, "S3-BEACON-A1B2C3") == 0, "ssid is built from mac[3..5] as uppercase hex");
    CHECK(id.major == 0x00A1, "major == mac[3]");
    CHECK(id.minor == 0xB2C3, "minor == (mac[4]<<8)|mac[5]");
}

static void test_derive_all_zero(void)
{
    printf("test_derive_all_zero\n");
    const uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };
    device_id_t id;
    device_id_derive(mac, "X", &id);

    CHECK(strcmp(id.ssid, "X-000000") == 0, "an all-zero MAC zero-pads to 6 hex digits");
    CHECK(id.major == 0, "major is 0 for an all-zero MAC");
    CHECK(id.minor == 0, "minor is 0 for an all-zero MAC");
}

static void test_derive_all_ff(void)
{
    printf("test_derive_all_ff\n");
    const uint8_t mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    device_id_t id;
    device_id_derive(mac, "X", &id);

    CHECK(strcmp(id.ssid, "X-FFFFFF") == 0, "hex uses uppercase letters");
    CHECK(id.major == 0x00FF, "major's upper bound is 0x00FF, no overflow into the high byte");
    CHECK(id.minor == 0xFFFF, "minor's upper bound is 0xFFFF");
}

static void test_derive_ssid_always_terminated(void)
{
    printf("test_derive_ssid_always_terminated\n");
    /* Prefix long enough to fill the buffer, to verify there's no overflow
     * and a NUL terminator is always present */
    const uint8_t mac[6] = { 0, 0, 0, 0x11, 0x22, 0x33 };
    device_id_t id;
    memset(&id, 0xAA, sizeof(id));
    device_id_derive(mac, "0123456789012345678901234567890123456789", &id);

    CHECK(id.ssid[DEVICE_ID_SSID_MAX - 1] == '\0', "ssid's last byte is always NUL");
    CHECK(strlen(id.ssid) <= DEVICE_ID_SSID_MAX - 1, "ssid length never exceeds the buffer");
    CHECK(strncmp(id.ssid, "0123456789", 10) == 0, "with an overlong prefix, ssid still starts with the prefix");
}

static void test_derive_suffix_survives(void)
{
    printf("test_derive_suffix_survives\n");
    /* The suffix carries per-device uniqueness; verify a normal prefix keeps it. */
    const uint8_t mac[6] = { 0, 0, 0, 0x11, 0x22, 0x33 };
    device_id_t id;
    device_id_derive(mac, "S3-BEACON", &id);

    size_t len = strlen(id.ssid);
    CHECK(len >= 7, "ssid is long enough to hold the suffix");
    CHECK(strcmp(id.ssid + len - 6, "112233") == 0, "the MAC suffix is preserved intact at the end of ssid");
    CHECK(strcmp(id.ssid, "S3-BEACON-112233") == 0, "the full ssid matches expectations");
}

static void test_ibeacon_payload_layout(void)
{
    printf("test_ibeacon_payload_layout\n");
    const uint8_t uuid[16] = {
        0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2,
        0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0
    };
    uint8_t buf[IBEACON_ADV_LEN];
    memset(buf, 0x5A, sizeof(buf));

    ibeacon_build_payload(uuid, 0x00A1, 0xB2C3, (int8_t)0xC5, buf);

    const uint8_t expect_header[9] = {
        0x02, 0x01, 0x06,
        0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15
    };
    CHECK(memcmp(buf, expect_header, 9) == 0, "the first 9 bytes are Flags + iBeacon header");
    /* Derived from IBEACON_ADV_LEN, so these fail if the length ever changes
     * without the header being updated to match. */
    CHECK(buf[3] == IBEACON_ADV_LEN - 4, "AD length byte covers type + data");
    CHECK(buf[8] == IBEACON_ADV_LEN - 9, "iBeacon data length byte matches payload");
    CHECK(memcmp(buf + 9, uuid, 16) == 0, "UUID is written in its original byte order at [9..24]");
    CHECK(buf[25] == 0x00 && buf[26] == 0xA1, "Major is written big-endian at [25..26]");
    CHECK(buf[27] == 0xB2 && buf[28] == 0xC3, "Minor is written big-endian at [27..28]");
    CHECK(buf[29] == 0xC5, "Measured Power is written at [29]");
}

static void test_ibeacon_payload_writes_every_byte(void)
{
    printf("test_ibeacon_payload_writes_every_byte\n");
    /* Build twice with two different pre-fill values; the results must be
     * identical -- if any byte were left unwritten, the two results would
     * differ. */
    const uint8_t uuid[16] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    uint8_t a[IBEACON_ADV_LEN], b[IBEACON_ADV_LEN];
    memset(a, 0x00, sizeof(a));
    memset(b, 0xFF, sizeof(b));

    ibeacon_build_payload(uuid, 0x1234, 0x5678, (int8_t)-59, a);
    ibeacon_build_payload(uuid, 0x1234, 0x5678, (int8_t)-59, b);

    CHECK(memcmp(a, b, IBEACON_ADV_LEN) == 0, "all 30 bytes are written, no leftover pre-fill");
}

static void test_ibeacon_zero_values(void)
{
    printf("test_ibeacon_zero_values\n");
    const uint8_t uuid[16] = { 0 };
    uint8_t buf[IBEACON_ADV_LEN];
    ibeacon_build_payload(uuid, 0, 0, 0, buf);

    CHECK(buf[3] == 0x1A, "the length byte is always 0x1A (26), regardless of the data");
    CHECK(buf[25] == 0 && buf[26] == 0, "both bytes are 0 when Major is 0");
    CHECK(buf[29] == 0, "0 is written when tx_power is 0");
}


static void test_scan_rsp_name_comes_from_the_mac(void)
{
    printf("test_scan_rsp_name_comes_from_the_mac\n");
    /* The name is never a literal: it is whatever device_id_derive built out
     * of this board's own MAC, so two boards never advertise the same one. */
    const uint8_t mac[6] = { 0x68, 0xEE, 0x8F, 0x6D, 0x3F, 0x21 };
    device_id_t id;
    device_id_derive(mac, "S3-BEACON", &id);

    uint8_t buf[SCAN_RSP_MAX_LEN];
    size_t len = scan_rsp_build_name(id.ssid, buf);

    CHECK(len == 18, "16-char name yields 18 bytes: length, type, 16 chars");
    CHECK(buf[0] == 17, "length byte counts the type byte plus the name");
    CHECK(buf[1] == 0x09, "type is Complete Local Name");
    CHECK(memcmp(buf + 2, "S3-BEACON-6D3F21", 16) == 0, "the name is the MAC-derived SSID");
}

static void test_scan_rsp_name_differs_with_the_mac(void)
{
    printf("test_scan_rsp_name_differs_with_the_mac\n");
    const uint8_t mac_a[6] = { 0x68, 0xEE, 0x8F, 0x6D, 0x3F, 0x21 };
    const uint8_t mac_b[6] = { 0x68, 0xEE, 0x8F, 0x11, 0x22, 0x33 };
    device_id_t a, b;
    uint8_t buf_a[SCAN_RSP_MAX_LEN], buf_b[SCAN_RSP_MAX_LEN];

    device_id_derive(mac_a, "S3-BEACON", &a);
    device_id_derive(mac_b, "S3-BEACON", &b);
    size_t len_a = scan_rsp_build_name(a.ssid, buf_a);
    size_t len_b = scan_rsp_build_name(b.ssid, buf_b);

    CHECK(len_a == len_b && len_a > 0, "same prefix gives the same length");
    CHECK(memcmp(buf_a, buf_b, len_a) != 0, "two boards do not advertise the same name");
}

static void test_scan_rsp_empty_name(void)
{
    printf("test_scan_rsp_empty_name\n");
    uint8_t buf[SCAN_RSP_MAX_LEN];
    memset(buf, 0xAA, sizeof(buf));

    CHECK(scan_rsp_build_name("", buf) == 0, "an empty name writes nothing");
    CHECK(buf[0] == 0xAA, "the buffer is left untouched when there is nothing to write");
}

static void test_scan_rsp_longest_name_that_fits(void)
{
    printf("test_scan_rsp_longest_name_that_fits\n");
    /* 31 bytes of AD space, 2 of them the length and type fields. */
    const char name[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123";   /* 29 chars */
    uint8_t buf[SCAN_RSP_MAX_LEN];
    size_t len = scan_rsp_build_name(name, buf);

    CHECK(len == SCAN_RSP_MAX_LEN, "29 chars fill the scan response exactly");
    CHECK(buf[1] == 0x09, "a name that fits is still Complete, not Shortened");
    CHECK(memcmp(buf + 2, name, 29) == 0, "all 29 chars are written");
}

static void test_scan_rsp_name_too_long_is_shortened(void)
{
    printf("test_scan_rsp_name_too_long_is_shortened\n");
    const char name[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234";  /* 30 chars, one too many */
    uint8_t buf[SCAN_RSP_MAX_LEN];
    memset(buf, 0xAA, sizeof(buf));
    size_t len = scan_rsp_build_name(name, buf);

    CHECK(len == SCAN_RSP_MAX_LEN, "the result never exceeds the 31-byte AD space");
    CHECK(buf[0] == 30, "the length byte matches what was actually written");
    CHECK(buf[1] == 0x08, "a truncated name is advertised as Shortened, not Complete");
    CHECK(memcmp(buf + 2, name, 29) == 0, "the first 29 chars are kept");
}

int main(void)
{
    test_derive_typical();
    test_derive_all_zero();
    test_derive_all_ff();
    test_derive_ssid_always_terminated();
    test_derive_suffix_survives();
    test_ibeacon_payload_layout();
    test_ibeacon_payload_writes_every_byte();
    test_ibeacon_zero_values();
    test_scan_rsp_name_comes_from_the_mac();
    test_scan_rsp_name_differs_with_the_mac();
    test_scan_rsp_empty_name();
    test_scan_rsp_longest_name_that_fits();
    test_scan_rsp_name_too_long_is_shortened();

    printf("\n%s (%d failures)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
