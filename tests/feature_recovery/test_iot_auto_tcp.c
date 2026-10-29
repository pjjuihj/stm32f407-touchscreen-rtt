#include "iot.h"
#include "board_io.h"
#include "bluetooth.h"
#include "wifi.h"

#include <assert.h>
#include <string.h>

GPIO_TypeDef test_gpiof;
GPIO_TypeDef test_uart4_instance;
GPIO_TypeDef test_usart3_instance;

static uint32_t g_tick;
static bool g_wifi_connected;
static WiFiStatus g_wifi_status;
static WiFiRuntimeInfo g_runtime;
static unsigned int g_status_requests;
static bool g_status_result_ready;
static WiFiStatus g_status_result;
static unsigned int g_tcp_status_requests;
static bool g_tcp_status_result_ready;
static WiFiAtTcpStatus g_tcp_status_result;
static unsigned int g_tcp_connect_calls;
static unsigned int g_tcp_send_calls;
static unsigned int g_tcp_close_calls;
static bool g_tcp_operation_result_ready;
static WiFiTcpOperationResult g_tcp_operation_result;

static void SetRuntime(WiFiStatus status, WiFiAtStationStatus station,
                       WiFiIpStatus ip, WiFiAtTcpStatus tcp)
{
    g_wifi_status = status;
    g_wifi_connected = status == WIFI_STATUS_CONNECTED;
    g_runtime.mode_known = 1U;
    g_runtime.mode = 3U;
    g_runtime.apsta = WIFI_APSTA_YES;
    g_runtime.station = station;
    g_runtime.ip = ip;
    g_runtime.tcp = tcp;
    g_runtime.network = WIFI_NETWORK_NOT_RUN;
}

static void PublishWifiStatus(WiFiStatus status, WiFiAtStationStatus station,
                              WiFiIpStatus ip, WiFiAtTcpStatus tcp)
{
    SetRuntime(status, station, ip, tcp);
    g_status_result = status;
    g_status_result_ready = true;
}

static void PublishTcpStatus(WiFiAtTcpStatus status)
{
    g_runtime.tcp = status;
    g_tcp_status_result = status;
    g_tcp_status_result_ready = true;
}

static void PublishTcpOperation(WiFiTcpOperationResult result)
{
    g_tcp_operation_result = result;
    g_tcp_operation_result_ready = true;
}

void LED_Set(uint8_t led, bool enabled) { (void)led; (void)enabled; }
bool LED_Get(uint8_t led) { (void)led; return false; }
void BEEP_Set(bool enabled) { (void)enabled; }
bool BEEP_Get(void) { return false; }
uint32_t HAL_GetTick(void) { return g_tick; }
void BT_Init(void) {}
bool BT_Connect(void) { return false; }
void BT_Disconnect(void) {}
bool BT_IsConnected(void) { return false; }
void BT_Process(void) {}
void BT_SetRxCallback(BT_RxCallback callback) { (void)callback; }
void USART1_SendReply(const char *text) { (void)text; }

void WiFi_Init(void) {}
void WiFi_Process(void) {}
bool WiFi_IsConnected(void) { return g_wifi_connected; }
WiFiStatus WiFi_GetStatus(void) { return g_wifi_status; }
WiFiAtTcpStatus WiFi_GetTcpStatus(void) { return g_runtime.tcp; }
bool WiFi_GetRuntimeInfo(WiFiRuntimeInfo *info)
{
    if (info == NULL) {
        return false;
    }
    *info = g_runtime;
    return true;
}
bool WiFi_RequestStatusCheck(void)
{
    g_status_requests++;
    return true;
}
bool WiFi_TakeStatusCheckResult(WiFiStatus *status)
{
    if (!g_status_result_ready || status == NULL) {
        return false;
    }
    *status = g_status_result;
    g_status_result_ready = false;
    return true;
}
bool WiFi_RequestTcpStatusCheck(void)
{
    g_tcp_status_requests++;
    return true;
}
bool WiFi_TakeTcpStatusCheckResult(WiFiAtTcpStatus *status)
{
    if (!g_tcp_status_result_ready || status == NULL) {
        return false;
    }
    *status = g_tcp_status_result;
    g_tcp_status_result_ready = false;
    return true;
}
bool WiFi_TcpConnect(const char *host, uint16_t port)
{
    assert(host != NULL && strcmp(host, "192.168.137.1") == 0);
    assert(port == 43117U);
    g_tcp_connect_calls++;
    return true;
}
bool WiFi_TcpSend(const uint8_t *data, size_t length)
{
    static const uint8_t expected[] = "PING\r\n";
    assert(data != NULL && length == sizeof(expected) - 1U);
    assert(memcmp(data, expected, sizeof(expected) - 1U) == 0);
    g_tcp_send_calls++;
    PublishTcpOperation(WIFI_TCP_OPERATION_SUCCEEDED);
    return true;
}
bool WiFi_TcpClose(void)
{
    g_tcp_close_calls++;
    PublishTcpOperation(WIFI_TCP_OPERATION_SUCCEEDED);
    return true;
}
bool WiFi_TakeTcpOperationResult(WiFiTcpOperationResult *result)
{
    if (!g_tcp_operation_result_ready || result == NULL) {
        return false;
    }
    *result = g_tcp_operation_result;
    g_tcp_operation_result_ready = false;
    return true;
}

static void Step(void)
{
    IoT_Process();
}

static void ResetFakes(void)
{
    g_tick = 0U;
    g_wifi_connected = false;
    g_wifi_status = WIFI_STATUS_UNKNOWN;
    memset(&g_runtime, 0, sizeof(g_runtime));
    g_runtime.station = WIFI_AT_STATION_UNKNOWN;
    g_runtime.ip = WIFI_IP_UNKNOWN;
    g_runtime.tcp = WIFI_AT_TCP_UNKNOWN;
    g_status_requests = 0U;
    g_status_result_ready = false;
    g_status_result = WIFI_STATUS_UNKNOWN;
    g_tcp_status_requests = 0U;
    g_tcp_status_result_ready = false;
    g_tcp_status_result = WIFI_AT_TCP_UNKNOWN;
    g_tcp_connect_calls = 0U;
    g_tcp_send_calls = 0U;
    g_tcp_close_calls = 0U;
    g_tcp_operation_result_ready = false;
    g_tcp_operation_result = WIFI_TCP_OPERATION_IDLE;
}

static void TestAutoTcpRefreshesStationAndIpAfterFailure(void)
{
    ResetFakes();
    IoT_Init(NULL);

    Step();
    assert(g_status_requests == 1U);
    assert(g_tcp_connect_calls == 0U);

    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_READY, WIFI_AT_TCP_UNKNOWN);
    Step();
    Step();
    assert(g_tcp_status_requests == 1U);
    assert(g_tcp_connect_calls == 0U);

    PublishTcpStatus(WIFI_AT_TCP_DISCONNECTED);
    Step();
    assert(g_tcp_connect_calls == 1U);

    PublishTcpOperation(WIFI_TCP_OPERATION_FAILED);
    Step();
    Step();
    assert(g_status_requests == 1U);
    assert(g_tcp_status_requests == 1U);

    g_tick += 1000U;
    Step();
    assert(g_status_requests == 2U);
    PublishWifiStatus(WIFI_STATUS_RETRY_WAIT, WIFI_AT_STATION_DISCONNECTED,
                      WIFI_IP_NOT_READY, WIFI_AT_TCP_UNKNOWN);
    Step();
    assert(g_tcp_connect_calls == 1U);
    assert(g_tcp_status_requests == 1U);

    g_tick += 60000U;
    Step();
    assert(g_status_requests == 3U);
    assert(g_tcp_connect_calls == 1U);

    PublishWifiStatus(WIFI_STATUS_CONNECTED, WIFI_AT_STATION_CONNECTED,
                      WIFI_IP_READY, WIFI_AT_TCP_UNKNOWN);
    Step();
    Step();
    assert(g_tcp_status_requests == 2U);
    assert(g_tcp_connect_calls == 1U);

    PublishTcpStatus(WIFI_AT_TCP_DISCONNECTED);
    Step();
    assert(g_tcp_connect_calls == 2U);

    PublishTcpOperation(WIFI_TCP_OPERATION_SUCCEEDED);
    Step();
    assert(g_tcp_send_calls == 1U);
    Step();
    assert(g_tcp_close_calls == 1U);
    Step();
    g_tick += 60000U;
    Step();
    assert(g_tcp_connect_calls == 2U);
    assert(g_tcp_send_calls == 1U);
    assert(g_tcp_close_calls == 1U);
}

int main(void)
{
    TestAutoTcpRefreshesStationAndIpAfterFailure();
    return 0;
}
