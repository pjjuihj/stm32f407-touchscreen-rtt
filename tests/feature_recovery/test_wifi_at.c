#include "wifi_at.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    char command[96];
    char station_ip[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_gateway[WIFI_AT_IPV4_TEXT_CAPACITY];
    char station_netmask[WIFI_AT_IPV4_TEXT_CAPACITY];
    static const char ip_config_modern[] =
        "+CIPSTA:ip:\"192.168.137.59\"\r\n"
        "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
        "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n";
    static const char ip_config_legacy[] =
        "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\n"
        "+CIPSTA_CUR:gateway:\"10.0.0.1\"\r\n"
        "+CIPSTA_CUR:netmask:\"255.255.255.0\"\r\nOK\r\n";
    static const char ip_config_without_gateway[] =
        "+CIPSTA:ip:\"192.168.137.59\"\r\nOK\r\n";
    WiFiAtDialect dialect;
    WiFiAtStationStatus station;
    WiFiAtTcpStatus tcp;
    WiFiAtTcpMuxMode tcp_mux_mode;
    WiFiAtTcpMode tcp_mode;
    WiFiAtDnsInfo dns_info;
    unsigned int mode;
    unsigned int channel;
    int rssi;
    char bssid[WIFI_AT_BSSID_TEXT_CAPACITY];
    const char modern[] = "+CWMODE:3\r\nOK\r\n";
    const char legacy[] = "+CWMODE_CUR:3\r\nOK\r\n";
    const char nonos_gmr[] =
        "AT version:1.7.4.0(Jul  8 2020 15:53:04)\r\n"
        "SDK version:3.0.5-dev(52383f9)\r\nOK\r\n";
    const char esp_at_gmr[] =
        "AT version:2.2.0.0\r\nSDK version:4.4.7\r\nOK\r\n";
    const char unsupported_gmr[] = "AT version:1.8.0.0\r\nOK\r\n";
    const char modern_connected[] = "+CWSTATE:2,\"private-network\"\r\nOK\r\n";
    const char modern_associated_without_ip[] = "+CWSTATE:1,\"private-network\"\r\nOK\r\n";
    const char modern_connecting[] = "+CWSTATE:3,\"private-network\"\r\nOK\r\n";
    const char modern_disconnected[] = "+CWSTATE:0\r\nOK\r\n";
    const char modern_no_ap[] = "AT+CWSTATE?\r\nNo AP\r\nOK\r\n";
    const char modern_no_ap_error_after_ok[] =
        "AT+CWSTATE?\r\nNo AP\r\nOK\r\nERROR\r\n";
    const char modern_cwjap_connected[] =
        "+CWJAP:\"private-network\",\"00:11:22:33:44:55\",11,-50,1,1,3,0,0\r\nOK\r\n";
    const char modern_cwjap_unquoted[] =
        "+CWJAP:desk-hotspot,00:11:22:33:44:55,11,-50,1,1,3,0\r\nOK\r\n";
    const char modern_cwjap_bad_bssid[] =
        "+CWJAP:desk-hotspot,not-a-mac,11,-50\r\nOK\r\n";
    const char modern_cwjap_failed[] =
        "+CWJAP:desk-hotspot,00:11:22:33:44:55,11,-50\r\nERROR\r\n";
    const char modern_cwjap_rejected[] = "AT+CWJAP?\r\nERROR\r\n";
    const char legacy_connected[] = "+CWJAP_CUR:\"private-network\",\"00:11:22:33:44:55\",6,-42\r\nOK\r\n";
    const char legacy_connected_unquoted[] =
        "+CWJAP_CUR:desk-hotspot,00:11:22:33:44:55,6,-42,0\r\nOK\r\n";
    const char legacy_disconnected[] = "No AP\r\nERROR\r\n";
    const char legacy_query_failed[] = "+CWJAP_CUR:3\r\nFAIL\r\n";
    const char legacy_query_diagnostic[] = "AT+CWJAP_CUR?\r\n+CWJAP_CUR:3\r\nFAIL\r\n";
    const char legacy_connected_diagnostic[] =
        "+CWJAP_CUR:\"private-network\",\"00:11:22:33:44:55\",6,-42\r\nOK\r\n";
    const char tcp_connected[] = "+CIPSTATUS:0,\"TCP\",\"192.0.2.1\",80,1234,1\r\nOK\r\n";
    const char tcp_station_connected[] = "STATUS:2\r\nOK\r\n";
    const char tcp_socket_connected[] = "STATUS:3\r\nOK\r\n";
    const char tcp_all_disconnected[] = "STATUS:4\r\nOK\r\n";
    const char tcp_station_inactive[] = "STATUS:0\r\nOK\r\n";
    const char tcp_station_idle[] = "STATUS:1\r\nOK\r\n";
    const char tcp_station_not_connected[] = "STATUS:5\r\nOK\r\n";
    const char modern_dns_auto[] =
        "+CIPDNS:0,\"192.168.137.1\",\"1.1.1.1\"\r\nOK\r\n";
    const char modern_dns_manual[] =
        "+CIPDNS:1,\"8.8.8.8\"\r\nOK\r\n";
    const char modern_dns_auto_without_servers[] = "+CIPDNS:0\r\nOK\r\n";
    const char modern_dns_malformed[] = "+CIPDNS:1,\"999.1.1.1\"\r\nOK\r\n";
    const char legacy_dns[] =
        "+CIPDNS_CUR:\"192.168.137.1\"\r\n"
        "+CIPDNS_CUR:1.1.1.1\r\nOK\r\n";
    const char ip_modern[] = "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n";
    const char ip_zero[] = "+CIPSTA:ip:\"0.0.0.0\"\r\nOK\r\n";
    const char ip_legacy[] = "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\nOK\r\n";
    const char ip_legacy_documented[] =
        "+CIPSTA_CUR:192.168.6.100\r\n"
        "+CIPSTA_CUR:192.168.6.1\r\n"
        "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n";
    const char ip_malformed[] = "+CIPSTA:ip:\"not-an-address\"\r\nOK\r\n";
    const char ip_overflow[] = "+CIPSTA:ip:\"192.168.999999999999999999999.1\"\r\nOK\r\n";

    dialect = WiFiAT_DetectDialect(modern, strlen(modern));
    assert(dialect == WIFI_AT_DIALECT_ESP_AT);
    dialect = WiFiAT_DetectDialect(legacy, strlen(legacy));
    assert(dialect == WIFI_AT_DIALECT_NONOS);
    assert(WiFiAT_DetectDialect("OK\r\n", 4) == WIFI_AT_DIALECT_UNKNOWN);
    assert(WiFiAT_DetectDialectFromVersion(nonos_gmr, strlen(nonos_gmr)) ==
           WIFI_AT_DIALECT_NONOS);
    assert(WiFiAT_DetectDialectFromVersion(esp_at_gmr, strlen(esp_at_gmr)) ==
           WIFI_AT_DIALECT_ESP_AT);
    assert(WiFiAT_DetectDialectFromVersion(unsupported_gmr, strlen(unsupported_gmr)) ==
           WIFI_AT_DIALECT_UNKNOWN);

    assert(WiFiAT_BuildStationModeCommand(WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE=1,0\r\n") == 0);
    assert(WiFiAT_BuildStationModeCommand(WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE_CUR=1\r\n") == 0);
    assert(!WiFiAT_BuildStationModeCommand(WIFI_AT_DIALECT_UNKNOWN, command, sizeof(command)));
    assert(WiFiAT_BuildModeQueryCommand(WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE?\r\n") == 0);
    assert(WiFiAT_BuildModeQueryCommand(WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE_CUR?\r\n") == 0);
    assert(WiFiAT_BuildRamStoreCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+SYSSTORE=0\r\n") == 0);
    assert(WiFiAT_BuildPersistentStoreCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+SYSSTORE=1\r\n") == 0);
    assert(WiFiAT_BuildPersistentApStaModeCommand(
        WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE=3,0\r\n") == 0);
    assert(WiFiAT_BuildPersistentApStaModeCommand(
        WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE_DEF=3\r\n") == 0);
    assert(!WiFiAT_BuildPersistentApStaModeCommand(
        WIFI_AT_DIALECT_UNKNOWN, command, sizeof(command)));
    assert(WiFiAT_BuildAutoConnectCommand(
        WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWAUTOCONN=1\r\n") == 0);
    assert(WiFiAT_BuildAutoConnectCommand(
        WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CWAUTOCONN=1\r\n") == 0);
    assert(!WiFiAT_BuildAutoConnectCommand(
        WIFI_AT_DIALECT_UNKNOWN, command, sizeof(command)));
    assert(WiFiAT_BuildApStaModeCommand(WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE=3,0\r\n") == 0);
    assert(WiFiAT_BuildApStaModeCommand(WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CWMODE_CUR=3\r\n") == 0);
    assert(!WiFiAT_BuildApStaModeCommand(WIFI_AT_DIALECT_UNKNOWN, command, sizeof(command)));
    assert(WiFiAT_BuildIpQueryCommand(WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSTA?\r\n") == 0);
    assert(WiFiAT_BuildIpQueryCommand(WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSTA_CUR?\r\n") == 0);
    assert(WiFiAT_BuildDnsQueryCommand(WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CIPDNS?\r\n") == 0);
    assert(WiFiAT_BuildDnsQueryCommand(WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(strcmp(command, "AT+CIPDNS_CUR?\r\n") == 0);
    assert(!WiFiAT_BuildDnsQueryCommand(WIFI_AT_DIALECT_UNKNOWN, command, sizeof(command)));
    assert(!WiFiAT_BuildDnsQueryCommand(WIFI_AT_DIALECT_ESP_AT, command, 4U));
    assert(WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_NONOS, "192.168.137.1",
                                         command, sizeof(command)));
    assert(strcmp(command, "AT+CIPDNS_CUR=1,\"192.168.137.1\"\r\n") == 0);
    assert(WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_ESP_AT, "192.168.137.1",
                                         command, sizeof(command)));
    assert(strcmp(command, "AT+CIPDNS=1,\"192.168.137.1\"\r\n") == 0);
    assert(!WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_NONOS, "256.168.137.1",
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_NONOS, "192.168.137.1\r\nAT+RST",
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_UNKNOWN, "192.168.137.1",
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildDnsGatewayCommand(WIFI_AT_DIALECT_NONOS, "192.168.137.1",
                                          command, 8U));
    assert(WiFiAT_BuildConnectedStationQueryCommand(
        WIFI_AT_DIALECT_ESP_AT, command, sizeof(command)));
    assert(strcmp(command, "AT+CWJAP?\r\n") == 0);
    assert(!WiFiAT_BuildConnectedStationQueryCommand(
        WIFI_AT_DIALECT_NONOS, command, sizeof(command)));
    assert(WiFiAT_BuildNormalModeCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+CIPMODE=0\r\n") == 0);
    assert(WiFiAT_BuildSingleConnectionCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+CIPMUX=0\r\n") == 0);
    assert(WiFiAT_BuildTcpMuxQueryCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+CIPMUX?\r\n") == 0);
    assert(!WiFiAT_BuildTcpMuxQueryCommand(command, 4U));
    assert(WiFiAT_BuildTcpModeQueryCommand(command, sizeof(command)));
    assert(strcmp(command, "AT+CIPMODE?\r\n") == 0);
    assert(!WiFiAT_BuildTcpModeQueryCommand(command, 4U));
    assert(WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_ESP_AT,
                                         WIFI_AT_TCP_MUX_SINGLE, "192.0.2.4", 443U,
                                         command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSTART=\"TCP\",\"192.0.2.4\",443\r\n") == 0);
    assert(WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_NONOS,
                                         WIFI_AT_TCP_MUX_MULTI, "example.com", 443U,
                                         command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSTART=0,\"TCP\",\"example.com\",443\r\n") == 0);
    assert(!WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_ESP_AT,
                                          WIFI_AT_TCP_MUX_UNKNOWN, "192.0.2.4", 443U,
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_ESP_AT,
                                          WIFI_AT_TCP_MUX_SINGLE, "bad host", 443U,
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_ESP_AT,
                                          WIFI_AT_TCP_MUX_SINGLE, "host\";AT+RST", 443U,
                                          command, sizeof(command)));
    assert(!WiFiAT_BuildTcpConnectCommand(WIFI_AT_DIALECT_ESP_AT,
                                          WIFI_AT_TCP_MUX_SINGLE, "host", 0U,
                                          command, sizeof(command)));
    assert(WiFiAT_BuildPingCommand(WIFI_AT_DIALECT_NONOS, "192.168.137.1",
                                   command, sizeof(command)));
    assert(strcmp(command, "AT+PING=\"192.168.137.1\"\r\n") == 0);
    assert(!WiFiAT_BuildPingCommand(WIFI_AT_DIALECT_NONOS, "host\";AT+RST",
                                    command, sizeof(command)));
    assert(!WiFiAT_BuildPingCommand(WIFI_AT_DIALECT_UNKNOWN, "192.168.137.1",
                                    command, sizeof(command)));
    assert(WiFiAT_BuildTcpSendCommand(WIFI_AT_TCP_MUX_SINGLE, 5U,
                                      command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSEND=5\r\n") == 0);
    assert(WiFiAT_BuildTcpSendCommand(WIFI_AT_TCP_MUX_MULTI, 5U,
                                      command, sizeof(command)));
    assert(strcmp(command, "AT+CIPSEND=0,5\r\n") == 0);
    assert(!WiFiAT_BuildTcpSendCommand(WIFI_AT_TCP_MUX_UNKNOWN, 5U,
                                       command, sizeof(command)));
    assert(!WiFiAT_BuildTcpSendCommand(WIFI_AT_TCP_MUX_SINGLE, 0U,
                                       command, sizeof(command)));
    assert(WiFiAT_BuildTcpCloseCommand(WIFI_AT_TCP_MUX_SINGLE,
                                       command, sizeof(command)));
    assert(strcmp(command, "AT+CIPCLOSE\r\n") == 0);
    assert(WiFiAT_BuildTcpCloseCommand(WIFI_AT_TCP_MUX_MULTI,
                                       command, sizeof(command)));
    assert(strcmp(command, "AT+CIPCLOSE=0\r\n") == 0);
    assert(!WiFiAT_BuildTcpCloseCommand(WIFI_AT_TCP_MUX_UNKNOWN,
                                        command, sizeof(command)));
    assert(WiFiAT_ParseMode(WIFI_AT_DIALECT_ESP_AT, "+CWMODE:3\r\nOK\r\n", 15U, &mode));
    assert(mode == 3U);
    assert(WiFiAT_ParseMode(WIFI_AT_DIALECT_NONOS, "+CWMODE_CUR:2\r\n", 16U, &mode));
    assert(mode == 2U);
    assert(!WiFiAT_ParseMode(WIFI_AT_DIALECT_ESP_AT, "ERROR", 5U, &mode));

    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_connected,
                                     strlen(modern_connected), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_associated_without_ip,
                                     strlen(modern_associated_without_ip), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_connecting,
                                      strlen(modern_connecting), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_disconnected,
                                     strlen(modern_disconnected), &station));
    assert(station == WIFI_AT_STATION_DISCONNECTED);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_no_ap,
                                     strlen(modern_no_ap), &station));
    assert(station == WIFI_AT_STATION_DISCONNECTED);
    {
        uint8_t diagnostic = WiFiAT_ClassifyStationResponse(
            WIFI_AT_DIALECT_ESP_AT, modern_no_ap, strlen(modern_no_ap));
        assert(diagnostic == (WIFI_AT_DIAG_QUERY_ECHO |
                              WIFI_AT_DIAG_TERMINAL_OK |
                              WIFI_AT_DIAG_NO_AP_LINE));
    }
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT,
                                      modern_no_ap_error_after_ok,
                                      strlen(modern_no_ap_error_after_ok), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    {
        uint8_t diagnostic = WiFiAT_ClassifyStationResponse(
            WIFI_AT_DIALECT_ESP_AT, modern_no_ap_error_after_ok,
            strlen(modern_no_ap_error_after_ok));
        assert(diagnostic == (WIFI_AT_DIAG_QUERY_ECHO |
                              WIFI_AT_DIAG_TERMINAL_ERROR |
                              WIFI_AT_DIAG_NO_AP_LINE));
    }
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_cwjap_connected,
                                     strlen(modern_cwjap_connected), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_cwjap_unquoted,
                                     strlen(modern_cwjap_unquoted), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_cwjap_bad_bssid,
                                      strlen(modern_cwjap_bad_bssid), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, modern_cwjap_failed,
                                      strlen(modern_cwjap_failed), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT,
                                      modern_cwjap_rejected,
                                      strlen(modern_cwjap_rejected), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    {
        const char modern_cwjap_echo[] =
            "AT+CWJAP?\r\n+CWJAP:\"private-network\",\"00:11:22:33:44:55\",11,-50\r\nOK\r\n";
        uint8_t diagnostic = WiFiAT_ClassifyStationResponse(
            WIFI_AT_DIALECT_ESP_AT, modern_cwjap_echo, strlen(modern_cwjap_echo));
        assert((diagnostic & WIFI_AT_DIAG_QUERY_ECHO) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_MODERN_PREFIX) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_TERMINAL_OK) != 0U);
    }
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_NONOS, legacy_connected,
                                     strlen(legacy_connected), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(WiFiAT_ParseStationLinkInfo(WIFI_AT_DIALECT_NONOS, legacy_connected,
                                       strlen(legacy_connected), bssid, sizeof(bssid),
                                       &channel, &rssi));
    assert(strcmp(bssid, "00:11:22:33:44:55") == 0);
    assert(channel == 6U && rssi == -42);
    assert(WiFiAT_ParseStationLinkInfo(WIFI_AT_DIALECT_ESP_AT, modern_cwjap_connected,
                                       strlen(modern_cwjap_connected), bssid,
                                       sizeof(bssid), &channel, &rssi));
    assert(strcmp(bssid, "00:11:22:33:44:55") == 0);
    assert(channel == 11U && rssi == -50);
    assert(!WiFiAT_ParseStationLinkInfo(WIFI_AT_DIALECT_NONOS, legacy_query_failed,
                                        strlen(legacy_query_failed), bssid,
                                        sizeof(bssid), &channel, &rssi));
    assert(bssid[0] == '\0' && channel == 0U && rssi == 0);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_NONOS, legacy_connected_unquoted,
                                     strlen(legacy_connected_unquoted), &station));
    assert(station == WIFI_AT_STATION_CONNECTED);
    assert(WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_NONOS, legacy_disconnected,
                                     strlen(legacy_disconnected), &station));
    assert(station == WIFI_AT_STATION_DISCONNECTED);
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_NONOS, legacy_query_failed,
                                      strlen(legacy_query_failed), &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);
    {
        uint8_t diagnostic = WiFiAT_ClassifyStationResponse(
            WIFI_AT_DIALECT_NONOS, legacy_query_diagnostic,
            strlen(legacy_query_diagnostic));
        assert((diagnostic & WIFI_AT_DIAG_QUERY_ECHO) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_LEGACY_PREFIX) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_TERMINAL_FAIL) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_LEGACY_QUOTED_RECORD) == 0U);
        assert(WiFiAT_ResponseLengthBucket(strlen(legacy_query_failed)) == 2U);
    }
    {
        uint8_t diagnostic = WiFiAT_ClassifyStationResponse(
            WIFI_AT_DIALECT_NONOS, legacy_connected_diagnostic,
            strlen(legacy_connected_diagnostic));
        assert((diagnostic & WIFI_AT_DIAG_LEGACY_PREFIX) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_LEGACY_QUOTED_RECORD) != 0U);
        assert((diagnostic & WIFI_AT_DIAG_TERMINAL_OK) != 0U);
        assert((diagnostic & (WIFI_AT_DIAG_TERMINAL_ERROR |
                              WIFI_AT_DIAG_TERMINAL_FAIL)) == 0U);
    }
    assert(!WiFiAT_ParseStationStatus(WIFI_AT_DIALECT_ESP_AT, "ERROR\r\n", 7, &station));
    assert(station == WIFI_AT_STATION_UNKNOWN);

    assert(WiFiAT_ParseTcpStatus(tcp_connected, strlen(tcp_connected), &tcp));
    assert(tcp == WIFI_AT_TCP_CONNECTED);
    assert(WiFiAT_ParseTcpStatus(tcp_station_connected, strlen(tcp_station_connected), &tcp));
    assert(tcp == WIFI_AT_TCP_DISCONNECTED);
    assert(WiFiAT_ParseTcpStatus(tcp_socket_connected, strlen(tcp_socket_connected), &tcp));
    assert(tcp == WIFI_AT_TCP_CONNECTED);
    assert(WiFiAT_ParseTcpStatus(tcp_all_disconnected, strlen(tcp_all_disconnected), &tcp));
    assert(tcp == WIFI_AT_TCP_DISCONNECTED);
    assert(WiFiAT_TcpStatusIndicatesStationUnavailable(tcp_station_inactive,
                                                       strlen(tcp_station_inactive)));
    assert(WiFiAT_TcpStatusIndicatesStationUnavailable(tcp_station_idle,
                                                       strlen(tcp_station_idle)));
    assert(WiFiAT_TcpStatusIndicatesStationUnavailable(tcp_station_not_connected,
                                                       strlen(tcp_station_not_connected)));
    assert(!WiFiAT_TcpStatusIndicatesStationUnavailable(tcp_station_connected,
                                                        strlen(tcp_station_connected)));
    assert(!WiFiAT_TcpStatusIndicatesStationUnavailable(tcp_all_disconnected,
                                                        strlen(tcp_all_disconnected)));
    assert(!WiFiAT_TcpStatusIndicatesStationUnavailable("STATUS:5\r\nERROR\r\n",
                                                        strlen("STATUS:5\r\nERROR\r\n")));
    assert(!WiFiAT_ParseTcpStatus("ERROR\r\n", 7, &tcp));
    assert(tcp == WIFI_AT_TCP_UNKNOWN);
    assert(!WiFiAT_ParseTcpStatus("OK\r\n", 4U, &tcp));
    assert(tcp == WIFI_AT_TCP_UNKNOWN);

    assert(WiFiAT_ParseTcpMuxMode("+CIPMUX:0\r\nOK\r\n",
                                  strlen("+CIPMUX:0\r\nOK\r\n"), &tcp_mux_mode));
    assert(tcp_mux_mode == WIFI_AT_TCP_MUX_SINGLE);
    assert(WiFiAT_ParseTcpMuxMode("+CIPMUX:1\r\nOK\r\n",
                                  strlen("+CIPMUX:1\r\nOK\r\n"), &tcp_mux_mode));
    assert(tcp_mux_mode == WIFI_AT_TCP_MUX_MULTI);
    assert(!WiFiAT_ParseTcpMuxMode("+CIPMUX:2\r\nOK\r\n",
                                   strlen("+CIPMUX:2\r\nOK\r\n"), &tcp_mux_mode));
    assert(tcp_mux_mode == WIFI_AT_TCP_MUX_UNKNOWN);
    assert(!WiFiAT_ParseTcpMuxMode("+CIPMUX:0\r\nERROR\r\n",
                                   strlen("+CIPMUX:0\r\nERROR\r\n"), &tcp_mux_mode));
    assert(!WiFiAT_ParseTcpMuxMode("prefix +CIPMUX:0\r\nOK\r\n",
                                   strlen("prefix +CIPMUX:0\r\nOK\r\n"),
                                   &tcp_mux_mode));
    assert(!WiFiAT_ParseTcpMuxMode("+CIPMUX:0\r\n+CIPMUX:1\r\nOK\r\n",
                                   strlen("+CIPMUX:0\r\n+CIPMUX:1\r\nOK\r\n"),
                                   &tcp_mux_mode));
    assert(WiFiAT_ParseTcpMode("+CIPMODE:0\r\nOK\r\n",
                               strlen("+CIPMODE:0\r\nOK\r\n"), &tcp_mode));
    assert(tcp_mode == WIFI_AT_TCP_MODE_NORMAL);
    assert(WiFiAT_ParseTcpMode("+CIPMODE:1\r\nOK\r\n",
                               strlen("+CIPMODE:1\r\nOK\r\n"), &tcp_mode));
    assert(tcp_mode == WIFI_AT_TCP_MODE_TRANSPARENT);
    assert(!WiFiAT_ParseTcpMode("+CIPMODE:2\r\nOK\r\n",
                                strlen("+CIPMODE:2\r\nOK\r\n"), &tcp_mode));
    assert(tcp_mode == WIFI_AT_TCP_MODE_UNKNOWN);
    assert(!WiFiAT_ParseTcpMode("+CIPMODE:0\r\nERROR\r\n",
                                strlen("+CIPMODE:0\r\nERROR\r\n"), &tcp_mode));
    assert(tcp_mode == WIFI_AT_TCP_MODE_UNKNOWN);
    assert(!WiFiAT_ParseTcpMode("prefix +CIPMODE:0\r\nOK\r\n",
                                strlen("prefix +CIPMODE:0\r\nOK\r\n"), &tcp_mode));
    assert(tcp_mode == WIFI_AT_TCP_MODE_UNKNOWN);

    assert(WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_ESP_AT, modern_dns_auto,
                               strlen(modern_dns_auto), &dns_info));
    assert(dns_info.mode == WIFI_AT_DNS_MODE_AUTO);
    assert(dns_info.server_count == 2U);
    assert(strcmp(dns_info.servers[0], "192.168.137.1") == 0);
    assert(strcmp(dns_info.servers[1], "1.1.1.1") == 0);
    assert(WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_ESP_AT, modern_dns_manual,
                               strlen(modern_dns_manual), &dns_info));
    assert(dns_info.mode == WIFI_AT_DNS_MODE_MANUAL);
    assert(dns_info.server_count == 1U);
    assert(strcmp(dns_info.servers[0], "8.8.8.8") == 0);
    assert(WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_ESP_AT,
                               modern_dns_auto_without_servers,
                               strlen(modern_dns_auto_without_servers), &dns_info));
    assert(dns_info.mode == WIFI_AT_DNS_MODE_AUTO);
    assert(dns_info.server_count == 0U);
    assert(WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_NONOS, legacy_dns,
                               strlen(legacy_dns), &dns_info));
    assert(dns_info.mode == WIFI_AT_DNS_MODE_UNKNOWN);
    assert(dns_info.server_count == 2U);
    assert(strcmp(dns_info.servers[0], "192.168.137.1") == 0);
    assert(strcmp(dns_info.servers[1], "1.1.1.1") == 0);
    assert(!WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_ESP_AT, modern_dns_malformed,
                                strlen(modern_dns_malformed), &dns_info));
    assert(dns_info.mode == WIFI_AT_DNS_MODE_UNKNOWN);
    assert(dns_info.server_count == 0U);
    assert(!WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_ESP_AT, legacy_dns,
                                strlen(legacy_dns), &dns_info));
    assert(!WiFiAT_ParseDnsInfo(WIFI_AT_DIALECT_NONOS, "ERROR\r\n",
                                strlen("ERROR\r\n"), &dns_info));

    assert(WiFiAT_ParseStationIp(WIFI_AT_DIALECT_ESP_AT, ip_modern, strlen(ip_modern)));
    assert(!WiFiAT_ParseStationIp(WIFI_AT_DIALECT_ESP_AT, ip_zero, strlen(ip_zero)));
    assert(WiFiAT_ParseStationIp(WIFI_AT_DIALECT_NONOS, ip_legacy, strlen(ip_legacy)));
    assert(!WiFiAT_ParseStationIp(WIFI_AT_DIALECT_NONOS, "ERROR\r\n", 7U));
    assert(WiFiAT_ParseStationIpStatus(WIFI_AT_DIALECT_ESP_AT, ip_modern,
                                       strlen(ip_modern)) == WIFI_AT_IP_READY);
    assert(WiFiAT_ParseStationIpStatus(WIFI_AT_DIALECT_ESP_AT, ip_zero,
                                       strlen(ip_zero)) == WIFI_AT_IP_NOT_READY);
    assert(WiFiAT_ParseStationIpStatus(WIFI_AT_DIALECT_ESP_AT, ip_malformed,
                                       strlen(ip_malformed)) == WIFI_AT_IP_UNKNOWN);
    assert(WiFiAT_ParseStationIpStatus(WIFI_AT_DIALECT_ESP_AT, ip_overflow,
                                       strlen(ip_overflow)) == WIFI_AT_IP_UNKNOWN);
    assert(WiFiAT_ParseStationIpStatus(WIFI_AT_DIALECT_NONOS, ip_legacy_documented,
                                       strlen(ip_legacy_documented)) == WIFI_AT_IP_READY);
    assert(WiFiAT_ParseStationIpConfig(
               WIFI_AT_DIALECT_NONOS, ip_legacy_documented,
               strlen(ip_legacy_documented), station_ip, sizeof(station_ip),
               station_gateway, sizeof(station_gateway),
               station_netmask, sizeof(station_netmask)) == WIFI_AT_IP_READY);
    assert(strcmp(station_ip, "192.168.6.100") == 0);
    assert(strcmp(station_gateway, "192.168.6.1") == 0);
    assert(strcmp(station_netmask, "255.255.255.0") == 0);
    assert(WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_ESP_AT, ip_modern,
                                        strlen(ip_modern), station_ip,
                                        sizeof(station_ip)));
    assert(strcmp(station_ip, "192.168.137.222") == 0);
    assert(WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_NONOS, ip_legacy_documented,
                                        strlen(ip_legacy_documented), station_ip,
                                        sizeof(station_ip)));
    assert(strcmp(station_ip, "192.168.6.100") == 0);
    assert(!WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_ESP_AT, ip_zero,
                                         strlen(ip_zero), station_ip,
                                         sizeof(station_ip)));
    assert(station_ip[0] == '\0');
    assert(!WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_ESP_AT, ip_malformed,
                                         strlen(ip_malformed), station_ip,
                                         sizeof(station_ip)));
    assert(station_ip[0] == '\0');
    assert(!WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_ESP_AT, ip_overflow,
                                         strlen(ip_overflow), station_ip,
                                         sizeof(station_ip)));
    assert(station_ip[0] == '\0');
    assert(!WiFiAT_ParseStationIpAddress(WIFI_AT_DIALECT_ESP_AT, ip_modern,
                                         strlen(ip_modern), station_ip, 8U));
    assert(station_ip[0] == '\0');
    assert(WiFiAT_ParseStationIpConfig(
               WIFI_AT_DIALECT_ESP_AT, ip_config_modern,
               strlen(ip_config_modern), station_ip, sizeof(station_ip),
               station_gateway, sizeof(station_gateway),
               station_netmask, sizeof(station_netmask)) == WIFI_AT_IP_READY);
    assert(strcmp(station_ip, "192.168.137.59") == 0);
    assert(strcmp(station_gateway, "192.168.137.1") == 0);
    assert(strcmp(station_netmask, "255.255.255.0") == 0);
    assert(WiFiAT_ParseStationIpConfig(
               WIFI_AT_DIALECT_NONOS, ip_config_legacy,
               strlen(ip_config_legacy), station_ip, sizeof(station_ip),
               station_gateway, sizeof(station_gateway),
               station_netmask, sizeof(station_netmask)) == WIFI_AT_IP_READY);
    assert(strcmp(station_ip, "10.0.0.8") == 0);
    assert(strcmp(station_gateway, "10.0.0.1") == 0);
    assert(strcmp(station_netmask, "255.255.255.0") == 0);
    assert(WiFiAT_ParseStationIpConfig(
               WIFI_AT_DIALECT_ESP_AT, ip_config_without_gateway,
               strlen(ip_config_without_gateway), station_ip, sizeof(station_ip),
               station_gateway, sizeof(station_gateway),
               station_netmask, sizeof(station_netmask)) == WIFI_AT_IP_READY);
    assert(station_gateway[0] == '\0');
    assert(station_netmask[0] == '\0');

    assert(WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "lab wifi", "password123",
                                  command, sizeof(command)));
    assert(strcmp(command, "AT+CWJAP_CUR=\"lab wifi\",\"password123\"\r\n") == 0);
    assert(WiFiAT_BuildPersistentJoinCommand(WIFI_AT_DIALECT_NONOS, "lab wifi",
                                             "password123", command, sizeof(command)));
    assert(strcmp(command, "AT+CWJAP_DEF=\"lab wifi\",\"password123\"\r\n") == 0);
    assert(WiFiAT_BuildPersistentJoinCommand(WIFI_AT_DIALECT_ESP_AT, "lab wifi",
                                             "password123", command, sizeof(command)));
    assert(strcmp(command, "AT+CWJAP=\"lab wifi\",\"password123\"\r\n") == 0);
    assert(!WiFiAT_BuildPersistentJoinCommand(WIFI_AT_DIALECT_NONOS, "ssid",
                                              "bad\"pass", command, sizeof(command)));
    assert(!WiFiAT_BuildPersistentJoinCommand(WIFI_AT_DIALECT_UNKNOWN, "ssid",
                                              "password123", command, sizeof(command)));
    assert(!WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "", "password123",
                                   command, sizeof(command)));
    assert(!WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "ssid", "",
                                   command, sizeof(command)));
    assert(!WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "ssid", "bad\"pass",
                                   command, sizeof(command)));
    assert(!WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "ssid", "bad\\pass",
                                   command, sizeof(command)));
    assert(!WiFiAT_BuildJoinCommand(WIFI_AT_DIALECT_NONOS, "ssid\rAT+RST", "password123",
                                   command, sizeof(command)));
    return 0;
}
