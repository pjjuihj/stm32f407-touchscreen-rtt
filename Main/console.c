#include "console.h"

#include "gui_driver.h"
#include "log.h"
#include "test_log.h"
#include "usart.h"
#include "usart1_rx_recovery.h"
#include "bluetooth.h"
#include "wifi.h"
#include "lcd.h"
#include "touch.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CONSOLE_RX_CAPACITY 256U
#define CONSOLE_COMMAND_CAPACITY 160U
#define CONSOLE_RX_BUDGET 64U

static uint8_t g_rx_byte;
static uint8_t g_rx_data[CONSOLE_RX_CAPACITY];
static volatile uint8_t g_rx_head;
static volatile uint8_t g_rx_tail;
static volatile uint8_t g_rx_overflow;
static volatile uint8_t g_reset_partial_command;
static volatile uint8_t g_rx_rearm_pending;
static USART1RxRecovery g_rx_recovery;
static char g_command[CONSOLE_COMMAND_CAPACITY];
static size_t g_command_length;
static uint8_t g_command_overflow;
static uint8_t g_consume_lf;
static uint8_t g_wifi_live_status_pending;

static void SecureClear(void *buffer, size_t length)
{
    volatile uint8_t *byte = (volatile uint8_t *)buffer;
    while (length-- > 0U) {
        *byte++ = 0U;
    }
}

static void Reply(const char *text)
{
    USART1_SendReply(text);
}

static void PollWifiLiveStatusResult(void)
{
    WiFiStatus status;
    char response[224];

    if (g_wifi_live_status_pending == 0U ||
        !WiFi_TakeStatusCheckResult(&status)) {
        return;
    }
    g_wifi_live_status_pending = 0U;
    (void)status;
    if (WiFi_FormatStatus(response, sizeof(response))) {
        Reply(response);
    } else {
        Reply("WIFI LIVE STATUS: RESULT_FORMAT_ERROR\r\n");
    }
    SecureClear(response, sizeof(response));
}

static void ShowLogStatus(void)
{
    int module;
    Reply("\r\n--- Log Status ---\r\n");
    for (module = 0; module < LOG_MODULE_MAX; module++) {
        char line[48];
        (void)snprintf(line, sizeof(line), "%s: %s\r\n", LogModuleNames[module],
                       g_logConfig.enabled[module] ? "ON" : "OFF");
        Reply(line);
    }
}

static void SetLevel(const char *command, const char *module, LogModule log_module)
{
    unsigned int level;
    char expected[32];
    static const char *const names[] = {"", "ERROR", "WARNING", "INFO", "DEBUG"};

    for (level = 1U; level <= 4U; level++) {
        (void)snprintf(expected, sizeof(expected), "LEVEL %s %u", module, level);
        if (strcmp(command, expected) == 0) {
            Log_SetLevel(log_module, (LogLevel)level);
            (void)snprintf(expected, sizeof(expected), "\r\n%s level: %s\r\n",
                           module, names[level]);
            Reply(expected);
            return;
        }
    }
}

static bool HandleLevel(const char *command)
{
    if (strncmp(command, "LEVEL TOUCH ", 12U) == 0) {
        SetLevel(command, "TOUCH", LOG_MODULE_TOUCH);
    } else if (strncmp(command, "LEVEL LCD ", 10U) == 0) {
        SetLevel(command, "LCD", LOG_MODULE_LCD);
    } else if (strncmp(command, "LEVEL SYSTEM ", 13U) == 0) {
        SetLevel(command, "SYSTEM", LOG_MODULE_SYSTEM);
    } else {
        return false;
    }
    return true;
}

static void HandleTestCommand(const char *argument)
{
    int index;
    if (strcmp(argument, "RUN") == 0) {
        Test_RunAll();
    } else if (strcmp(argument, "WRITE") == 0) {
        for (index = 0; index < 5; index++) {
            Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Test write %d", index);
        }
        Reply("\r\nTest write complete\r\n");
    } else if (strcmp(argument, "OVERFLOW") == 0) {
        for (index = 0; index < 70; index++) {
            Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "Overflow %d", index);
        }
        Reply("\r\nTest overflow complete\r\n");
    } else if (strcmp(argument, "CLEAR") == 0) {
        Log_Clear();
        Reply("\r\nTest log cleared\r\n");
    } else {
        Reply("\r\nUnknown TEST command. Use: RUN/WRITE/OVERFLOW/CLEAR\r\n");
    }
}

static void HandleHelp(void)
{
    Reply("\r\n--- Commands ---\r\n");
    Reply("STATUS; TOUCH/LCD/SYSTEM ON|OFF; LCD REINIT; LEVEL <MOD> <1-4>\r\n");
    Reply("GUI DIAG; DUMP; CLEAR; LOGMODE RTT|UART; TEST RUN|WRITE|OVERFLOW|CLEAR\r\n");
    Reply("WIFI STATUS; WIFI LIVE STATUS; WIFI IP; WIFI DNS STATUS; WIFI DNS USE GATEWAY; WIFI GMR; BT STATUS; WIFI CONNECT <ssid>|<password>; WIFI CONNECT SAVE <ssid>|<password>; WIFI CLEAR; HELP\r\n");
    Reply("WIFI CONNECT SAVE stores credentials in ESP flash and enables boot auto-connect; WIFI CLEAR does not erase that saved profile.\r\n");
    Reply("WIFI PING <host>; WIFI TCP CONNECT <host> <port>; WIFI TCP SEND <text>; WIFI TCP CLOSE; WIFI TCP STATUS; WIFI TCP MUX; WIFI TCP MODE\r\n");
}

static bool HandleWifiPing(const char *arguments)
{
    char host[WIFI_TCP_HOST_MAX_LENGTH + 1U];
    const char *end;
    size_t host_length;

    if (arguments == NULL) {
        return false;
    }
    while (*arguments == ' ') {
        arguments++;
    }
    end = arguments;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    host_length = (size_t)(end - arguments);
    if (host_length == 0U || host_length > WIFI_TCP_HOST_MAX_LENGTH) {
        return false;
    }
    while (*end == ' ') {
        end++;
    }
    if (*end != '\0') {
        return false;
    }
    memcpy(host, arguments, host_length);
    host[host_length] = '\0';
    {
        bool accepted = WiFi_Ping(host);
        SecureClear(host, sizeof(host));
        return accepted;
    }
}

static bool HandleWifiTcpConnect(const char *arguments)
{
    char host[WIFI_TCP_HOST_MAX_LENGTH + 1U];
    const char *separator;
    const char *port_text;
    size_t host_length;
    unsigned int port = 0U;
    bool accepted = false;

    if (arguments == NULL) {
        return false;
    }
    separator = strchr(arguments, ' ');
    if (separator == NULL) {
        return false;
    }
    host_length = (size_t)(separator - arguments);
    if (host_length == 0U || host_length > WIFI_TCP_HOST_MAX_LENGTH) {
        return false;
    }
    memcpy(host, arguments, host_length);
    host[host_length] = '\0';

    port_text = separator + 1;
    while (*port_text == ' ') {
        port_text++;
    }
    if (*port_text == '\0') {
        SecureClear(host, sizeof(host));
        return false;
    }
    while (*port_text >= '0' && *port_text <= '9') {
        unsigned int digit = (unsigned int)(*port_text - '0');
        if (port > (65535U - digit) / 10U) {
            SecureClear(host, sizeof(host));
            return false;
        }
        port = port * 10U + digit;
        port_text++;
    }
    while (*port_text == ' ') {
        port_text++;
    }
    if (*port_text == '\0' && port > 0U) {
        accepted = WiFi_TcpConnect(host, (uint16_t)port);
    }
    SecureClear(host, sizeof(host));
    return accepted;
}

static bool IsPrintableTcpText(const char *text, size_t length)
{
    size_t index;
    if (text == NULL || length == 0U || length > WIFI_TCP_MAX_PAYLOAD_LENGTH) {
        return false;
    }
    for (index = 0U; index < length; index++) {
        unsigned char ch = (unsigned char)text[index];
        if (ch < 0x20U || ch > 0x7eU) {
            return false;
        }
    }
    return true;
}

static void ProcessCommand(char *command)
{
    char response[224];

    if (strcmp(command, "STATUS") == 0) {
        ShowLogStatus();
    } else if (strcmp(command, "GUI DIAG") == 0) {
        gui_diag_snapshot_t diag;
        uint32_t now = HAL_GetTick();
        int diag_length;

        gui_diag_get_snapshot(&diag);
        diag_length = snprintf(
            response, sizeof(response),
            "GUI DIAG: LOOP=%lu AGE_MS=%lu; FLUSH=%lu/%lu AGE_MS=%lu; "
            "TOUCH=%lu/%lu AGE_MS=%lu; AREA=%d,%d-%d,%d PIX=%lu\r\n",
            (unsigned long)diag.loop_completed,
            (unsigned long)(now - diag.last_loop_tick),
            (unsigned long)diag.flush_completed,
            (unsigned long)diag.flush_started,
            (unsigned long)(now - diag.last_flush_tick),
            (unsigned long)diag.touch_completed,
            (unsigned long)diag.touch_started,
            (unsigned long)(now - diag.last_touch_tick),
            (int)diag.last_flush_x1, (int)diag.last_flush_y1,
            (int)diag.last_flush_x2, (int)diag.last_flush_y2,
            (unsigned long)diag.last_flush_pixels);
        if (diag_length > 0 && (size_t)diag_length < sizeof(response)) {
            Reply(response);
        } else {
            Reply("GUI DIAG: FORMAT_ERROR\r\n");
        }
    } else if (strcmp(command, "TOUCH ON") == 0) {
        Log_Enable(LOG_MODULE_TOUCH, true);
        Reply("\r\nTouch log: ON\r\n");
    } else if (strcmp(command, "TOUCH OFF") == 0) {
        Log_Enable(LOG_MODULE_TOUCH, false);
        Reply("\r\nTouch log: OFF\r\n");
    } else if (strcmp(command, "LCD ON") == 0) {
        Log_Enable(LOG_MODULE_LCD, true);
        Reply("\r\nLCD log: ON\r\n");
    } else if (strcmp(command, "LCD OFF") == 0) {
        Log_Enable(LOG_MODULE_LCD, false);
        Reply("\r\nLCD log: OFF\r\n");
    } else if (strcmp(command, "LCD REINIT") == 0) {
        LCD_Init();
        Touch_Init();
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
        Reply("\r\nLCD REINIT: SEQUENCE_COMPLETE; VERIFY_SCREEN\r\n");
    } else if (strcmp(command, "SYSTEM ON") == 0) {
        Log_Enable(LOG_MODULE_SYSTEM, true);
        Reply("\r\nSystem log: ON\r\n");
    } else if (strcmp(command, "SYSTEM OFF") == 0) {
        Log_Enable(LOG_MODULE_SYSTEM, false);
        Reply("\r\nSystem log: OFF\r\n");
    } else if (strncmp(command, "LEVEL TOUCH ", 12U) == 0) {
        (void)HandleLevel(command);
    } else if (strncmp(command, "LEVEL LCD ", 10U) == 0 ||
               strncmp(command, "LEVEL SYSTEM ", 13U) == 0) {
        (void)HandleLevel(command);
    } else if (strcmp(command, "DUMP") == 0) {
        Log_Dump();
    } else if (strcmp(command, "CLEAR") == 0) {
        Log_Clear();
        Reply("\r\nLog cleared\r\n");
    } else if (strncmp(command, "TEST ", 5U) == 0) {
        HandleTestCommand(command + 5U);
    } else if (strcmp(command, "HELP") == 0) {
        HandleHelp();
    } else if (strcmp(command, "LOGMODE RTT") == 0) {
        gui_log_set_output_mode(LOG_OUTPUT_RTT);
        Reply("\r\nLog output: RTT\r\n");
    } else if (strcmp(command, "LOGMODE UART") == 0) {
        gui_log_set_output_mode(LOG_OUTPUT_UART);
        Reply("\r\nLog output: UART\r\n");
    } else if (strcmp(command, "WIFI STATUS") == 0) {
        if (WiFi_FormatStatus(response, sizeof(response))) {
            Reply(response);
        } else {
            Reply("WIFI STATUS: FORMAT_ERROR\r\n");
        }
    } else if (strcmp(command, "WIFI LIVE STATUS") == 0) {
        if (WiFi_RequestStatusCheck()) {
            g_wifi_live_status_pending = 1U;
            Reply("WIFI LIVE STATUS: QUERY_REQUESTED\r\n");
        } else {
            Reply("WIFI LIVE STATUS: QUERY_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI IP") == 0) {
        if (WiFi_FormatIp(response, sizeof(response))) {
            Reply(response);
        } else {
            Reply("WIFI IP: FORMAT_ERROR\r\n");
        }
    } else if (strcmp(command, "WIFI DNS STATUS") == 0) {
        if (WiFi_RequestDnsStatusCheck()) {
            Reply("WIFI DNS STATUS: QUERY_REQUESTED\r\n");
        } else {
            Reply("WIFI DNS STATUS: QUERY_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI DNS USE GATEWAY") == 0) {
        if (WiFi_RequestDnsGatewayUse()) {
            Reply("WIFI DNS GATEWAY: REQUESTED\r\n");
        } else {
            Reply("WIFI DNS GATEWAY: REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI GMR") == 0) {
        if (WiFi_RequestModuleVersionCheck()) {
            Reply("WIFI GMR: REQUESTED\r\n");
        } else {
            Reply("WIFI GMR: REJECTED\r\n");
        }
    } else if (strcmp(command, "BT STATUS") == 0) {
        Reply(BT_IsConnected() ? "BT STATUS: CONNECTED\r\n" :
                                "BT STATUS: DISCONNECTED\r\n");
    } else if (strncmp(command, "WIFI PING ", 10U) == 0) {
        if (HandleWifiPing(command + 10U)) {
            Reply("WIFI PING: REQUESTED\r\n");
        } else {
            Reply("WIFI PING: REJECTED\r\n");
        }
    } else if (strncmp(command, "WIFI CONNECT ", 13U) == 0) {
        if (WiFi_ConnectSpec(command + 13U)) {
            Reply(strncmp(command + 13U, "SAVE ", 5U) == 0
                      ? "WIFI CONNECT SAVE: REQUESTED\r\n"
                      : "WIFI CONNECT: REQUESTED\r\n");
        } else {
            int response_length = snprintf(
                response, sizeof(response),
                "WIFI CONNECT: REJECTED; REASON=%s\r\n",
                WiFi_GetLastConnectRejectReason());
            if (response_length > 0 && (size_t)response_length < sizeof(response)) {
                Reply(response);
            } else {
                Reply("WIFI CONNECT: REJECTED; REASON=FORMAT_ERROR\r\n");
            }
        }
    } else if (strcmp(command, "WIFI CLEAR") == 0) {
        if (WiFi_ClearCredentials() == WIFI_CLEAR_DISCONNECT_QUEUED) {
            Reply("WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_QUEUED\r\n");
        } else {
            Reply("WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED\r\n");
        }
    } else if (strncmp(command, "WIFI TCP CONNECT ", 17U) == 0) {
        if (HandleWifiTcpConnect(command + 17U)) {
            Reply("WIFI TCP: CONNECT_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP: CONNECT_REJECTED\r\n");
        }
    } else if (strncmp(command, "WIFI TCP SEND ", 14U) == 0) {
        const char *payload = command + 14U;
        size_t payload_length = strlen(payload);
        if (IsPrintableTcpText(payload, payload_length) &&
            WiFi_TcpSend((const uint8_t *)payload, payload_length)) {
            Reply("WIFI TCP: SEND_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP: SEND_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI TCP CLOSE") == 0) {
        if (WiFi_TcpClose()) {
            Reply("WIFI TCP: CLOSE_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP: CLOSE_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI TCP STATUS") == 0) {
        if (WiFi_RequestTcpStatusCheck()) {
            Reply("WIFI TCP STATUS: QUERY_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP STATUS: QUERY_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI TCP MUX") == 0) {
        if (WiFi_RequestTcpMuxModeCheck()) {
            Reply("WIFI TCP MUX: QUERY_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP MUX: QUERY_REJECTED\r\n");
        }
    } else if (strcmp(command, "WIFI TCP MODE") == 0) {
        if (WiFi_RequestTcpModeCheck()) {
            Reply("WIFI TCP MODE: QUERY_REQUESTED\r\n");
        } else {
            Reply("WIFI TCP MODE: QUERY_REJECTED\r\n");
        }
    } else {
        Reply("\r\nUnknown command. Type HELP.\r\n");
    }
    SecureClear(command, CONSOLE_COMMAND_CAPACITY);
}

void Console_Init(void)
{
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_rx_overflow = 0U;
    g_reset_partial_command = 0U;
    g_rx_rearm_pending = 0U;
    g_command_length = 0U;
    g_command_overflow = 0U;
    g_consume_lf = 0U;
    g_wifi_live_status_pending = 0U;
    SecureClear(g_rx_data, sizeof(g_rx_data));
    SecureClear(g_command, sizeof(g_command));
    USART1RxRecovery_Init(&g_rx_recovery);
    if (HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U) != HAL_OK) {
        g_rx_rearm_pending = 1U;
        USART1RxRecovery_ReportRearmFailure(&g_rx_recovery);
    }
}

void Console_UART_RxComplete(UART_HandleTypeDef *uart)
{
    uint8_t next;
    uint8_t byte;

    if (uart == NULL || uart->Instance != USART1) {
        return;
    }
    byte = g_rx_byte;
    g_rx_byte = 0U;
    if (!USART1RxRecovery_ShouldDiscard(&g_rx_recovery, byte)) {
        next = (uint8_t)(g_rx_head + 1U);
        if (next != g_rx_tail) {
            g_rx_data[g_rx_head] = byte;
            g_rx_head = next;
        } else {
            g_rx_overflow = 1U;
        }
    }
    if (HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U) != HAL_OK) {
        g_rx_rearm_pending = 1U;
        USART1RxRecovery_ReportRearmFailure(&g_rx_recovery);
    }
}

void Console_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == NULL || uart->Instance != USART1 ||
        !USART1RxRecovery_HandleError(&g_rx_recovery, true, uart->ErrorCode)) {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(uart);
    g_reset_partial_command = 1U;
    if (HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U) != HAL_OK) {
        g_rx_rearm_pending = 1U;
        USART1RxRecovery_ReportRearmFailure(&g_rx_recovery);
    }
}

static void ProcessInputByte(uint8_t byte)
{
    if (g_consume_lf != 0U) {
        g_consume_lf = 0U;
        if (byte == '\n') {
            return;
        }
    }
    if (byte == '\r' || byte == '\n') {
        if (byte == '\r') {
            g_consume_lf = 1U;
        }
        if (g_command_overflow != 0U) {
            Reply("CONSOLE: COMMAND_TOO_LONG\r\n");
        } else if (g_command_length > 0U) {
            g_command[g_command_length] = '\0';
            ProcessCommand(g_command);
        }
        g_command_length = 0U;
        g_command_overflow = 0U;
        SecureClear(g_command, sizeof(g_command));
        return;
    }
    if (g_command_overflow != 0U) {
        return;
    }
    if (g_command_length + 1U < sizeof(g_command)) {
        g_command[g_command_length++] = (char)byte;
    } else {
        g_command_overflow = 1U;
        SecureClear(g_command, sizeof(g_command));
        g_command_length = 0U;
    }
}

void Console_Process(void)
{
    uint8_t processed = 0U;
    uint8_t events = USART1RxRecovery_TakeEvents(&g_rx_recovery);
    if (g_rx_rearm_pending != 0U) {
        g_rx_rearm_pending = 0U;
        if (HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U) != HAL_OK) {
            g_rx_rearm_pending = 1U;
        }
    }
    PollWifiLiveStatusResult();

    if ((events & USART1_RX_EVENT_RECOVERED) != 0U) {
        Reply("USART1 RX RECOVERED; PARTIAL_COMMAND_DISCARDED\r\n");
    }
    if ((events & USART1_RX_EVENT_REARM_FAILED) != 0U) {
        Reply("USART1 RX REARM FAILED\r\n");
    }
    if (g_reset_partial_command != 0U) {
        g_reset_partial_command = 0U;
        SecureClear(g_command, sizeof(g_command));
        g_command_length = 0U;
        g_command_overflow = 0U;
    }
    if (g_rx_overflow != 0U) {
        g_rx_overflow = 0U;
        SecureClear(g_command, sizeof(g_command));
        g_command_length = 0U;
        g_command_overflow = 1U;
    }
    while (g_rx_tail != g_rx_head && processed < CONSOLE_RX_BUDGET) {
        uint8_t byte = g_rx_data[g_rx_tail];
        g_rx_data[g_rx_tail] = 0U;
        g_rx_tail = (uint8_t)(g_rx_tail + 1U);
        ProcessInputByte(byte);
        processed++;
    }
}
