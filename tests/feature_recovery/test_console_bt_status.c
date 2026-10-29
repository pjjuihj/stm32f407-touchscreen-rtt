#include "console.h"
#include "bluetooth.h"
#include "gui_driver.h"
#include "log.h"
#include "usart.h"
#include "wifi.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

GPIO_TypeDef test_usart1_instance;
UART_HandleTypeDef huart1 = { .Instance = USART1 };

static uint8_t *g_rx_target;
static bool g_bt_connected;
static bool g_wifi_version_check_accepted;
static bool g_wifi_version_check_requested;
static bool g_wifi_ping_accepted;
static bool g_wifi_ping_requested;
static char g_wifi_ping_host[WIFI_TCP_HOST_MAX_LENGTH + 1U];
static bool g_wifi_status_check_accepted;
static bool g_wifi_status_check_requested;
static bool g_wifi_status_result_ready;
static WiFiStatus g_wifi_status_result;
static bool g_wifi_tcp_mux_check_accepted;
static bool g_wifi_tcp_mux_check_requested;
static bool g_wifi_ip_format_success;
static const char g_wifi_ip_text[] = "WIFI IP: 192.168.137.42\r\n";
static const char g_wifi_status_text[] =
    "WIFI STATUS: UPSTREAM_LOST; CREDENTIALS=loaded; RETRY_IN=5; MODE=APSTA; "
    "APSTA=YES; STA=DISCONNECTED; IP=NOT_READY; TCP=DISCONNECTED; NET=NOT_RUN\r\n";
static WiFiClearResult g_wifi_clear_result;
static char g_reply[512];
static unsigned int g_rearm_failures;
static unsigned int g_receive_attempts;
uint32_t HAL_GetTick(void) { return 0U; }

const char *const LogModuleNames[LOG_MODULE_MAX] = {"TOUCH", "LCD", "SYSTEM"};
LogConfig g_logConfig;

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                     uint16_t length)
{
    assert(uart == &huart1);
    assert(length == 1U);
    g_receive_attempts++;
    if (g_rearm_failures != 0U) {
        g_rearm_failures--;
        return HAL_ERROR;
    }
    g_rx_target = data;
    return HAL_OK;
}

void USART1_SendReply(const char *text)
{
    size_t used = strlen(g_reply);
    size_t available = sizeof(g_reply) - used - 1U;
    size_t length = strlen(text);
    if (length > available) {
        length = available;
    }
    memcpy(g_reply + used, text, length);
    g_reply[used + length] = '\0';
}

bool BT_IsConnected(void) { return g_bt_connected; }
void USART1_SendString(const char *text) { (void)text; }
void Log_Enable(LogModule module, bool enabled) { (void)module; (void)enabled; }
void Log_SetLevel(LogModule module, LogLevel level) { (void)module; (void)level; }
void Log_Write(LogModule module, LogLevel level, const char *format, ...)
{ (void)module; (void)level; (void)format; }
void Log_Dump(void) {}
void Log_Clear(void) {}
void Test_RunAll(void) {}
void LCD_Init(void) {}
void Touch_Init(void) {}
void *lv_screen_active(void) { return NULL; }
void lv_obj_invalidate(void *object) { (void)object; }
void lv_refr_now(void *display) { (void)display; }
void gui_log_set_output_mode(LogOutputMode mode) { (void)mode; }
void gui_diag_get_snapshot(gui_diag_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
}

bool WiFi_FormatStatus(char *output, size_t capacity)
{
    size_t length = strlen(g_wifi_status_text);
    if (output == NULL || capacity <= length) {
        return false;
    }
    memcpy(output, g_wifi_status_text, length + 1U);
    return true;
}
bool WiFi_FormatIp(char *output, size_t capacity)
{
    size_t length = strlen(g_wifi_ip_text);
    if (!g_wifi_ip_format_success || output == NULL || capacity <= length) {
        return false;
    }
    memcpy(output, g_wifi_ip_text, length + 1U);
    return true;
}
bool WiFi_RequestModuleVersionCheck(void)
{
    g_wifi_version_check_requested = true;
    return g_wifi_version_check_accepted;
}
bool WiFi_RequestStatusCheck(void)
{
    g_wifi_status_check_requested = true;
    return g_wifi_status_check_accepted;
}
bool WiFi_TakeStatusCheckResult(WiFiStatus *status)
{
    if (status == NULL || !g_wifi_status_result_ready) {
        return false;
    }
    g_wifi_status_result_ready = false;
    *status = g_wifi_status_result;
    return true;
}
bool WiFi_ConnectSpec(const char *specification) { (void)specification; return false; }
const char *WiFi_GetLastConnectRejectReason(void) { return "WIFI_STATE_MACHINE_BUSY"; }
bool WiFi_RequestDnsStatusCheck(void) { return false; }
bool WiFi_RequestDnsGatewayUse(void) { return false; }
bool WiFi_Ping(const char *host)
{
    g_wifi_ping_requested = true;
    if (host == NULL || strlen(host) > WIFI_TCP_HOST_MAX_LENGTH) {
        return false;
    }
    memcpy(g_wifi_ping_host, host, strlen(host) + 1U);
    return g_wifi_ping_accepted;
}
WiFiClearResult WiFi_ClearCredentials(void) { return g_wifi_clear_result; }
bool WiFi_TcpConnect(const char *host, uint16_t port)
{ (void)host; (void)port; return false; }
bool WiFi_TcpSend(const uint8_t *data, size_t length)
{ (void)data; (void)length; return false; }
bool WiFi_TcpClose(void) { return false; }
bool WiFi_RequestTcpStatusCheck(void) { return false; }
bool WiFi_RequestTcpMuxModeCheck(void)
{
    g_wifi_tcp_mux_check_requested = true;
    return g_wifi_tcp_mux_check_accepted;
}
bool WiFi_RequestTcpModeCheck(void) { return false; }

static void SendCommand(const char *command)
{
    size_t index;
    for (index = 0U; command[index] != '\0'; index++) {
        assert(g_rx_target != NULL);
        *g_rx_target = (uint8_t)command[index];
        Console_UART_RxComplete(&huart1);
    }
    *g_rx_target = (uint8_t)'\r';
    Console_UART_RxComplete(&huart1);
    Console_Process();
}

int main(void)
{
    Console_Init();
    g_bt_connected = true;
    SendCommand("BT STATUS");
    assert(strcmp(g_reply, "BT STATUS: CONNECTED\r\n") == 0);
    assert(strstr(g_reply, "SSID") == NULL);

    g_reply[0] = '\0';
    g_bt_connected = false;
    SendCommand("BT STATUS");
    assert(strcmp(g_reply, "BT STATUS: DISCONNECTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_version_check_accepted = true;
    g_wifi_version_check_requested = false;
    SendCommand("WIFI GMR");
    assert(g_wifi_version_check_requested);
    assert(strcmp(g_reply, "WIFI GMR: REQUESTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_version_check_accepted = false;
    g_wifi_version_check_requested = false;
    SendCommand("WIFI GMR");
    assert(g_wifi_version_check_requested);
    assert(strcmp(g_reply, "WIFI GMR: REJECTED\r\n") == 0);

    g_reply[0] = '\0';
    SendCommand("WIFI CONNECT test-ssid|test-password");
    assert(strcmp(g_reply,
                  "WIFI CONNECT: REJECTED; REASON=WIFI_STATE_MACHINE_BUSY\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_ping_accepted = true;
    g_wifi_ping_requested = false;
    g_wifi_ping_host[0] = '\0';
    SendCommand("WIFI PING 192.168.137.1");
    assert(g_wifi_ping_requested);
    assert(strcmp(g_wifi_ping_host, "192.168.137.1") == 0);
    assert(strcmp(g_reply, "WIFI PING: REQUESTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_ping_requested = false;
    SendCommand("WIFI PING 192.168.137.1 extra");
    assert(!g_wifi_ping_requested);
    assert(strcmp(g_reply, "WIFI PING: REJECTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_status_check_accepted = true;
    g_wifi_status_check_requested = false;
    SendCommand("WIFI LIVE STATUS");
    assert(g_wifi_status_check_requested);
    assert(strcmp(g_reply, "WIFI LIVE STATUS: QUERY_REQUESTED\r\n") == 0);
    g_reply[0] = '\0';
    g_wifi_status_result = WIFI_STATUS_UPSTREAM_LOST;
    g_wifi_status_result_ready = true;
    Console_Process();
    assert(strcmp(g_reply, g_wifi_status_text) == 0);

    g_reply[0] = '\0';
    g_wifi_status_check_accepted = false;
    g_wifi_status_check_requested = false;
    SendCommand("WIFI LIVE STATUS");
    assert(g_wifi_status_check_requested);
    assert(strcmp(g_reply, "WIFI LIVE STATUS: QUERY_REJECTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_ip_format_success = true;
    SendCommand("WIFI IP");
    assert(strcmp(g_reply, g_wifi_ip_text) == 0);
    g_reply[0] = '\0';
    g_wifi_ip_format_success = false;
    SendCommand("WIFI IP");
    assert(strcmp(g_reply, "WIFI IP: FORMAT_ERROR\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_tcp_mux_check_accepted = true;
    g_wifi_tcp_mux_check_requested = false;
    SendCommand("WIFI TCP MUX");
    assert(g_wifi_tcp_mux_check_requested);
    assert(strcmp(g_reply, "WIFI TCP MUX: QUERY_REQUESTED\r\n") == 0);

    g_reply[0] = '\0';
    g_wifi_tcp_mux_check_accepted = false;
    g_wifi_tcp_mux_check_requested = false;
    SendCommand("WIFI TCP MUX");
    assert(g_wifi_tcp_mux_check_requested);
    assert(strcmp(g_reply, "WIFI TCP MUX: QUERY_REJECTED\r\n") == 0);

    g_rearm_failures = 1U;
    *g_rx_target = (uint8_t)'X';
    Console_UART_RxComplete(&huart1);
    {
        unsigned int attempts = g_receive_attempts;
        Console_Process();
        assert(g_receive_attempts == attempts + 1U);
    }
    *g_rx_target = (uint8_t)'\r';
    Console_UART_RxComplete(&huart1);
    Console_Process();
    g_reply[0] = '\0';
    g_wifi_clear_result = WIFI_CLEAR_LOCAL_ONLY;
    SendCommand("WIFI CLEAR");
    assert(strcmp(g_reply, "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_NOT_REQUESTED\r\n") == 0);
    g_reply[0] = '\0';
    g_wifi_clear_result = WIFI_CLEAR_DISCONNECT_QUEUED;
    SendCommand("WIFI CLEAR");
    assert(strcmp(g_reply, "WIFI CLEAR: LOCAL_CLEARED; MODULE_DISCONNECT_QUEUED\r\n") == 0);
    return 0;
}
