#include "wifi_tcp_ipd_parser.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t at[128];
    size_t at_length;
    uint8_t tcp[128];
    size_t tcp_length;
    uint8_t last_link_id;
} Capture;

static void CaptureAt(void *context, uint8_t byte)
{
    Capture *capture = (Capture *)context;
    assert(capture->at_length < sizeof(capture->at));
    capture->at[capture->at_length++] = byte;
}

static void CaptureTcp(void *context, uint8_t link_id, uint8_t byte)
{
    Capture *capture = (Capture *)context;
    assert(capture->tcp_length < sizeof(capture->tcp));
    capture->last_link_id = link_id;
    capture->tcp[capture->tcp_length++] = byte;
}

static void FeedBytes(WifiTcpIpdParser *parser, Capture *capture,
                      const uint8_t *bytes, size_t length)
{
    size_t index;
    for (index = 0U; index < length; index++) {
        WifiTcpIpdParser_Feed(parser, bytes[index], CaptureAt, CaptureTcp, capture);
    }
}

static void TestPassThroughAndFragmentedSingleLinkFrame(void)
{
    WifiTcpIpdParser parser;
    Capture capture = {0};
    static const uint8_t first[] = "OK\r\n+IP";
    static const uint8_t second[] = "D,4:AB\0D\r\n";

    WifiTcpIpdParser_Init(&parser);
    FeedBytes(&parser, &capture, first, sizeof(first) - 1U);
    FeedBytes(&parser, &capture, second, sizeof(second) - 1U);

    assert(capture.at_length == 6U);
    assert(memcmp(capture.at, "OK\r\n\r\n", 6U) == 0);
    assert(capture.tcp_length == 4U);
    assert(capture.tcp[0] == (uint8_t)'A');
    assert(capture.tcp[1] == (uint8_t)'B');
    assert(capture.tcp[2] == 0U);
    assert(capture.tcp[3] == (uint8_t)'D');
    assert(capture.last_link_id == 0U);
    assert(capture.at[4] == (uint8_t)'\r');
    assert(capture.at[5] == (uint8_t)'\n');
}

static void TestMultipleLinkHeaderRoutesOnlyLinkZero(void)
{
    WifiTcpIpdParser parser;
    Capture capture = {0};
    static const uint8_t frame[] = "+IPD,0,3:abc+IPD,1,2:zz+IPD,3:xyz";

    WifiTcpIpdParser_Init(&parser);
    FeedBytes(&parser, &capture, frame, sizeof(frame) - 1U);

    assert(capture.tcp_length == 6U);
    assert(memcmp(capture.tcp, "abcxyz", 6U) == 0);
    assert(capture.at_length == 0U);
}

static void TestPrefixInsidePayloadIsNotReparsed(void)
{
    WifiTcpIpdParser parser;
    Capture capture = {0};
    static const uint8_t payload[] = "a+IPD,3:xyz";
    static const uint8_t frame[] = "+IPD,11:a+IPD,3:xyz";

    (void)payload;
    WifiTcpIpdParser_Init(&parser);
    FeedBytes(&parser, &capture, frame, sizeof(frame) - 1U);

    assert(capture.tcp_length == 11U);
    assert(memcmp(capture.tcp, "a+IPD,3:xyz", 11U) == 0);
    assert(capture.at_length == 0U);
}

static void TestFalseAndMalformedHeadersRemainAtText(void)
{
    WifiTcpIpdParser parser;
    Capture capture = {0};
    static const uint8_t input[] = "+INFO\r\n+IPD,x:\r\n+IPD,1,2,3:\r\n";

    WifiTcpIpdParser_Init(&parser);
    FeedBytes(&parser, &capture, input, sizeof(input) - 1U);

    assert(capture.at_length == sizeof(input) - 1U);
    assert(memcmp(capture.at, input, sizeof(input) - 1U) == 0);
    assert(capture.tcp_length == 0U);
}

static void TestZeroLengthFrameAndParserReset(void)
{
    WifiTcpIpdParser parser;
    Capture capture = {0};
    static const uint8_t input[] = "+IPD,0:OK\r\n+IP";

    WifiTcpIpdParser_Init(&parser);
    FeedBytes(&parser, &capture, input, sizeof(input) - 1U);
    assert(capture.at_length == 4U);
    assert(memcmp(capture.at, "OK\r\n", 4U) == 0);
    assert(parser.state == WIFI_TCP_IPD_PREFIX);

    WifiTcpIpdParser_Init(&parser);
    {
        static const uint8_t after_reset[] = "READY\r\n";
        FeedBytes(&parser, &capture, after_reset, sizeof(after_reset) - 1U);
    }
    assert(capture.at_length == 11U);
    assert(memcmp(capture.at + 4U, "READY\r\n", 7U) == 0);
}

int main(void)
{
    TestPassThroughAndFragmentedSingleLinkFrame();
    TestMultipleLinkHeaderRoutesOnlyLinkZero();
    TestPrefixInsidePayloadIsNotReparsed();
    TestFalseAndMalformedHeadersRemainAtText();
    TestZeroLengthFrameAndParserReset();
    puts("wifi TCP IPD parser tests passed");
    return 0;
}
