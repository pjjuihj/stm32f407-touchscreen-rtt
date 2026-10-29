#include "wifi.h"
#include "wifi_uart_rx_recovery.h"

#include "usart.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define WIFI_RESPONSE_CAPACITY 384U
#define WIFI_RX_CAPACITY 512U
#define WIFI_RX_DRAIN_BUDGET 384U
#define WIFI_COMMAND_CAPACITY 160U
#define WIFI_CONNECT_TIMEOUT_MS 60000U
#define WIFI_TCP_CONNECT_TIMEOUT_MS 20000U
#define WIFI_TCP_SEND_TIMEOUT_MS 10000U
#define WIFI_PING_TIMEOUT_MS 10000U
#define WIFI_QUERY_TIMEOUT_MS 1200U
#define WIFI_SYNC_RETRY_MS 1000U
#define WIFI_IP_QUERY_RETRY_MS 1000U
#define WIFI_IP_QUERY_MAX_ATTEMPTS 5U
#define WIFI_TCP_DETAIL_CAPACITY 96U

typedef enum {
    WIFI_PHASE_PROBE_SEND = 0,
    WIFI_PHASE_PROBE_WAIT,
    WIFI_PHASE_MODERN_MODE_SEND,
    WIFI_PHASE_MODERN_MODE_WAIT,
    WIFI_PHASE_LEGACY_MODE_SEND,
    WIFI_PHASE_LEGACY_MODE_WAIT,
    WIFI_PHASE_STARTUP_TCP_QUERY_SEND,
    WIFI_PHASE_STARTUP_TCP_QUERY_WAIT,
    WIFI_PHASE_APSTA_MODE_SET_SEND,
    WIFI_PHASE_APSTA_MODE_SET_WAIT,
    WIFI_PHASE_STATION_QUERY_SEND,
    WIFI_PHASE_STATION_QUERY_WAIT,
    WIFI_PHASE_STATION_FALLBACK_QUERY_SEND,
    WIFI_PHASE_STATION_FALLBACK_QUERY_WAIT,
    WIFI_PHASE_IP_QUERY_SEND,
    WIFI_PHASE_IP_QUERY_WAIT,
    WIFI_PHASE_IP_QUERY_RETRY_WAIT,
    WIFI_PHASE_CONNECT_PREPARE,
    WIFI_PHASE_CONNECT_TCP_QUERY_SEND,
    WIFI_PHASE_CONNECT_TCP_QUERY_WAIT,
    WIFI_PHASE_CONNECT_VERSION_QUERY_SEND,
    WIFI_PHASE_CONNECT_VERSION_QUERY_WAIT,
    WIFI_PHASE_DIALECT_PROBE_SEND,
    WIFI_PHASE_DIALECT_PROBE_WAIT,
    WIFI_PHASE_STORE_RAM_SEND,
    WIFI_PHASE_STORE_RAM_WAIT,
    WIFI_PHASE_CONNECT_MODE_QUERY_SEND,
    WIFI_PHASE_CONNECT_MODE_QUERY_WAIT,
    WIFI_PHASE_CONNECT_MODE_SET_SEND,
    WIFI_PHASE_CONNECT_MODE_SET_WAIT,
    WIFI_PHASE_JOIN_SEND,
    WIFI_PHASE_JOIN_WAIT,
    WIFI_PHASE_PERSIST_STORE_SEND,
    WIFI_PHASE_PERSIST_STORE_WAIT,
    WIFI_PHASE_PERSIST_MODE_SEND,
    WIFI_PHASE_PERSIST_MODE_WAIT,
    WIFI_PHASE_PERSIST_JOIN_SEND,
    WIFI_PHASE_PERSIST_JOIN_WAIT,
    WIFI_PHASE_PERSIST_AUTOCONN_SEND,
    WIFI_PHASE_PERSIST_AUTOCONN_WAIT,
    WIFI_PHASE_PERSIST_RAM_SEND,
    WIFI_PHASE_PERSIST_RAM_WAIT,
    WIFI_PHASE_DNS_QUERY_SEND,
    WIFI_PHASE_DNS_QUERY_WAIT,
    WIFI_PHASE_DNS_GATEWAY_STORE_RAM_SEND,
    WIFI_PHASE_DNS_GATEWAY_STORE_RAM_WAIT,
    WIFI_PHASE_DNS_GATEWAY_SET_SEND,
    WIFI_PHASE_DNS_GATEWAY_SET_WAIT,
    WIFI_PHASE_TCP_QUERY_SEND,
    WIFI_PHASE_TCP_QUERY_WAIT,
    WIFI_PHASE_TCP_MUX_QUERY_SEND,
    WIFI_PHASE_TCP_MUX_QUERY_WAIT,
    WIFI_PHASE_TCP_MODE_QUERY_SEND,
    WIFI_PHASE_TCP_MODE_QUERY_WAIT,
    WIFI_PHASE_TCP_NORMAL_MODE_SET_SEND,
    WIFI_PHASE_TCP_NORMAL_MODE_SET_WAIT,
    WIFI_PHASE_TCP_NORMAL_MODE_QUERY_SEND,
    WIFI_PHASE_TCP_NORMAL_MODE_QUERY_WAIT,
    WIFI_PHASE_TCP_CONNECT_SEND,
    WIFI_PHASE_TCP_CONNECT_WAIT,
    WIFI_PHASE_TCP_SEND_COMMAND_SEND,
    WIFI_PHASE_TCP_SEND_PROMPT_WAIT,
    WIFI_PHASE_TCP_SEND_DATA,
    WIFI_PHASE_TCP_SEND_RESULT_WAIT,
    WIFI_PHASE_TCP_CLOSE_SEND,
    WIFI_PHASE_TCP_CLOSE_WAIT,
    WIFI_PHASE_PING_SEND,
    WIFI_PHASE_PING_WAIT,
    WIFI_PHASE_CLEAR_SEND,
    WIFI_PHASE_CLEAR_WAIT,
    WIFI_PHASE_IDLE,
    WIFI_PHASE_SYNC_RETRY_WAIT,
    WIFI_PHASE_MODULE_VERSION_QUERY_SEND,
    WIFI_PHASE_MODULE_VERSION_QUERY_WAIT,
    WIFI_PHASE_TX_FAULT
} WiFiPhase;

typedef struct {
    WiFiState state;
    WiFiAtDialect dialect;
    WiFiAtTcpStatus tcp_status;
    WiFiAtTcpMuxMode tcp_mux_mode;
    WiFiAtTcpMode tcp_mode;
    volatile WiFiPhase phase;
    uint32_t deadline;
    uint32_t next_sync_retry;
    uint32_t next_ip_query;
    uint32_t retry_started_at;
    uint8_t retry_count;
    uint8_t ip_query_attempts;
    uint8_t mode;
    uint8_t mode_known;
    uint8_t apsta_set_attempted;
    WiFiApStaStatus apsta;
    WiFiAtStationStatus station;
    WiFiIpStatus ip;
    WiFiNetworkStatus network;
    uint8_t station_link_info_valid;
    uint8_t station_channel;
    int8_t station_rssi;
    volatile uint8_t tx_active;
    uint8_t rx_rearm_deferred;
    uint8_t rx_byte;
    uint8_t rx_data[WIFI_RX_CAPACITY];
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;
    volatile uint8_t rx_overflow;
    uint8_t query_result_ready;
    uint8_t status_check_pending;
    uint8_t tcp_query_result_ready;
    uint8_t tcp_mux_check_pending;
    uint8_t tcp_mode_check_pending;
    uint8_t dns_query_pending;
    uint8_t dns_gateway_pending;
    uint8_t tcp_operation_result_ready;
    uint8_t clear_pending;
    uint8_t clear_command_started;
    uint8_t connect_pending;
    uint8_t credentials_loaded;
    uint8_t persist_requested;
    uint8_t persist_store_maybe_enabled;
    uint8_t persist_store_mode_unknown;
    uint8_t persist_join_associated;
    uint8_t persist_join_ok;
    uint8_t persist_failure_pending;
    uint8_t persist_abort_requested;
    uint8_t sync_started;
    uint16_t response_length;
    uint16_t tcp_payload_length;
    uint16_t tcp_port;
    WiFiTcpOperationResult tcp_operation_result;
    char response[WIFI_RESPONSE_CAPACITY];
    char tx_command[WIFI_COMMAND_CAPACITY];
    char ssid[33];
    char password[65];
    char tcp_host[WIFI_TCP_HOST_MAX_LENGTH + 1U];
    char ping_host[WIFI_TCP_HOST_MAX_LENGTH + 1U];
    char station_ip[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_gateway[WIFI_AT_IPV4_TEXT_CAPACITY];
    char dns_gateway_expected[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_netmask[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_bssid[WIFI_AT_BSSID_TEXT_CAPACITY];
    uint8_t tcp_payload[WIFI_TCP_MAX_PAYLOAD_LENGTH];
    WiFiUartRxRecovery rx_recovery;
} WiFiContext;

static WiFiContext g_wifi;
static const char *g_connect_reject_reason = "NOT_REJECTED";

static void SecureClear(void *buffer, size_t length)
{
    volatile uint8_t *byte = (volatile uint8_t *)buffer;
    while (length-- > 0U) {
        *byte++ = 0U;
    }
}

static void ClearStationIpAddress(void)
{
    SecureClear(g_wifi.station_ip, sizeof(g_wifi.station_ip));
    SecureClear(g_wifi.station_gateway, sizeof(g_wifi.station_gateway));
    SecureClear(g_wifi.station_netmask, sizeof(g_wifi.station_netmask));
}

static void ClearDnsGatewayExpected(void)
{
    SecureClear(g_wifi.dns_gateway_expected, sizeof(g_wifi.dns_gateway_expected));
}

static void ClearStationLinkInfo(void)
{
    SecureClear(g_wifi.station_bssid, sizeof(g_wifi.station_bssid));
    g_wifi.station_channel = 0U;
    g_wifi.station_rssi = 0;
    g_wifi.station_link_info_valid = 0U;
}

static bool Contains(const char *text, size_t length, const char *needle)
{
    size_t needle_length = strlen(needle);
    size_t index;
    if (needle_length == 0U || needle_length > length) {
        return false;
    }
    for (index = 0U; index <= length - needle_length; index++) {
        if (memcmp(&text[index], needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool ContainsNumericPingReply(const char *text, size_t length)
{
    size_t index;

    for (index = 0U; index < length; index++) {
        size_t digit;

        if (text[index] != '+' || (index > 0U && text[index - 1U] != '\n')) {
            continue;
        }
        digit = index + 1U;
        while (digit < length && text[digit] >= '0' && text[digit] <= '9') {
            digit++;
        }
        if (digit > index + 1U &&
            (digit == length || text[digit] == '\r' || text[digit] == '\n')) {
            return true;
        }
    }
    return false;
}

static bool ContainsCaseInsensitive(const char *text, size_t length,
                                    const char *needle)
{
    size_t needle_length = strlen(needle);
    size_t index;

    if (needle_length == 0U || needle_length > length) {
        return false;
    }
    for (index = 0U; index <= length - needle_length; index++) {
        size_t offset;
        for (offset = 0U; offset < needle_length; offset++) {
            unsigned char text_byte = (unsigned char)text[index + offset];
            unsigned char needle_byte = (unsigned char)needle[offset];
            if (tolower(text_byte) != tolower(needle_byte)) {
                break;
            }
        }
        if (offset == needle_length) {
            return true;
        }
    }
    return false;
}

static const char *ClassifyTcpConnectFailure(bool timed_out)
{
    const char *response = g_wifi.response;
    size_t response_length = g_wifi.response_length;
    bool has_error = ContainsCaseInsensitive(response, response_length, "ERROR");
    bool has_fail = ContainsCaseInsensitive(response, response_length, "FAIL");

    if (ContainsCaseInsensitive(response, response_length, "DNS") &&
        (has_error || has_fail)) {
        return "DNS_FAILURE";
    }
    if (ContainsCaseInsensitive(response, response_length, "CONNECT FAIL")) {
        return "CONNECT_FAILURE";
    }
    if (ContainsCaseInsensitive(response, response_length, "ALREADY CONNECTED")) {
        return "ALREADY_CONNECTED";
    }
    if (ContainsCaseInsensitive(response, response_length, "BUSY")) {
        return "MODULE_BUSY";
    }
    if (has_error) {
        return "AT_ERROR";
    }
    if (has_fail) {
        return "AT_FAIL";
    }
    if (timed_out) {
        return response_length == 0U ? "TIMEOUT_NO_RESPONSE" : "TIMEOUT_WITH_RESPONSE";
    }
    return "UNCLASSIFIED_RESPONSE";
}

static bool IsTcpFailureDetailLine(const char *line, size_t length)
{
    return ContainsCaseInsensitive(line, length, "ERR CODE:") ||
           ContainsCaseInsensitive(line, length, "ERROR") ||
           ContainsCaseInsensitive(line, length, "FAIL") ||
           ContainsCaseInsensitive(line, length, "BUSY") ||
           ContainsCaseInsensitive(line, length, "DNS") ||
           ContainsCaseInsensitive(line, length, "ALREADY CONNECTED");
}

static bool IsAtCommandEcho(const char *line, size_t length)
{
    size_t offset = 0U;

    while (offset < length && isspace((unsigned char)line[offset])) {
        offset++;
    }
    return length - offset >= 3U &&
           tolower((unsigned char)line[offset]) == 'a' &&
           tolower((unsigned char)line[offset + 1U]) == 't' &&
           line[offset + 2U] == '+';
}

static bool MatchesCaseInsensitive(const char *left, const char *right, size_t length)
{
    size_t index;

    for (index = 0U; index < length; index++) {
        if (tolower((unsigned char)left[index]) !=
            tolower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static size_t MatchSensitiveValueAt(const char *text, size_t offset, size_t length)
{
    const char *values[] = {
        g_wifi.ssid,
        g_wifi.password,
        g_wifi.tcp_host,
        g_wifi.station_ip
    };
    size_t value_index;

    for (value_index = 0U; value_index < sizeof(values) / sizeof(values[0]);
         value_index++) {
        size_t value_length = strlen(values[value_index]);
        if (value_length > 0U && value_length <= length - offset &&
            MatchesCaseInsensitive(&text[offset], values[value_index], value_length)) {
            return value_length;
        }
    }
    return 0U;
}

static size_t SanitizeTcpFailureDetail(const char *line, size_t length,
                                       char *output, size_t capacity)
{
    static const char redacted[] = "<redacted>";
    size_t input_index = 0U;
    size_t output_length = 0U;
    bool pending_space = false;

    if (output == NULL || capacity == 0U) {
        return 0U;
    }
    output[0] = '\0';
    while (input_index < length && output_length < capacity - 1U) {
        size_t matched_length = MatchSensitiveValueAt(line, input_index, length);

        if (matched_length > 0U) {
            size_t replacement_length = sizeof(redacted) - 1U;
            if (pending_space && output_length > 0U &&
                output_length + 1U < capacity - 1U) {
                output[output_length++] = ' ';
            }
            if (output_length + replacement_length >= capacity) {
                break;
            }
            memcpy(&output[output_length], redacted, replacement_length);
            output_length += replacement_length;
            input_index += matched_length;
            pending_space = false;
            continue;
        }

        {
            unsigned char character = (unsigned char)line[input_index++];
            if (isspace(character)) {
                pending_space = output_length > 0U;
                continue;
            }
            if (character < 0x20U || character > 0x7eU) {
                continue;
            }
            if (pending_space && output_length + 1U < capacity - 1U) {
                output[output_length++] = ' ';
            }
            pending_space = false;
            output[output_length++] = (char)character;
        }
    }
    output[output_length] = '\0';
    return output_length;
}

static void ReportTcpConnectFailureDetail(void)
{
    char detail[WIFI_TCP_DETAIL_CAPACITY];
    char message[WIFI_TCP_DETAIL_CAPACITY + 32U];
    size_t line_start = 0U;

    while (line_start < g_wifi.response_length) {
        size_t line_end = line_start;
        size_t line_length;

        while (line_end < g_wifi.response_length &&
               g_wifi.response[line_end] != '\r' &&
               g_wifi.response[line_end] != '\n') {
            line_end++;
        }
        line_length = line_end - line_start;
        if (line_length > 0U &&
            !IsAtCommandEcho(&g_wifi.response[line_start], line_length) &&
            IsTcpFailureDetailLine(&g_wifi.response[line_start], line_length)) {
            size_t detail_length = SanitizeTcpFailureDetail(
                &g_wifi.response[line_start], line_length, detail, sizeof(detail));
            if (detail_length > 0U) {
                int message_length = snprintf(message, sizeof(message),
                                              "WIFI TCP DETAIL: %s\r\n", detail);
                if (message_length > 0 && (size_t)message_length < sizeof(message)) {
                    USART1_SendReply(message);
                }
            }
            SecureClear(detail, sizeof(detail));
            SecureClear(message, sizeof(message));
            return;
        }
        line_start = line_end;
        while (line_start < g_wifi.response_length &&
               (g_wifi.response[line_start] == '\r' ||
                g_wifi.response[line_start] == '\n')) {
            line_start++;
        }
    }
}

static bool HasTerminalResponse(void)
{
    return Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") ||
           Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
           Contains(g_wifi.response, g_wifi.response_length, "FAIL") ||
           Contains(g_wifi.response, g_wifi.response_length, "No AP");
}

static void ClearResponse(void)
{
    SecureClear(g_wifi.response, sizeof(g_wifi.response));
    g_wifi.response_length = 0U;
}

static void DiscardPendingRx(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    SecureClear(g_wifi.rx_data, sizeof(g_wifi.rx_data));
    g_wifi.rx_tail = g_wifi.rx_head;
    g_wifi.rx_overflow = 0U;
    __set_PRIMASK(primask);
}

static void DrainWifiRx(void)
{
    uint16_t processed = 0U;

    if (g_wifi.rx_overflow != 0U) {
        DiscardPendingRx();
        ClearResponse();
        return;
    }
    while (g_wifi.rx_tail != g_wifi.rx_head && processed < WIFI_RX_DRAIN_BUDGET) {
        uint8_t byte = g_wifi.rx_data[g_wifi.rx_tail];
        g_wifi.rx_data[g_wifi.rx_tail] = 0U;
        g_wifi.rx_tail = (uint16_t)((g_wifi.rx_tail + 1U) % WIFI_RX_CAPACITY);
        if (g_wifi.response_length >= sizeof(g_wifi.response) - 1U) {
            DiscardPendingRx();
            wifi_uart_rx_recovery_discard_partial_line(&g_wifi.rx_recovery);
            ClearResponse();
            return;
        }
        g_wifi.response[g_wifi.response_length++] = (char)byte;
        g_wifi.response[g_wifi.response_length] = '\0';
        processed++;
        if (g_wifi.rx_overflow != 0U) {
            DiscardPendingRx();
            ClearResponse();
            return;
        }
    }
}

static void ClearCommand(void)
{
    SecureClear(g_wifi.tx_command, sizeof(g_wifi.tx_command));
}

static bool RearmWifiRx(void *context)
{
    UART_HandleTypeDef *uart = (UART_HandleTypeDef *)context;
    return uart != NULL && HAL_UART_Receive_IT(uart, &g_wifi.rx_byte, 1U) == HAL_OK;
}

static void ReportFixed(const char *message)
{
    USART1_SendString(message);
}

static void ReportVolatileStorageFailure(void)
{
    if (Contains(g_wifi.response, g_wifi.response_length, "ERROR")) {
        ReportFixed("WIFI CONNECT: VOLATILE_STORAGE_ERROR\r\n");
    } else if (Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
        ReportFixed("WIFI CONNECT: VOLATILE_STORAGE_FAIL\r\n");
    } else if (Contains(g_wifi.response, g_wifi.response_length, "No AP")) {
        ReportFixed("WIFI CONNECT: VOLATILE_STORAGE_NO_AP\r\n");
    } else {
        ReportFixed("WIFI CONNECT: VOLATILE_STORAGE_UNEXPECTED_RESPONSE\r\n");
    }
}

static bool ExtractVersionField(const char *response, size_t response_length,
                                const char *label, char *output, size_t capacity)
{
    size_t label_length;
    size_t index;

    if (response == NULL || label == NULL || output == NULL || capacity == 0U) {
        return false;
    }
    label_length = strlen(label);
    if (label_length == 0U || label_length > response_length) {
        return false;
    }
    for (index = 0U; index <= response_length - label_length; index++) {
        size_t value_start;
        size_t value_end;
        size_t value_length;
        size_t value_index;

        if (memcmp(&response[index], label, label_length) != 0) {
            continue;
        }
        value_start = index + label_length;
        while (value_start < response_length &&
               (response[value_start] == ' ' || response[value_start] == '\t')) {
            value_start++;
        }
        value_end = value_start;
        while (value_end < response_length && response[value_end] != '\r' &&
               response[value_end] != '\n' && response[value_end] != '\0') {
            value_end++;
        }
        while (value_end > value_start &&
               (response[value_end - 1U] == ' ' || response[value_end - 1U] == '\t')) {
            value_end--;
        }
        value_length = value_end - value_start;
        if (value_length == 0U || value_length >= capacity) {
            return false;
        }
        for (value_index = 0U; value_index < value_length; value_index++) {
            unsigned char character = (unsigned char)response[value_start + value_index];
            if (character < 0x20U || character > 0x7eU) {
                return false;
            }
        }
        memcpy(output, &response[value_start], value_length);
        output[value_length] = '\0';
        return true;
    }
    return false;
}

static void ReportModuleVersion(void)
{
    char at_version[48];
    char sdk_version[48];
    char message[128];
    int length;

    if (!ExtractVersionField(g_wifi.response, g_wifi.response_length, "AT version:",
                             at_version, sizeof(at_version)) ||
        !ExtractVersionField(g_wifi.response, g_wifi.response_length, "SDK version:",
                             sdk_version, sizeof(sdk_version))) {
        ReportFixed("WIFI GMR: VERSION_FORMAT_UNKNOWN\r\n");
    } else {
        length = snprintf(message, sizeof(message), "WIFI GMR: AT=%s; SDK=%s\r\n",
                          at_version, sdk_version);
        if (length > 0 && (size_t)length < sizeof(message)) {
            ReportFixed(message);
        } else {
            ReportFixed("WIFI GMR: VERSION_FORMAT_UNKNOWN\r\n");
        }
    }
    SecureClear(at_version, sizeof(at_version));
    SecureClear(sdk_version, sizeof(sdk_version));
    SecureClear(message, sizeof(message));
}

static void ReportStationQueryDiagnostic(void)
{
    char message[64];
    uint8_t flags = WiFiAT_ClassifyStationResponse(
        g_wifi.dialect, g_wifi.response, g_wifi.response_length);
    uint8_t length_bucket = WiFiAT_ResponseLengthBucket(g_wifi.response_length);
    int written = snprintf(message, sizeof(message),
                           "WIFI SYNC DIAG: D=%u F=%02X L=%u R=U\r\n",
                           (unsigned int)g_wifi.dialect, (unsigned int)flags,
                           (unsigned int)length_bucket);

    if (written > 0 && (size_t)written < sizeof(message)) {
        ReportFixed(message);
    }
}

static bool ExtractJoinFailureCode(uint8_t *code)
{
    const char *prefix;
    size_t prefix_length;
    size_t index;

    if (code == NULL) {
        return false;
    }
    if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT) {
        prefix = "+CWJAP:";
    } else if (g_wifi.dialect == WIFI_AT_DIALECT_NONOS) {
        prefix = "+CWJAP_CUR:";
    } else {
        return false;
    }
    prefix_length = strlen(prefix);

    for (index = 0U; index < g_wifi.response_length; index++) {
        size_t cursor;
        unsigned int value = 0U;
        unsigned int digits = 0U;
        bool valid = true;

        if (index != 0U && g_wifi.response[index - 1U] != '\n') {
            continue;
        }
        if (prefix_length > g_wifi.response_length - index ||
            memcmp(&g_wifi.response[index], prefix, prefix_length) != 0) {
            continue;
        }

        cursor = index + prefix_length;
        while (cursor < g_wifi.response_length &&
               (g_wifi.response[cursor] == ' ' || g_wifi.response[cursor] == '\t')) {
            cursor++;
        }
        while (cursor < g_wifi.response_length &&
               g_wifi.response[cursor] >= '0' && g_wifi.response[cursor] <= '9') {
            if (digits >= 3U) {
                valid = false;
                break;
            }
            value = value * 10U + (unsigned int)(g_wifi.response[cursor] - '0');
            digits++;
            cursor++;
        }
        if (!valid || digits == 0U || value > 255U) {
            continue;
        }
        if (cursor < g_wifi.response_length && g_wifi.response[cursor] == '\r') {
            cursor++;
        }
        if (cursor >= g_wifi.response_length || g_wifi.response[cursor] != '\n') {
            continue;
        }

        *code = (uint8_t)value;
        return true;
    }
    return false;
}

static void ReportJoinFailureDiagnostic(void)
{
    uint8_t code;
    char message[64];
    int length;

    if (ExtractJoinFailureCode(&code)) {
        length = snprintf(message, sizeof(message),
                          "WIFI CONNECT: JOIN_ERROR_CODE=%u\r\n",
                          (unsigned int)code);
        if (length > 0 && (size_t)length < sizeof(message)) {
            ReportFixed(message);
        } else {
            ReportFixed("WIFI CONNECT: JOIN_ERROR\r\n");
        }
    } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR")) {
        ReportFixed("WIFI CONNECT: JOIN_ERROR\r\n");
    } else if (Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
        ReportFixed("WIFI CONNECT: JOIN_FAIL\r\n");
    } else {
        ReportFixed("WIFI CONNECT: JOIN_TIMEOUT\r\n");
    }
}

static void ReportTcpStatus(void)
{
    if (g_wifi.tcp_status == WIFI_AT_TCP_CONNECTED) {
        ReportFixed("WIFI TCP STATUS: CONNECTED\r\n");
    } else if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED) {
        ReportFixed("WIFI TCP STATUS: DISCONNECTED\r\n");
    } else {
        ReportFixed("WIFI TCP STATUS: UNKNOWN\r\n");
    }
}

static void ReportTcpMuxMode(WiFiAtTcpMuxMode mode)
{
    if (mode == WIFI_AT_TCP_MUX_SINGLE) {
        ReportFixed("WIFI TCP MUX: SINGLE\r\n");
    } else if (mode == WIFI_AT_TCP_MUX_MULTI) {
        ReportFixed("WIFI TCP MUX: MULTI\r\n");
    } else {
        ReportFixed("WIFI TCP MUX: UNKNOWN\r\n");
    }
}

static void ReportTcpMode(WiFiAtTcpMode mode)
{
    if (mode == WIFI_AT_TCP_MODE_NORMAL) {
        ReportFixed("WIFI TCP MODE: NORMAL\r\n");
    } else if (mode == WIFI_AT_TCP_MODE_TRANSPARENT) {
        ReportFixed("WIFI TCP MODE: TRANSPARENT\r\n");
    } else {
        ReportFixed("WIFI TCP MODE: UNKNOWN\r\n");
    }
}

static void ReportDnsStatus(const WiFiAtDnsInfo *info)
{
    char line[64];
    const char *mode = "UNKNOWN";
    unsigned int index;
    int length;

    if (info == NULL || info->server_count > WIFI_AT_DNS_SERVER_COUNT) {
        ReportFixed("WIFI DNS STATUS: UNKNOWN\r\n");
        return;
    }
    if (info->mode == WIFI_AT_DNS_MODE_AUTO) {
        mode = "AUTO";
    } else if (info->mode == WIFI_AT_DNS_MODE_MANUAL) {
        mode = "MANUAL";
    }
    length = snprintf(line, sizeof(line), "WIFI DNS STATUS: MODE=%s\r\n", mode);
    if (length <= 0 || (size_t)length >= sizeof(line)) {
        ReportFixed("WIFI DNS STATUS: UNKNOWN\r\n");
        SecureClear(line, sizeof(line));
        return;
    }
    ReportFixed(line);
    if (info->server_count == 0U) {
        ReportFixed("WIFI DNS STATUS: SERVERS=NOT_REPORTED\r\n");
    }
    for (index = 0U; index < info->server_count; index++) {
        length = snprintf(line, sizeof(line), "WIFI DNS STATUS: DNS%u=%s\r\n",
                          index + 1U, info->servers[index]);
        if (length <= 0 || (size_t)length >= sizeof(line)) {
            ReportFixed("WIFI DNS STATUS: UNKNOWN\r\n");
            break;
        }
        ReportFixed(line);
    }
    SecureClear(line, sizeof(line));
}

static void SetTcpOperationResult(WiFiTcpOperationResult result)
{
    g_wifi.tcp_operation_result = result;
    g_wifi.tcp_operation_result_ready = 1U;
}

static void CompleteStatusCheck(void)
{
    if (g_wifi.status_check_pending != 0U) {
        g_wifi.status_check_pending = 0U;
        g_wifi.query_result_ready = 1U;
    }
}

static void FinishTcpConnect(bool connected, const char *failure_reason)
{
    char diagnostic[64];

    g_wifi.tcp_status = connected ? WIFI_AT_TCP_CONNECTED : WIFI_AT_TCP_UNKNOWN;
    SetTcpOperationResult(connected ? WIFI_TCP_OPERATION_SUCCEEDED
                                    : WIFI_TCP_OPERATION_FAILED);
    if (connected) {
        SecureClear(g_wifi.tcp_host, sizeof(g_wifi.tcp_host));
        g_wifi.tcp_port = 0U;
        g_wifi.phase = WIFI_PHASE_IDLE;
        ClearResponse();
        ReportFixed("WIFI TCP: CONNECTED\r\n");
    } else {
        int length;
        const char *reason = failure_reason != NULL ? failure_reason :
                                                       "UNCLASSIFIED_RESPONSE";
        ReportFixed("WIFI TCP: CONNECT_FAILED\r\n");
        length = snprintf(diagnostic, sizeof(diagnostic), "WIFI TCP DIAG: %s\r\n", reason);
        if (length > 0 && (size_t)length < sizeof(diagnostic)) {
            ReportFixed(diagnostic);
        } else {
            ReportFixed("WIFI TCP DIAG: UNKNOWN\r\n");
        }
        ReportTcpConnectFailureDetail();
        SecureClear(diagnostic, sizeof(diagnostic));
        SecureClear(g_wifi.tcp_host, sizeof(g_wifi.tcp_host));
        g_wifi.tcp_port = 0U;
        g_wifi.phase = WIFI_PHASE_IDLE;
        ClearResponse();
    }
}

static void FinishTcpSend(bool sent, const char *failure_diagnostic)
{
    if (!sent) {
        g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    }
    SetTcpOperationResult(sent ? WIFI_TCP_OPERATION_SUCCEEDED : WIFI_TCP_OPERATION_FAILED);
    SecureClear(g_wifi.tcp_payload, sizeof(g_wifi.tcp_payload));
    g_wifi.tcp_payload_length = 0U;
    g_wifi.phase = WIFI_PHASE_IDLE;
    ClearResponse();
    if (sent) {
        ReportFixed("WIFI TCP: SEND_OK\r\n");
    } else {
        ReportFixed("WIFI TCP: SEND_FAILED\r\n");
        if (failure_diagnostic != NULL) {
            ReportFixed(failure_diagnostic);
        }
    }
}

static void FinishTcpClose(bool closed)
{
    g_wifi.tcp_status = closed ? WIFI_AT_TCP_DISCONNECTED : WIFI_AT_TCP_UNKNOWN;
    SetTcpOperationResult(closed ? WIFI_TCP_OPERATION_SUCCEEDED
                                 : WIFI_TCP_OPERATION_FAILED);
    g_wifi.phase = WIFI_PHASE_IDLE;
    ClearResponse();
    ReportFixed(closed ? "WIFI TCP: CLOSED\r\n" : "WIFI TCP: CLOSE_FAILED\r\n");
}

static void FinishPing(bool replied, bool timed_out)
{
    g_wifi.phase = WIFI_PHASE_IDLE;
    SecureClear(g_wifi.ping_host, sizeof(g_wifi.ping_host));
    ClearResponse();
    ReportFixed(replied ? "WIFI PING: REPLY\r\n" :
                (timed_out ? "WIFI PING: TIMEOUT\r\n" : "WIFI PING: ERROR\r\n"));
}

static void FailCommandChannel(const char *reason)
{
    uint8_t clear_pending = g_wifi.clear_pending;
    uint8_t clear_command_started = g_wifi.clear_command_started;
    uint8_t persistent_connect = g_wifi.persist_requested;
    g_wifi.phase = WIFI_PHASE_TX_FAULT;
    if (g_wifi.tx_active == 0U || HAL_UART_AbortTransmit(&huart4) == HAL_OK) {
        g_wifi.tx_active = 0U;
        ClearCommand();
        SecureClear(g_wifi.tcp_payload, sizeof(g_wifi.tcp_payload));
        g_wifi.tcp_payload_length = 0U;
    }
    g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    g_wifi.connect_pending = 0U;
    g_wifi.clear_pending = 0U;
    g_wifi.clear_command_started = 0U;
    if (persistent_connect != 0U) {
        if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT &&
            g_wifi.persist_store_maybe_enabled != 0U) {
            g_wifi.persist_store_mode_unknown = 1U;
        }
        g_wifi.persist_requested = 0U;
        g_wifi.persist_abort_requested = 0U;
        g_wifi.credentials_loaded = 0U;
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
    }
    if (g_wifi.tcp_mux_check_pending != 0U) {
        g_wifi.tcp_mux_check_pending = 0U;
        ReportTcpMuxMode(WIFI_AT_TCP_MUX_UNKNOWN);
    }
    if (g_wifi.dns_query_pending != 0U) {
        g_wifi.dns_query_pending = 0U;
        ReportDnsStatus(NULL);
    }
    if (g_wifi.dns_gateway_pending != 0U) {
        g_wifi.dns_gateway_pending = 0U;
        ClearDnsGatewayExpected();
        ReportFixed("WIFI DNS GATEWAY: UNKNOWN\r\n");
    }
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    WiFiState_SetUnknown(&g_wifi.state);
    CompleteStatusCheck();
    if (g_wifi.tcp_operation_result == WIFI_TCP_OPERATION_PENDING) {
        SetTcpOperationResult(WIFI_TCP_OPERATION_FAILED);
    }
    ClearResponse();
    ReportFixed(reason);
    if (persistent_connect != 0U) {
        ReportFixed(g_wifi.persist_store_mode_unknown != 0U
                        ? "WIFI CONNECT SAVE: STORE_MODE_UNKNOWN; STOPPED\r\n"
                        : "WIFI CONNECT SAVE: ABORTED; UART_FAULT\r\n");
    }
    if (clear_pending != 0U) {
        ReportFixed(clear_command_started != 0U
                        ? "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_UNCONFIRMED\r\n"
                        : "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED\r\n");
    }
}

static bool StartCommand(const char *command, uint32_t now, uint32_t timeout_ms,
                         WiFiPhase wait_phase)
{
    size_t length;
    HAL_StatusTypeDef result;

    if (command == NULL || g_wifi.tx_active != 0U) {
        return false;
    }
    length = strlen(command);
    if (length == 0U || length >= sizeof(g_wifi.tx_command)) {
        return false;
    }
    ClearResponse();
    DiscardPendingRx();
    ClearCommand();
    memcpy(g_wifi.tx_command, command, length);
    g_wifi.tx_active = 1U;
    g_wifi.deadline = now + timeout_ms;
    g_wifi.phase = wait_phase;
    result = HAL_UART_Transmit_IT(&huart4, (uint8_t *)g_wifi.tx_command, (uint16_t)length);
    if (result != HAL_OK) {
        g_wifi.tx_active = 0U;
        ClearCommand();
        FailCommandChannel("WIFI UART4: TX_START_FAILED; COMMAND_CHANNEL_UNSYNCHRONIZED\r\n");
        return false;
    }
    return true;
}

static bool StartBuiltCommand(bool built, const char *command, uint32_t now,
                              uint32_t timeout_ms, WiFiPhase wait_phase)
{
    if (!built) {
        FailCommandChannel("WIFI: COMMAND_BUILD_FAILED\r\n");
        return false;
    }
    return StartCommand(command, now, timeout_ms, wait_phase);
}

static bool StartTcpPayload(uint32_t now)
{
    HAL_StatusTypeDef result;

    if (g_wifi.tx_active != 0U || g_wifi.tcp_payload_length == 0U) {
        return false;
    }
    ClearResponse();
    g_wifi.tx_active = 1U;
    g_wifi.deadline = now + WIFI_TCP_SEND_TIMEOUT_MS;
    g_wifi.phase = WIFI_PHASE_TCP_SEND_DATA;
    result = HAL_UART_Transmit_IT(&huart4, g_wifi.tcp_payload,
                                 g_wifi.tcp_payload_length);
    if (result != HAL_OK) {
        g_wifi.tx_active = 0U;
        FailCommandChannel("WIFI UART4: PAYLOAD_TX_START_FAILED; COMMAND_CHANNEL_UNSYNCHRONIZED\r\n");
        return false;
    }
    return true;
}

static void FailPayloadTransmit(void)
{
    uint8_t clear_pending = g_wifi.clear_pending;
    /* CIPSEND may still expect data. Do not issue AT commands after a
       partial transfer until the module has been externally resynchronized. */
    g_wifi.phase = WIFI_PHASE_TX_FAULT;
    if (HAL_UART_AbortTransmit(&huart4) == HAL_OK) {
        g_wifi.tx_active = 0U;
        ClearCommand();
        SecureClear(g_wifi.tcp_payload, sizeof(g_wifi.tcp_payload));
        g_wifi.tcp_payload_length = 0U;
    }
    g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    g_wifi.connect_pending = 0U;
    g_wifi.clear_pending = 0U;
    g_wifi.clear_command_started = 0U;
    WiFiState_SetUnknown(&g_wifi.state);
    ClearResponse();
    SetTcpOperationResult(WIFI_TCP_OPERATION_FAILED);
    ReportFixed("WIFI TCP: SEND_FAILED\r\n");
    ReportFixed("WIFI UART4: TX_TIMEOUT; COMMAND_CHANNEL_UNSYNCHRONIZED\r\n");
    if (clear_pending != 0U) {
        ReportFixed("WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED\r\n");
    }
}

static void ScheduleSyncRetry(uint32_t now)
{
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    WiFiState_SetUnknown(&g_wifi.state);
    g_wifi.next_sync_retry = now + WIFI_SYNC_RETRY_MS;
    g_wifi.phase = WIFI_PHASE_SYNC_RETRY_WAIT;
    if (g_wifi.retry_count < 255U) {
        g_wifi.retry_count++;
    }
}

static void FinishConnected(void)
{
    WiFiState_AdoptConnected(&g_wifi.state);
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    g_wifi.connect_pending = 0U;
    g_wifi.phase = WIFI_PHASE_IP_QUERY_SEND;
    g_wifi.retry_count = 0U;
    g_wifi.ip_query_attempts = 0U;
    g_wifi.next_ip_query = 0U;
    ClearResponse();
    ReportFixed("WIFI SYNC: STA_ASSOCIATED\r\n");
}

static void FinishDisconnected(uint32_t now)
{
    g_wifi.ip = WIFI_IP_NOT_READY;
    ClearStationIpAddress();
    ClearStationLinkInfo();
    g_wifi.ip_query_attempts = 0U;
    g_wifi.next_ip_query = 0U;
    g_wifi.station = WIFI_AT_STATION_DISCONNECTED;
    if (g_wifi.credentials_loaded != 0U) {
        WiFiState_ConnectionLost(&g_wifi.state, now);
        g_wifi.retry_started_at = now;
    } else {
        WiFiState_Clear(&g_wifi.state);
    }
    g_wifi.phase = WIFI_PHASE_IDLE;
    ClearResponse();
    ReportFixed("WIFI SYNC: STA_DISCONNECTED\r\n");
}

static void HandleStationQuery(uint32_t now)
{
    WiFiAtStationStatus station = WIFI_AT_STATION_UNKNOWN;
    bool parsed = WiFiAT_ParseStationStatus(g_wifi.dialect, g_wifi.response,
                                            g_wifi.response_length, &station);
    char bssid[WIFI_AT_BSSID_TEXT_CAPACITY];
    unsigned int channel = 0U;
    int rssi = 0;

    ClearStationLinkInfo();
    if (parsed && station == WIFI_AT_STATION_CONNECTED) {
        g_wifi.station = WIFI_AT_STATION_CONNECTED;
        if (WiFiAT_ParseStationLinkInfo(g_wifi.dialect, g_wifi.response,
                                        g_wifi.response_length, bssid,
                                        sizeof(bssid), &channel, &rssi)) {
            memcpy(g_wifi.station_bssid, bssid, sizeof(g_wifi.station_bssid));
            g_wifi.station_channel = (uint8_t)channel;
            g_wifi.station_rssi = (int8_t)rssi;
            g_wifi.station_link_info_valid = 1U;
        }
        SecureClear(bssid, sizeof(bssid));
        FinishConnected();
    } else if (parsed && station == WIFI_AT_STATION_DISCONNECTED) {
        SecureClear(bssid, sizeof(bssid));
        g_wifi.station = WIFI_AT_STATION_DISCONNECTED;
        FinishDisconnected(now);
        CompleteStatusCheck();
    } else {
        SecureClear(bssid, sizeof(bssid));
        ReportStationQueryDiagnostic();
        g_wifi.station = WIFI_AT_STATION_UNKNOWN;
        ScheduleSyncRetry(now);
        ClearResponse();
        CompleteStatusCheck();
    }
}

static bool IsStationModeEnabled(void)
{
    return g_wifi.mode_known != 0U && (g_wifi.mode & 1U) != 0U;
}

static void HandleIpQuery(uint32_t now)
{
    char station_ip[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_gateway[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_netmask[WIFI_AT_IPV4_TEXT_CAPACITY];
    WiFiAtIpStatus ip_status = WiFiAT_ParseStationIpConfig(
        g_wifi.dialect, g_wifi.response, g_wifi.response_length,
        station_ip, sizeof(station_ip), station_gateway, sizeof(station_gateway),
        station_netmask, sizeof(station_netmask));
    bool address_ready = ip_status == WIFI_AT_IP_READY;

    if (ip_status == WIFI_AT_IP_READY) {
        g_wifi.ip = WIFI_IP_READY;
        ClearStationIpAddress();
        if (address_ready) {
            memcpy(g_wifi.station_ip, station_ip, strlen(station_ip) + 1U);
            memcpy(g_wifi.station_gateway, station_gateway,
                   strlen(station_gateway) + 1U);
            memcpy(g_wifi.station_netmask, station_netmask,
                   strlen(station_netmask) + 1U);
        }
        SecureClear(station_ip, sizeof(station_ip));
        SecureClear(station_gateway, sizeof(station_gateway));
        SecureClear(station_netmask, sizeof(station_netmask));
        WiFiState_AdoptConnected(&g_wifi.state);
        g_wifi.ip_query_attempts = 0U;
        ClearResponse();
        g_wifi.phase = WIFI_PHASE_IDLE;
        ReportFixed("WIFI SYNC: IP_READY\r\n");
        CompleteStatusCheck();
        return;
    }

    g_wifi.ip = ip_status == WIFI_AT_IP_NOT_READY ? WIFI_IP_NOT_READY : WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    SecureClear(station_ip, sizeof(station_ip));
    SecureClear(station_gateway, sizeof(station_gateway));
    SecureClear(station_netmask, sizeof(station_netmask));
    WiFiState_AdoptConnected(&g_wifi.state);
    ClearResponse();
    if (g_wifi.ip_query_attempts < WIFI_IP_QUERY_MAX_ATTEMPTS) {
        g_wifi.network = WIFI_NETWORK_NOT_RUN;
        g_wifi.next_ip_query = now + WIFI_IP_QUERY_RETRY_MS;
        g_wifi.phase = WIFI_PHASE_IP_QUERY_RETRY_WAIT;
        ReportFixed("WIFI SYNC: IP_RETRY\r\n");
        return;
    }
    g_wifi.network = WIFI_NETWORK_NOT_RUN;
    g_wifi.phase = WIFI_PHASE_IDLE;
    ReportFixed(ip_status == WIFI_AT_IP_NOT_READY
                    ? "WIFI SYNC: IP_NOT_READY\r\n"
                    : "WIFI SYNC: IP_UNKNOWN\r\n");
    CompleteStatusCheck();
}

static void HandleIpQueryTimeout(uint32_t now)
{
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    WiFiState_AdoptConnected(&g_wifi.state);
    ClearResponse();
    if (g_wifi.ip_query_attempts < WIFI_IP_QUERY_MAX_ATTEMPTS) {
        g_wifi.network = WIFI_NETWORK_NOT_RUN;
        g_wifi.next_ip_query = now + WIFI_IP_QUERY_RETRY_MS;
        g_wifi.phase = WIFI_PHASE_IP_QUERY_RETRY_WAIT;
        ReportFixed("WIFI SYNC: IP_RETRY\r\n");
        return;
    }
    g_wifi.network = WIFI_NETWORK_NOT_RUN;
    g_wifi.phase = WIFI_PHASE_IDLE;
    ReportFixed("WIFI SYNC: IP_TIMEOUT\r\n");
    CompleteStatusCheck();
}

static bool ValidateCredentials(const char *ssid, const char *password)
{
    char test_command[WIFI_COMMAND_CAPACITY];
    bool valid = WiFiAT_BuildJoinCommand(g_wifi.dialect, ssid, password,
                                         test_command, sizeof(test_command));
    SecureClear(test_command, sizeof(test_command));
    return valid;
}

void WiFi_ModuleEnableHold(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOF_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_14, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_14;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &gpio);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_14, GPIO_PIN_SET);
}

void WiFi_Init(void)
{
    SecureClear(&g_wifi, sizeof(g_wifi));
    g_connect_reject_reason = "NOT_REJECTED";
    WiFiState_Init(&g_wifi.state);
    WiFiState_BeginSync(&g_wifi.state);
    g_wifi.dialect = WIFI_AT_DIALECT_UNKNOWN;
    g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    g_wifi.tcp_mux_mode = WIFI_AT_TCP_MUX_UNKNOWN;
    g_wifi.phase = WIFI_PHASE_PROBE_SEND;
    g_wifi.sync_started = 1U;

    WiFi_ModuleEnableHold();
    UART4_Init();
    if (HAL_UART_Receive_IT(&huart4, &g_wifi.rx_byte, 1U) != HAL_OK) {
        g_wifi.rx_rearm_deferred = 1U;
        ReportFixed("WIFI UART4 RX REARM FAILED\r\n");
    }
    ReportFixed("WIFI SYNC: START\r\n");
}

void WiFi_UART_RxComplete(UART_HandleTypeDef *uart)
{
    uint16_t next;
    if (uart == NULL || uart->Instance != UART4) {
        return;
    }
    if (!wifi_uart_rx_recovery_should_discard(&g_wifi.rx_recovery, g_wifi.rx_byte)) {
        next = (uint16_t)((g_wifi.rx_head + 1U) % WIFI_RX_CAPACITY);
        if (next != g_wifi.rx_tail) {
            g_wifi.rx_data[g_wifi.rx_head] = g_wifi.rx_byte;
            g_wifi.rx_head = next;
        } else {
            g_wifi.rx_overflow = 1U;
            wifi_uart_rx_recovery_discard_partial_line(&g_wifi.rx_recovery);
        }
    }
    if (wifi_uart_rx_recovery_defer_rearm_on_rx_complete(uart->ErrorCode,
                                                          HAL_UART_ERROR_ORE)) {
        g_wifi.rx_rearm_deferred = 1U;
    } else if (!RearmWifiRx(&huart4)) {
        g_wifi.rx_rearm_deferred = 1U;
        ReportFixed("WIFI UART4 RX REARM FAILED\r\n");
    }
}

void WiFi_UART_TxComplete(void)
{
    g_wifi.tx_active = 0U;
    ClearCommand();
    if (g_wifi.phase == WIFI_PHASE_TCP_SEND_DATA) {
        g_wifi.phase = WIFI_PHASE_TCP_SEND_RESULT_WAIT;
    }
}

void WiFi_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    WiFiUartRxRecoveryResult result;
    uint8_t rearm_was_deferred;
    uint32_t error_code;

    if (uart == NULL || uart->Instance != UART4) {
        return;
    }
    error_code = uart->ErrorCode;
    if ((error_code & HAL_UART_ERROR_ORE) != 0U) {
        __HAL_UART_CLEAR_OREFLAG(uart);
        g_wifi.rx_overflow = 1U;
        wifi_uart_rx_recovery_discard_partial_line(&g_wifi.rx_recovery);
    }
    rearm_was_deferred = g_wifi.rx_rearm_deferred;
    g_wifi.rx_rearm_deferred = 0U;
    result = wifi_uart_rx_recovery_handle_error_after_rx(
        HAL_UART_ERROR_ORE, error_code, rearm_was_deferred != 0U,
        RearmWifiRx, &huart4);
    if (result == WIFI_UART_RX_RECOVERY_REARM_FAILED) {
        g_wifi.rx_rearm_deferred = 1U;
        ReportFixed("WIFI UART4 RX REARM FAILED\r\n");
    }
}

static bool QueueConnect(const char *ssid, const char *password, bool persistent)
{
    bool station_already_connected =
        g_wifi.station == WIFI_AT_STATION_CONNECTED;

    g_connect_reject_reason = "NOT_REJECTED";
    if (g_wifi.persist_store_mode_unknown != 0U) {
        g_connect_reject_reason = "PERSISTENT_STORE_MODE_UNKNOWN";
        return false;
    }
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN) {
        g_connect_reject_reason = "AT_DIALECT_UNKNOWN";
        return false;
    }
    if (g_wifi.phase != WIFI_PHASE_IDLE &&
        g_wifi.phase != WIFI_PHASE_SYNC_RETRY_WAIT) {
        g_connect_reject_reason = "WIFI_STATE_MACHINE_BUSY";
        return false;
    }
    if (g_wifi.tcp_status == WIFI_AT_TCP_CONNECTED) {
        g_connect_reject_reason = "TCP_ALREADY_CONNECTED";
        return false;
    }
    if (station_already_connected && !persistent) {
        g_connect_reject_reason = "STA_ALREADY_CONNECTED";
        return false;
    }
    if (!ValidateCredentials(ssid, password)) {
        g_connect_reject_reason = "CREDENTIALS_INVALID";
        return false;
    }
    ClearStationLinkInfo();
    SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
    SecureClear(g_wifi.password, sizeof(g_wifi.password));
    memcpy(g_wifi.ssid, ssid, strlen(ssid));
    memcpy(g_wifi.password, password, strlen(password));
    g_wifi.credentials_loaded = 1U;
    g_wifi.persist_requested = persistent ? 1U : 0U;
    g_wifi.persist_store_maybe_enabled = 0U;
    g_wifi.persist_join_associated = 0U;
    g_wifi.persist_join_ok = 0U;
    g_wifi.persist_failure_pending = 0U;
    g_wifi.persist_abort_requested = 0U;
    g_wifi.connect_pending = 1U;
    g_wifi.clear_pending = 0U;
    if (!WiFiState_RequestConnect(&g_wifi.state, true, true)) {
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
        g_wifi.credentials_loaded = 0U;
        g_wifi.connect_pending = 0U;
        g_connect_reject_reason = "WIFI_STATE_POLICY";
        return false;
    }
    g_wifi.phase = WIFI_PHASE_CONNECT_PREPARE;
    return true;
}

bool WiFi_RequestConnect(const char *ssid, const char *password)
{
    return QueueConnect(ssid, password, false);
}

bool WiFi_Connect(const char *ssid, const char *password)
{
    return WiFi_RequestConnect(ssid, password);
}

bool WiFi_ConnectSpec(const char *specification)
{
    const char *separator;
    char ssid[33];
    char password[65];
    size_t ssid_length;
    size_t password_length;
    bool accepted;
    bool persistent = false;

    g_connect_reject_reason = "NOT_REJECTED";
    if (specification == NULL) {
        g_connect_reject_reason = "SPECIFICATION_INVALID";
        return false;
    }
    if (strncmp(specification, "SAVE ", 5U) == 0) {
        persistent = true;
        specification += 5U;
    }
    separator = strchr(specification, '|');
    if (separator == NULL || strchr(separator + 1, '|') != NULL) {
        g_connect_reject_reason = "SPECIFICATION_INVALID";
        return false;
    }
    ssid_length = (size_t)(separator - specification);
    password_length = strlen(separator + 1);
    if (ssid_length == 0U || ssid_length >= sizeof(ssid) || password_length >= sizeof(password)) {
        g_connect_reject_reason = "SPECIFICATION_INVALID";
        return false;
    }
    memcpy(ssid, specification, ssid_length);
    ssid[ssid_length] = '\0';
    memcpy(password, separator + 1, password_length + 1U);
    accepted = QueueConnect(ssid, password, persistent);
    SecureClear(ssid, sizeof(ssid));
    SecureClear(password, sizeof(password));
    return accepted;
}

const char *WiFi_GetLastConnectRejectReason(void)
{
    return g_connect_reject_reason;
}

WiFiClearResult WiFi_ClearCredentials(void)
{
    bool persistent_connect_active =
        g_wifi.persist_requested != 0U &&
        g_wifi.phase >= WIFI_PHASE_CONNECT_PREPARE &&
        g_wifi.phase <= WIFI_PHASE_PERSIST_RAM_WAIT;

    if (g_wifi.clear_pending == 0U) {
        g_wifi.clear_command_started = 0U;
    }
    if (persistent_connect_active) {
        g_wifi.persist_abort_requested = 1U;
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
    } else {
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
        g_wifi.persist_requested = 0U;
        g_wifi.persist_store_maybe_enabled = 0U;
        g_wifi.persist_join_associated = 0U;
        g_wifi.persist_join_ok = 0U;
        g_wifi.persist_failure_pending = 0U;
        g_wifi.persist_abort_requested = 0U;
    }
    g_wifi.credentials_loaded = 0U;
    g_wifi.connect_pending = 0U;
    g_wifi.retry_count = 0U;
    g_wifi.clear_pending = 1U;
    g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    ClearStationLinkInfo();
    WiFiState_Clear(&g_wifi.state);
    /* Clearing credentials means the previous station/IP snapshot is no
       longer valid. Keep it unknown until the module is queried again. */
    g_wifi.station = WIFI_AT_STATION_UNKNOWN;
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    g_wifi.network = WIFI_NETWORK_NOT_RUN;
    if (g_wifi.phase == WIFI_PHASE_TX_FAULT) {
        g_wifi.clear_pending = 0U;
        return WIFI_CLEAR_LOCAL_ONLY;
    }
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN) {
        g_wifi.clear_pending = 0U;
        return WIFI_CLEAR_LOCAL_ONLY;
    }
    if (g_wifi.phase == WIFI_PHASE_IDLE || g_wifi.phase == WIFI_PHASE_SYNC_RETRY_WAIT) {
        g_wifi.phase = WIFI_PHASE_CLEAR_SEND;
    }
    return WIFI_CLEAR_DISCONNECT_QUEUED;
}

void WiFi_Disconnect(void)
{
    (void)WiFi_ClearCredentials();
}

bool WiFi_RequestStatusCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN || g_wifi.phase != WIFI_PHASE_IDLE ||
        g_wifi.clear_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
    g_wifi.query_result_ready = 0U;
    g_wifi.status_check_pending = 1U;
    g_wifi.tcp_query_result_ready = 0U;
    return true;
}

bool WiFi_RequestTcpStatusCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN || g_wifi.phase != WIFI_PHASE_IDLE ||
        g_wifi.clear_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_TCP_QUERY_SEND;
    g_wifi.tcp_query_result_ready = 0U;
    g_wifi.query_result_ready = 0U;
    return true;
}

bool WiFi_RequestTcpMuxModeCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || g_wifi.tx_active != 0U ||
        g_wifi.clear_pending != 0U || g_wifi.connect_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_TCP_MUX_QUERY_SEND;
    g_wifi.tcp_mux_check_pending = 1U;
    return true;
}

bool WiFi_RequestTcpModeCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || g_wifi.tx_active != 0U ||
        g_wifi.clear_pending != 0U || g_wifi.connect_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_TCP_MODE_QUERY_SEND;
    g_wifi.tcp_mode_check_pending = 1U;
    return true;
}

bool WiFi_RequestDnsStatusCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || g_wifi.tx_active != 0U ||
        g_wifi.clear_pending != 0U || g_wifi.connect_pending != 0U ||
        g_wifi.dns_query_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_DNS_QUERY_SEND;
    g_wifi.dns_query_pending = 1U;
    return true;
}

bool WiFi_RequestDnsGatewayUse(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || g_wifi.tx_active != 0U ||
        !WiFi_IsConnected() || g_wifi.station != WIFI_AT_STATION_CONNECTED ||
        g_wifi.ip != WIFI_IP_READY || g_wifi.station_gateway[0] == '\0' ||
        g_wifi.tcp_status != WIFI_AT_TCP_DISCONNECTED ||
        g_wifi.clear_pending != 0U || g_wifi.connect_pending != 0U ||
        g_wifi.dns_query_pending != 0U || g_wifi.dns_gateway_pending != 0U) {
        return false;
    }
    ClearDnsGatewayExpected();
    memcpy(g_wifi.dns_gateway_expected, g_wifi.station_gateway,
           strlen(g_wifi.station_gateway) + 1U);
    g_wifi.phase = g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT
                       ? WIFI_PHASE_DNS_GATEWAY_STORE_RAM_SEND
                       : WIFI_PHASE_DNS_GATEWAY_SET_SEND;
    g_wifi.dns_gateway_pending = 1U;
    return true;
}

bool WiFi_TakeStatusCheckResult(WiFiStatus *status)
{
    if (status == NULL || g_wifi.query_result_ready == 0U) {
        return false;
    }
    *status = g_wifi.state.status;
    g_wifi.query_result_ready = 0U;
    return true;
}

bool WiFi_TakeTcpStatusCheckResult(WiFiAtTcpStatus *status)
{
    if (status == NULL || g_wifi.tcp_query_result_ready == 0U) {
        return false;
    }
    *status = g_wifi.tcp_status;
    g_wifi.tcp_query_result_ready = 0U;
    return true;
}

bool WiFi_RequestModuleVersionCheck(void)
{
    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || g_wifi.tx_active != 0U ||
        g_wifi.clear_pending != 0U || g_wifi.connect_pending != 0U) {
        return false;
    }
    g_wifi.phase = WIFI_PHASE_MODULE_VERSION_QUERY_SEND;
    return true;
}

WiFiStatus WiFi_GetStatus(void)
{
    return g_wifi.state.status;
}

WiFiAtTcpStatus WiFi_GetTcpStatus(void)
{
    return g_wifi.tcp_status;
}

bool WiFi_GetRuntimeInfo(WiFiRuntimeInfo *info)
{
    if (info == NULL) {
        return false;
    }
    info->mode_known = g_wifi.mode_known;
    info->mode = g_wifi.mode;
    info->apsta = g_wifi.apsta;
    info->station = g_wifi.station;
    info->ip = g_wifi.ip;
    info->tcp = g_wifi.tcp_status;
    info->network = g_wifi.network;
    return true;
}

bool WiFi_TcpConnect(const char *host, uint16_t port)
{
    char command[WIFI_COMMAND_CAPACITY];
    bool built;

    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || !WiFi_IsConnected() ||
        g_wifi.ip != WIFI_IP_READY || g_wifi.station_ip[0] == '\0' ||
        g_wifi.clear_pending != 0U || g_wifi.tcp_status != WIFI_AT_TCP_DISCONNECTED ||
        port == 0U) {
        return false;
    }
    if (g_wifi.tcp_mux_mode == WIFI_AT_TCP_MUX_UNKNOWN) {
        (void)WiFi_RequestTcpMuxModeCheck();
        return false;
    }
    built = WiFiAT_BuildTcpConnectCommand(g_wifi.dialect, g_wifi.tcp_mux_mode,
                                          host, (unsigned int)port,
                                          command, sizeof(command));
    SecureClear(command, sizeof(command));
    if (!built) {
        return false;
    }
    SecureClear(g_wifi.tcp_host, sizeof(g_wifi.tcp_host));
    memcpy(g_wifi.tcp_host, host, strlen(host));
    g_wifi.tcp_port = port;
    g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
    g_wifi.tcp_operation_result = WIFI_TCP_OPERATION_PENDING;
    g_wifi.tcp_operation_result_ready = 0U;
    g_wifi.phase = WIFI_PHASE_TCP_NORMAL_MODE_SET_SEND;
    return true;
}

bool WiFi_Ping(const char *host)
{
    char command[WIFI_COMMAND_CAPACITY];
    bool built;

    if (g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN ||
        g_wifi.phase != WIFI_PHASE_IDLE || !WiFi_IsConnected() ||
        g_wifi.station != WIFI_AT_STATION_CONNECTED || g_wifi.ip != WIFI_IP_READY ||
        g_wifi.clear_pending != 0U || g_wifi.tcp_status != WIFI_AT_TCP_DISCONNECTED) {
        return false;
    }
    built = WiFiAT_BuildPingCommand(g_wifi.dialect, host, command, sizeof(command));
    SecureClear(command, sizeof(command));
    if (!built) {
        return false;
    }
    SecureClear(g_wifi.ping_host, sizeof(g_wifi.ping_host));
    memcpy(g_wifi.ping_host, host, strlen(host));
    g_wifi.phase = WIFI_PHASE_PING_SEND;
    return true;
}

bool WiFi_TcpSend(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0U || length > WIFI_TCP_MAX_PAYLOAD_LENGTH ||
        g_wifi.phase != WIFI_PHASE_IDLE || !WiFi_IsConnected() ||
        g_wifi.clear_pending != 0U || g_wifi.tcp_status != WIFI_AT_TCP_CONNECTED) {
        return false;
    }
    SecureClear(g_wifi.tcp_payload, sizeof(g_wifi.tcp_payload));
    memcpy(g_wifi.tcp_payload, data, length);
    g_wifi.tcp_payload_length = (uint16_t)length;
    g_wifi.tcp_operation_result = WIFI_TCP_OPERATION_PENDING;
    g_wifi.tcp_operation_result_ready = 0U;
    g_wifi.phase = WIFI_PHASE_TCP_SEND_COMMAND_SEND;
    return true;
}

bool WiFi_TcpClose(void)
{
    if (g_wifi.phase != WIFI_PHASE_IDLE || !WiFi_IsConnected() ||
        g_wifi.clear_pending != 0U || g_wifi.tcp_status != WIFI_AT_TCP_CONNECTED) {
        return false;
    }
    g_wifi.tcp_operation_result = WIFI_TCP_OPERATION_PENDING;
    g_wifi.tcp_operation_result_ready = 0U;
    g_wifi.phase = WIFI_PHASE_TCP_CLOSE_SEND;
    return true;
}

bool WiFi_TakeTcpOperationResult(WiFiTcpOperationResult *result)
{
    if (result == NULL || g_wifi.tcp_operation_result_ready == 0U) {
        return false;
    }
    *result = g_wifi.tcp_operation_result;
    g_wifi.tcp_operation_result_ready = 0U;
    return true;
}

bool WiFi_IsConnected(void)
{
    return g_wifi.state.status == WIFI_STATUS_CONNECTED;
}

bool WiFi_FormatStatus(char *output, size_t capacity)
{
    static const char *const names[] = {
        "NO_CREDENTIALS", "MODULE_SYNCING", "UNKNOWN", "CONNECTING",
        "CONNECTED", "RETRY_WAIT", "UPSTREAM_LOST"
    };
    const char *name = "UNKNOWN";
    const char *mode = "UNKNOWN";
    const char *station = "UNKNOWN";
    const char *ip = "UNKNOWN";
    const char *tcp = "UNKNOWN";
    const char *network = "NOT_RUN";
    const char *channel = "UNKNOWN";
    const char *rssi = "UNKNOWN";
    char channel_buffer[4];
    char rssi_buffer[8];
    uint32_t retry_seconds = 0U;
    uint32_t now = HAL_GetTick();
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if ((unsigned int)g_wifi.state.status < (sizeof(names) / sizeof(names[0]))) {
        name = names[g_wifi.state.status];
    }
    if (g_wifi.state.status == WIFI_STATUS_RETRY_WAIT &&
        (int32_t)(g_wifi.state.retry_deadline - now) > 0) {
        retry_seconds = (g_wifi.state.retry_deadline - now + 999U) / 1000U;
    }
    if (g_wifi.mode_known != 0U) {
        static const char *const mode_names[] = {"OFF", "STA", "AP", "APSTA"};
        if (g_wifi.mode < (sizeof(mode_names) / sizeof(mode_names[0]))) {
            mode = mode_names[g_wifi.mode];
        }
    }
    if (g_wifi.station == WIFI_AT_STATION_CONNECTED) station = "CONNECTED";
    else if (g_wifi.station == WIFI_AT_STATION_DISCONNECTED) station = "DISCONNECTED";
    if (g_wifi.ip == WIFI_IP_READY) ip = "READY";
    else if (g_wifi.ip == WIFI_IP_NOT_READY) ip = "NOT_READY";
    if (g_wifi.tcp_status == WIFI_AT_TCP_CONNECTED) tcp = "CONNECTED";
    else if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED) tcp = "DISCONNECTED";
    if (g_wifi.station_link_info_valid != 0U) {
        (void)snprintf(channel_buffer, sizeof(channel_buffer), "%u",
                       (unsigned int)g_wifi.station_channel);
        (void)snprintf(rssi_buffer, sizeof(rssi_buffer), "%d",
                       (int)g_wifi.station_rssi);
        channel = channel_buffer;
        rssi = rssi_buffer;
    }
    length = snprintf(output, capacity,
                      "WIFI STATUS: %s; CREDENTIALS=%s; RETRY_IN=%lu; MODE=%s; "
                      "APSTA=%s; STA=%s; IP=%s; TCP=%s; AP_BSSID=%s; "
                      "AP_CHANNEL=%s; AP_RSSI=%s; NET=%s\r\n",
                      name, g_wifi.credentials_loaded != 0U ? "loaded" : "none",
                      (unsigned long)retry_seconds, mode,
                      g_wifi.apsta == WIFI_APSTA_YES ? "YES" :
                          (g_wifi.apsta == WIFI_APSTA_NO ? "NO" : "UNKNOWN"),
                      station, ip, tcp,
                      g_wifi.station_link_info_valid != 0U ? g_wifi.station_bssid : "UNKNOWN",
                      channel, rssi, network);
    return length > 0 && (size_t)length < capacity;
}

bool WiFi_FormatIp(char *output, size_t capacity)
{
    const char *value = "UNKNOWN";
    const char *gateway = "UNKNOWN";
    const char *netmask = "UNKNOWN";
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (g_wifi.ip == WIFI_IP_READY && g_wifi.station_ip[0] != '\0') {
        value = g_wifi.station_ip;
        if (g_wifi.station_gateway[0] != '\0') {
            gateway = g_wifi.station_gateway;
        }
        if (g_wifi.station_netmask[0] != '\0') {
            netmask = g_wifi.station_netmask;
        }
    } else if (g_wifi.ip == WIFI_IP_NOT_READY) {
        value = "NOT_READY";
    }
    length = snprintf(output, capacity,
                      "WIFI IP: %s; GATEWAY=%s; NETMASK=%s\r\n",
                      value, gateway, netmask);
    return length > 0 && (size_t)length < capacity;
}

static void StartStationQuery(uint32_t now)
{
    char command[32];
    if (!IsStationModeEnabled()) {
        g_wifi.station = WIFI_AT_STATION_UNKNOWN;
        g_wifi.ip = WIFI_IP_UNKNOWN;
        ClearStationIpAddress();
        WiFiState_SetUnknown(&g_wifi.state);
        g_wifi.phase = WIFI_PHASE_IDLE;
        CompleteStatusCheck();
        ReportFixed("WIFI SYNC: STA_MODE_UNAVAILABLE\r\n");
        return;
    }
    if (!WiFiAT_BuildStationQueryCommand(g_wifi.dialect, command, sizeof(command))) {
        ScheduleSyncRetry(now);
        CompleteStatusCheck();
        return;
    }
    if (!StartCommand(command, now, WIFI_QUERY_TIMEOUT_MS, WIFI_PHASE_STATION_QUERY_WAIT)) {
        ScheduleSyncRetry(now);
        CompleteStatusCheck();
    }
}

static void ProcessSync(uint32_t now)
{
    char command[WIFI_COMMAND_CAPACITY];
    unsigned int parsed_mode;
    WiFiAtTcpStatus tcp;
    bool built;

    switch (g_wifi.phase) {
    case WIFI_PHASE_PROBE_SEND:
        if (StartCommand("AT\r\n", now, WIFI_QUERY_TIMEOUT_MS, WIFI_PHASE_PROBE_WAIT)) {
            ReportFixed("WIFI SYNC: PROBE\r\n");
        }
        break;
    case WIFI_PHASE_PROBE_WAIT:
        if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
            ClearResponse();
            g_wifi.phase = WIFI_PHASE_MODERN_MODE_SEND;
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ScheduleSyncRetry(now);
        }
        break;
    case WIFI_PHASE_MODERN_MODE_SEND:
        if (StartCommand("AT+CWMODE?\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_MODERN_MODE_WAIT)) {
            ReportFixed("WIFI SYNC: DIALECT_QUERY\r\n");
        }
        break;
    case WIFI_PHASE_MODERN_MODE_WAIT:
        if (HasTerminalResponse()) {
            if (WiFiAT_ParseMode(WIFI_AT_DIALECT_ESP_AT, g_wifi.response,
                                 g_wifi.response_length, &parsed_mode)) {
                g_wifi.dialect = WIFI_AT_DIALECT_ESP_AT;
                g_wifi.mode = (uint8_t)parsed_mode;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = parsed_mode == 3U ? WIFI_APSTA_YES : WIFI_APSTA_NO;
                ClearResponse();
                g_wifi.phase = WIFI_PHASE_STARTUP_TCP_QUERY_SEND;
            } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
                       Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
                ClearResponse();
                g_wifi.phase = WIFI_PHASE_LEGACY_MODE_SEND;
            } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
                ScheduleSyncRetry(now);
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.phase = WIFI_PHASE_LEGACY_MODE_SEND;
        }
        break;
    case WIFI_PHASE_LEGACY_MODE_SEND:
        if (StartCommand("AT+CWMODE_CUR?\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_LEGACY_MODE_WAIT)) {
            ReportFixed("WIFI SYNC: LEGACY_QUERY\r\n");
        }
        break;
    case WIFI_PHASE_LEGACY_MODE_WAIT:
        if (HasTerminalResponse()) {
            if (WiFiAT_ParseMode(WIFI_AT_DIALECT_NONOS, g_wifi.response,
                                 g_wifi.response_length, &parsed_mode)) {
                g_wifi.dialect = WIFI_AT_DIALECT_NONOS;
                g_wifi.mode = (uint8_t)parsed_mode;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = parsed_mode == 3U ? WIFI_APSTA_YES : WIFI_APSTA_NO;
                ClearResponse();
                g_wifi.phase = WIFI_PHASE_STARTUP_TCP_QUERY_SEND;
            } else {
                ScheduleSyncRetry(now);
                ClearResponse();
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ScheduleSyncRetry(now);
        }
        break;
    case WIFI_PHASE_STARTUP_TCP_QUERY_SEND:
        if (StartCommand("AT+CIPSTATUS\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_STARTUP_TCP_QUERY_WAIT)) {
            ReportFixed("WIFI SYNC: TCP_QUERY\r\n");
        }
        break;
    case WIFI_PHASE_STARTUP_TCP_QUERY_WAIT:
        if (HasTerminalResponse()) {
            g_wifi.tcp_status = WiFiAT_ParseTcpStatus(g_wifi.response,
                                                       g_wifi.response_length, &tcp)
                                    ? tcp : WIFI_AT_TCP_UNKNOWN;
            ClearResponse();
            if (g_wifi.mode == 3U) {
                g_wifi.apsta = WIFI_APSTA_YES;
                g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
            } else if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED &&
                       g_wifi.apsta_set_attempted == 0U) {
                g_wifi.phase = WIFI_PHASE_APSTA_MODE_SET_SEND;
            } else {
                g_wifi.apsta = WIFI_APSTA_NO;
                g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
                if (g_wifi.tcp_status != WIFI_AT_TCP_DISCONNECTED) {
                    ReportFixed("WIFI SYNC: MODE_CHANGE_SKIPPED_TCP_PROTECTED\r\n");
                }
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
            ClearResponse();
            g_wifi.apsta = g_wifi.mode == 3U ? WIFI_APSTA_YES : WIFI_APSTA_NO;
            g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
            ReportFixed("WIFI SYNC: MODE_CHANGE_SKIPPED_TCP_UNKNOWN\r\n");
        }
        break;
    case WIFI_PHASE_APSTA_MODE_SET_SEND:
        built = WiFiAT_BuildApStaModeCommand(g_wifi.dialect, command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_APSTA_MODE_SET_WAIT)) {
            g_wifi.apsta_set_attempted = 1U;
            ReportFixed("WIFI SYNC: APSTA_CONFIG\r\n");
        }
        break;
    case WIFI_PHASE_APSTA_MODE_SET_WAIT:
        if (HasTerminalResponse()) {
            if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
                g_wifi.mode = 3U;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = WIFI_APSTA_YES;
            } else {
                g_wifi.apsta = WIFI_APSTA_UNKNOWN;
                ReportFixed("WIFI SYNC: APSTA_UNAVAILABLE\r\n");
            }
            ClearResponse();
            g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.apsta = WIFI_APSTA_UNKNOWN;
            ClearResponse();
            g_wifi.phase = WIFI_PHASE_STATION_QUERY_SEND;
        }
        break;
    case WIFI_PHASE_STATION_QUERY_SEND:
        StartStationQuery(now);
        break;
    case WIFI_PHASE_STATION_QUERY_WAIT:
        if (HasTerminalResponse()) {
            if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT &&
                (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
                 Contains(g_wifi.response, g_wifi.response_length, "FAIL"))) {
                ClearResponse();
                g_wifi.phase = WIFI_PHASE_STATION_FALLBACK_QUERY_SEND;
                ReportFixed("WIFI SYNC: CWJAP_FALLBACK_QUERY\r\n");
            } else {
                HandleStationQuery(now);
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ScheduleSyncRetry(now);
            ClearResponse();
            CompleteStatusCheck();
        }
        break;
    case WIFI_PHASE_STATION_FALLBACK_QUERY_SEND:
        built = WiFiAT_BuildConnectedStationQueryCommand(
            g_wifi.dialect, command, sizeof(command));
        if (!StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                               WIFI_PHASE_STATION_FALLBACK_QUERY_WAIT) &&
            g_wifi.phase != WIFI_PHASE_TX_FAULT) {
            ScheduleSyncRetry(now);
        }
        break;
    case WIFI_PHASE_STATION_FALLBACK_QUERY_WAIT:
        if (HasTerminalResponse()) {
            HandleStationQuery(now);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ReportStationQueryDiagnostic();
            ScheduleSyncRetry(now);
            ClearResponse();
            CompleteStatusCheck();
        }
        break;
    case WIFI_PHASE_IP_QUERY_SEND:
        if (!WiFiAT_BuildIpQueryCommand(g_wifi.dialect, command, sizeof(command))) {
            g_wifi.ip = WIFI_IP_UNKNOWN;
            ClearStationIpAddress();
            g_wifi.network = WIFI_NETWORK_NOT_RUN;
            WiFiState_AdoptConnected(&g_wifi.state);
            g_wifi.phase = WIFI_PHASE_IDLE;
            CompleteStatusCheck();
        } else if (StartCommand(command, now, WIFI_QUERY_TIMEOUT_MS,
                                WIFI_PHASE_IP_QUERY_WAIT)) {
            if (g_wifi.ip_query_attempts < 255U) {
                g_wifi.ip_query_attempts++;
            }
            ReportFixed("WIFI SYNC: IP_QUERY\r\n");
        }
        break;
    case WIFI_PHASE_IP_QUERY_WAIT:
        if (HasTerminalResponse()) {
            HandleIpQuery(now);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            HandleIpQueryTimeout(now);
        }
        break;
    case WIFI_PHASE_IP_QUERY_RETRY_WAIT:
        if (g_wifi.clear_pending != 0U) {
            g_wifi.ip_query_attempts = 0U;
            g_wifi.phase = WIFI_PHASE_CLEAR_SEND;
        } else if ((int32_t)(now - g_wifi.next_ip_query) >= 0) {
            g_wifi.phase = WIFI_PHASE_IP_QUERY_SEND;
        }
        break;
    case WIFI_PHASE_SYNC_RETRY_WAIT:
        if ((int32_t)(now - g_wifi.next_sync_retry) >= 0) {
            g_wifi.phase = WIFI_PHASE_PROBE_SEND;
            WiFiState_BeginSync(&g_wifi.state);
        }
        break;
    default:
        break;
    }
}

static bool IsConnectResponseWaitPhase(WiFiPhase phase)
{
    switch (phase) {
    case WIFI_PHASE_CONNECT_TCP_QUERY_WAIT:
    case WIFI_PHASE_CONNECT_VERSION_QUERY_WAIT:
    case WIFI_PHASE_DIALECT_PROBE_WAIT:
    case WIFI_PHASE_STORE_RAM_WAIT:
    case WIFI_PHASE_CONNECT_MODE_QUERY_WAIT:
    case WIFI_PHASE_CONNECT_MODE_SET_WAIT:
    case WIFI_PHASE_JOIN_WAIT:
    case WIFI_PHASE_PERSIST_STORE_WAIT:
    case WIFI_PHASE_PERSIST_MODE_WAIT:
    case WIFI_PHASE_PERSIST_JOIN_WAIT:
    case WIFI_PHASE_PERSIST_AUTOCONN_WAIT:
    case WIFI_PHASE_PERSIST_RAM_WAIT:
        return true;
    default:
        return false;
    }
}

static void CompletePersistentConnectFailure(uint32_t now, bool store_mode_safe)
{
    g_wifi.persist_join_associated = 0U;
    g_wifi.persist_join_ok = 0U;
    g_wifi.persist_failure_pending = 0U;

    if (!store_mode_safe) {
        g_wifi.persist_store_mode_unknown = 1U;
        g_wifi.persist_requested = 0U;
        g_wifi.persist_abort_requested = 0U;
        g_wifi.connect_pending = 0U;
        g_wifi.credentials_loaded = 0U;
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
        WiFiState_SetUnknown(&g_wifi.state);
        g_wifi.phase = WIFI_PHASE_IDLE;
        ReportFixed("WIFI CONNECT SAVE: STORE_MODE_UNKNOWN; STOPPED\r\n");
        return;
    }

    g_wifi.persist_store_maybe_enabled = 0U;
    if (g_wifi.persist_abort_requested != 0U) {
        g_wifi.persist_requested = 0U;
        g_wifi.persist_abort_requested = 0U;
        g_wifi.connect_pending = 0U;
        g_wifi.credentials_loaded = 0U;
        SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
        SecureClear(g_wifi.password, sizeof(g_wifi.password));
        g_wifi.phase = WIFI_PHASE_IDLE;
        ReportFixed("WIFI CONNECT SAVE: CANCELLED\r\n");
        return;
    }

    WiFiState_ConnectFailed(&g_wifi.state, now);
    g_wifi.retry_started_at = now;
    g_wifi.connect_pending = 0U;
    g_wifi.phase = WIFI_PHASE_IDLE;
    /* Keep the explicit SAVE request for the normal bounded reconnect retry. */
    ReportFixed("WIFI CONNECT SAVE: FAILED; RETRY_SCHEDULED\r\n");
    ReportFixed("WIFI CONNECT: RETRY_SCHEDULED\r\n");
}

static void FailPersistentConnect(uint32_t now)
{
    g_wifi.persist_failure_pending = 1U;
    ClearResponse();
    if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT &&
        g_wifi.persist_store_maybe_enabled != 0U) {
        g_wifi.phase = WIFI_PHASE_PERSIST_RAM_SEND;
    } else {
        CompletePersistentConnectFailure(now, true);
    }
}

static void CompletePersistentConnectSuccess(void)
{
    g_wifi.persist_requested = 0U;
    g_wifi.persist_abort_requested = 0U;
    g_wifi.persist_store_maybe_enabled = 0U;
    g_wifi.persist_join_associated = 0U;
    g_wifi.persist_join_ok = 0U;
    g_wifi.persist_failure_pending = 0U;
    g_wifi.connect_pending = 0U;
    WiFiState_ConnectSucceeded(&g_wifi.state);
    g_wifi.station = WIFI_AT_STATION_CONNECTED;
    g_wifi.ip = WIFI_IP_UNKNOWN;
    ClearStationIpAddress();
    g_wifi.network = WIFI_NETWORK_NOT_RUN;
    g_wifi.ip_query_attempts = 0U;
    g_wifi.phase = WIFI_PHASE_IP_QUERY_SEND;
    ClearResponse();
    ReportFixed("WIFI CONNECT SAVE: STORED; AUTO_CONNECT_ENABLED\r\n");
    ReportFixed("WIFI CONNECT: ASSOCIATED\r\n");
}

static void ProcessConnect(uint32_t now)
{
    char command[WIFI_COMMAND_CAPACITY];
    bool built;
    bool valid_gmr;
    unsigned int parsed_mode;

    if (g_wifi.persist_abort_requested != 0U &&
        g_wifi.phase != WIFI_PHASE_PERSIST_RAM_SEND &&
        g_wifi.phase != WIFI_PHASE_PERSIST_RAM_WAIT) {
        if (g_wifi.tx_active != 0U ||
            (IsConnectResponseWaitPhase(g_wifi.phase) &&
             !HasTerminalResponse() && (int32_t)(now - g_wifi.deadline) < 0)) {
            return;
        }
        if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT &&
            g_wifi.persist_store_maybe_enabled != 0U) {
            g_wifi.persist_failure_pending = 1U;
            g_wifi.phase = WIFI_PHASE_PERSIST_RAM_SEND;
            ClearResponse();
        } else {
            CompletePersistentConnectFailure(now, true);
            return;
        }
    }

    switch (g_wifi.phase) {
    case WIFI_PHASE_CONNECT_PREPARE:
        if (g_wifi.tx_active != 0U) {
            break;
        }
        if (g_wifi.tcp_status == WIFI_AT_TCP_UNKNOWN) {
            g_wifi.phase = WIFI_PHASE_CONNECT_TCP_QUERY_SEND;
        } else if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED) {
            g_wifi.phase = WIFI_PHASE_CONNECT_VERSION_QUERY_SEND;
        } else {
            WiFiState_SetUnknown(&g_wifi.state);
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ReportFixed("WIFI CONNECT: TCP_PROTECTED\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_TCP_QUERY_SEND:
        if (StartCommand("AT+CIPSTATUS\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_CONNECT_TCP_QUERY_WAIT)) {
            ReportFixed("WIFI CONNECT: CHECK_TCP\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_TCP_QUERY_WAIT:
        if (HasTerminalResponse()) {
            WiFiAtTcpStatus tcp_status = WIFI_AT_TCP_UNKNOWN;
            if (WiFiAT_ParseTcpStatus(g_wifi.response, g_wifi.response_length,
                                      &tcp_status)) {
                g_wifi.tcp_status = tcp_status;
            } else {
                g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
            }
            ClearResponse();
            if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED) {
                g_wifi.phase = WIFI_PHASE_CONNECT_VERSION_QUERY_SEND;
            } else {
                WiFiState_SetUnknown(&g_wifi.state);
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_IDLE;
                ReportFixed(g_wifi.tcp_status == WIFI_AT_TCP_CONNECTED
                                ? "WIFI CONNECT: TCP_PROTECTED\r\n"
                                : "WIFI CONNECT: TCP_UNKNOWN\r\n");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
            WiFiState_SetUnknown(&g_wifi.state);
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI CONNECT: TCP_UNKNOWN\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_VERSION_QUERY_SEND:
        if (StartCommand("AT+GMR\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_CONNECT_VERSION_QUERY_WAIT)) {
            ReportFixed("WIFI CONNECT: CHECK_AT_VERSION\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_VERSION_QUERY_WAIT:
        if (HasTerminalResponse()) {
            valid_gmr = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                        !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                        !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            g_wifi.dialect = valid_gmr
                                 ? WiFiAT_DetectDialectFromVersion(
                                       g_wifi.response, g_wifi.response_length)
                                 : WIFI_AT_DIALECT_UNKNOWN;
            ClearResponse();
            if (g_wifi.dialect == WIFI_AT_DIALECT_NONOS) {
                g_wifi.mode_known = 0U;
                g_wifi.apsta = WIFI_APSTA_UNKNOWN;
                g_wifi.phase = g_wifi.persist_requested != 0U
                                   ? WIFI_PHASE_PERSIST_MODE_SEND
                                   : WIFI_PHASE_CONNECT_MODE_QUERY_SEND;
                ReportFixed("WIFI CONNECT: NONOS_AT_CONFIRMED\r\n");
            } else if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT) {
                g_wifi.phase = WIFI_PHASE_DIALECT_PROBE_SEND;
                ReportFixed("WIFI CONNECT: ESP_AT_VERSION_CONFIRMED\r\n");
            } else {
                WiFiState_ConnectFailed(&g_wifi.state, now);
                g_wifi.retry_started_at = now;
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_IDLE;
                ReportFixed("WIFI CONNECT: DIALECT_UNKNOWN\r\n");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.dialect = WIFI_AT_DIALECT_UNKNOWN;
            WiFiState_ConnectFailed(&g_wifi.state, now);
            g_wifi.retry_started_at = now;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI CONNECT: AT_VERSION_TIMEOUT\r\n");
        }
        break;
    case WIFI_PHASE_DIALECT_PROBE_SEND:
        if (StartCommand("AT+CWSTATE?\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_DIALECT_PROBE_WAIT)) {
            ReportFixed("WIFI CONNECT: CHECK_AT_DIALECT\r\n");
        }
        break;
    case WIFI_PHASE_DIALECT_PROBE_WAIT:
        if (HasTerminalResponse()) {
            bool modern_response =
                WiFiAT_DetectDialect(g_wifi.response, g_wifi.response_length) ==
                    WIFI_AT_DIALECT_ESP_AT &&
                Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                !Contains(g_wifi.response, g_wifi.response_length, "FAIL");

            if (modern_response) {
                g_wifi.dialect = WIFI_AT_DIALECT_ESP_AT;
                g_wifi.phase = WIFI_PHASE_STORE_RAM_SEND;
                ReportFixed("WIFI CONNECT: ESP_AT_PROBE_CONFIRMED\r\n");
            } else {
                WiFiState_ConnectFailed(&g_wifi.state, now);
                g_wifi.retry_started_at = now;
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_IDLE;
                g_wifi.dialect = WIFI_AT_DIALECT_UNKNOWN;
                ReportFixed("WIFI CONNECT: DIALECT_UNKNOWN\r\n");
            }
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            WiFiState_ConnectFailed(&g_wifi.state, now);
            g_wifi.retry_started_at = now;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI CONNECT: DIALECT_PROBE_TIMEOUT\r\n");
        }
        break;
    case WIFI_PHASE_STORE_RAM_SEND:
        built = WiFiAT_BuildRamStoreCommand(command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_STORE_RAM_WAIT)) {
            ReportFixed("WIFI CONNECT: VOLATILE_STORAGE\r\n");
        }
        break;
    case WIFI_PHASE_STORE_RAM_WAIT:
        if (HasTerminalResponse()) {
            if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
                g_wifi.phase = g_wifi.persist_requested != 0U
                                   ? WIFI_PHASE_PERSIST_STORE_SEND
                                   : WIFI_PHASE_CONNECT_MODE_QUERY_SEND;
            } else {
                ReportVolatileStorageFailure();
                WiFiState_ConnectFailed(&g_wifi.state, now);
                g_wifi.retry_started_at = now;
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_IDLE;
            }
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ReportFixed("WIFI CONNECT: VOLATILE_STORAGE_TIMEOUT\r\n");
            WiFiState_ConnectFailed(&g_wifi.state, now);
            g_wifi.retry_started_at = now;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
        }
        break;
    case WIFI_PHASE_CONNECT_MODE_QUERY_SEND:
        built = WiFiAT_BuildModeQueryCommand(g_wifi.dialect, command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_CONNECT_MODE_QUERY_WAIT)) {
            ReportFixed("WIFI CONNECT: CHECK_MODE\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_MODE_QUERY_WAIT:
        if (HasTerminalResponse()) {
            if (WiFiAT_ParseMode(g_wifi.dialect, g_wifi.response,
                                 g_wifi.response_length, &parsed_mode)) {
                g_wifi.mode = (uint8_t)parsed_mode;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = parsed_mode == 3U ? WIFI_APSTA_YES : WIFI_APSTA_NO;
                if (parsed_mode == 3U) {
                    g_wifi.phase = WIFI_PHASE_JOIN_SEND;
                } else if (g_wifi.tcp_status == WIFI_AT_TCP_DISCONNECTED) {
                    g_wifi.phase = WIFI_PHASE_CONNECT_MODE_SET_SEND;
                } else {
                    WiFiState_SetUnknown(&g_wifi.state);
                    g_wifi.connect_pending = 0U;
                    g_wifi.phase = WIFI_PHASE_IDLE;
                    ReportFixed("WIFI CONNECT: MODE_CHANGE_BLOCKED_TCP\r\n");
                }
            } else {
                WiFiState_SetUnknown(&g_wifi.state);
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_SYNC_RETRY_WAIT;
                g_wifi.next_sync_retry = now + WIFI_SYNC_RETRY_MS;
            }
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            WiFiState_SetUnknown(&g_wifi.state);
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_SYNC_RETRY_WAIT;
            g_wifi.next_sync_retry = now + WIFI_SYNC_RETRY_MS;
            ClearResponse();
        }
        break;
    case WIFI_PHASE_CONNECT_MODE_SET_SEND:
        built = WiFiAT_BuildApStaModeCommand(g_wifi.dialect, command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_CONNECT_MODE_SET_WAIT)) {
            ReportFixed("WIFI CONNECT: APSTA_MODE\r\n");
        }
        break;
    case WIFI_PHASE_CONNECT_MODE_SET_WAIT:
        if (HasTerminalResponse()) {
            if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
                g_wifi.mode = 3U;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = WIFI_APSTA_YES;
                g_wifi.phase = WIFI_PHASE_JOIN_SEND;
            } else {
                g_wifi.apsta = WIFI_APSTA_UNKNOWN;
                WiFiState_SetUnknown(&g_wifi.state);
                g_wifi.connect_pending = 0U;
                g_wifi.phase = WIFI_PHASE_IDLE;
                ReportFixed("WIFI CONNECT: APSTA_UNAVAILABLE\r\n");
            }
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.apsta = WIFI_APSTA_UNKNOWN;
            WiFiState_SetUnknown(&g_wifi.state);
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
        }
        break;
    case WIFI_PHASE_JOIN_SEND:
        built = WiFiAT_BuildJoinCommand(g_wifi.dialect, g_wifi.ssid, g_wifi.password,
                                        command, sizeof(command));
        if (!built) {
            WiFiState_Clear(&g_wifi.state);
            g_wifi.credentials_loaded = 0U;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            SecureClear(g_wifi.ssid, sizeof(g_wifi.ssid));
            SecureClear(g_wifi.password, sizeof(g_wifi.password));
        } else {
            StartBuiltCommand(true, command, now, WIFI_CONNECT_TIMEOUT_MS,
                              WIFI_PHASE_JOIN_WAIT);
            SecureClear(command, sizeof(command));
            ReportFixed("WIFI CONNECT: JOINING\r\n");
        }
        break;
    case WIFI_PHASE_JOIN_WAIT:
        if (Contains(g_wifi.response, g_wifi.response_length, "WIFI GOT IP") ||
            Contains(g_wifi.response, g_wifi.response_length, "WIFI CONNECTED")) {
            WiFiState_ConnectSucceeded(&g_wifi.state);
            g_wifi.station = WIFI_AT_STATION_CONNECTED;
            g_wifi.ip = WIFI_IP_UNKNOWN;
            ClearStationIpAddress();
            g_wifi.network = WIFI_NETWORK_NOT_RUN;
            g_wifi.ip_query_attempts = 0U;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IP_QUERY_SEND;
            ClearResponse();
            ReportFixed("WIFI CONNECT: ASSOCIATED\r\n");
        } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
                   Contains(g_wifi.response, g_wifi.response_length, "FAIL") ||
                   (int32_t)(now - g_wifi.deadline) >= 0) {
            ReportJoinFailureDiagnostic();
            WiFiState_ConnectFailed(&g_wifi.state, now);
            g_wifi.retry_started_at = now;
            g_wifi.connect_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI CONNECT: RETRY_SCHEDULED\r\n");
        }
        break;
    case WIFI_PHASE_PERSIST_STORE_SEND:
        built = WiFiAT_BuildPersistentStoreCommand(command, sizeof(command));
        /* A lost acknowledgement still requires a best-effort SYSSTORE=0. */
        g_wifi.persist_store_maybe_enabled = 1U;
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_PERSIST_STORE_WAIT)) {
            ReportFixed("WIFI CONNECT SAVE: ENABLING_STORAGE\r\n");
        }
        SecureClear(command, sizeof(command));
        break;
    case WIFI_PHASE_PERSIST_STORE_WAIT:
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            ClearResponse();
            if (!accepted || g_wifi.persist_abort_requested != 0U) {
                FailPersistentConnect(now);
            } else {
                g_wifi.phase = WIFI_PHASE_PERSIST_MODE_SEND;
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FailPersistentConnect(now);
        }
        break;
    case WIFI_PHASE_PERSIST_MODE_SEND:
        built = WiFiAT_BuildPersistentApStaModeCommand(g_wifi.dialect, command,
                                                       sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_PERSIST_MODE_WAIT)) {
            ReportFixed("WIFI CONNECT SAVE: STORING_MODE\r\n");
        }
        SecureClear(command, sizeof(command));
        break;
    case WIFI_PHASE_PERSIST_MODE_WAIT:
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            ClearResponse();
            if (!accepted || g_wifi.persist_abort_requested != 0U) {
                FailPersistentConnect(now);
            } else {
                g_wifi.mode = 3U;
                g_wifi.mode_known = 1U;
                g_wifi.apsta = WIFI_APSTA_YES;
                g_wifi.phase = WIFI_PHASE_PERSIST_JOIN_SEND;
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FailPersistentConnect(now);
        }
        break;
    case WIFI_PHASE_PERSIST_JOIN_SEND:
        built = WiFiAT_BuildPersistentJoinCommand(g_wifi.dialect, g_wifi.ssid,
                                                  g_wifi.password, command,
                                                  sizeof(command));
        if (!built) {
            FailPersistentConnect(now);
        } else if (StartBuiltCommand(built, command, now, WIFI_CONNECT_TIMEOUT_MS,
                                     WIFI_PHASE_PERSIST_JOIN_WAIT)) {
            g_wifi.persist_join_associated =
                g_wifi.station == WIFI_AT_STATION_CONNECTED ? 1U : 0U;
            g_wifi.persist_join_ok = 0U;
            ReportFixed("WIFI CONNECT SAVE: JOINING\r\n");
        }
        SecureClear(command, sizeof(command));
        break;
    case WIFI_PHASE_PERSIST_JOIN_WAIT:
        if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
            Contains(g_wifi.response, g_wifi.response_length, "FAIL") ||
            Contains(g_wifi.response, g_wifi.response_length, "No AP") ||
            (int32_t)(now - g_wifi.deadline) >= 0) {
            ReportJoinFailureDiagnostic();
            FailPersistentConnect(now);
        } else {
            if (Contains(g_wifi.response, g_wifi.response_length, "WIFI GOT IP") ||
                Contains(g_wifi.response, g_wifi.response_length, "WIFI CONNECTED")) {
                g_wifi.persist_join_associated = 1U;
            }
            if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
                g_wifi.persist_join_ok = 1U;
            }
            if (g_wifi.persist_join_associated != 0U &&
                g_wifi.persist_join_ok != 0U) {
                g_wifi.phase = WIFI_PHASE_PERSIST_AUTOCONN_SEND;
                ClearResponse();
            }
        }
        break;
    case WIFI_PHASE_PERSIST_AUTOCONN_SEND:
        built = WiFiAT_BuildAutoConnectCommand(g_wifi.dialect, command,
                                               sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_PERSIST_AUTOCONN_WAIT)) {
            ReportFixed("WIFI CONNECT SAVE: ENABLING_AUTO_CONNECT\r\n");
        }
        SecureClear(command, sizeof(command));
        break;
    case WIFI_PHASE_PERSIST_AUTOCONN_WAIT:
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            ClearResponse();
            if (!accepted || g_wifi.persist_abort_requested != 0U) {
                FailPersistentConnect(now);
            } else if (g_wifi.dialect == WIFI_AT_DIALECT_ESP_AT &&
                       g_wifi.persist_store_maybe_enabled != 0U) {
                g_wifi.phase = WIFI_PHASE_PERSIST_RAM_SEND;
            } else {
                CompletePersistentConnectSuccess();
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FailPersistentConnect(now);
        }
        break;
    case WIFI_PHASE_PERSIST_RAM_SEND:
        built = WiFiAT_BuildRamStoreCommand(command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_PERSIST_RAM_WAIT)) {
            ReportFixed("WIFI CONNECT SAVE: RESTORING_VOLATILE_MODE\r\n");
        }
        SecureClear(command, sizeof(command));
        break;
    case WIFI_PHASE_PERSIST_RAM_WAIT:
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            ClearResponse();
            if (!accepted) {
                CompletePersistentConnectFailure(now, false);
            } else if (g_wifi.persist_failure_pending != 0U ||
                       g_wifi.persist_abort_requested != 0U) {
                CompletePersistentConnectFailure(now, true);
            } else {
                g_wifi.persist_store_maybe_enabled = 0U;
                CompletePersistentConnectSuccess();
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ClearResponse();
            CompletePersistentConnectFailure(now, false);
        }
        break;
    default:
        break;
    }
}

static void ProcessMaintenance(uint32_t now)
{
    char command[WIFI_COMMAND_CAPACITY];
    WiFiAtTcpStatus tcp;
    WiFiAtTcpMuxMode tcp_mux_mode;
    WiFiAtDnsInfo dns_info;
    bool built;

    if (g_wifi.phase == WIFI_PHASE_PING_SEND) {
        built = WiFiAT_BuildPingCommand(g_wifi.dialect, g_wifi.ping_host,
                                        command, sizeof(command));
        if (!built) {
            FinishPing(false, false);
        } else if (StartBuiltCommand(built, command, now, WIFI_PING_TIMEOUT_MS,
                                     WIFI_PHASE_PING_WAIT)) {
            ReportFixed("WIFI PING: STARTED\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_PING_WAIT) {
        if ((Contains(g_wifi.response, g_wifi.response_length, "+PING:") ||
             ContainsNumericPingReply(g_wifi.response, g_wifi.response_length)) &&
            Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
            FinishPing(true, false);
        } else if (Contains(g_wifi.response, g_wifi.response_length, "+timeout") ||
                   Contains(g_wifi.response, g_wifi.response_length, "+PING:TIMEOUT")) {
            FinishPing(false, true);
        } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
                   Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
            FinishPing(false, false);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FinishPing(false, true);
        }
    } else if (g_wifi.phase == WIFI_PHASE_MODULE_VERSION_QUERY_SEND) {
        if (StartCommand("AT+GMR\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_MODULE_VERSION_QUERY_WAIT)) {
            ReportFixed("WIFI GMR: QUERY\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_MODULE_VERSION_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
                ReportModuleVersion();
            } else {
                ReportFixed("WIFI GMR: ERROR\r\n");
            }
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            ReportFixed("WIFI GMR: TIMEOUT\r\n");
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
        }
    } else if (g_wifi.phase == WIFI_PHASE_DNS_QUERY_SEND) {
        built = WiFiAT_BuildDnsQueryCommand(g_wifi.dialect, command, sizeof(command));
        if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                              WIFI_PHASE_DNS_QUERY_WAIT)) {
            ReportFixed("WIFI DNS STATUS: QUERY\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_DNS_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            bool gateway_verified = false;
            if (WiFiAT_ParseDnsInfo(g_wifi.dialect, g_wifi.response,
                                    g_wifi.response_length, &dns_info)) {
                gateway_verified = g_wifi.dns_gateway_pending != 0U &&
                                   (g_wifi.dialect == WIFI_AT_DIALECT_NONOS ||
                                    dns_info.mode == WIFI_AT_DNS_MODE_MANUAL) &&
                                   dns_info.server_count > 0U &&
                                   strcmp(dns_info.servers[0],
                                          g_wifi.dns_gateway_expected) == 0;
                ReportDnsStatus(&dns_info);
            } else {
                ReportDnsStatus(NULL);
            }
            SecureClear(&dns_info, sizeof(dns_info));
            g_wifi.dns_query_pending = 0U;
            if (g_wifi.dns_gateway_pending != 0U) {
                g_wifi.dns_gateway_pending = 0U;
                ClearDnsGatewayExpected();
                ReportFixed(gateway_verified
                                ? "WIFI DNS GATEWAY: VERIFIED\r\n"
                                : "WIFI DNS GATEWAY: VERIFY_FAILED\r\n");
            }
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.dns_query_pending = 0U;
            if (g_wifi.dns_gateway_pending != 0U) {
                g_wifi.dns_gateway_pending = 0U;
                ClearDnsGatewayExpected();
                ReportFixed("WIFI DNS GATEWAY: VERIFY_TIMEOUT\r\n");
            }
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI DNS STATUS: TIMEOUT\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_DNS_GATEWAY_STORE_RAM_SEND) {
        built = WiFiAT_BuildRamStoreCommand(command, sizeof(command));
        if (!built) {
            g_wifi.dns_gateway_pending = 0U;
            ClearDnsGatewayExpected();
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI DNS GATEWAY: STORE_MODE_BUILD_FAILED\r\n");
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_DNS_GATEWAY_STORE_RAM_WAIT)) {
            ReportFixed("WIFI DNS GATEWAY: VOLATILE_STORAGE\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_DNS_GATEWAY_STORE_RAM_WAIT) {
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            ClearResponse();
            if (accepted) {
                g_wifi.phase = WIFI_PHASE_DNS_GATEWAY_SET_SEND;
            } else {
                g_wifi.dns_gateway_pending = 0U;
                ClearDnsGatewayExpected();
                g_wifi.phase = WIFI_PHASE_IDLE;
                ReportFixed("WIFI DNS GATEWAY: STORE_MODE_FAILED\r\n");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.dns_gateway_pending = 0U;
            ClearDnsGatewayExpected();
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI DNS GATEWAY: STORE_MODE_TIMEOUT\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_DNS_GATEWAY_SET_SEND) {
        if (!WiFi_IsConnected() ||
            g_wifi.station != WIFI_AT_STATION_CONNECTED ||
            g_wifi.ip != WIFI_IP_READY ||
            g_wifi.tcp_status != WIFI_AT_TCP_DISCONNECTED) {
            g_wifi.dns_gateway_pending = 0U;
            ClearDnsGatewayExpected();
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI DNS GATEWAY: STATE_CHANGED\r\n");
        } else {
            built = WiFiAT_BuildDnsGatewayCommand(g_wifi.dialect,
                                                  g_wifi.dns_gateway_expected,
                                                  command, sizeof(command));
            if (!built) {
                g_wifi.dns_gateway_pending = 0U;
                ClearDnsGatewayExpected();
                g_wifi.phase = WIFI_PHASE_IDLE;
                ClearResponse();
                ReportFixed("WIFI DNS GATEWAY: NO_VALID_GATEWAY\r\n");
            } else if (StartBuiltCommand(built, command, now,
                                         WIFI_QUERY_TIMEOUT_MS,
                                         WIFI_PHASE_DNS_GATEWAY_SET_WAIT)) {
                ReportFixed("WIFI DNS GATEWAY: SETTING\r\n");
            }
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_DNS_GATEWAY_SET_WAIT) {
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            if (accepted) {
                g_wifi.phase = WIFI_PHASE_DNS_QUERY_SEND;
                g_wifi.dns_query_pending = 1U;
                ClearResponse();
                ReportFixed("WIFI DNS GATEWAY: SET\r\n");
            } else {
                g_wifi.dns_gateway_pending = 0U;
                ClearDnsGatewayExpected();
                g_wifi.phase = WIFI_PHASE_IDLE;
                ClearResponse();
                ReportFixed("WIFI DNS GATEWAY: SET_FAILED\r\n");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.dns_gateway_pending = 0U;
            ClearDnsGatewayExpected();
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportFixed("WIFI DNS GATEWAY: TIMEOUT\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_QUERY_SEND) {
        if (StartCommand("AT+CIPSTATUS\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_TCP_QUERY_WAIT)) {
            ReportFixed("WIFI TCP: QUERY\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            bool station_unavailable =
                WiFiAT_TcpStatusIndicatesStationUnavailable(g_wifi.response,
                                                            g_wifi.response_length);

            if (!WiFiAT_ParseTcpStatus(g_wifi.response, g_wifi.response_length, &tcp)) {
                tcp = WIFI_AT_TCP_UNKNOWN;
            }
            if (station_unavailable) {
                tcp = WIFI_AT_TCP_DISCONNECTED;
                FinishDisconnected(now);
            } else {
                g_wifi.phase = WIFI_PHASE_IDLE;
                ClearResponse();
            }
            g_wifi.tcp_status = tcp;
            g_wifi.query_result_ready = 1U;
            g_wifi.tcp_query_result_ready = 1U;
            ReportTcpStatus();
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
            g_wifi.query_result_ready = 1U;
            g_wifi.tcp_query_result_ready = 1U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportTcpStatus();
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_MUX_QUERY_SEND) {
        built = WiFiAT_BuildTcpMuxQueryCommand(command, sizeof(command));
        if (!built) {
            g_wifi.phase = WIFI_PHASE_IDLE;
            g_wifi.tcp_mux_check_pending = 0U;
            g_wifi.tcp_mux_mode = WIFI_AT_TCP_MUX_UNKNOWN;
            ReportTcpMuxMode(WIFI_AT_TCP_MUX_UNKNOWN);
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_MUX_QUERY_WAIT)) {
            ReportFixed("WIFI TCP MUX: QUERY\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_MUX_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            if (!WiFiAT_ParseTcpMuxMode(g_wifi.response, g_wifi.response_length,
                                        &tcp_mux_mode)) {
                tcp_mux_mode = WIFI_AT_TCP_MUX_UNKNOWN;
            }
            g_wifi.tcp_mux_mode = tcp_mux_mode;
            g_wifi.tcp_mux_check_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportTcpMuxMode(tcp_mux_mode);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_mux_check_pending = 0U;
            g_wifi.tcp_mux_mode = WIFI_AT_TCP_MUX_UNKNOWN;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportTcpMuxMode(WIFI_AT_TCP_MUX_UNKNOWN);
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_MODE_QUERY_SEND) {
        built = WiFiAT_BuildTcpModeQueryCommand(command, sizeof(command));
        if (!built) {
            g_wifi.phase = WIFI_PHASE_IDLE;
            g_wifi.tcp_mode_check_pending = 0U;
            g_wifi.tcp_mode = WIFI_AT_TCP_MODE_UNKNOWN;
            ReportTcpMode(WIFI_AT_TCP_MODE_UNKNOWN);
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_MODE_QUERY_WAIT)) {
            ReportFixed("WIFI TCP MODE: QUERY\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_MODE_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            if (!WiFiAT_ParseTcpMode(g_wifi.response, g_wifi.response_length,
                                     &g_wifi.tcp_mode)) {
                g_wifi.tcp_mode = WIFI_AT_TCP_MODE_UNKNOWN;
            }
            g_wifi.tcp_mode_check_pending = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportTcpMode(g_wifi.tcp_mode);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_mode_check_pending = 0U;
            g_wifi.tcp_mode = WIFI_AT_TCP_MODE_UNKNOWN;
            g_wifi.phase = WIFI_PHASE_IDLE;
            ClearResponse();
            ReportTcpMode(WIFI_AT_TCP_MODE_UNKNOWN);
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_NORMAL_MODE_SET_SEND) {
        built = WiFiAT_BuildNormalModeCommand(command, sizeof(command));
        if (!built) {
            FinishTcpConnect(false, "CIPMODE_SET_BUILD_FAILED");
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_NORMAL_MODE_SET_WAIT)) {
            ReportFixed("WIFI TCP MODE GUARD: SET_NORMAL\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_NORMAL_MODE_SET_WAIT) {
        if (HasTerminalResponse()) {
            bool accepted = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                            !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            if (accepted) {
                g_wifi.phase = WIFI_PHASE_TCP_NORMAL_MODE_QUERY_SEND;
                ClearResponse();
                ReportFixed("WIFI TCP MODE GUARD: SET_OK\r\n");
            } else {
                FinishTcpConnect(false, "CIPMODE_SET_FAILED");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FinishTcpConnect(false, "CIPMODE_SET_TIMEOUT");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_NORMAL_MODE_QUERY_SEND) {
        built = WiFiAT_BuildTcpModeQueryCommand(command, sizeof(command));
        if (!built) {
            FinishTcpConnect(false, "CIPMODE_VERIFY_BUILD_FAILED");
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_NORMAL_MODE_QUERY_WAIT)) {
            ReportFixed("WIFI TCP MODE GUARD: VERIFY_NORMAL\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_NORMAL_MODE_QUERY_WAIT) {
        if (HasTerminalResponse()) {
            if (!WiFiAT_ParseTcpMode(g_wifi.response, g_wifi.response_length,
                                     &g_wifi.tcp_mode)) {
                g_wifi.tcp_mode = WIFI_AT_TCP_MODE_UNKNOWN;
            }
            if (g_wifi.tcp_mode == WIFI_AT_TCP_MODE_NORMAL) {
                g_wifi.phase = WIFI_PHASE_TCP_CONNECT_SEND;
                ClearResponse();
                ReportTcpMode(WIFI_AT_TCP_MODE_NORMAL);
            } else {
                ReportTcpMode(g_wifi.tcp_mode);
                FinishTcpConnect(false, "CIPMODE_VERIFY_FAILED");
            }
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            g_wifi.tcp_mode = WIFI_AT_TCP_MODE_UNKNOWN;
            FinishTcpConnect(false, "CIPMODE_VERIFY_TIMEOUT");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_CONNECT_SEND) {
        built = WiFiAT_BuildTcpConnectCommand(g_wifi.dialect, g_wifi.tcp_mux_mode,
                                               g_wifi.tcp_host, g_wifi.tcp_port,
                                               command, sizeof(command));
        if (!built) {
            FinishTcpConnect(false, "COMMAND_BUILD_REJECTED");
        } else if (StartBuiltCommand(built, command, now, WIFI_TCP_CONNECT_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_CONNECT_WAIT)) {
            ReportFixed("WIFI TCP: CONNECTING\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_CONNECT_WAIT) {
        if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
            Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
            FinishTcpConnect(false, ClassifyTcpConnectFailure(false));
        } else if (Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
            FinishTcpConnect(true, NULL);
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FinishTcpConnect(false, ClassifyTcpConnectFailure(true));
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_SEND_COMMAND_SEND) {
        built = WiFiAT_BuildTcpSendCommand(g_wifi.tcp_mux_mode,
                                            g_wifi.tcp_payload_length,
                                            command, sizeof(command));
        if (!built) {
            FinishTcpSend(false, "WIFI TCP DIAG: CIPSEND_COMMAND_BUILD_FAILED\r\n");
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_SEND_PROMPT_WAIT)) {
            ReportFixed("WIFI TCP: SENDING\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_SEND_PROMPT_WAIT) {
        if (Contains(g_wifi.response, g_wifi.response_length, ">")) {
            if (!StartTcpPayload(now)) {
                if (g_wifi.phase != WIFI_PHASE_TX_FAULT) {
                    FinishTcpSend(false, "WIFI TCP DIAG: PAYLOAD_START_REJECTED\r\n");
                }
            }
        } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR")) {
            FinishTcpSend(false, "WIFI TCP DIAG: CIPSEND_PROMPT_ERROR\r\n");
        } else if (Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
            FinishTcpSend(false, "WIFI TCP DIAG: CIPSEND_PROMPT_FAIL\r\n");
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FinishTcpSend(false, g_wifi.response_length == 0U
                                    ? "WIFI TCP DIAG: CIPSEND_PROMPT_TIMEOUT_EMPTY\r\n"
                                    : "WIFI TCP DIAG: CIPSEND_PROMPT_TIMEOUT_WITH_RESPONSE\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_SEND_DATA) {
        if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FailPayloadTransmit();
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_SEND_RESULT_WAIT) {
        if (Contains(g_wifi.response, g_wifi.response_length, "SEND OK")) {
            FinishTcpSend(true, NULL);
        } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR")) {
            FinishTcpSend(false, "WIFI TCP DIAG: SEND_RESULT_ERROR\r\n");
        } else if (Contains(g_wifi.response, g_wifi.response_length, "FAIL")) {
            FinishTcpSend(false, "WIFI TCP DIAG: SEND_RESULT_FAIL\r\n");
        } else if ((int32_t)(now - g_wifi.deadline) >= 0) {
            FinishTcpSend(false, g_wifi.response_length == 0U
                                    ? "WIFI TCP DIAG: SEND_OK_TIMEOUT_EMPTY\r\n"
                                    : "WIFI TCP DIAG: SEND_OK_TIMEOUT_WITH_RESPONSE\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_TCP_CLOSE_SEND) {
        built = WiFiAT_BuildTcpCloseCommand(g_wifi.tcp_mux_mode,
                                            command, sizeof(command));
        if (!built) {
            FinishTcpClose(false);
        } else if (StartBuiltCommand(built, command, now, WIFI_QUERY_TIMEOUT_MS,
                                     WIFI_PHASE_TCP_CLOSE_WAIT)) {
            ReportFixed("WIFI TCP: CLOSE_REQUESTED\r\n");
        }
        SecureClear(command, sizeof(command));
    } else if (g_wifi.phase == WIFI_PHASE_TCP_CLOSE_WAIT) {
        if (Contains(g_wifi.response, g_wifi.response_length, "CLOSED") ||
            Contains(g_wifi.response, g_wifi.response_length, "OK\r\n")) {
            FinishTcpClose(true);
        } else if (Contains(g_wifi.response, g_wifi.response_length, "ERROR") ||
                   Contains(g_wifi.response, g_wifi.response_length, "FAIL") ||
                   (int32_t)(now - g_wifi.deadline) >= 0) {
            FinishTcpClose(false);
        }
    } else if (g_wifi.phase == WIFI_PHASE_CLEAR_SEND) {
        if (StartCommand("AT+CWQAP\r\n", now, WIFI_QUERY_TIMEOUT_MS,
                         WIFI_PHASE_CLEAR_WAIT)) {
            g_wifi.clear_command_started = 1U;
            ReportFixed("WIFI CLEAR: DISCONNECT_REQUESTED\r\n");
        }
    } else if (g_wifi.phase == WIFI_PHASE_CLEAR_WAIT) {
        if (HasTerminalResponse() || (int32_t)(now - g_wifi.deadline) >= 0) {
            bool confirmed = Contains(g_wifi.response, g_wifi.response_length, "OK\r\n") &&
                             !Contains(g_wifi.response, g_wifi.response_length, "ERROR") &&
                             !Contains(g_wifi.response, g_wifi.response_length, "FAIL");
            g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
            g_wifi.phase = WIFI_PHASE_IDLE;
            g_wifi.clear_pending = 0U;
            g_wifi.clear_command_started = 0U;
            ClearResponse();
            ReportFixed(confirmed ? "WIFI CLEAR: MODULE_DISCONNECTED\r\n" :
                                    "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_UNCONFIRMED\r\n");
        }
    }
}

void WiFi_Process(void)
{
    uint32_t now = HAL_GetTick();

    if (g_wifi.phase == WIFI_PHASE_TX_FAULT) {
        return;
    }
    DrainWifiRx();
    if (g_wifi.rx_rearm_deferred != 0U) {
        g_wifi.rx_rearm_deferred = 0U;
        if (!RearmWifiRx(&huart4)) {
            g_wifi.rx_rearm_deferred = 1U;
        }
    }
    if (g_wifi.tx_active != 0U &&
        g_wifi.phase != WIFI_PHASE_TCP_SEND_DATA &&
        (int32_t)(now - g_wifi.deadline) >= 0) {
        FailCommandChannel("WIFI UART4: TX_TIMEOUT; COMMAND_CHANNEL_UNSYNCHRONIZED\r\n");
        return;
    }

    if (g_wifi.response_length > 0U &&
        Contains(g_wifi.response, g_wifi.response_length, "WIFI DISCONNECT") &&
        g_wifi.state.status == WIFI_STATUS_CONNECTED) {
        WiFiState_ConnectionLost(&g_wifi.state, now);
        g_wifi.retry_started_at = now;
        g_wifi.station = WIFI_AT_STATION_DISCONNECTED;
        g_wifi.ip = WIFI_IP_NOT_READY;
        ClearStationIpAddress();
        ClearStationLinkInfo();
        g_wifi.tcp_status = WIFI_AT_TCP_UNKNOWN;
        g_wifi.network = WIFI_NETWORK_NOT_RUN;
        CompleteStatusCheck();
        ReportFixed("WIFI EVENT: DISCONNECTED\r\n");
        ClearResponse();
        if (g_wifi.phase == WIFI_PHASE_IP_QUERY_SEND ||
            g_wifi.phase == WIFI_PHASE_IP_QUERY_WAIT ||
            g_wifi.phase == WIFI_PHASE_IP_QUERY_RETRY_WAIT) {
            g_wifi.ip_query_attempts = 0U;
            g_wifi.next_ip_query = 0U;
            g_wifi.phase = WIFI_PHASE_IDLE;
        }
    }

    if (g_wifi.response_length > 0U &&
        Contains(g_wifi.response, g_wifi.response_length, "WIFI GOT IP") &&
        (g_wifi.phase == WIFI_PHASE_IDLE ||
         g_wifi.phase == WIFI_PHASE_IP_QUERY_RETRY_WAIT)) {
        bool query_ip = g_wifi.state.status == WIFI_STATUS_CONNECTED &&
                        g_wifi.station == WIFI_AT_STATION_CONNECTED &&
                        g_wifi.ip != WIFI_IP_READY && g_wifi.clear_pending == 0U;
        ClearResponse();
        if (query_ip) {
            g_wifi.ip = WIFI_IP_UNKNOWN;
            ClearStationIpAddress();
            g_wifi.ip_query_attempts = 0U;
            g_wifi.next_ip_query = 0U;
            g_wifi.network = WIFI_NETWORK_NOT_RUN;
            g_wifi.phase = WIFI_PHASE_IP_QUERY_SEND;
            ReportFixed("WIFI EVENT: GOT_IP\r\n");
        }
    }

    if (g_wifi.phase == WIFI_PHASE_IDLE && g_wifi.clear_pending != 0U) {
        g_wifi.phase = WIFI_PHASE_CLEAR_SEND;
    }
    if (g_wifi.phase == WIFI_PHASE_IP_QUERY_RETRY_WAIT &&
        g_wifi.clear_pending != 0U) {
        g_wifi.ip_query_attempts = 0U;
        g_wifi.phase = WIFI_PHASE_CLEAR_SEND;
    }
    if (g_wifi.phase == WIFI_PHASE_IDLE && g_wifi.connect_pending != 0U) {
        g_wifi.phase = WIFI_PHASE_CONNECT_PREPARE;
    }
    if (g_wifi.phase == WIFI_PHASE_IDLE &&
        g_wifi.state.status == WIFI_STATUS_RETRY_WAIT &&
        WiFiState_RetryDue(&g_wifi.state, now)) {
        g_wifi.connect_pending = 1U;
        g_wifi.phase = WIFI_PHASE_CONNECT_PREPARE;
    }
    if (g_wifi.phase == WIFI_PHASE_IDLE && g_wifi.sync_started != 0U &&
        g_wifi.dialect == WIFI_AT_DIALECT_UNKNOWN) {
        g_wifi.phase = WIFI_PHASE_PROBE_SEND;
        WiFiState_BeginSync(&g_wifi.state);
    }

    if (g_wifi.phase <= WIFI_PHASE_IP_QUERY_RETRY_WAIT ||
        g_wifi.phase == WIFI_PHASE_SYNC_RETRY_WAIT) {
        ProcessSync(now);
    } else if (g_wifi.phase >= WIFI_PHASE_CONNECT_PREPARE &&
               g_wifi.phase <= WIFI_PHASE_PERSIST_RAM_WAIT) {
        ProcessConnect(now);
    } else {
        ProcessMaintenance(now);
    }
}
