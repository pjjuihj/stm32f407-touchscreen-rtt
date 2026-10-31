#include "wifi_tcp_ipd_parser.h"

#include <stdbool.h>
#include <string.h>

static const uint8_t g_ipd_prefix[] = {'I', 'P', 'D', ','};

static void BeginPrefix(WifiTcpIpdParser *parser)
{
    parser->state = WIFI_TCP_IPD_PREFIX;
    parser->candidate[0] = (uint8_t)'+';
    parser->candidate_length = 1U;
    parser->prefix_index = 0U;
}

static void ClearCandidate(WifiTcpIpdParser *parser)
{
    memset(parser->candidate, 0, sizeof(parser->candidate));
    parser->candidate_length = 0U;
    parser->prefix_index = 0U;
}

static void FlushCandidate(WifiTcpIpdParser *parser,
                           WifiTcpIpdAtByteFn at_byte, void *context)
{
    uint8_t index;
    if (at_byte != NULL) {
        for (index = 0U; index < parser->candidate_length; index++) {
            at_byte(context, parser->candidate[index]);
        }
    }
    ClearCandidate(parser);
}

static uint32_t *CurrentNumber(WifiTcpIpdParser *parser)
{
    return parser->field_index == 0U ? &parser->first_number
                                     : &parser->second_number;
}

static bool BeginPayload(WifiTcpIpdParser *parser)
{
    if (parser->header_invalid != 0U || parser->field_has_digit == 0U) {
        return false;
    }
    if (parser->field_index == 0U) {
        parser->link_id = 0U;
        parser->payload_remaining = parser->first_number;
    } else if (parser->field_index == 1U && parser->first_number <= UINT8_MAX) {
        parser->link_id = (uint8_t)parser->first_number;
        parser->payload_remaining = parser->second_number;
    } else {
        return false;
    }
    return true;
}

static void ResetHeader(WifiTcpIpdParser *parser)
{
    parser->field_index = 0U;
    parser->field_has_digit = 0U;
    parser->header_invalid = 0U;
    parser->first_number = 0U;
    parser->second_number = 0U;
    parser->link_id = 0U;
    parser->payload_remaining = 0U;
}

void WifiTcpIpdParser_Init(WifiTcpIpdParser *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
        parser->state = WIFI_TCP_IPD_NORMAL;
    }
}

static void FeedPrefix(WifiTcpIpdParser *parser, uint8_t byte,
                       WifiTcpIpdAtByteFn at_byte, void *context)
{
    if (parser->prefix_index < sizeof(g_ipd_prefix) &&
        byte == g_ipd_prefix[parser->prefix_index] &&
        parser->candidate_length < sizeof(parser->candidate)) {
        parser->candidate[parser->candidate_length++] = byte;
        parser->prefix_index++;
        if (parser->prefix_index == sizeof(g_ipd_prefix)) {
            parser->state = WIFI_TCP_IPD_HEADER;
            ResetHeader(parser);
        }
        return;
    }

    FlushCandidate(parser, at_byte, context);
    parser->state = WIFI_TCP_IPD_NORMAL;
    if (byte == (uint8_t)'+') {
        BeginPrefix(parser);
    } else if (at_byte != NULL) {
        at_byte(context, byte);
    }
}

static void FeedHeader(WifiTcpIpdParser *parser, uint8_t byte,
                       WifiTcpIpdAtByteFn at_byte, void *context)
{
    uint32_t *number;

    if (byte == (uint8_t)':') {
        bool valid = BeginPayload(parser);
        if (!valid) {
            if (parser->candidate_length < sizeof(parser->candidate)) {
                parser->candidate[parser->candidate_length++] = byte;
            }
            FlushCandidate(parser, at_byte, context);
            parser->state = WIFI_TCP_IPD_NORMAL;
            ResetHeader(parser);
            return;
        }

        ClearCandidate(parser);
        if (parser->payload_remaining == 0U) {
            parser->state = WIFI_TCP_IPD_NORMAL;
            ResetHeader(parser);
        } else {
            parser->state = WIFI_TCP_IPD_PAYLOAD;
        }
        return;
    }

    if (parser->candidate_length >= sizeof(parser->candidate)) {
        FlushCandidate(parser, at_byte, context);
        if (at_byte != NULL) at_byte(context, byte);
        parser->state = WIFI_TCP_IPD_NORMAL;
        ResetHeader(parser);
        return;
    }
    parser->candidate[parser->candidate_length++] = byte;

    if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9') {
        uint32_t digit = (uint32_t)(byte - (uint8_t)'0');
        number = CurrentNumber(parser);
        if (*number > (UINT32_MAX - digit) / 10U) {
            parser->header_invalid = 1U;
        } else {
            *number = (*number * 10U) + digit;
            parser->field_has_digit = 1U;
        }
    } else if (byte == (uint8_t)',' && parser->field_index == 0U &&
               parser->field_has_digit != 0U) {
        parser->field_index = 1U;
        parser->field_has_digit = 0U;
    } else {
        parser->header_invalid = 1U;
    }
}

void WifiTcpIpdParser_Feed(WifiTcpIpdParser *parser, uint8_t byte,
                           WifiTcpIpdAtByteFn at_byte,
                           WifiTcpIpdPayloadByteFn payload_byte,
                           void *context)
{
    if (parser == NULL) {
        if (at_byte != NULL) at_byte(context, byte);
        return;
    }

    if (parser->state == WIFI_TCP_IPD_PAYLOAD) {
        if (parser->link_id == 0U && payload_byte != NULL) {
            payload_byte(context, parser->link_id, byte);
        }
        parser->payload_remaining--;
        if (parser->payload_remaining == 0U) {
            parser->state = WIFI_TCP_IPD_NORMAL;
            ResetHeader(parser);
        }
    } else if (parser->state == WIFI_TCP_IPD_HEADER) {
        FeedHeader(parser, byte, at_byte, context);
    } else if (parser->state == WIFI_TCP_IPD_PREFIX) {
        FeedPrefix(parser, byte, at_byte, context);
    } else if (byte == (uint8_t)'+') {
        BeginPrefix(parser);
    } else if (at_byte != NULL) {
        at_byte(context, byte);
    }
}
