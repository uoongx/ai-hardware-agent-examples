#ifndef INCLUDE_WEB_SERVER_H
#define INCLUDE_WEB_SERVER_H
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/netifapi.h"
#include "service_manager.h"

#define WEB_PORT 80
#define MAX_FORM_DATA 512
#define FORM2_MAX_SIZE 800
#define MAX_LINE 20
#define MAX_LINE_LEN 32
typedef struct {
    char network_name[32];
    char password[32];
    char device_name[32];
    char background[256];
    char cloud_model[32];
} ConfigInfo;

typedef struct AccountList_t
{
    char ssid[WIFI_MAX_SSID_LEN*2]; 
}AccountList_t;

void web_server_start(void);
#endif
