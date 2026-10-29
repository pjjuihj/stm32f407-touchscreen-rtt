#include "wifi_at.h"

#include <stdio.h>
#include <string.h>

static bool Contains(const char *text, size_t length, const char *needle)
{
    size_t needle_length = strlen(needle);
    size_t i;

    if (text == NULL || needle_length == 0U || needle_length > length) {
        return false;
    }
    for (i = 0U; i <= length - needle_length; i++) {
        if (memcmp(&text[i], needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool HasLine(const char *text, size_t length, const char *expected)
{
    size_t position = 0U;
    size_t expected_length;

    if (text == NULL || expected == NULL) {
        return false;
    }
    expected_length = strlen(expected);
    while (position < length) {
        size_t start = position;
        size_t line_length;

        while (position < length && text[position] != '\r' && text[position] != '\n') {
            position++;
        }
        line_length = position - start;
        if (line_length == expected_length &&
            memcmp(&text[start], expected, expected_length) == 0) {
            return true;
        }
        while (position < length && (text[position] == '\r' || text[position] == '\n')) {
            position++;
        }
    }
    return false;
}

static bool HasFinalLine(const char *text, size_t length, const char *expected)
{
    size_t end = length;
    size_t start;
    size_t expected_length;

    if (text == NULL || expected == NULL) {
        return false;
    }
    expected_length = strlen(expected);
    if (expected_length == 0U) {
        return false;
    }
    while (end > 0U && (text[end - 1U] == '\r' || text[end - 1U] == '\n')) {
        end--;
    }
    start = end;
    while (start > 0U && text[start - 1U] != '\r' && text[start - 1U] != '\n') {
        start--;
    }
    return end - start == expected_length &&
           memcmp(&text[start], expected, expected_length) == 0;
}

static bool HasPrefix(const char *text, size_t length, const char *prefix)
{
    const char *found = NULL;
    size_t prefix_length = strlen(prefix);
    size_t i;

    if (text == NULL || prefix_length > length) {
        return false;
    }
    for (i = 0U; i <= length - prefix_length; i++) {
        if (memcmp(&text[i], prefix, prefix_length) == 0) {
            found = &text[i];
            break;
        }
    }
    return found != NULL;
}

static bool FindLineValue(const char *response, size_t length, const char *prefix,
                          const char **value, const char **line_end)
{
    size_t position = 0U;
    size_t prefix_length;

    if (response == NULL || prefix == NULL || value == NULL || line_end == NULL) {
        return false;
    }
    prefix_length = strlen(prefix);
    while (position < length) {
        size_t start = position;
        size_t end;

        while (position < length && response[position] != '\r' && response[position] != '\n') {
            position++;
        }
        end = position;
        if (end - start >= prefix_length &&
            memcmp(&response[start], prefix, prefix_length) == 0) {
            *value = &response[start + prefix_length];
            *line_end = &response[end];
            return true;
        }
        while (position < length && (response[position] == '\r' || response[position] == '\n')) {
            position++;
        }
    }
    return false;
}

static const char *SkipSpaces(const char *position, const char *end)
{
    while (position < end && (*position == ' ' || *position == '\t')) {
        position++;
    }
    return position;
}

static bool ParseIntegerField(const char **position, const char *end,
                              bool allow_sign, int *value)
{
    const char *cursor;
    int sign = 1;
    int parsed = 0;
    bool has_digit = false;

    if (position == NULL || *position == NULL || value == NULL) {
        return false;
    }
    cursor = SkipSpaces(*position, end);
    if (cursor < end && (*cursor == '+' || *cursor == '-')) {
        if (!allow_sign) {
            return false;
        }
        if (*cursor == '-') {
            sign = -1;
        }
        cursor++;
    }
    while (cursor < end && *cursor >= '0' && *cursor <= '9') {
        int digit = (int)(*cursor - '0');
        if (parsed > 100000) {
            return false;
        }
        parsed = parsed * 10 + digit;
        has_digit = true;
        cursor++;
    }
    if (!has_digit) {
        return false;
    }
    cursor = SkipSpaces(cursor, end);
    if (cursor < end && *cursor != ',') {
        return false;
    }
    *position = cursor;
    *value = sign * parsed;
    return true;
}

static bool ParseBssid(const char **position, const char *end)
{
    const char *cursor = SkipSpaces(*position, end);
    const char *mac;
    size_t i;
    bool quoted = false;

    if (cursor < end && *cursor == '"') {
        quoted = true;
        cursor++;
    }
    mac = cursor;
    if ((size_t)(end - cursor) < 17U) {
        return false;
    }
    for (i = 0U; i < 17U; i++) {
        char ch = mac[i];
        if (i == 2U || i == 5U || i == 8U || i == 11U || i == 14U) {
            if (ch != ':') {
                return false;
            }
        } else if (!((ch >= '0' && ch <= '9') ||
                     (ch >= 'a' && ch <= 'f') ||
                     (ch >= 'A' && ch <= 'F'))) {
            return false;
        }
    }
    cursor += 17U;
    if (quoted) {
        if (cursor >= end || *cursor != '"') {
            return false;
        }
        cursor++;
    }
    cursor = SkipSpaces(cursor, end);
    if (cursor >= end || *cursor != ',') {
        return false;
    }
    *position = cursor + 1U;
    return true;
}

static bool ParseSuccessfulStationRecord(const char *response, size_t length,
                                         const char *prefix, char *bssid_output,
                                         size_t bssid_capacity,
                                         unsigned int *channel_output,
                                         int *rssi_output)
{
    const char *position;
    const char *line_end;
    const char *ssid_start;
    const char *bssid_start;
    int channel;
    int rssi;
    size_t index;

    if ((bssid_output != NULL && bssid_capacity < WIFI_AT_BSSID_TEXT_CAPACITY) ||
        ((channel_output == NULL) != (rssi_output == NULL))) {
        return false;
    }

    if (!FindLineValue(response, length, prefix, &position, &line_end) ||
        !HasLine(response, length, "OK") ||
        HasLine(response, length, "ERROR") || HasLine(response, length, "FAIL")) {
        return false;
    }

    position = SkipSpaces(position, line_end);
    if (position >= line_end) {
        return false;
    }
    if (*position == '"') {
        bool closed = false;
        position++;
        ssid_start = position;
        while (position < line_end) {
            if (*position == '\\' && position + 1U < line_end) {
                position += 2U;
            } else if (*position == '"') {
                closed = true;
                break;
            } else {
                position++;
            }
        }
        if (!closed || position == ssid_start) {
            return false;
        }
        position = SkipSpaces(position + 1U, line_end);
        if (position >= line_end || *position != ',') {
            return false;
        }
        position++;
    } else {
        ssid_start = position;
        while (position < line_end && *position != ',') {
            position++;
        }
        if (position == line_end || position == ssid_start) {
            return false;
        }
        position++;
    }

    bssid_start = SkipSpaces(position, line_end);
    if (bssid_start < line_end && *bssid_start == '"') {
        bssid_start++;
    }
    if (!ParseBssid(&position, line_end) ||
        !ParseIntegerField(&position, line_end, false, &channel) ||
        position >= line_end || *position != ',') {
        return false;
    }
    position++;
    if (!ParseIntegerField(&position, line_end, true, &rssi) ||
        channel < 1 || channel > 14 || rssi < -128 || rssi > 0) {
        return false;
    }

    while (position < line_end) {
        int ignored_field;
        if (*position != ',') {
            return false;
        }
        position++;
        if (!ParseIntegerField(&position, line_end, true, &ignored_field)) {
            return false;
        }
    }

    if (bssid_output != NULL) {
        for (index = 0U; index < 17U; index++) {
            char ch = bssid_start[index];
            bssid_output[index] = (ch >= 'a' && ch <= 'f')
                                      ? (char)(ch - 'a' + 'A') : ch;
        }
        bssid_output[17] = '\0';
    }
    if (channel_output != NULL) {
        *channel_output = (unsigned int)channel;
        *rssi_output = rssi;
    }
    return true;
}

static bool HasSuccessfulStationRecord(const char *response, size_t length,
                                       const char *prefix)
{
    return ParseSuccessfulStationRecord(response, length, prefix, NULL, 0U, NULL, NULL);
}

static bool IsSafeField(const char *field, size_t maximum_length, bool allow_empty)
{
    size_t i;
    size_t length;

    if (field == NULL) {
        return false;
    }
    length = strlen(field);
    if ((!allow_empty && length == 0U) || length > maximum_length) {
        return false;
    }
    for (i = 0U; i < length; i++) {
        unsigned char ch = (unsigned char)field[i];
        if (ch < 0x20U || ch > 0x7eU || ch == '"' || ch == '\\' || ch == ',') {
            return false;
        }
    }
    return true;
}

static bool IsSafeHost(const char *host)
{
    size_t length;
    size_t i;

    if (host == NULL) {
        return false;
    }
    length = strlen(host);
    if (length == 0U || length > 63U || host[0] == '.' || host[0] == '-' ||
        host[length - 1U] == '.' || host[length - 1U] == '-') {
        return false;
    }
    for (i = 0U; i < length; i++) {
        unsigned char ch = (unsigned char)host[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '-')) {
            return false;
        }
    }
    return true;
}

WiFiAtDialect WiFiAT_DetectDialect(const char *response, size_t length)
{
    if (HasPrefix(response, length, "+CWMODE_CUR:") ||
        HasPrefix(response, length, "+CWJAP_CUR:")) {
        return WIFI_AT_DIALECT_NONOS;
    }
    if (HasPrefix(response, length, "+CWMODE:") ||
        HasPrefix(response, length, "+CWSTATE:") ||
        HasPrefix(response, length, "+CWJAP:")) {
        return WIFI_AT_DIALECT_ESP_AT;
    }
    return WIFI_AT_DIALECT_UNKNOWN;
}

WiFiAtDialect WiFiAT_DetectDialectFromVersion(const char *response, size_t length)
{
    const char *value;
    const char *line_end;
    const char *cursor;
    unsigned int major = 0U;
    unsigned int minor = 0U;

    if (!FindLineValue(response, length, "AT version:", &value, &line_end)) {
        return WIFI_AT_DIALECT_UNKNOWN;
    }
    cursor = SkipSpaces(value, line_end);
    if (cursor >= line_end || *cursor < '0' || *cursor > '9') {
        return WIFI_AT_DIALECT_UNKNOWN;
    }
    while (cursor < line_end && *cursor >= '0' && *cursor <= '9') {
        if (major > 9U) {
            return WIFI_AT_DIALECT_UNKNOWN;
        }
        major = major * 10U + (unsigned int)(*cursor - '0');
        cursor++;
    }
    if (cursor >= line_end || *cursor++ != '.') {
        return WIFI_AT_DIALECT_UNKNOWN;
    }
    if (cursor >= line_end || *cursor < '0' || *cursor > '9') {
        return WIFI_AT_DIALECT_UNKNOWN;
    }
    while (cursor < line_end && *cursor >= '0' && *cursor <= '9') {
        if (minor > 9U) {
            return WIFI_AT_DIALECT_UNKNOWN;
        }
        minor = minor * 10U + (unsigned int)(*cursor - '0');
        cursor++;
    }
    if (cursor >= line_end || *cursor != '.') {
        return WIFI_AT_DIALECT_UNKNOWN;
    }
    if (major == 1U && minor == 7U) {
        return WIFI_AT_DIALECT_NONOS;
    }
    if (major == 2U) {
        return WIFI_AT_DIALECT_ESP_AT;
    }
    return WIFI_AT_DIALECT_UNKNOWN;
}

bool WiFiAT_BuildStationModeCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CWMODE=1,0\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CWMODE_CUR=1\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildApStaModeCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CWMODE=3,0\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CWMODE_CUR=3\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildStationQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CWSTATE?\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CWJAP_CUR?\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildConnectedStationQueryCommand(WiFiAtDialect dialect, char *output,
                                             size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U || dialect != WIFI_AT_DIALECT_ESP_AT) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CWJAP?\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildModeQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CWMODE?\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CWMODE_CUR?\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildIpQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CIPSTA?\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CIPSTA_CUR?\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildDnsQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CIPDNS?\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CIPDNS_CUR?\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildRamStoreCommand(char *output, size_t capacity)
{
    int length;
    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+SYSSTORE=0\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildPersistentStoreCommand(char *output, size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+SYSSTORE=1\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildPersistentApStaModeCommand(WiFiAtDialect dialect, char *output,
                                             size_t capacity)
{
    const char *command;
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        command = "AT+CWMODE=3,0\r\n";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        command = "AT+CWMODE_DEF=3\r\n";
    } else {
        return false;
    }
    length = snprintf(output, capacity, "%s", command);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildAutoConnectCommand(WiFiAtDialect dialect, char *output,
                                    size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS)) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CWAUTOCONN=1\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildNormalModeCommand(char *output, size_t capacity)
{
    int length;
    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CIPMODE=0\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildSingleConnectionCommand(char *output, size_t capacity)
{
    int length;
    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CIPMUX=0\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildTcpMuxQueryCommand(char *output, size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CIPMUX?\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildTcpModeQueryCommand(char *output, size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U) {
        return false;
    }
    length = snprintf(output, capacity, "AT+CIPMODE?\r\n");
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildTcpConnectCommand(WiFiAtDialect dialect,
                                   WiFiAtTcpMuxMode mux_mode, const char *host,
                                   unsigned int port, char *output, size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS) ||
        (mux_mode != WIFI_AT_TCP_MUX_SINGLE && mux_mode != WIFI_AT_TCP_MUX_MULTI) ||
        !IsSafeHost(host) || port == 0U || port > 65535U) {
        return false;
    }
    if (mux_mode == WIFI_AT_TCP_MUX_SINGLE) {
        length = snprintf(output, capacity, "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                          host, port);
    } else {
        length = snprintf(output, capacity, "AT+CIPSTART=0,\"TCP\",\"%s\",%u\r\n",
                          host, port);
    }
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildPingCommand(WiFiAtDialect dialect, const char *host,
                             char *output, size_t capacity)
{
    int length;

    if (output == NULL || capacity == 0U ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS) ||
        !IsSafeHost(host)) {
        return false;
    }
    length = snprintf(output, capacity, "AT+PING=\"%s\"\r\n", host);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildTcpSendCommand(WiFiAtTcpMuxMode mux_mode, size_t length,
                                char *output, size_t capacity)
{
    int written;

    if (output == NULL || capacity == 0U || length == 0U || length > 2048U ||
        (mux_mode != WIFI_AT_TCP_MUX_SINGLE && mux_mode != WIFI_AT_TCP_MUX_MULTI)) {
        return false;
    }
    if (mux_mode == WIFI_AT_TCP_MUX_SINGLE) {
        written = snprintf(output, capacity, "AT+CIPSEND=%lu\r\n",
                           (unsigned long)length);
    } else {
        written = snprintf(output, capacity, "AT+CIPSEND=0,%lu\r\n",
                           (unsigned long)length);
    }
    return written > 0 && (size_t)written < capacity;
}

bool WiFiAT_BuildTcpCloseCommand(WiFiAtTcpMuxMode mux_mode, char *output,
                                 size_t capacity)
{
    int length;
    if (output == NULL || capacity == 0U ||
        (mux_mode != WIFI_AT_TCP_MUX_SINGLE && mux_mode != WIFI_AT_TCP_MUX_MULTI)) {
        return false;
    }
    length = (mux_mode == WIFI_AT_TCP_MUX_SINGLE)
                 ? snprintf(output, capacity, "AT+CIPCLOSE\r\n")
                 : snprintf(output, capacity, "AT+CIPCLOSE=0\r\n");
    return length > 0 && (size_t)length < capacity;
}

static bool BuildJoinCommand(WiFiAtDialect dialect, const char *ssid,
                             const char *password, bool persistent,
                             char *output, size_t capacity)
{
    const char *format;
    int length;

    if (output == NULL || capacity == 0U ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS) ||
        !IsSafeField(ssid, 32U, false) || !IsSafeField(password, 64U, false) ||
        strlen(password) < 8U) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_NONOS) {
        format = persistent ? "AT+CWJAP_DEF=\"%s\",\"%s\"\r\n"
                            : "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n";
    } else {
        format = "AT+CWJAP=\"%s\",\"%s\"\r\n";
    }
    length = snprintf(output, capacity, format, ssid, password);
    return length > 0 && (size_t)length < capacity;
}

bool WiFiAT_BuildJoinCommand(WiFiAtDialect dialect, const char *ssid,
                             const char *password, char *output, size_t capacity)
{
    return BuildJoinCommand(dialect, ssid, password, false, output, capacity);
}

bool WiFiAT_BuildPersistentJoinCommand(WiFiAtDialect dialect, const char *ssid,
                                       const char *password, char *output,
                                       size_t capacity)
{
    return BuildJoinCommand(dialect, ssid, password, true, output, capacity);
}

bool WiFiAT_ParseMode(WiFiAtDialect dialect, const char *response, size_t length,
                      unsigned int *mode)
{
    const char *prefix;
    size_t prefix_length;
    size_t i;

    if (response == NULL || mode == NULL) {
        return false;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        prefix = "+CWMODE:";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        prefix = "+CWMODE_CUR:";
    } else {
        return false;
    }
    prefix_length = strlen(prefix);
    for (i = 0U; i + prefix_length < length; i++) {
        if (memcmp(&response[i], prefix, prefix_length) == 0) {
            size_t value = i + prefix_length;
            while (value < length && (response[value] == ' ' || response[value] == '\t')) {
                value++;
            }
            if (value >= length || response[value] < '0' || response[value] > '3') {
                return false;
            }
            *mode = (unsigned int)(response[value] - '0');
            return true;
        }
    }
    return false;
}

bool WiFiAT_ParseStationStatus(WiFiAtDialect dialect, const char *response, size_t length,
                               WiFiAtStationStatus *status)
{
    if (status == NULL || response == NULL ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS)) {
        return false;
    }
    *status = WIFI_AT_STATION_UNKNOWN;
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        size_t i;
        for (i = 0U; i + 10U <= length; i++) {
            if (memcmp(&response[i], "+CWSTATE:", 9U) == 0) {
                size_t value = i + 9U;
                if (value >= length || response[value] < '0' || response[value] > '4') {
                    return false;
                }
                /* State 1 means association is established but IPv4 is not
                 * ready yet; IP readiness is queried separately. */
                if (response[value] == '1' || response[value] == '2') {
                    *status = WIFI_AT_STATION_CONNECTED;
                    return true;
                }
                if (response[value] == '0' || response[value] == '4') {
                    *status = WIFI_AT_STATION_DISCONNECTED;
                    return true;
                }
                return false;
            }
        }
        if (HasSuccessfulStationRecord(response, length, "+CWJAP:")) {
            *status = WIFI_AT_STATION_CONNECTED;
            return true;
        }
        if (HasLine(response, length, "No AP") &&
            HasFinalLine(response, length, "OK")) {
            *status = WIFI_AT_STATION_DISCONNECTED;
            return true;
        }
        return false;
    }

    /* A legacy query can return "+CWJAP_CUR:<error>\r\nFAIL".  The
     * command prefix alone is not proof of an active association.  Require
     * the quoted SSID record and a successful terminal response. */
    if (HasSuccessfulStationRecord(response, length, "+CWJAP_CUR:")) {
        *status = WIFI_AT_STATION_CONNECTED;
        return true;
    }
    if (HasLine(response, length, "No AP")) {
        *status = WIFI_AT_STATION_DISCONNECTED;
        return true;
    }
    return false;
}

bool WiFiAT_ParseStationLinkInfo(WiFiAtDialect dialect, const char *response, size_t length,
                                 char *bssid, size_t bssid_capacity,
                                 unsigned int *channel, int *rssi)
{
    const char *prefix;

    if (bssid == NULL || bssid_capacity == 0U || channel == NULL || rssi == NULL) {
        return false;
    }
    bssid[0] = '\0';
    *channel = 0U;
    *rssi = 0;
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        prefix = "+CWJAP:";
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        prefix = "+CWJAP_CUR:";
    } else {
        return false;
    }
    return ParseSuccessfulStationRecord(response, length, prefix, bssid,
                                        bssid_capacity, channel, rssi);
}

uint8_t WiFiAT_ClassifyStationResponse(WiFiAtDialect dialect,
                                       const char *response, size_t length)
{
    uint8_t flags = 0U;

    if (response == NULL || length == 0U) {
        return flags;
    }
    if ((dialect == WIFI_AT_DIALECT_ESP_AT &&
         (HasLine(response, length, "AT+CWSTATE?") ||
          HasLine(response, length, "AT+CWJAP?"))) ||
        (dialect == WIFI_AT_DIALECT_NONOS && HasLine(response, length, "AT+CWJAP_CUR?"))) {
        flags |= WIFI_AT_DIAG_QUERY_ECHO;
    }
    if (Contains(response, length, "+CWJAP_CUR:")) {
        flags |= WIFI_AT_DIAG_LEGACY_PREFIX;
    }
    if (Contains(response, length, "+CWJAP_CUR:\"")) {
        flags |= WIFI_AT_DIAG_LEGACY_QUOTED_RECORD;
    }
    if (Contains(response, length, "+CWSTATE:") ||
        Contains(response, length, "+CWJAP:")) {
        flags |= WIFI_AT_DIAG_MODERN_PREFIX;
    }
    if (HasFinalLine(response, length, "OK")) {
        flags |= WIFI_AT_DIAG_TERMINAL_OK;
    }
    if (HasFinalLine(response, length, "ERROR")) {
        flags |= WIFI_AT_DIAG_TERMINAL_ERROR;
    }
    if (HasFinalLine(response, length, "FAIL")) {
        flags |= WIFI_AT_DIAG_TERMINAL_FAIL;
    }
    if (HasLine(response, length, "No AP")) {
        flags |= WIFI_AT_DIAG_NO_AP_LINE;
    }
    return flags;
}

uint8_t WiFiAT_ResponseLengthBucket(size_t length)
{
    if (length == 0U) {
        return 0U;
    }
    if (length <= 15U) {
        return 1U;
    }
    if (length <= 31U) {
        return 2U;
    }
    if (length <= 63U) {
        return 3U;
    }
    if (length <= 127U) {
        return 4U;
    }
    if (length <= 255U) {
        return 5U;
    }
    return 6U;
}

bool WiFiAT_TcpStatusIndicatesStationUnavailable(const char *response, size_t length)
{
    if (!HasFinalLine(response, length, "OK")) {
        return false;
    }
    return HasLine(response, length, "STATUS:0") ||
           HasLine(response, length, "STATUS:1") ||
           HasLine(response, length, "STATUS:5");
}

bool WiFiAT_ParseTcpStatus(const char *response, size_t length, WiFiAtTcpStatus *status)
{
    size_t i;
    bool active = false;

    if (response == NULL || status == NULL) {
        return false;
    }
    *status = WIFI_AT_TCP_UNKNOWN;
    for (i = 0U; i + 11U <= length; i++) {
        if (memcmp(&response[i], "+CIPSTATUS:", 11U) == 0) {
            active = true;
            break;
        }
    }
    if (active) {
        *status = WIFI_AT_TCP_CONNECTED;
        return true;
    }

    /* STATUS is a complete response line. Requiring the line boundary avoids
     * mistaking the link ID after +CIPSTATUS: for the device status code. */
    for (i = 0U; i + 7U < length; i++) {
        size_t value;
        if ((i == 0U || response[i - 1U] == '\n') &&
            memcmp(&response[i], "STATUS:", 7U) == 0) {
            value = i + 7U;
            if (response[value] >= '0' && response[value] <= '5' &&
                (value + 1U == length || response[value + 1U] == '\r' ||
                 response[value + 1U] == '\n') && Contains(response, length, "OK")) {
                *status = response[value] == '3' ? WIFI_AT_TCP_CONNECTED
                                                 : WIFI_AT_TCP_DISCONNECTED;
                return true;
            }
        }
    }
    return false;
}

bool WiFiAT_ParseTcpMuxMode(const char *response, size_t length,
                            WiFiAtTcpMuxMode *mode)
{
    static const char prefix[] = "+CIPMUX:";
    size_t position = 0U;
    bool found = false;
    WiFiAtTcpMuxMode parsed = WIFI_AT_TCP_MUX_UNKNOWN;

    if (mode == NULL) {
        return false;
    }
    *mode = WIFI_AT_TCP_MUX_UNKNOWN;
    if (response == NULL || !HasFinalLine(response, length, "OK")) {
        return false;
    }

    while (position < length) {
        size_t start = position;
        size_t line_length;

        while (position < length && response[position] != '\r' &&
               response[position] != '\n') {
            position++;
        }
        line_length = position - start;
        if (line_length >= sizeof(prefix) &&
            memcmp(&response[start], prefix, sizeof(prefix) - 1U) == 0) {
            WiFiAtTcpMuxMode line_mode;

            if (line_length != sizeof(prefix) ||
                (response[start + sizeof(prefix) - 1U] != '0' &&
                 response[start + sizeof(prefix) - 1U] != '1')) {
                return false;
            }
            line_mode = response[start + sizeof(prefix) - 1U] == '0'
                            ? WIFI_AT_TCP_MUX_SINGLE
                            : WIFI_AT_TCP_MUX_MULTI;
            if (found && line_mode != parsed) {
                return false;
            }
            parsed = line_mode;
            found = true;
        }
        while (position < length &&
               (response[position] == '\r' || response[position] == '\n')) {
            position++;
        }
    }

    if (!found) {
        return false;
    }
    *mode = parsed;
    return true;
}

bool WiFiAT_ParseTcpMode(const char *response, size_t length,
                         WiFiAtTcpMode *mode)
{
    static const char prefix[] = "+CIPMODE:";
    size_t position = 0U;
    bool found = false;
    WiFiAtTcpMode parsed = WIFI_AT_TCP_MODE_UNKNOWN;

    if (mode == NULL) {
        return false;
    }
    *mode = WIFI_AT_TCP_MODE_UNKNOWN;
    if (response == NULL || !HasFinalLine(response, length, "OK")) {
        return false;
    }

    while (position < length) {
        size_t start = position;
        size_t line_length;

        while (position < length && response[position] != '\r' &&
               response[position] != '\n') {
            position++;
        }
        line_length = position - start;
        if (line_length >= sizeof(prefix) &&
            memcmp(&response[start], prefix, sizeof(prefix) - 1U) == 0) {
            WiFiAtTcpMode line_mode;

            if (line_length != sizeof(prefix) ||
                (response[start + sizeof(prefix) - 1U] != '0' &&
                 response[start + sizeof(prefix) - 1U] != '1')) {
                return false;
            }
            line_mode = response[start + sizeof(prefix) - 1U] == '0'
                            ? WIFI_AT_TCP_MODE_NORMAL
                            : WIFI_AT_TCP_MODE_TRANSPARENT;
            if (found && line_mode != parsed) {
                return false;
            }
            parsed = line_mode;
            found = true;
        }
        while (position < length &&
               (response[position] == '\r' || response[position] == '\n')) {
            position++;
        }
    }

    if (!found) {
        return false;
    }
    *mode = parsed;
    return true;
}

static WiFiAtIpStatus ParseIpv4Status(const char *text, size_t length,
                                      char *address, size_t address_capacity)
{
    size_t index = 0U;
    unsigned int part;
    unsigned int octets[4] = {0U, 0U, 0U, 0U};
    bool any_nonzero = false;

    if (address != NULL) {
        if (address_capacity == 0U) {
            return WIFI_AT_IP_UNKNOWN;
        }
        address[0] = '\0';
    }
    if (text == NULL) {
        return WIFI_AT_IP_UNKNOWN;
    }
    for (part = 0U; part < 4U; part++) {
        bool have_digit = false;
        while (index < length && text[index] >= '0' && text[index] <= '9') {
            unsigned int digit = (unsigned int)(text[index] - '0');
            have_digit = true;
            if (octets[part] > (255U - digit) / 10U) {
                return WIFI_AT_IP_UNKNOWN;
            }
            octets[part] = octets[part] * 10U + digit;
            index++;
        }
        if (!have_digit) {
            return WIFI_AT_IP_UNKNOWN;
        }
        any_nonzero = any_nonzero || octets[part] != 0U;
        if (part < 3U) {
            if (index >= length || text[index] != '.') {
                return WIFI_AT_IP_UNKNOWN;
            }
            index++;
        }
    }

    if (index < length && text[index] == '"') {
        index++;
    }
    while (index < length && (text[index] == ' ' || text[index] == '\t')) {
        index++;
    }
    if (index < length && text[index] != '\r' && text[index] != '\n') {
        return WIFI_AT_IP_UNKNOWN;
    }
    if (!any_nonzero) {
        return WIFI_AT_IP_NOT_READY;
    }
    if (address != NULL) {
        int written = snprintf(address, address_capacity, "%u.%u.%u.%u",
                               octets[0], octets[1], octets[2], octets[3]);
        if (written <= 0 || (size_t)written >= address_capacity) {
            address[0] = '\0';
            return WIFI_AT_IP_UNKNOWN;
        }
    }
    return WIFI_AT_IP_READY;
}

bool WiFiAT_BuildDnsGatewayCommand(WiFiAtDialect dialect, const char *gateway,
                                  char *output, size_t capacity)
{
    char normalized[WIFI_AT_IPV4_TEXT_CAPACITY];
    const char *format;
    int length;

    if (gateway == NULL || output == NULL || capacity == 0U ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS) ||
        ParseIpv4Status(gateway, strlen(gateway), normalized, sizeof(normalized)) !=
            WIFI_AT_IP_READY ||
        strcmp(normalized, gateway) != 0) {
        return false;
    }
    format = dialect == WIFI_AT_DIALECT_NONOS
                 ? "AT+CIPDNS_CUR=1,\"%s\"\r\n"
                 : "AT+CIPDNS=1,\"%s\"\r\n";
    length = snprintf(output, capacity, format, gateway);
    return length > 0 && (size_t)length < capacity;
}

static bool ParseDnsAddressToken(const char *line, size_t line_length,
                                 size_t *position, char *output, size_t capacity)
{
    size_t start;
    size_t end;
    bool quoted;

    if (line == NULL || position == NULL || output == NULL || capacity == 0U) {
        return false;
    }
    output[0] = '\0';
    while (*position < line_length &&
           (line[*position] == ' ' || line[*position] == '\t')) {
        (*position)++;
    }
    if (*position >= line_length) {
        return false;
    }
    quoted = line[*position] == '"';
    if (quoted) {
        (*position)++;
    }
    start = *position;
    if (quoted) {
        while (*position < line_length && line[*position] != '"') {
            (*position)++;
        }
        if (*position >= line_length) {
            return false;
        }
        end = *position;
        (*position)++;
    } else {
        while (*position < line_length && line[*position] != ',' &&
               line[*position] != ' ' && line[*position] != '\t') {
            (*position)++;
        }
        end = *position;
    }
    if (start == end ||
        ParseIpv4Status(&line[start], end - start, output, capacity) != WIFI_AT_IP_READY) {
        output[0] = '\0';
        return false;
    }
    while (*position < line_length &&
           (line[*position] == ' ' || line[*position] == '\t')) {
        (*position)++;
    }
    return *position == line_length || line[*position] == ',';
}

static bool ParseModernDnsRecord(const char *line, size_t line_length,
                                 WiFiAtDnsInfo *info)
{
    static const char prefix[] = "+CIPDNS:";
    size_t position = sizeof(prefix) - 1U;

    if (line_length < position + 1U || info == NULL ||
        memcmp(line, prefix, sizeof(prefix) - 1U) != 0 ||
        (line[position] != '0' && line[position] != '1')) {
        return false;
    }
    info->mode = line[position] == '0' ? WIFI_AT_DNS_MODE_AUTO : WIFI_AT_DNS_MODE_MANUAL;
    position++;
    while (position < line_length &&
           (line[position] == ' ' || line[position] == '\t')) {
        position++;
    }
    if (position == line_length) {
        return true;
    }
    if (line[position] != ',') {
        return false;
    }

    while (position < line_length) {
        if (line[position] != ',' || info->server_count >= WIFI_AT_DNS_SERVER_COUNT) {
            return false;
        }
        position++;
        if (!ParseDnsAddressToken(line, line_length, &position,
                                  info->servers[info->server_count],
                                  sizeof(info->servers[info->server_count]))) {
            return false;
        }
        info->server_count++;
        if (position == line_length) {
            break;
        }
    }
    return true;
}

static bool ParseLegacyDnsRecord(const char *line, size_t line_length,
                                 WiFiAtDnsInfo *info)
{
    static const char prefix[] = "+CIPDNS_CUR:";
    size_t position = sizeof(prefix) - 1U;

    if (line_length < position || info == NULL ||
        memcmp(line, prefix, sizeof(prefix) - 1U) != 0 ||
        info->server_count >= 2U) {
        return false;
    }
    if (!ParseDnsAddressToken(line, line_length, &position,
                              info->servers[info->server_count],
                              sizeof(info->servers[info->server_count])) ||
        position != line_length) {
        return false;
    }
    info->server_count++;
    return true;
}

bool WiFiAT_ParseDnsInfo(WiFiAtDialect dialect, const char *response, size_t length,
                         WiFiAtDnsInfo *info)
{
    WiFiAtDnsInfo parsed;
    size_t position = 0U;
    bool modern_record_found = false;

    if (info == NULL) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->mode = WIFI_AT_DNS_MODE_UNKNOWN;
    if (response == NULL ||
        (dialect != WIFI_AT_DIALECT_ESP_AT && dialect != WIFI_AT_DIALECT_NONOS) ||
        !HasFinalLine(response, length, "OK") ||
        HasLine(response, length, "ERROR") || HasLine(response, length, "FAIL")) {
        return false;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.mode = WIFI_AT_DNS_MODE_UNKNOWN;

    while (position < length) {
        size_t line_start = position;
        size_t line_end;
        size_t record_start;
        size_t record_length;

        while (position < length && response[position] != '\r' &&
               response[position] != '\n') {
            position++;
        }
        line_end = position;
        while (position < length &&
               (response[position] == '\r' || response[position] == '\n')) {
            position++;
        }
        record_start = line_start;
        while (record_start < line_end &&
               (response[record_start] == ' ' || response[record_start] == '\t')) {
            record_start++;
        }
        while (line_end > record_start &&
               (response[line_end - 1U] == ' ' || response[line_end - 1U] == '\t')) {
            line_end--;
        }
        record_length = line_end - record_start;
        if (dialect == WIFI_AT_DIALECT_ESP_AT &&
            record_length >= sizeof("+CIPDNS:") - 1U &&
            memcmp(&response[record_start], "+CIPDNS:", sizeof("+CIPDNS:") - 1U) == 0) {
            if (modern_record_found ||
                !ParseModernDnsRecord(&response[record_start], record_length, &parsed)) {
                return false;
            }
            modern_record_found = true;
        } else if (dialect == WIFI_AT_DIALECT_NONOS &&
                   record_length >= sizeof("+CIPDNS_CUR:") - 1U &&
                   memcmp(&response[record_start], "+CIPDNS_CUR:",
                          sizeof("+CIPDNS_CUR:") - 1U) == 0) {
            if (!ParseLegacyDnsRecord(&response[record_start], record_length, &parsed)) {
                return false;
            }
        }
    }

    if ((dialect == WIFI_AT_DIALECT_ESP_AT && !modern_record_found) ||
        (dialect == WIFI_AT_DIALECT_NONOS && parsed.server_count == 0U)) {
        return false;
    }
    *info = parsed;
    return true;
}

static WiFiAtIpStatus ParseStationIpStatus(WiFiAtDialect dialect,
                                           const char *response, size_t length,
                                           char *address, size_t address_capacity)
{
    const char *prefix;
    size_t prefix_length;
    size_t index;
    bool legacy;

    if (address != NULL) {
        if (address_capacity == 0U) {
            return WIFI_AT_IP_UNKNOWN;
        }
        address[0] = '\0';
    }
    if (response == NULL) {
        return WIFI_AT_IP_UNKNOWN;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        prefix = "+CIPSTA:ip:";
        legacy = false;
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        prefix = "+CIPSTA_CUR:";
        legacy = true;
    } else {
        return WIFI_AT_IP_UNKNOWN;
    }
    prefix_length = strlen(prefix);
    for (index = 0U; index + prefix_length < length; index++) {
        size_t value;
        if (legacy && index > 0U && response[index - 1U] != '\n') {
            continue;
        }
        if (memcmp(&response[index], prefix, prefix_length) != 0) {
            continue;
        }
        value = index + prefix_length;
        if (legacy) {
            while (value < length && (response[value] == ' ' || response[value] == '\t')) {
                value++;
            }
            if (value + 3U <= length && memcmp(&response[value], "ip:", 3U) == 0) {
                value += 3U;
            } else if ((value + 8U <= length &&
                        memcmp(&response[value], "gateway:", 8U) == 0) ||
                       (value + 8U <= length &&
                        memcmp(&response[value], "netmask:", 8U) == 0)) {
                while (index < length && response[index] != '\n') {
                    index++;
                }
                continue;
            } else if (value < length && response[value] != '"' &&
                       (response[value] < '0' || response[value] > '9')) {
                return WIFI_AT_IP_UNKNOWN;
            }
        }
        while (value < length && (response[value] == ' ' || response[value] == '\t')) {
            value++;
        }
        if (value < length && response[value] == '"') {
            value++;
        }
        return ParseIpv4Status(&response[value], length - value,
                               address, address_capacity);
    }
    return WIFI_AT_IP_UNKNOWN;
}

static WiFiAtIpStatus ParseStationIpField(WiFiAtDialect dialect,
                                          const char *response, size_t length,
                                          const char *field, char *output,
                                          size_t output_capacity)
{
    const char *prefix;
    size_t prefix_length;
    size_t field_length;
    size_t index;
    bool legacy;

    if (output != NULL) {
        if (output_capacity == 0U) {
            return WIFI_AT_IP_UNKNOWN;
        }
        output[0] = '\0';
    }
    if (response == NULL || field == NULL ||
        (output != NULL && output_capacity == 0U)) {
        return WIFI_AT_IP_UNKNOWN;
    }
    if (dialect == WIFI_AT_DIALECT_ESP_AT) {
        prefix = "+CIPSTA:";
        legacy = false;
    } else if (dialect == WIFI_AT_DIALECT_NONOS) {
        prefix = "+CIPSTA_CUR:";
        legacy = true;
    } else {
        return WIFI_AT_IP_UNKNOWN;
    }

    prefix_length = strlen(prefix);
    field_length = strlen(field);
    for (index = 0U; index + prefix_length + field_length <= length; index++) {
        size_t value;
        size_t line_end;

        if (legacy && index > 0U && response[index - 1U] != '\n') {
            continue;
        }
        if (memcmp(&response[index], prefix, prefix_length) != 0) {
            continue;
        }
        value = index + prefix_length;
        while (value < length && (response[value] == ' ' || response[value] == '\t')) {
            value++;
        }
        if (value + field_length > length ||
            memcmp(&response[value], field, field_length) != 0) {
            while (value < length && response[value] != '\n') {
                value++;
            }
            index = value;
            continue;
        }

        value += field_length;
        while (value < length && (response[value] == ' ' || response[value] == '\t')) {
            value++;
        }
        if (value < length && response[value] == '"') {
            value++;
        }
        line_end = value;
        while (line_end < length && response[line_end] != '\n') {
            line_end++;
        }
        return ParseIpv4Status(&response[value], line_end - value,
                               output, output_capacity);
    }

    if (legacy) {
        size_t requested_record;
        size_t record = 0U;

        if (strcmp(field, "gateway:") == 0) {
            requested_record = 1U;
        } else if (strcmp(field, "netmask:") == 0) {
            requested_record = 2U;
        } else {
            return WIFI_AT_IP_UNKNOWN;
        }

        for (index = 0U; index + prefix_length <= length; index++) {
            size_t value;
            size_t line_end;
            WiFiAtIpStatus status;

            if (index > 0U && response[index - 1U] != '\n') {
                continue;
            }
            if (memcmp(&response[index], prefix, prefix_length) != 0) {
                continue;
            }

            value = index + prefix_length;
            line_end = value;
            while (line_end < length && response[line_end] != '\n') {
                line_end++;
            }
            while (value < line_end &&
                   (response[value] == ' ' || response[value] == '\t')) {
                value++;
            }
            if ((value + 3U <= line_end &&
                 memcmp(&response[value], "ip:", 3U) == 0) ||
                (value + 8U <= line_end &&
                 (memcmp(&response[value], "gateway:", 8U) == 0 ||
                  memcmp(&response[value], "netmask:", 8U) == 0))) {
                index = line_end;
                continue;
            }
            if (value < line_end && response[value] == '"') {
                value++;
            }

            status = ParseIpv4Status(&response[value], line_end - value,
                                     output, output_capacity);
            if (status == WIFI_AT_IP_READY || status == WIFI_AT_IP_NOT_READY) {
                if (record == requested_record) {
                    return status;
                }
                record++;
            }
            index = line_end;
        }
    }
    return WIFI_AT_IP_UNKNOWN;
}

WiFiAtIpStatus WiFiAT_ParseStationIpStatus(WiFiAtDialect dialect,
                                           const char *response, size_t length)
{
    return ParseStationIpStatus(dialect, response, length, NULL, 0U);
}

bool WiFiAT_ParseStationIpAddress(WiFiAtDialect dialect, const char *response,
                                  size_t length, char *output, size_t capacity)
{
    if (output == NULL || capacity == 0U) {
        return false;
    }
    return ParseStationIpStatus(dialect, response, length, output, capacity) ==
           WIFI_AT_IP_READY;
}

WiFiAtIpStatus WiFiAT_ParseStationIpConfig(
    WiFiAtDialect dialect, const char *response, size_t length,
    char *ip, size_t ip_capacity,
    char *gateway, size_t gateway_capacity,
    char *netmask, size_t netmask_capacity)
{
    WiFiAtIpStatus ip_status;

    if (ip == NULL || ip_capacity == 0U ||
        gateway == NULL || gateway_capacity == 0U ||
        netmask == NULL || netmask_capacity == 0U) {
        return WIFI_AT_IP_UNKNOWN;
    }
    ip[0] = '\0';
    gateway[0] = '\0';
    netmask[0] = '\0';

    ip_status = ParseStationIpStatus(dialect, response, length, ip, ip_capacity);
    if (ip_status != WIFI_AT_IP_READY) {
        return ip_status;
    }
    if (ParseStationIpField(dialect, response, length, "gateway:",
                            gateway, gateway_capacity) != WIFI_AT_IP_READY) {
        gateway[0] = '\0';
    }
    if (ParseStationIpField(dialect, response, length, "netmask:",
                            netmask, netmask_capacity) != WIFI_AT_IP_READY) {
        netmask[0] = '\0';
    }
    return WIFI_AT_IP_READY;
}

bool WiFiAT_ParseStationIp(WiFiAtDialect dialect, const char *response, size_t length)
{
    return WiFiAT_ParseStationIpStatus(dialect, response, length) == WIFI_AT_IP_READY;
}
