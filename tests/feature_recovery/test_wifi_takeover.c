#include "wifi.h"
#include "usart.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

bool WiFi_RequestModuleVersionCheck(void);

GPIO_TypeDef test_gpiof;
GPIO_TypeDef test_uart4_instance;
UART_HandleTypeDef huart4 = { .Instance = UART4 };

static uint8_t *g_rx_target;
static char g_tx[192];
static size_t g_tx_length;
static uint32_t g_tick;
static char g_log[2048];
static size_t g_log_length;
static char g_history[24][192];
static size_t g_history_count;
static unsigned int g_abort_count;
static unsigned int g_rx_rearm_failures;
static unsigned int g_tx_start_failures;

HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart)
{
    assert(uart == &huart4);
    ++g_abort_count;
    g_tx_length = 0U;
    return HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, uint32_t state)
{
    (void)port;
    (void)pin;
    (void)state;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
    (void)port;
    (void)init;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                      uint16_t length)
{
    assert(uart == &huart4);
    if (g_tx_start_failures != 0U) {
        g_tx_start_failures--;
        return HAL_ERROR;
    }
    assert(length < sizeof(g_tx));
    memcpy(g_tx, data, length);
    g_tx[length] = '\0';
    g_tx_length = length;
    assert(g_history_count < sizeof(g_history) / sizeof(g_history[0]));
    memcpy(g_history[g_history_count++], data, length);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                      uint16_t length)
{
    assert(uart == &huart4);
    assert(length == 1U);
    if (g_rx_rearm_failures != 0U) {
        g_rx_rearm_failures--;
        return HAL_ERROR;
    }
    g_rx_target = data;
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    return g_tick;
}

void UART4_Init(void) {}

void USART1_SendString(const char *text)
{
    size_t length = strlen(text);
    if (length > sizeof(g_log) - g_log_length - 1U) {
        return;
    }
    memcpy(&g_log[g_log_length], text, length);
    g_log_length += length;
    g_log[g_log_length] = '\0';
}

void USART1_SendReply(const char *text)
{
    /* Both UART output priorities share the host test's captured output. */
    USART1_SendString(text);
}

static void SendCommand(const char *expected)
{
    WiFi_Process();
    assert(g_tx_length > 0U);
    if (strcmp(g_tx, expected) != 0) {
        fprintf(stderr, "expected AT command <%s>, received <%s>\n", expected, g_tx);
    }
    assert(strcmp(g_tx, expected) == 0);
    g_tx_length = 0U;
    g_tx[0] = '\0';
    WiFi_UART_TxComplete();
}

static void FeedResponse(const char *response)
{
    size_t index;
    for (index = 0U; response[index] != '\0'; index++) {
        assert(g_rx_target != NULL);
        *g_rx_target = (uint8_t)response[index];
        WiFi_UART_RxComplete(&huart4);
    }
    WiFi_Process();
}

static void Step(const char *command, const char *response)
{
    SendCommand(command);
    FeedResponse(response);
}

static void AssertNoPassthroughModeCommand(void)
{
    size_t index;

    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CIPMODE=1") == NULL);
        assert(strstr(g_history[index], "CIPMUX=") == NULL);
    }
}

static void SendRawPayload(const char *expected, size_t length)
{
    assert(g_tx_length == length);
    assert(memcmp(g_tx, expected, length) == 0);
    g_tx_length = 0U;
    g_tx[0] = '\0';
    WiFi_UART_TxComplete();
}

static void ResetFake(void)
{
    g_abort_count = 0U;
    g_rx_rearm_failures = 0U;
    g_tx_start_failures = 0U;
    g_rx_target = NULL;
    g_tx_length = 0U;
    g_tx[0] = '\0';
    g_tick = 0U;
    g_log_length = 0U;
    g_log[0] = '\0';
    g_history_count = 0U;
    memset(g_history, 0, sizeof(g_history));
}

static void StartModernDisconnected(uint8_t mode)
{
    char mode_response[40];
    WiFi_Init();
    Step("AT\r\n", "\r\nOK\r\n");
    (void)snprintf(mode_response, sizeof(mode_response), "+CWMODE:%u\r\nOK\r\n", mode);
    Step("AT+CWMODE?\r\n", mode_response);
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    if ((mode & 1U) == 0U) {
        Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    }
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
}

static void StartLegacyConnected(void)
{
    WiFi_Init();
    Step("AT\r\n", "\r\nOK\r\n");
    Step("AT+CWMODE?\r\n", "ERROR\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWJAP_CUR?\r\n",
         "+CWJAP_CUR:\"private-network\",\"00:11:22:33:44:55\",6,-42\r\nOK\r\n");
}

static void StartLegacyDisconnected(void)
{
    WiFi_Init();
    Step("AT\r\n", "\r\nOK\r\n");
    Step("AT+CWMODE?\r\n", "ERROR\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWJAP_CUR?\r\n", "No AP\r\nERROR\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
}

static void ExhaustModernIpRetriesNotReady(void)
{
    size_t retry;
    for (retry = 0U; retry < 4U; retry++) {
        g_tick += 1000U;
        WiFi_Process();
        Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"0.0.0.0\"\r\nOK\r\n");
    }
}

static void StepModernConnectDialect(void)
{
    Step("AT+GMR\r\n", "AT version:2.2.0.0\r\nSDK version:4.4.7\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");
}

static void PrepareModernJoin(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
}

static void PrepareLegacyJoin(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    Step("AT+GMR\r\n",
         "AT version:1.7.4.0(Jul  8 2020 15:53:04)\r\n"
         "SDK version:3.0.5-dev(52383f9)\r\nOK\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:3\r\nOK\r\n");
    SendCommand("AT+CWJAP_CUR=\"lab-ssid\",\"pass1234\"\r\n");
}

static void TestModernTakeover(void)
{
    char status[224];
    char ip_config[128];
    WiFiRuntimeInfo details;
    size_t index;
    size_t station_mode_commands = 0U;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "\r\nOK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n",
         "+CIPSTATUS:0,\"TCP\",\"192.0.2.7\",443,1234,1\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.137.222\"\r\n"
         "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.mode_known && details.mode == 3U);
    assert(details.apsta == WIFI_APSTA_YES);
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(details.tcp == WIFI_AT_TCP_CONNECTED);
    assert(WiFi_FormatStatus(status, sizeof(status)));
    assert(strstr(status, "CONNECTED") != NULL);
    assert(strstr(status, "CREDENTIALS=none") != NULL);
    assert(strstr(status, "MODE=APSTA") != NULL);
    assert(strstr(status, "IP=READY") != NULL);
    assert(strstr(status, "TCP=CONNECTED") != NULL);
    assert(strstr(status, "private-network") == NULL);
    assert(WiFi_FormatIp(ip_config, sizeof(ip_config)));
    assert(strcmp(ip_config,
                  "WIFI IP: 192.168.137.222; GATEWAY=192.168.137.1; "
                  "NETMASK=255.255.255.0\r\n") == 0);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CWQAP") == NULL);
        assert(strstr(g_history[index], "RST") == NULL);
        assert(strstr(g_history[index], "CWMODE=") == NULL);
        assert(strstr(g_history[index], "CIPMODE=") == NULL);
        assert(strstr(g_history[index], "CIPMUX=") == NULL);
        assert(strstr(g_history[index], "CIPCLOSE") == NULL);
        assert(strstr(g_history[index], "CWJAP=\"") == NULL);
        if (strstr(g_history[index], "CWMODE=3,0") != NULL) {
            station_mode_commands++;
        }
    }
    assert(station_mode_commands == 0U);
}

static void TestModernRejectedStateQueryFallsBackToCwjap(void)
{
    WiFiRuntimeInfo details;
    size_t index;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n",
         "+CIPSTATUS:0,\"TCP\",\"192.0.2.7\",443,1234,1\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "AT+CWSTATE?\r\nERROR\r\n");
    Step("AT+CWJAP?\r\n",
         "+CWJAP:\"private-network\",\"00:11:22:33:44:55\",11,-50,1,1,3,0,0\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(details.tcp == WIFI_AT_TCP_CONNECTED);
    assert(strstr(g_log, "private-network") == NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CWQAP") == NULL);
        assert(strstr(g_history[index], "CWMODE=") == NULL);
        assert(strstr(g_history[index], "CIPCLOSE") == NULL);
        assert(strstr(g_history[index], "CWJAP=\"") == NULL);
    }
}

static void TestLegacyTakeover(void)
{
    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "ERROR\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:3\r\nOK\r\n");
    Step("AT+CWJAP_CUR?\r\n",
         "+CWJAP_CUR:\"private-network\",\"00:11:22:33:44:55\",6,-42\r\nOK\r\n");
    Step("AT+CIPSTA_CUR?\r\n", "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(g_history_count == 6U);
}

static void TestExplicitConnectPreemptsSyncRetryWait(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "AT+CWSTATE?\r\nERROR\r\n");
    Step("AT+CWJAP?\r\n", "AT+CWJAP?\r\nERROR\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_UNKNOWN);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTING);
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.apsta == WIFI_APSTA_YES);
    assert(details.station == WIFI_AT_STATION_CONNECTED);
}

static void TestConnectStillRejectsActiveSyncAndConnectedTcp(void)
{
    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    assert(!WiFi_RequestConnect("lab-ssid", "pass1234"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "WIFI_STATE_MACHINE_BUSY") == 0);

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n",
         "+CIPSTATUS:0,\"TCP\",\"192.0.2.7\",443,1234,1\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(!WiFi_RequestConnect("lab-ssid", "pass1234"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "TCP_ALREADY_CONNECTED") == 0);
}

static void TestConnectRejectReasonsAreSpecific(void)
{
    ResetFake();
    WiFi_Init();
    assert(!WiFi_ConnectSpec("lab-ssid|pass1234"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "AT_DIALECT_UNKNOWN") == 0);

    ResetFake();
    StartModernDisconnected(3U);
    assert(!WiFi_RequestConnect("lab-ssid", "short"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "CREDENTIALS_INVALID") == 0);

    ResetFake();
    StartLegacyConnected();
    Step("AT+CIPSTA_CUR?\r\n", "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\nOK\r\n");
    assert(!WiFi_RequestConnect("lab-ssid", "pass1234"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "STA_ALREADY_CONNECTED") == 0);

    ResetFake();
    assert(!WiFi_ConnectSpec("missing-separator"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "SPECIFICATION_INVALID") == 0);
}

static void TestDisconnectedModeAndExplicitConnect(void)
{
    char status[224];
    WiFiRuntimeInfo details;
    size_t index;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
    assert(WiFi_GetStatus() != WIFI_STATUS_CONNECTED);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"0.0.0.0\"\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_FormatStatus(status, sizeof(status)));
    assert(strstr(status, "CREDENTIALS=loaded") != NULL);
    assert(strstr(status, "lab-ssid") == NULL);
    assert(strstr(status, "pass1234") == NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strcmp(g_history[index], "AT+SYSSTORE=1\r\n") != 0);
        assert(strcmp(g_history[index], "AT+CWAUTOCONN=1\r\n") != 0);
        assert(strstr(g_history[index], "CWJAP_DEF") == NULL);
    }

    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    Step("AT+CWQAP\r\n", "OK\r\n");
    assert(strstr(g_log, "WIFI CLEAR: MODULE_DISCONNECTED") != NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_UNKNOWN);
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
    assert(WiFi_FormatStatus(status, sizeof(status)));
    assert(strstr(status, "CREDENTIALS=none") != NULL);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_UNKNOWN);
    assert(details.ip == WIFI_IP_UNKNOWN);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    Step("AT+CWQAP\r\n", "ERROR\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_UNKNOWN);
    assert(strstr(g_log, "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_UNCONFIRMED") != NULL);
    assert(WiFi_RequestConnect("replacement-ssid", "new-pass123"));
}

static void TestPersistentConnectModernAndClearRetainsSavedProfile(void)
{
    size_t index;
    bool saw_persistent_store = false;
    bool saw_persistent_join = false;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+SYSSTORE=1\r\n", "OK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    Step("AT+CWAUTOCONN=1\r\n", "OK\r\n");
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.137.222\"\r\n"
         "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");

    assert(strstr(g_log, "WIFI CONNECT SAVE: STORED; AUTO_CONNECT_ENABLED\r\n") != NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    for (index = 0U; index < g_history_count; index++) {
        if (strcmp(g_history[index], "AT+SYSSTORE=1\r\n") == 0) {
            saw_persistent_store = true;
        }
        assert(strstr(g_history[index], "CWJAP_CUR") == NULL);
        if (strcmp(g_history[index], "AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n") == 0) {
            saw_persistent_join = true;
        }
    }
    assert(saw_persistent_store);
    assert(saw_persistent_join);
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);

    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    Step("AT+CWQAP\r\n", "OK\r\n");
    assert(strstr(g_log, "WIFI CLEAR: MODULE_DISCONNECTED\r\n") != NULL);
    /* CWQAP disconnects now; no command here erases the ESP saved profile. */
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "RESTORE") == NULL);
        assert(strstr(g_history[index], "CWJAP_DEF") == NULL);
    }
}

static void TestPersistentSaveAcceptedWhileStationAlreadyConnected(void)
{
    size_t index;
    bool saw_persistent_store = false;
    bool saw_persistent_join = false;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"lab-ssid\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.137.222\"\r\n"
         "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
    assert(!WiFi_ConnectSpec("lab-ssid|pass1234"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(), "STA_ALREADY_CONNECTED") == 0);
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));

    WiFi_Process();
    Step("AT+GMR\r\n", "AT version:2.2.0.0\r\nSDK version:4.4.7\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"lab-ssid\"\r\nOK\r\n");
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+SYSSTORE=1\r\n", "OK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("OK\r\n");
    Step("AT+CWAUTOCONN=1\r\n", "OK\r\n");
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.137.222\"\r\n"
         "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");

    assert(strstr(g_log, "WIFI CONNECT SAVE: STORED; AUTO_CONNECT_ENABLED\r\n") != NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
    for (index = 0U; index < g_history_count; index++) {
        if (strcmp(g_history[index], "AT+SYSSTORE=1\r\n") == 0) {
            saw_persistent_store = true;
        }
        if (strcmp(g_history[index], "AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n") == 0) {
            saw_persistent_join = true;
        }
    }
    assert(saw_persistent_store);
    assert(saw_persistent_join);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
}

static void TestPersistentConnectLegacyUsesDefaultCommands(void)
{
    size_t index;

    ResetFake();
    StartLegacyDisconnected();
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));
    WiFi_Process();
    Step("AT+GMR\r\n",
         "AT version:1.7.4.0(Jul  8 2020 15:53:04)\r\n"
         "SDK version:3.0.5-dev(52383f9)\r\nOK\r\n");
    Step("AT+CWMODE_DEF=3\r\n", "OK\r\n");
    SendCommand("AT+CWJAP_DEF=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    Step("AT+CWAUTOCONN=1\r\n", "OK\r\n");
    Step("AT+CIPSTA_CUR?\r\n",
         "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\n"
         "+CIPSTA_CUR:gateway:\"10.0.0.1\"\r\n"
         "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n");

    assert(strstr(g_log, "WIFI CONNECT SAVE: STORED; AUTO_CONNECT_ENABLED\r\n") != NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "SYSSTORE") == NULL);
        assert(strstr(g_history[index], "CWJAP_CUR=\"lab-ssid\"") == NULL);
    }
}

static void TestPersistentConnectRestoresRamModeOnJoinFailure(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+SYSSTORE=1\r\n", "OK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("+CWJAP:2\r\nERROR\r\n");
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    assert(strstr(g_log, "WIFI CONNECT SAVE: FAILED; RETRY_SCHEDULED\r\n") != NULL);
    assert(strstr(g_log, "WIFI CONNECT SAVE: STORED") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
}

static void TestPersistentConnectStopsIfRamModeRestoreFails(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+SYSSTORE=1\r\n", "OK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    Step("AT+CWAUTOCONN=1\r\n", "OK\r\n");
    Step("AT+SYSSTORE=0\r\n", "ERROR\r\n");

    assert(strstr(g_log, "WIFI CONNECT SAVE: STORE_MODE_UNKNOWN; STOPPED\r\n") != NULL);
    assert(!WiFi_ConnectSpec("replacement|new-password"));
    assert(strcmp(WiFi_GetLastConnectRejectReason(),
                  "PERSISTENT_STORE_MODE_UNKNOWN") == 0);
    assert(g_tx_length == 0U);
}

static void TestPersistentConnectClearRestoresRamModeBeforeDisconnect(void)
{
    size_t index;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_ConnectSpec("SAVE lab-ssid|pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+SYSSTORE=1\r\n", "OK\r\n");

    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWQAP\r\n", "OK\r\n");
    assert(strstr(g_log, "WIFI CONNECT SAVE: CANCELLED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CWMODE=3,0") == NULL);
        assert(strstr(g_history[index], "CWJAP=") == NULL);
        assert(strstr(g_history[index], "CWAUTOCONN") == NULL);
    }
}

static void TestExplicitConnectRestoresApStaAfterStartupFailure(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "ERROR\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");

    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    SendCommand("AT+CWJAP=\"lab-ssid\",\"pass1234\"\r\n");
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.apsta == WIFI_APSTA_YES);
}

static void TestExplicitConnectStopsWhenApStaUnavailable(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "ERROR\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");

    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "ERROR\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_UNKNOWN);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.apsta == WIFI_APSTA_UNKNOWN);
    assert(strstr(g_log, "WIFI CONNECT: APSTA_UNAVAILABLE\r\n") != NULL);
    assert(g_tx_length == 0U);
}

static void TestConnectReportsVolatileStorageTerminalError(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "ERROR\r\n");

    assert(strstr(g_log, "WIFI CONNECT: VOLATILE_STORAGE_ERROR\r\n") != NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
}

static void TestGmrIdentifiesNonosAndSkipsVolatileStorageCommand(void)
{
    size_t first_connect_command;
    size_t index;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    first_connect_command = g_history_count;
    WiFi_Process();
    Step("AT+GMR\r\n", "AT version:1.7.4.0(Jul  8 2020 15:53:04)\r\n"
         "SDK version:3.0.5-dev(52383f9)\r\nOK\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:3\r\nOK\r\n");
    SendCommand("AT+CWJAP_CUR=\"lab-ssid\",\"pass1234\"\r\n");
    FeedResponse("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
    Step("AT+CIPSTA_CUR?\r\n", "+CIPSTA_CUR:ip:\"10.0.0.8\"\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(strstr(g_log, "WIFI CONNECT: NONOS_AT_CONFIRMED\r\n") != NULL);
    for (index = first_connect_command; index < g_history_count; index++) {
        assert(strstr(g_history[index], "SYSSTORE") == NULL);
        assert(strstr(g_history[index], "CWSTATE") == NULL);
        assert(strstr(g_history[index], "AT+CWJAP=\"") == NULL);
        assert(strstr(g_history[index], "AT+CWJAP_DEF") == NULL);
    }
}

static void TestModernJoinFailureReportsOnlyNumericCode(void)
{
    PrepareModernJoin();
    FeedResponse("+CWJAP:2\r\nERROR\r\n");

    assert(strstr(g_log, "WIFI CONNECT: JOIN_ERROR_CODE=2\r\n") != NULL);
    assert(strstr(g_log, "WIFI CONNECT: RETRY_SCHEDULED\r\n") != NULL);
    assert(strstr(g_log, "+CWJAP:") == NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
}

static void TestLegacyJoinFailureReportsOnlyNumericCode(void)
{
    PrepareLegacyJoin();
    FeedResponse("+CWJAP_CUR:3\r\nFAIL\r\n");

    assert(strstr(g_log, "WIFI CONNECT: JOIN_ERROR_CODE=3\r\n") != NULL);
    assert(strstr(g_log, "WIFI CONNECT: RETRY_SCHEDULED\r\n") != NULL);
    assert(strstr(g_log, "+CWJAP_CUR:") == NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
}

static void TestMalformedJoinErrorCodeIsNotReported(void)
{
    PrepareModernJoin();
    FeedResponse("+CWJAP:999\r\nERROR\r\n");

    assert(strstr(g_log, "WIFI CONNECT: JOIN_ERROR\r\n") != NULL);
    assert(strstr(g_log, "JOIN_ERROR_CODE=") == NULL);
    assert(strstr(g_log, "+CWJAP:") == NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
}

static void TestJoinTimeoutReportsFixedTimeout(void)
{
    PrepareModernJoin();
    g_tick += 60000U;
    WiFi_Process();

    assert(strstr(g_log, "WIFI CONNECT: JOIN_TIMEOUT\r\n") != NULL);
    assert(strstr(g_log, "WIFI CONNECT: RETRY_SCHEDULED\r\n") != NULL);
    assert(strstr(g_log, "JOIN_ERROR_CODE=") == NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
}

static void TestUnknownGmrStopsBeforeAnyJoinCommand(void)
{
    size_t first_connect_command;
    size_t index;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    first_connect_command = g_history_count;
    WiFi_Process();
    Step("AT+GMR\r\n", "AT version:1.8.0.0\r\nSDK version:unknown\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
    assert(strstr(g_log, "WIFI CONNECT: DIALECT_UNKNOWN\r\n") != NULL);
    for (index = first_connect_command; index < g_history_count; index++) {
        assert(strstr(g_history[index], "SYSSTORE") == NULL);
        assert(strstr(g_history[index], "CWJAP") == NULL);
        assert(strstr(g_history[index], "CWMODE_CUR") == NULL);
    }
}

static void TestModernGmrProbeFailureDoesNotUseLegacyCommands(void)
{
    size_t first_connect_command;
    size_t index;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    first_connect_command = g_history_count;
    WiFi_Process();
    Step("AT+GMR\r\n", "AT version:2.2.0.0\r\nSDK version:4.4.7\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "ERROR\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
    assert(strstr(g_log, "WIFI CONNECT: DIALECT_UNKNOWN\r\n") != NULL);
    for (index = first_connect_command; index < g_history_count; index++) {
        assert(strstr(g_history[index], "SYSSTORE") == NULL);
        assert(strstr(g_history[index], "CWMODE_CUR") == NULL);
        assert(strstr(g_history[index], "CWJAP") == NULL);
    }
}

static void TestConnectReportsVolatileStorageTimeout(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    SendCommand("AT+SYSSTORE=0\r\n");
    g_tick += 1200U;
    WiFi_Process();

    assert(strstr(g_log, "WIFI CONNECT: VOLATILE_STORAGE_TIMEOUT\r\n") != NULL);
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
}

static void TestModuleVersionCheckPreservesRetryCredentials(void)
{
    char status[224];
    size_t history_before;

    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_RequestConnect("lab-ssid", "pass1234"));
    WiFi_Process();
    StepModernConnectDialect();
    Step("AT+SYSSTORE=0\r\n", "ERROR\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
    history_before = g_history_count;

    assert(WiFi_RequestModuleVersionCheck());
    WiFi_Process();
    SendCommand("AT+GMR\r\n");
    FeedResponse("AT version:2.2.0.0\r\nSDK version:4.4.7\r\n"
                 "compile time:2026-10-01 12:00:00\r\nOK\r\n");

    assert(WiFi_GetStatus() == WIFI_STATUS_RETRY_WAIT);
    assert(WiFi_FormatStatus(status, sizeof(status)));
    assert(strstr(status, "CREDENTIALS=loaded") != NULL);
    assert(strstr(g_log, "WIFI GMR: AT=2.2.0.0; SDK=4.4.7\r\n") != NULL);
    assert(strstr(g_log, "lab-ssid") == NULL);
    assert(strstr(g_log, "pass1234") == NULL);
    assert(g_history_count == history_before + 1U);
    assert(strcmp(g_history[history_before], "AT+GMR\r\n") == 0);
}

static void TestUnknownIsNotAssumedDisconnected(void)
{
    WiFiRuntimeInfo details;
    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "ERROR\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.tcp == WIFI_AT_TCP_UNKNOWN);
    assert(details.apsta == WIFI_APSTA_YES);
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(strstr(g_history[g_history_count - 1U], "CIPSTA") != NULL);
    for (size_t index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CWMODE=3") == NULL);
        assert(strstr(g_history[index], "CIPSTART") == NULL);
        assert(strstr(g_history[index], "CIPCLOSE") == NULL);
    }
}

static void TestUnknownStationResponseReportsOnlySanitizedShape(void)
{
    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:3,\"private-network\"\r\nOK\r\n");

    assert(strstr(g_log, "WIFI SYNC DIAG: D=1 F=18 L=3 R=U\r\n") != NULL);
    assert(strstr(g_log, "private-network") == NULL);
}

static void TestApStaOnlyChangesAfterTcpProvedIdle(void)
{
    WiFiRuntimeInfo details;
    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");
    assert(WiFi_GetStatus() == WIFI_STATUS_NO_CREDENTIALS);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.apsta == WIFI_APSTA_YES);
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
}

static void TestLegacyApStaModeUsesVolatileCommand(void)
{
    WiFiRuntimeInfo details;
    size_t index;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "ERROR\r\n");
    Step("AT+CWMODE_CUR?\r\n", "+CWMODE_CUR:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE_CUR=3\r\n", "OK\r\n");
    Step("AT+CWJAP_CUR?\r\n", "No AP\r\nERROR\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.mode_known && details.mode == 3U);
    assert(details.apsta == WIFI_APSTA_YES);
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "SYSSTORE") == NULL);
        assert(strstr(g_history[index], "CWMODE=3") == NULL);
    }
}

static void TestUnknownTcpPreventsModeChange(void)
{
    WiFiRuntimeInfo details;
    size_t index;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.tcp == WIFI_AT_TCP_UNKNOWN);
    assert(details.apsta == WIFI_APSTA_NO);
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CWMODE=3") == NULL);
        assert(strstr(g_history[index], "CWMODE_CUR=3") == NULL);
        assert(strstr(g_history[index], "CIPSTART") == NULL);
    }
}

static void TestRejectedApStaModeRemainsUnknown(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "ERROR\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.tcp == WIFI_AT_TCP_DISCONNECTED);
    assert(details.apsta == WIFI_APSTA_UNKNOWN);
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
}

static void TestMalformedIpStaysUnknownAndNetworkProbeIsNotRun(void)
{
    WiFiRuntimeInfo details;
    size_t index;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"bad\"\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_UNKNOWN);
    assert(details.tcp == WIFI_AT_TCP_DISCONNECTED);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CIPSTART") == NULL);
    }
}

static void TestLegacyIpQueryRetriesAfterDhcpNotReady(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    StartLegacyConnected();
    Step("AT+CIPSTA_CUR?\r\n",
         "+CIPSTA_CUR:0.0.0.0\r\n"
         "+CIPSTA_CUR:192.168.6.1\r\n"
         "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_NOT_READY);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(g_tx_length == 0U);

    g_tick += 999U;
    WiFi_Process();
    assert(g_tx_length == 0U);
    ++g_tick;
    WiFi_Process();
    SendCommand("AT+CIPSTA_CUR?\r\n");
    FeedResponse("+CIPSTA_CUR:192.168.6.100\r\n"
                 "+CIPSTA_CUR:192.168.6.1\r\n"
                 "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(strstr(g_log, "WIFI SYNC: IP_RETRY\r\n") != NULL);
}

static void TestIpQueryTimeoutRetriesAreBounded(void)
{
    WiFiRuntimeInfo details;
    const size_t query_limit = 5U;
    size_t index;
    size_t history_before_queries;

    ResetFake();
    StartLegacyConnected();
    history_before_queries = g_history_count;

    for (index = 0U; index < query_limit; index++) {
        if (index > 0U) {
            g_tick += 1000U;
            WiFi_Process();
        }
        SendCommand("AT+CIPSTA_CUR?\r\n");
        g_tick += 1200U;
        WiFi_Process();
        assert(g_tx_length == 0U);
    }

    assert(g_history_count == history_before_queries + query_limit);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(WiFi_GetStatus() == WIFI_STATUS_CONNECTED);
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_UNKNOWN);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(strstr(g_log, "WIFI SYNC: IP_TIMEOUT\r\n") != NULL);

    g_tick += 10000U;
    WiFi_Process();
    assert(g_history_count == history_before_queries + query_limit);

    FeedResponse("WIFI GOT IP\r\n");
    SendCommand("AT+CIPSTA_CUR?\r\n");
    FeedResponse("+CIPSTA_CUR:192.168.6.100\r\n"
                 "+CIPSTA_CUR:192.168.6.1\r\n"
                 "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n");
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.ip == WIFI_IP_READY);
    assert(details.network == WIFI_NETWORK_NOT_RUN);
}

static void TestIpQueryRetryStopsAfterDisconnect(void)
{
    WiFiRuntimeInfo details;
    size_t history_after_first_query;

    ResetFake();
    StartLegacyConnected();
    Step("AT+CIPSTA_CUR?\r\n", "+CIPSTA_CUR:0.0.0.0\r\nOK\r\n");
    history_after_first_query = g_history_count;
    FeedResponse("WIFI DISCONNECT\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
    assert(details.ip == WIFI_IP_NOT_READY);
    assert(g_history_count == history_after_first_query);

    g_tick += 5000U;
    WiFi_Process();
    WiFi_Process();
    assert(g_history_count == history_after_first_query);
}

static void TestAutomaticNetworkProbeIsDisabled(void)
{
    static const char *const status_fields[] = {
        "WIFI STATUS: ", "; CREDENTIALS=", "; RETRY_IN=", "; MODE=",
        "; APSTA=", "; STA=", "; IP=", "; TCP=", "; NET=NOT_RUN\r\n"
    };
    WiFiRuntimeInfo details;
    char status[224];
    size_t index;
    size_t position = 0U;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");
    g_tick += 5000U;
    WiFi_Process();

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.network == WIFI_NETWORK_NOT_RUN);
    assert(WiFi_FormatStatus(status, sizeof(status)));
    for (index = 0U; index < sizeof(status_fields) / sizeof(status_fields[0]); index++) {
        const char *field = strstr(&status[position], status_fields[index]);
        assert(field != NULL);
        position = (size_t)(field - status) + strlen(status_fields[index]);
    }
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "CIPSTART") == NULL);
        assert(strstr(g_history[index], "CIPSEND") == NULL);
        assert(strstr(g_history[index], "CIPCLOSE") == NULL);
    }
}

static void TestLiveWifiStatusCheckRefreshesStationAndIp(void)
{
    WiFiRuntimeInfo details;
    WiFiStatus status;
    char address[96];

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");

    assert(WiFi_RequestStatusCheck());
    Step("AT+CWSTATE?\r\n", "+CWSTATE:1\r\nOK\r\n");
    assert(!WiFi_TakeStatusCheckResult(&status));
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.137.59\"\r\n"
         "+CIPSTA:gateway:\"192.168.137.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(WiFi_TakeStatusCheckResult(&status));
    assert(status == WIFI_STATUS_CONNECTED);
    assert(!WiFi_TakeStatusCheckResult(&status));
    assert(WiFi_FormatIp(address, sizeof(address)));
    assert(strcmp(address,
                  "WIFI IP: 192.168.137.59; GATEWAY=192.168.137.1; "
                  "NETMASK=255.255.255.0\r\n") == 0);
}

static void TestLiveWifiStatusCheckClearsStaleConnection(void)
{
    WiFiRuntimeInfo details;
    WiFiStatus status;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");

    assert(WiFi_RequestStatusCheck());
    Step("AT+CWSTATE?\r\n", "+CWSTATE:0\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
    assert(details.ip == WIFI_IP_NOT_READY);
    assert(WiFi_TakeStatusCheckResult(&status));
    assert(status == WIFI_STATUS_NO_CREDENTIALS);
    assert(strstr(g_log, "WIFI SYNC: STA_DISCONNECTED\r\n") != NULL);
}

static void TestTcpStatusStationLossInvalidatesCachedNetwork(void)
{
    WiFiRuntimeInfo details;
    WiFiAtTcpStatus tcp_status;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.137.222\"\r\nOK\r\n");

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_READY);
    assert(WiFi_RequestTcpStatusCheck());
    Step("AT+CIPSTATUS\r\n", "STATUS:5\r\nOK\r\n");

    assert(WiFi_TakeTcpStatusCheckResult(&tcp_status));
    assert(tcp_status == WIFI_AT_TCP_DISCONNECTED);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_DISCONNECTED);
    assert(details.ip == WIFI_IP_NOT_READY);
    assert(!WiFi_IsConnected());
    assert(!WiFi_TcpConnect("example.com", 443U));
}

static void StepTcpNormalModeGuard(void)
{
    Step("AT+CIPMODE=0\r\n", "OK\r\n");
    Step("AT+CIPMODE?\r\n", "+CIPMODE:0\r\nOK\r\n");
}

static void TestTcpClientOperations(void)
{
    static const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};
    WiFiTcpOperationResult result;
    WiFiAtTcpStatus status;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.6.100\"\r\nOK\r\n");

    assert(!WiFi_TcpSend(payload, sizeof(payload)));
    assert(!WiFi_TcpConnect("192.0.2.10", 8080U));
    Step("AT+CIPMUX?\r\n", "+CIPMUX:0\r\nOK\r\n");
    assert(WiFi_TcpConnect("192.0.2.10", 8080U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",8080\r\n");
    FeedResponse("CONNECT\r\n\r\nOK\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);

    assert(WiFi_TcpSend(payload, sizeof(payload)));
    SendCommand("AT+CIPSEND=5\r\n");
    FeedResponse("OK\r\n>");
    SendRawPayload("hello", sizeof(payload));
    FeedResponse("\r\nSEND OK\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);

    assert(WiFi_RequestTcpStatusCheck());
    Step("AT+CIPSTATUS\r\n", "STATUS:3\r\nOK\r\n");
    assert(WiFi_TakeTcpStatusCheckResult(&status));
    assert(status == WIFI_AT_TCP_CONNECTED);

    assert(WiFi_TcpClose());
    Step("AT+CIPCLOSE\r\n", "CLOSED\r\nOK\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);
}

static void PrepareTcpConnectReady(void)
{
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.6.100\"\r\nOK\r\n");
    assert(WiFi_RequestTcpMuxModeCheck());
    Step("AT+CIPMUX?\r\n", "+CIPMUX:0\r\nOK\r\n");
}

static void TestTcpConnectRequiresVerifiedNormalMode(void)
{
    WiFiTcpOperationResult result;
    size_t index;

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    Step("AT+CIPMODE=0\r\n", "ERROR\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: CIPMODE_SET_FAILED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPSTART") == NULL);
    }

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    Step("AT+CIPMODE=0\r\n", "OK\r\n");
    Step("AT+CIPMODE?\r\n", "+CIPMODE:1\r\nOK\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: CIPMODE_VERIFY_FAILED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPSTART") == NULL);
    }
}

static void PrepareDnsGatewayReady(void)
{
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n",
         "+CIPSTA:ip:\"192.168.6.100\"\r\n"
         "+CIPSTA:gateway:\"192.168.6.1\"\r\n"
         "+CIPSTA:netmask:\"255.255.255.0\"\r\nOK\r\n");
}

static void TestDnsGatewayUseIsExplicitAndVerified(void)
{
    size_t index;

    ResetFake();
    PrepareDnsGatewayReady();

    assert(WiFi_RequestDnsGatewayUse());
    assert(!WiFi_RequestDnsGatewayUse());
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CIPDNS=1,\"192.168.6.1\"\r\n", "OK\r\n");
    assert(strstr(g_log, "WIFI DNS GATEWAY: SET\r\n") != NULL);
    Step("AT+CIPDNS?\r\n", "+CIPDNS:1,\"192.168.6.1\"\r\nOK\r\n");
    assert(strstr(g_log, "WIFI DNS STATUS: MODE=MANUAL\r\n") != NULL);
    assert(strstr(g_log, "WIFI DNS STATUS: DNS1=192.168.6.1\r\n") != NULL);
    assert(strstr(g_log, "WIFI DNS GATEWAY: VERIFIED\r\n") != NULL);

    ResetFake();
    PrepareDnsGatewayReady();
    assert(WiFi_RequestDnsGatewayUse());
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CIPDNS=1,\"192.168.6.1\"\r\n", "OK\r\n");
    Step("AT+CIPDNS?\r\n", "+CIPDNS:1,\"8.8.8.8\"\r\nOK\r\n");
    assert(strstr(g_log, "WIFI DNS GATEWAY: VERIFY_FAILED\r\n") != NULL);
    assert(strstr(g_log, "WIFI DNS GATEWAY: VERIFIED\r\n") == NULL);

    ResetFake();
    PrepareDnsGatewayReady();
    assert(WiFi_RequestDnsGatewayUse());
    Step("AT+SYSSTORE=0\r\n", "ERROR\r\n");
    assert(strstr(g_log, "WIFI DNS GATEWAY: STORE_MODE_FAILED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPDNS=") == NULL);
        assert(strstr(g_history[index], "AT+CIPDNS_CUR=") == NULL);
    }

    ResetFake();
    PrepareDnsGatewayReady();
    assert(WiFi_RequestDnsGatewayUse());
    SendCommand("AT+SYSSTORE=0\r\n");
    g_tick += 1200U;
    WiFi_Process();
    assert(strstr(g_log, "WIFI DNS GATEWAY: STORE_MODE_TIMEOUT\r\n") != NULL);
    assert(g_tx_length == 0U);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPDNS=") == NULL);
        assert(strstr(g_history[index], "AT+CIPDNS_CUR=") == NULL);
    }

    ResetFake();
    StartLegacyConnected();
    Step("AT+CIPSTA_CUR?\r\n",
         "+CIPSTA_CUR:192.168.6.100\r\n"
         "+CIPSTA_CUR:192.168.6.1\r\n"
         "+CIPSTA_CUR:255.255.255.0\r\nOK\r\n");
    assert(WiFi_RequestDnsGatewayUse());
    Step("AT+CIPDNS_CUR=1,\"192.168.6.1\"\r\n", "OK\r\n");
    Step("AT+CIPDNS_CUR?\r\n",
         "+CIPDNS_CUR:\"192.168.6.1\"\r\nOK\r\n");
    assert(strstr(g_log, "WIFI DNS STATUS: DNS1=192.168.6.1\r\n") != NULL);
    assert(strstr(g_log, "WIFI DNS GATEWAY: VERIFIED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strcmp(g_history[index], "AT+SYSSTORE=0\r\n") != 0);
    }

    ResetFake();
    PrepareDnsGatewayReady();
    assert(WiFi_RequestDnsGatewayUse());
    Step("AT+SYSSTORE=0\r\n", "OK\r\n");
    Step("AT+CIPDNS=1,\"192.168.6.1\"\r\n", "ERROR\r\n");
    assert(strstr(g_log, "WIFI DNS GATEWAY: SET_FAILED\r\n") != NULL);
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPDNS?") == NULL);
    }

    ResetFake();
    PrepareTcpConnectReady();
    assert(!WiFi_RequestDnsGatewayUse());
}

static void TestTcpMuxModeQueryIsReadOnly(void)
{
    size_t index;

    ResetFake();
    PrepareTcpConnectReady();

    assert(WiFi_RequestTcpMuxModeCheck());
    assert(!WiFi_RequestTcpMuxModeCheck());
    Step("AT+CIPMUX?\r\n", "+CIPMUX:0\r\nOK\r\n");
    assert(strstr(g_log, "WIFI TCP MUX: SINGLE\r\n") != NULL);
    AssertNoPassthroughModeCommand();

    assert(WiFi_RequestTcpMuxModeCheck());
    Step("AT+CIPMUX?\r\n", "+CIPMUX:1\r\nOK\r\n");
    assert(strstr(g_log, "WIFI TCP MUX: MULTI\r\n") != NULL);

    assert(WiFi_RequestTcpMuxModeCheck());
    Step("AT+CIPMUX?\r\n", "ERROR\r\n");
    assert(strstr(g_log, "WIFI TCP MUX: UNKNOWN\r\n") != NULL);

    assert(WiFi_RequestTcpMuxModeCheck());
    SendCommand("AT+CIPMUX?\r\n");
    g_tick += 1201U;
    WiFi_Process();
    assert(strstr(g_log, "WIFI TCP MUX: UNKNOWN\r\n") != NULL);
    AssertNoPassthroughModeCommand();

    assert(!WiFi_TcpConnect("192.0.2.10", 80U));
    Step("AT+CIPMUX?\r\n", "ERROR\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
    assert(!WiFi_TakeTcpOperationResult(&(WiFiTcpOperationResult){0}));
    for (index = 0U; index < g_history_count; index++) {
        assert(strstr(g_history[index], "AT+CIPSTART") == NULL);
    }
}

static void TestTcpModeQueryIsReadOnly(void)
{
    ResetFake();
    PrepareTcpConnectReady();

    assert(WiFi_RequestTcpModeCheck());
    assert(!WiFi_RequestTcpModeCheck());
    Step("AT+CIPMODE?\r\n", "+CIPMODE:0\r\nOK\r\n");
    assert(strstr(g_log, "WIFI TCP MODE: NORMAL\r\n") != NULL);
    AssertNoPassthroughModeCommand();

    assert(WiFi_RequestTcpModeCheck());
    Step("AT+CIPMODE?\r\n", "+CIPMODE:1\r\nOK\r\n");
    assert(strstr(g_log, "WIFI TCP MODE: TRANSPARENT\r\n") != NULL);

    assert(WiFi_RequestTcpModeCheck());
    Step("AT+CIPMODE?\r\n", "ERROR\r\n");
    assert(strstr(g_log, "WIFI TCP MODE: UNKNOWN\r\n") != NULL);

    assert(WiFi_RequestTcpModeCheck());
    SendCommand("AT+CIPMODE?\r\n");
    g_tick += 1201U;
    WiFi_Process();
    assert(strstr(g_log, "WIFI TCP MODE: UNKNOWN\r\n") != NULL);
    AssertNoPassthroughModeCommand();
}

static void TestWifiPingGateway(void)
{
    ResetFake();
    PrepareTcpConnectReady();

    assert(WiFi_Ping("192.168.137.1"));
    SendCommand("AT+PING=\"192.168.137.1\"\r\n");
    FeedResponse("+PING:2\r\nOK\r\n");
    assert(strstr(g_log, "WIFI PING: REPLY\r\n") != NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_Ping("192.168.137.1"));
    SendCommand("AT+PING=\"192.168.137.1\"\r\n");
    FeedResponse("+2\r\nOK\r\n");
    assert(strstr(g_log, "WIFI PING: REPLY\r\n") != NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_Ping("192.168.137.1"));
    SendCommand("AT+PING=\"192.168.137.1\"\r\n");
    FeedResponse("+timeout\r\nERROR\r\n");
    assert(strstr(g_log, "WIFI PING: TIMEOUT\r\n") != NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_Ping("192.168.137.1"));
    SendCommand("AT+PING=\"192.168.137.1\"\r\n");
    FeedResponse("+PING:TIMEOUT\r\nERROR\r\n");
    assert(strstr(g_log, "WIFI PING: TIMEOUT\r\n") != NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
}

static void TestTcpConnectDiscoversMuxModeBeforeCipstart(void)
{
    static const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};
    WiFiTcpOperationResult result;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.6.100\"\r\nOK\r\n");
    assert(!WiFi_TcpConnect("192.0.2.10", 80U));
    Step("AT+CIPMUX?\r\n", "+CIPMUX:1\r\nOK\r\n");
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    Step("AT+CIPSTART=0,\"TCP\",\"192.0.2.10\",80\r\n",
         "0,CONNECT\r\nOK\r\n");

    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_CONNECTED);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);

    assert(WiFi_TcpSend(payload, sizeof(payload)));
    SendCommand("AT+CIPSEND=0,5\r\n");
    FeedResponse("OK\r\n>");
    SendRawPayload("hello", sizeof(payload));
    FeedResponse("\r\nSEND OK\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);

    assert(WiFi_TcpClose());
    Step("AT+CIPCLOSE=0\r\n", "0,CLOSED\r\nOK\r\n");
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_DISCONNECTED);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_SUCCEEDED);
}

static void TestTcpConnectRejectedUntilStationIpIsReady(void)
{
    WiFiRuntimeInfo details;

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:1\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWMODE=3,0\r\n", "OK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"private-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"0.0.0.0\"\r\nOK\r\n");
    ExhaustModernIpRetriesNotReady();

    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.station == WIFI_AT_STATION_CONNECTED);
    assert(details.ip == WIFI_IP_NOT_READY);
    assert(!WiFi_TcpConnect("192.0.2.10", 80U));
}

static void TestTcpConnectFailureDiagnostics(void)
{
    WiFiTcpOperationResult result;
    char oversized_response[160];
    const char *detail_start;
    const char *detail_end;
    size_t index;

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("connect FAIL\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: CONNECT_FAILURE\r\n") != NULL);
    assert(strstr(g_log, "WIFI TCP DETAIL: connect FAIL\r\n") != NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("ALREADY CONNECTED\r\nERROR\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: ALREADY_CONNECTED\r\n") != NULL);
    assert(strstr(g_log, "WIFI TCP DETAIL: ALREADY CONNECTED\r\n") != NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("ERROR\x01\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: AT_ERROR\r\n") != NULL);
    assert(strstr(g_log, "WIFI TCP DETAIL: ERROR\r\n") != NULL);
    assert(strchr(g_log, '\x01') == NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("connection to 192.0.2.10 failed: ERROR\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DETAIL: connection to <redacted> failed: ERROR\r\n") != NULL);
    assert(strstr(g_log, "192.0.2.10") == NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("ERR CODE:0x010A0000\r\nERROR\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DETAIL: ERR CODE:0x010A0000\r\n") != NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    for (index = 0U; index < 130U; index++) {
        oversized_response[index] = 'x';
    }
    memcpy(&oversized_response[130], " ERROR\r\n", 8U);
    oversized_response[138] = '\0';
    FeedResponse(oversized_response);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    detail_start = strstr(g_log, "WIFI TCP DETAIL: ");
    assert(detail_start != NULL);
    detail_end = strstr(detail_start, "\r\n");
    assert(detail_end != NULL);
    assert((size_t)(detail_end - detail_start) <= 114U);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    g_tick += 20000U;
    WiFi_Process();
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: TIMEOUT_NO_RESPONSE\r\n") != NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("module-specific detail\r\n");
    assert(!WiFi_TakeTcpOperationResult(&result));
    g_tick += 20000U;
    WiFi_Process();
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: TIMEOUT_WITH_RESPONSE\r\n") != NULL);
    assert(strstr(g_log, "module-specific detail") == NULL);

    ResetFake();
    PrepareTcpConnectReady();
    assert(WiFi_TcpConnect("192.0.2.10", 80U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    SendCommand("AT+CIPSTART=\"TCP\",\"192.0.2.10\",80\r\n");
    FeedResponse("DNS Fail\r\n");
    assert(!WiFi_TakeTcpOperationResult(&result));
    g_tick += 20000U;
    WiFi_Process();
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    assert(strstr(g_log, "WIFI TCP DIAG: DNS_FAILURE\r\n") != NULL);
    assert(strstr(g_log, "WIFI TCP DETAIL: DNS Fail\r\n") != NULL);
}

static void TestPayloadTimeout(void)
{
    WiFiRuntimeInfo details;
    WiFiTcpOperationResult result;
    size_t history_count;
    char command[32];
    size_t length = 5U;
    char status[224];

    ResetFake();
    WiFi_Init();
    Step("AT\r\n", "OK\r\n");
    Step("AT+CWMODE?\r\n", "+CWMODE:3\r\nOK\r\n");
    Step("AT+CIPSTATUS\r\n", "STATUS:4\r\nOK\r\n");
    Step("AT+CWSTATE?\r\n", "+CWSTATE:2,\"synthetic-network\"\r\nOK\r\n");
    Step("AT+CIPSTA?\r\n", "+CIPSTA:ip:\"192.168.6.100\"\r\nOK\r\n");
    assert(WiFi_RequestTcpMuxModeCheck());
    Step("AT+CIPMUX?\r\n", "+CIPMUX:0\r\nOK\r\n");
    assert(WiFi_TcpConnect("192.0.2.10", 8080U));
    StepTcpNormalModeGuard();
    AssertNoPassthroughModeCommand();
    Step("AT+CIPSTART=\"TCP\",\"192.0.2.10\",8080\r\n", "CONNECT\r\nOK\r\n");
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(WiFi_TcpSend((const uint8_t *)"hello", length));
    snprintf(command, sizeof(command), "AT+CIPSEND=%u\r\n", (unsigned int)length);
    Step(command, "OK\r\n>");
    assert(g_tx_length == length);
    history_count = g_history_count;
    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    g_tick += 9999U;
    WiFi_Process();
    assert(g_abort_count == 0U);
    ++g_tick;
    WiFi_Process();
    assert(g_abort_count == 1U);
    assert(strstr(g_log, "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED") != NULL);
    assert(WiFi_GetRuntimeInfo(&details));
    assert(details.tcp == WIFI_AT_TCP_UNKNOWN);
    assert(WiFi_TakeTcpOperationResult(&result));
    assert(result == WIFI_TCP_OPERATION_FAILED);
    /* A partial CIPSEND may still consume AT commands as payload. */
    assert(!WiFi_RequestStatusCheck());
    assert(!WiFi_RequestModuleVersionCheck());
    WiFi_UART_TxComplete();
    FeedResponse("SEND OK\r\nWIFI DISCONNECT\r\n");
    assert(WiFi_ClearCredentials() == WIFI_CLEAR_LOCAL_ONLY);
    g_tick += 120000U;
    WiFi_Process();
    assert(WiFi_FormatStatus(status, sizeof(status)));
    assert(g_history_count == history_count);
    assert(g_abort_count == 1U);
}

static void TestCommandTransmitTimeoutStopsStateMachine(void)
{
    ResetFake();
    WiFi_Init();
    WiFi_Process();
    assert(strcmp(g_tx, "AT\r\n") == 0);
    g_tick += 1200U;
    WiFi_Process();
    assert(g_abort_count == 1U);
    assert(WiFi_GetStatus() == WIFI_STATUS_UNKNOWN);
    assert(strstr(g_log, "TX_TIMEOUT") != NULL);
    WiFi_Process();
    assert(g_history_count == 1U);
}

static void TestReceiveRearmRetriesInMainLoop(void)
{
    ResetFake();
    g_rx_rearm_failures = 1U;
    WiFi_Init();
    assert(g_rx_target == NULL);
    WiFi_Process();
    assert(g_rx_target != NULL);
}

static void TestClearWithoutDialectIsLocalOnly(void)
{
    ResetFake();
    WiFi_Init();
    assert(WiFi_ClearCredentials() == WIFI_CLEAR_LOCAL_ONLY);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_UNKNOWN);
    assert(g_history_count == 0U);
}

static void TestQueuedClearReportsTransmitFailure(void)
{
    ResetFake();
    StartModernDisconnected(3U);
    assert(WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED);
    g_tx_start_failures = 1U;
    WiFi_Process();
    assert(strstr(g_log, "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED") != NULL);
    assert(strstr(g_log, "WIFI CLEAR: DISCONNECT_REQUESTED") == NULL);
    assert(WiFi_GetTcpStatus() == WIFI_AT_TCP_UNKNOWN);
}

int main(void)
{
    TestModernTakeover();
    TestModernRejectedStateQueryFallsBackToCwjap();
    TestLegacyTakeover();
    TestExplicitConnectPreemptsSyncRetryWait();
    TestConnectStillRejectsActiveSyncAndConnectedTcp();
    TestConnectRejectReasonsAreSpecific();
    TestDisconnectedModeAndExplicitConnect();
    TestPersistentConnectModernAndClearRetainsSavedProfile();
    TestPersistentSaveAcceptedWhileStationAlreadyConnected();
    TestPersistentConnectLegacyUsesDefaultCommands();
    TestPersistentConnectRestoresRamModeOnJoinFailure();
    TestPersistentConnectStopsIfRamModeRestoreFails();
    TestPersistentConnectClearRestoresRamModeBeforeDisconnect();
    TestExplicitConnectRestoresApStaAfterStartupFailure();
    TestExplicitConnectStopsWhenApStaUnavailable();
    TestConnectReportsVolatileStorageTerminalError();
    TestGmrIdentifiesNonosAndSkipsVolatileStorageCommand();
    TestModernJoinFailureReportsOnlyNumericCode();
    TestLegacyJoinFailureReportsOnlyNumericCode();
    TestMalformedJoinErrorCodeIsNotReported();
    TestJoinTimeoutReportsFixedTimeout();
    TestUnknownGmrStopsBeforeAnyJoinCommand();
    TestModernGmrProbeFailureDoesNotUseLegacyCommands();
    TestConnectReportsVolatileStorageTimeout();
    TestModuleVersionCheckPreservesRetryCredentials();
    TestUnknownIsNotAssumedDisconnected();
    TestUnknownStationResponseReportsOnlySanitizedShape();
    TestApStaOnlyChangesAfterTcpProvedIdle();
    TestLegacyApStaModeUsesVolatileCommand();
    TestUnknownTcpPreventsModeChange();
    TestRejectedApStaModeRemainsUnknown();
    TestMalformedIpStaysUnknownAndNetworkProbeIsNotRun();
    TestLegacyIpQueryRetriesAfterDhcpNotReady();
    TestIpQueryTimeoutRetriesAreBounded();
    TestIpQueryRetryStopsAfterDisconnect();
    TestAutomaticNetworkProbeIsDisabled();
    TestLiveWifiStatusCheckRefreshesStationAndIp();
    TestLiveWifiStatusCheckClearsStaleConnection();
    TestTcpStatusStationLossInvalidatesCachedNetwork();
    TestTcpClientOperations();
    TestTcpMuxModeQueryIsReadOnly();
    TestTcpModeQueryIsReadOnly();
    TestTcpConnectRequiresVerifiedNormalMode();
    TestDnsGatewayUseIsExplicitAndVerified();
    TestWifiPingGateway();
    TestTcpConnectDiscoversMuxModeBeforeCipstart();
    TestTcpConnectRejectedUntilStationIpIsReady();
    TestTcpConnectFailureDiagnostics();
    TestPayloadTimeout();
    TestCommandTransmitTimeoutStopsStateMachine();
    TestReceiveRearmRetriesInMainLoop();
    TestClearWithoutDialectIsLocalOnly();
    TestQueuedClearReportsTransmitFailure();
    return 0;
}
