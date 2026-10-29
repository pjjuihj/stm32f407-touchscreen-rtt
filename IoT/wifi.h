#ifndef WIFI_H
#define WIFI_H

#include "wifi_at.h"
#include "wifi_state.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_TCP_HOST_MAX_LENGTH 63U
#define WIFI_TCP_MAX_PAYLOAD_LENGTH 256U

typedef enum {
    WIFI_TCP_OPERATION_IDLE = 0,
    WIFI_TCP_OPERATION_PENDING,
    WIFI_TCP_OPERATION_SUCCEEDED,
    WIFI_TCP_OPERATION_FAILED
} WiFiTcpOperationResult;

typedef enum {
    WIFI_CLEAR_LOCAL_ONLY = 0,
    WIFI_CLEAR_DISCONNECT_QUEUED = 1
} WiFiClearResult;

typedef enum {
    WIFI_APSTA_UNKNOWN = 0,
    WIFI_APSTA_NO,
    WIFI_APSTA_YES
} WiFiApStaStatus;

typedef enum {
    WIFI_IP_UNKNOWN = 0,
    WIFI_IP_NOT_READY,
    WIFI_IP_READY
} WiFiIpStatus;

typedef enum {
    WIFI_NETWORK_NOT_RUN = 0
} WiFiNetworkStatus;

typedef struct {
    uint8_t mode_known;
    uint8_t mode;
    WiFiApStaStatus apsta;
    WiFiAtStationStatus station;
    WiFiIpStatus ip;
    WiFiAtTcpStatus tcp;
    WiFiNetworkStatus network;
} WiFiRuntimeInfo;

void WiFi_Init(void);
void WiFi_Process(void);
bool WiFi_RequestConnect(const char *ssid, const char *password);
bool WiFi_Connect(const char *ssid, const char *password);
bool WiFi_ConnectSpec(const char *specification);
const char *WiFi_GetLastConnectRejectReason(void);
bool WiFi_RequestStatusCheck(void);
bool WiFi_TakeStatusCheckResult(WiFiStatus *status);
bool WiFi_RequestTcpStatusCheck(void);
bool WiFi_TakeTcpStatusCheckResult(WiFiAtTcpStatus *status);
bool WiFi_RequestTcpMuxModeCheck(void);
bool WiFi_RequestTcpModeCheck(void);
bool WiFi_RequestDnsStatusCheck(void);
bool WiFi_RequestDnsGatewayUse(void);
bool WiFi_RequestModuleVersionCheck(void);
bool WiFi_TcpConnect(const char *host, uint16_t port);
bool WiFi_Ping(const char *host);
bool WiFi_TcpSend(const uint8_t *data, size_t length);
bool WiFi_TcpClose(void);
bool WiFi_TakeTcpOperationResult(WiFiTcpOperationResult *result);
WiFiClearResult WiFi_ClearCredentials(void);
void WiFi_Disconnect(void);
WiFiStatus WiFi_GetStatus(void);
WiFiAtTcpStatus WiFi_GetTcpStatus(void);
bool WiFi_GetRuntimeInfo(WiFiRuntimeInfo *info);
bool WiFi_IsConnected(void);
void WiFi_UART_RxComplete(UART_HandleTypeDef *uart);
void WiFi_UART_TxComplete(void);
void WiFi_UART_ErrorCallback(UART_HandleTypeDef *uart);
bool WiFi_FormatStatus(char *output, size_t capacity);
bool WiFi_FormatIp(char *output, size_t capacity);

#endif
