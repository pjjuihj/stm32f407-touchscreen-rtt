#ifndef WIFI_TCP_IPD_PARSER_H
#define WIFI_TCP_IPD_PARSER_H

#include <stdint.h>

#define WIFI_TCP_IPD_HEADER_CAPACITY 32U

typedef void (*WifiTcpIpdAtByteFn)(void *context, uint8_t byte);
typedef void (*WifiTcpIpdPayloadByteFn)(void *context, uint8_t link_id,
                                        uint8_t byte);

typedef enum {
    WIFI_TCP_IPD_NORMAL = 0,
    WIFI_TCP_IPD_PREFIX,
    WIFI_TCP_IPD_HEADER,
    WIFI_TCP_IPD_PAYLOAD
} WifiTcpIpdParserState;

typedef struct {
    WifiTcpIpdParserState state;
    uint8_t candidate[WIFI_TCP_IPD_HEADER_CAPACITY];
    uint8_t candidate_length;
    uint8_t prefix_index;
    uint8_t field_index;
    uint8_t field_has_digit;
    uint8_t header_invalid;
    uint8_t link_id;
    uint32_t first_number;
    uint32_t second_number;
    uint32_t payload_remaining;
} WifiTcpIpdParser;

void WifiTcpIpdParser_Init(WifiTcpIpdParser *parser);
void WifiTcpIpdParser_Feed(WifiTcpIpdParser *parser, uint8_t byte,
                           WifiTcpIpdAtByteFn at_byte,
                           WifiTcpIpdPayloadByteFn payload_byte,
                           void *context);

#endif
