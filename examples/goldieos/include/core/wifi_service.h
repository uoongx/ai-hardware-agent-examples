#ifndef INCLUDE_WIFI_SERVICE_H
#define INCLUDE_WIFI_SERVICE_H
#include "driver_manager.h"

#define MAX_CONFIG_AP_NUM 15
#define WIFI_CFG_FILE "0:/wificfg.txt"
#define SOFTAP_CFG_FILE "0:/apcfg.txt"

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_CHECK_INTERVAL_MS 1000
#define WIFI_EVENT_QUEUE_SIZE 10

enum WIFI_MODE_INDEX{
	WIFI_MODE_STA_INDEX =0,
	WIFI_MODE_AP_INDEX,
	MAX_WIFI_MODE_INDEX,
};

// ����WiFi�¼�����
typedef enum {
    WIFI_EVENT_NONE = 0,
    WIFI_EVENT_STA_ENABLE,
    WIFI_EVENT_STA_DISABLE,
    WIFI_EVENT_STA_SCAN,
    WIFI_EVENT_STA_CONNECT,
    WIFI_EVENT_STA_DISCONNECT,
    WIFI_EVENT_SOFTAP_ENABLE,
    WIFI_EVENT_SOFTAP_DISABLE,
    WIFI_EVENT_SOFTAP_CONFIG,
    WIFI_EVENT_MAX
} WifiEventType;


// ����WiFi�¼�����
typedef enum {
    WIFI_STATUS_NONE = 0,
    WIFI_STATUS_STA_ENABLED,    
    WIFI_STATUS_STA_DISABLED,
    WIFI_STATUS_STA_CONNECTED,
    WIFI_STATUS_STA_DISCONNECTED,
    WIFI_STATUS_STA_SCANDONE,
    WIFI_STATUS_SOFTAP_ENABLED,
    WIFI_STATUS_SOFTAP_DISABLED,
    WIFI_STATUS_MAX
} WifiStatus;

// WiFi�¼��ṹ
typedef struct {
    WifiEventType type;
    void* data;
} WifiEvent;

// �ȵ������¼�����
typedef struct {
    char ssid[WIFI_MAX_SSID_LEN];
    char passwd[WIFI_MAX_KEY_LEN];
} WifiSoftApConfigData;

// �ص���������
typedef void (*WifiEventCallback)(int event, void* data);

typedef struct {
    char ssid[WIFI_MAX_SSID_LEN];
    char passwd[WIFI_MAX_KEY_LEN];
} WifiConfig;

typedef struct WifiService {
	int (*svr_sta_isEnabled)(void);
	int (*svr_sta_isConnected)(void);
	int (*svr_sta_isScanDone)(void);
	char* (*svr_sta_connection_name)(void);
	int (*svr_sta_get_ap_list)(ApInfo_t*);
	int (*svr_sta_add_account)(char*,char*);
	int (*svr_sta_remove_account)(char*);
	int (*svr_softap_config)(char*,char*);
	int (*svr_softap_get_config)(WifiConfig*);
	int (*svr_softap_isEnabled)(void);
	void (*svr_net_components_setup)(void);
	int (*svr_get_netddr)(char*,int);
	int (*register_callback)(WifiEventCallback);
	int (*trigger_event)(WifiEventType, void*);
	int (*svr_check_ap_config)(char*,char*);
	int (*svr_get_hwddr)(uint8_t*,int);
	int (*svr_sta_try_connect)(void);
} WifiService;


#endif
