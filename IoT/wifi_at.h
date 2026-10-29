#ifndef WIFI_AT_H
#define WIFI_AT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WIFI_AT_DIALECT_UNKNOWN = 0,
    WIFI_AT_DIALECT_ESP_AT,
    WIFI_AT_DIALECT_NONOS
} WiFiAtDialect;

typedef enum {
    WIFI_AT_STATION_UNKNOWN = 0,
    WIFI_AT_STATION_DISCONNECTED,
    WIFI_AT_STATION_CONNECTED
} WiFiAtStationStatus;

typedef enum {
    WIFI_AT_TCP_UNKNOWN = 0,
    WIFI_AT_TCP_DISCONNECTED,
    WIFI_AT_TCP_CONNECTED
} WiFiAtTcpStatus;

typedef enum {
    WIFI_AT_TCP_MUX_UNKNOWN = 0,
    WIFI_AT_TCP_MUX_SINGLE,
    WIFI_AT_TCP_MUX_MULTI
} WiFiAtTcpMuxMode;

typedef enum {
    WIFI_AT_TCP_MODE_UNKNOWN = 0,
    WIFI_AT_TCP_MODE_NORMAL,
    WIFI_AT_TCP_MODE_TRANSPARENT
} WiFiAtTcpMode;

typedef enum {
    WIFI_AT_IP_UNKNOWN = 0,
    WIFI_AT_IP_NOT_READY,
    WIFI_AT_IP_READY
} WiFiAtIpStatus;

#define WIFI_AT_IPV4_TEXT_CAPACITY 16U
#define WIFI_AT_BSSID_TEXT_CAPACITY 18U
#define WIFI_AT_DNS_SERVER_COUNT 3U

typedef enum {
    WIFI_AT_DNS_MODE_UNKNOWN = 0,
    WIFI_AT_DNS_MODE_AUTO,
    WIFI_AT_DNS_MODE_MANUAL
} WiFiAtDnsMode;

typedef struct {
    WiFiAtDnsMode mode;
    uint8_t server_count;
    char servers[WIFI_AT_DNS_SERVER_COUNT][WIFI_AT_IPV4_TEXT_CAPACITY];
} WiFiAtDnsInfo;

typedef enum {
    WIFI_AT_DIAG_QUERY_ECHO = 1U << 0,
    WIFI_AT_DIAG_LEGACY_PREFIX = 1U << 1,
    WIFI_AT_DIAG_LEGACY_QUOTED_RECORD = 1U << 2,
    WIFI_AT_DIAG_MODERN_PREFIX = 1U << 3,
    WIFI_AT_DIAG_TERMINAL_OK = 1U << 4,
    WIFI_AT_DIAG_TERMINAL_ERROR = 1U << 5,
    WIFI_AT_DIAG_TERMINAL_FAIL = 1U << 6,
    WIFI_AT_DIAG_NO_AP_LINE = 1U << 7
} WiFiAtStationDiagnosticFlag;

WiFiAtDialect WiFiAT_DetectDialect(const char *response, size_t length);
WiFiAtDialect WiFiAT_DetectDialectFromVersion(const char *response, size_t length);
bool WiFiAT_BuildStationModeCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildApStaModeCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildStationQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildConnectedStationQueryCommand(WiFiAtDialect dialect, char *output,
                                             size_t capacity);
bool WiFiAT_BuildModeQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildIpQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildDnsQueryCommand(WiFiAtDialect dialect, char *output, size_t capacity);
bool WiFiAT_BuildDnsGatewayCommand(WiFiAtDialect dialect, const char *gateway,
                                  char *output, size_t capacity);
bool WiFiAT_BuildRamStoreCommand(char *output, size_t capacity);
bool WiFiAT_BuildPersistentStoreCommand(char *output, size_t capacity);
bool WiFiAT_BuildPersistentApStaModeCommand(WiFiAtDialect dialect, char *output,
                                             size_t capacity);
bool WiFiAT_BuildAutoConnectCommand(WiFiAtDialect dialect, char *output,
                                    size_t capacity);
bool WiFiAT_BuildNormalModeCommand(char *output, size_t capacity);
bool WiFiAT_BuildSingleConnectionCommand(char *output, size_t capacity);
bool WiFiAT_BuildTcpMuxQueryCommand(char *output, size_t capacity);
bool WiFiAT_BuildTcpModeQueryCommand(char *output, size_t capacity);
bool WiFiAT_BuildTcpConnectCommand(WiFiAtDialect dialect,
                                   WiFiAtTcpMuxMode mux_mode, const char *host,
                                   unsigned int port, char *output, size_t capacity);
bool WiFiAT_BuildPingCommand(WiFiAtDialect dialect, const char *host,
                             char *output, size_t capacity);
bool WiFiAT_BuildTcpSendCommand(WiFiAtTcpMuxMode mux_mode, size_t length,
                                char *output, size_t capacity);
bool WiFiAT_BuildTcpCloseCommand(WiFiAtTcpMuxMode mux_mode, char *output,
                                 size_t capacity);
bool WiFiAT_BuildJoinCommand(WiFiAtDialect dialect, const char *ssid, const char *password,
                             char *output, size_t capacity);
bool WiFiAT_BuildPersistentJoinCommand(WiFiAtDialect dialect, const char *ssid,
                                       const char *password, char *output,
                                       size_t capacity);
bool WiFiAT_ParseMode(WiFiAtDialect dialect, const char *response, size_t length,
                      unsigned int *mode);
bool WiFiAT_ParseStationStatus(WiFiAtDialect dialect, const char *response, size_t length,
                               WiFiAtStationStatus *status);
bool WiFiAT_ParseStationLinkInfo(WiFiAtDialect dialect, const char *response, size_t length,
                                 char *bssid, size_t bssid_capacity,
                                 unsigned int *channel, int *rssi);
uint8_t WiFiAT_ClassifyStationResponse(WiFiAtDialect dialect,
                                       const char *response, size_t length);
uint8_t WiFiAT_ResponseLengthBucket(size_t length);
bool WiFiAT_ParseTcpStatus(const char *response, size_t length, WiFiAtTcpStatus *status);
bool WiFiAT_TcpStatusIndicatesStationUnavailable(const char *response, size_t length);
bool WiFiAT_ParseTcpMuxMode(const char *response, size_t length,
                            WiFiAtTcpMuxMode *mode);
bool WiFiAT_ParseTcpMode(const char *response, size_t length,
                         WiFiAtTcpMode *mode);
bool WiFiAT_ParseDnsInfo(WiFiAtDialect dialect, const char *response, size_t length,
                         WiFiAtDnsInfo *info);
WiFiAtIpStatus WiFiAT_ParseStationIpStatus(WiFiAtDialect dialect,
                                           const char *response, size_t length);
bool WiFiAT_ParseStationIpAddress(WiFiAtDialect dialect, const char *response,
                                  size_t length, char *output, size_t capacity);
WiFiAtIpStatus WiFiAT_ParseStationIpConfig(
    WiFiAtDialect dialect, const char *response, size_t length,
    char *ip, size_t ip_capacity,
    char *gateway, size_t gateway_capacity,
    char *netmask, size_t netmask_capacity);
bool WiFiAT_ParseStationIp(WiFiAtDialect dialect, const char *response, size_t length);
#endif
