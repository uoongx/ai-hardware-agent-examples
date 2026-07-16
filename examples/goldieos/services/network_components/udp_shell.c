#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include "udp_shell.h"
// #ifdef GC9D01_SPI_LCD
#include "wifi_service.h"
// #endif

#define DISCOVERY_PORT 5560
static const char* DEVICE_ID = "GOLDIE_DEVICE_001";
static const char* DEVICE_HOSTNAME = "goldie_dev";
static struct udp_pcb *discovery_pcb = NULL;
static void discovery_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                  const ip_addr_t *addr, u16_t port);
static void send_discovery_response(const ip_addr_t *addr, u16_t port);
static void send_response(const ip_addr_t *addr, u16_t port, const char *msg);

static CloudService* cloud_service = NULL;

// #ifdef GC9D01_SPI_LCD
static WifiService* g_wifi_service = NULL;

int scan_pending = 0;
ip_addr_t scan_pending_addr_copy;
u16_t scan_pending_port = 0;
int scan_result_ready = 0;
int callback_registered = 0;
static ApInfo_t g_ap_list[MAX_CONFIG_AP_NUM];
static char g_json_response[2048];
static char g_temp[256];

static void wifi_scan_callback(int event, void* data) {
    if (event == WIFI_STATUS_STA_SCANDONE && scan_pending) {
        printf("[WiFi] Scan completed, preparing result\n");
        
        memset(g_ap_list, 0, sizeof(g_ap_list));
        int count = g_wifi_service->svr_sta_get_ap_list(g_ap_list);
        char* connected_ssid = g_wifi_service->svr_sta_connection_name();
        
        // 构建 JSON
        memset(g_json_response, 0, sizeof(g_json_response));
        snprintf(g_json_response, sizeof(g_json_response), "{\"type\":\"wifi_list\",\"list\":[");
        
        for (int i = 0; i < count && i < MAX_CONFIG_AP_NUM; i++) {
            if (g_ap_list[i].ssid[0] == '\0') continue;
            
            int is_connected = (connected_ssid && strcmp(g_ap_list[i].ssid, connected_ssid) == 0) ? 1 : 0;
            memset(g_temp, 0, sizeof(g_temp));
            snprintf(g_temp, sizeof(g_temp), "{\"ssid\":\"%s\",\"rssi\":%d,\"connected\":%d}",
                     g_ap_list[i].ssid, g_ap_list[i].rssi, is_connected);
            
            if (i > 0) strcat(g_json_response, ",");
            strcat(g_json_response, g_temp);
        }
        strcat(g_json_response, "]}");
        
        printf("[WiFi] JSON length: %d\n", (int)strlen(g_json_response));
        
        //发送wifi列表
        // printf("[WiFi] About to send - addr IP: %s, port: %d\r\n",ip4addr_ntoa(&scan_pending_addr_copy), scan_pending_port);
        send_response(&scan_pending_addr_copy, scan_pending_port, g_json_response);
        
        scan_pending = 0;
        printf("[WiFi] Result sent\n");
    }
}
// #endif
/*
创建 UDP PCB，绑定 DISCOVERY_PORT = 5560。
注册回调 discovery_recv_callback。
*/
void device_discovery_init(void) {
    discovery_pcb = udp_new();
    if (discovery_pcb) {
        udp_bind(discovery_pcb, IP_ADDR_ANY, DISCOVERY_PORT);
        udp_recv(discovery_pcb, discovery_recv_callback, NULL);
        printf("[Discovery] Device discovery service started on port %d\n", DISCOVERY_PORT);
    }
}

#if 0
static void discovery_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                  const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;
    printf("recv a discovery msg!\n");
    if (!p || !addr) {	
	printf("p or addr is null !\n");
        if (p) pbuf_free(p);
        return;
    }

    char msg_buf[p->len + 1];
    pbuf_copy_partial(p, msg_buf, p->len, 0);
    msg_buf[p->len] = '\0';

    if (strstr(msg_buf, "\"type\":\"discovery\"")) {
        printf("[Discovery] Received discovery request from %s:%d\n", 
               ip4addr_ntoa(addr), port);
        printf("recv msg: %s !\n",msg_buf);	
        send_discovery_response(addr, 5560);
    }else{
	printf("recv msg: %s !\n",msg_buf);
   }
    pbuf_free(p);
}

#else
static void discovery_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                  const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;
    printf("recv a discovery msg!\n");
    if (!p || !addr) {	
        printf("p or addr is null !\n");
        if (p) pbuf_free(p);
        return;
    }

    char msg_buf[p->len + 1];
    pbuf_copy_partial(p, msg_buf, p->len, 0);
    msg_buf[p->len] = '\0';

    if (strstr(msg_buf, "\"type\":\"discovery\"")) {
        printf("[Discovery] Received discovery request from %s:%d\n", 
               ip4addr_ntoa(addr), port);
        printf("recv msg: %s !\n", msg_buf);
        
        int target_port = 5560; // I????
        const char *port_field = "\"port\":";
        char *port_start = strstr(msg_buf, port_field);
        
        if (port_start) {
            port_start += strlen(port_field); // ?z?????????'???
            
            while (*port_start == ' ') port_start++;
            
            if (*port_start >= '0' && *port_start <= '9') {
                target_port = 0;
                while (*port_start >= '0' && *port_start <= '9') {
                    target_port = target_port * 10 + (*port_start - '0');
                    port_start++;
                }
                
	   if (target_port > 0 && target_port <= 65535) {
                    printf("[Discovery] Parsed target port from message: %d\n", target_port);
                } else {
                    printf("[Discovery] Parsed port %d out of range, using default port: 5560\n", target_port);
                    target_port = 5560;
                }
            } else {
                printf("[Discovery] Invalid port format, using default port: 5560\n");
                target_port = 5560;
            }
        } else {
            printf("[Discovery] Port field not found in message, using default port: 5560\n");
        }
        
        send_discovery_response(addr, target_port);
    } else {
        printf("recv msg: %s !\n", msg_buf);
    }
    pbuf_free(p);
}

#endif

static devconfig_subcmd_t parse_subcmd(const char *cmd) {
    if (strcmp(cmd, "name") == 0) return DEVCONFIG_NAME;
    if (strcmp(cmd, "gender") == 0) return DEVCONFIG_GENDER; 
    if (strcmp(cmd, "voice") == 0) return DEVCONFIG_VOICE;
    if (strcmp(cmd, "prompt") == 0) return DEVCONFIG_PROMPT;
    if (strcmp(cmd, "personality") == 0) return DEVCONFIG_PERSONALITY;
    if (strcmp(cmd, "relationship") == 0) return DEVCONFIG_RELATIONSHIP;
    if (strcmp(cmd, "wifi_scan") == 0) return DEVCONFIG_WIFI_SCAN;
    if (strcmp(cmd, "wifi_connect") == 0) return DEVCONFIG_WIFI_CONNECT;
    return DEVCONFIG_UNKNOWN;
}
// #ifdef GC9D01_SPI_LCD
static void send_wifi_list_response(const ip_addr_t *addr, u16_t port) {
    printf("[WiFi] send_wifi_list_response called\n");
    
    if (g_wifi_service == NULL) {
        send_response(addr, port, "ERROR: WiFi service not available");
        return;
    }
    
    if (scan_pending) {
        send_response(addr, port, "ERROR: Scan already in progress");
        return;
    }
    
    scan_pending_addr_copy = *addr;
    scan_pending_port = port;
    scan_pending = 1;
    scan_result_ready = 0;
    
    if (!callback_registered) {
        g_wifi_service->register_callback(wifi_scan_callback);
        callback_registered = 1;
        printf("[WiFi] Callback registered\n");
    }
    
    printf("[WiFi] Triggering scan...\n");
    g_wifi_service->trigger_event(WIFI_EVENT_STA_SCAN, NULL);
    
    send_response(addr, port, "SCAN_STARTED");
    printf("[WiFi] Sent SCAN_STARTED response\n");
}


static void connect_to_wifi(const char* ssid, const char* password, const ip_addr_t *addr, u16_t port) {
    printf("[WiFi] connect_to_wifi: SSID='%s', Password='%s'\n", ssid, password);
    
    // 打印传入的 SSID 十六进制
    printf("[WiFi] Received SSID hex (%d bytes): ", (int)strlen(ssid));
    for (size_t i = 0; i < strlen(ssid); i++) {
        printf("%02X ", (unsigned char)ssid[i]);
    }
    printf("\n");
    
    if (g_wifi_service == NULL) {
        send_response(addr, port, "ERROR: WiFi service not available");
        return;
    }
    
    // 获取当前的 AP 列表，打印每个 SSID 的十六进制
    static ApInfo_t ap_list[MAX_CONFIG_AP_NUM];
    memset(ap_list, 0, sizeof(ap_list));
    int count = g_wifi_service->svr_sta_get_ap_list(ap_list);
    
    printf("[WiFi] Current AP list count: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("[WiFi] AP[%d] SSID: '%s', hex: ", i, ap_list[i].ssid);
        for (size_t j = 0; j < strlen(ap_list[i].ssid); j++) {
            printf("%02X ", (unsigned char)ap_list[i].ssid[j]);
        }
        printf("\n");
    }
    
    WifiConfig config;
    memset(&config, 0, sizeof(WifiConfig));
    strncpy(config.ssid, ssid, WIFI_MAX_SSID_LEN - 1);
    strncpy(config.passwd, password, WIFI_MAX_KEY_LEN - 1);

#ifdef GC9D01_SPI_LCD
 if (g_wifi_service->svr_softap_isEnabled()) {
    g_wifi_service->trigger_event(WIFI_EVENT_STA_DISABLE, &config);
    goldie_msleep(100);
    g_wifi_service->trigger_event(WIFI_EVENT_STA_ENABLE, &config);
    goldie_msleep(100);
 }
#endif

    int ret = g_wifi_service->trigger_event(WIFI_EVENT_STA_CONNECT, &config);
    
    if (ret == 0) {
        send_response(addr, port, "WIFI_CONNECT_OK");
        printf("[WiFi] Connection triggered\n");
        
#ifdef GC9D01_SPI_LCD
        if (g_wifi_service->svr_softap_isEnabled()) {
            g_wifi_service->trigger_event(WIFI_EVENT_SOFTAP_DISABLE, &config); 
        }
#endif

    } else {
        send_response(addr, port, "WIFI_CONNECT_ERROR");
        printf("[WiFi] Failed to trigger connection, ret=%d\n", ret);
    }
}
// #endif

static int set_device_name(const char *name, size_t len) {
    printf("Set device name: %.*s (len: %zu)\r\n", (int)len, name, len);
    return 0;
}
static int set_gender_config(int gender_id) {
    printf("Set gender config: gender=%d\n", gender_id);
    // gender_id: 0=女声, 1=男声
    char config_str[16];
    snprintf(config_str, sizeof(config_str), "id_%d", gender_id);
    return cloud_service->set_config(CONFIG_AVATAR, config_str);
}

static int set_voice_config(int voice_id) {
    printf("Set voice config: voice=%d\n", voice_id);
    // voice_id: 0,1,2
    char config_str[16];
    snprintf(config_str, sizeof(config_str), "id_%d", voice_id);
    return cloud_service->set_config(CONFIG_VOICE, config_str);
}

static int set_personality_config(int personality_id) {
    printf("Set personality config: id=%d\n", personality_id);
    // personality_id: 0,1,2
    char config_str[16];
    snprintf(config_str, sizeof(config_str), "id_%d", personality_id);
    return cloud_service->set_config(CONFIG_PERSONALITY, config_str);
}

static int set_relationship_config(int relationship_id) {
    printf("Set relationship config: id=%d\n", relationship_id);
    // relationship_id: 0,1,2
    char config_str[16];
    snprintf(config_str, sizeof(config_str), "id_%d", relationship_id);
    return cloud_service->set_config(CONFIG_RELATIONSHIP, config_str);
}

static int set_prompt(const char *prompt, size_t len) {
    printf("Set prompt: %.*s (len: %zu)\r\n", (int)len, prompt, len);
    
    if (cloud_service == NULL) {
        printf("[Cloud] Cloud service not available\n");
        return -1;
    }
    
    return cloud_service->set_config(CONFIG_CUSTOM_PROMPT, prompt);
}

static void send_discovery_response(const ip_addr_t *addr, u16_t port) {
    struct netif *netif = netif_default;
    if (!netif || !netif_is_up(netif)) {
        return;
    }

    // 增加：打印设备IP
    printf("\r\n[Device] My IP address: %s\r\n\r\n", ip4addr_ntoa(&netif->ip_addr));


    char mac_str[18];
    if (netif->hwaddr_len == 6) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
                 netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);
    } else {
        strcpy(mac_str, "00:00:00:00:00:00");
    }

    char response[256];
    snprintf(response, sizeof(response),
             "{\"type\":\"discovery_response\","
             "\"device_id\":\"%s\","
             "\"mac\":\"%s\","
             "\"hostname\":\"%s\","
             "\"ip\":\"%s\"}",
             DEVICE_ID, mac_str, DEVICE_HOSTNAME, 
             ip4addr_ntoa(&netif->ip_addr));

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(response), PBUF_RAM);
    if (p) {
        memcpy(p->payload, response, p->len);
        udp_sendto(discovery_pcb, p, addr, port);
        pbuf_free(p);
        printf("[Discovery] Sent discovery response to %s:%d\n", 
               ip4addr_ntoa(addr), port);
    }
}



static EventService *evt_service =NULL;

// ????
static const char ls_cmd_header[] = "Name			 Type		Size\n"
				"---------------- ---------- --------\n";

//extern lfs_t* ext_fs_get_glfs(void);


static struct udp_pcb *shell_pcb = NULL;

static Touch_Data g_touchdata ={0};
static Keyboard_Data g_kbdata = {0};


char response[512];

// ??????(??????)
//static char current_path[256] = "/";

extern struct netif *netif_default;

static void shell_recv_callback(
    void *arg, struct udp_pcb *pcb, struct pbuf *p,
    const ip_addr_t *addr, u16_t port);

static void handle_ifconfig(const ip_addr_t *addr, u16_t port, int argc, char **argv);

static void handle_lcd_test(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);

static void handle_send_event(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);

static void handle_send_key(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);

static void handle_send_touch(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);


static void handle_mkdir_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);

static void handle_ls_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) ;
static void handle_rm_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) ;
static void handle_heartbit(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv);
static void handle_devconfig(const ip_addr_t *addr, u16_t port, 
                              int argc, char **argv);


// ????(????)
static const shell_command commands[] = {
   // {"lcdtest",   handle_lcd_test},

   /* {"ls",   handle_ls},
    {"mkdir", handle_mkdir},
    {"rm",   handle_rm},
    {"disk",   handle_disk},
	*/
    {"devconfig",   handle_devconfig},
    {"ifconfig",   handle_ifconfig},
    {"sendevent",   handle_send_event},
    {"sendkb",   handle_send_key},
    {"sendtouch",   handle_send_touch},
    {"mkdir", handle_mkdir_fs},
    {"ls", handle_ls_fs},
    {"rm", handle_rm_fs},
    {"heartbit", handle_heartbit},
    {NULL, NULL}
};


static void handle_devconfig(const ip_addr_t *addr, u16_t port, 
                              int argc, char **argv) {
    // ??????
    if (argc < 2) {
        send_response(addr, port, "Error: Missing subcommand");
        return;
    }
    
    devconfig_subcmd_t subcmd = parse_subcmd(argv[1]);
    int ret = -1;
    char response[256];
    printf("handle_devconfig:%s \r\n",argv[1]);
    
    //获取云服务
    if (cloud_service == NULL) {
            cloud_service = (CloudService*)get_service(CLOUD_SERVICE_INDEX);
            if (cloud_service == NULL) {
                printf("[Cloud] Service not ready yet\n");
            }
    }
// #ifdef GC9D01_SPI_LCD
    if (g_wifi_service == NULL) {
        g_wifi_service = (WifiService*)get_service(WIFI_SERVICE_INDEX);
        if (g_wifi_service == NULL) {
            printf("[WiFi] Service not ready yet\n");
        }
    }
// #endif
    switch (subcmd) {
        case DEVCONFIG_NAME:
            // devconfig name <device_name>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing device name");
                return;
            }
            
            // ????????????(??UTF-8??)
            size_t name_len = 0;
            for (int i = 2; i < argc; i++) {
                if (i > 2) name_len++; // ??????
                name_len += strlen(argv[i]);
            }
            
            // ?????????(????????)
            char *full_name = malloc(name_len + 1);
            if (!full_name) {
                send_response(addr, port, "Error: Memory allocation failed");
                return;
            }
            
            full_name[0] = '\0';
            for (int i = 2; i < argc; i++) {
                if (i > 2) strcat(full_name, " ");
                strcat(full_name, argv[i]);
            }
            
            ret = set_device_name(full_name, name_len);
            free(full_name);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                         "Success: Device name configured");
            } else {
                snprintf(response, sizeof(response), 
                         "Error: Failed to configure device name");
            }
            break;
            
        case DEVCONFIG_GENDER:
            // devconfig gender <gender_id>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing gender id");
                return;
            }
            
            char *endptr_gender;
            int gender_id = (int)strtol(argv[2], &endptr_gender, 10);
            if (*endptr_gender != '\0') {
                send_response(addr, port, "Error: Invalid gender id");
                return;
            }
            
            ret = set_gender_config(gender_id);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                        "Success: Gender config set (gender=%d)", gender_id);
            } else {
                snprintf(response, sizeof(response), 
                        "Error: Failed to set gender config");
            }
            break;
        
        case DEVCONFIG_VOICE:
            // devconfig voice <voice_id>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing voice id");
                return;
            }
            
            char *endptr_voice;
            int voice_id = (int)strtol(argv[2], &endptr_voice, 10);
            if (*endptr_voice != '\0') {
                send_response(addr, port, "Error: Invalid voice id");
                return;
            }
            
            ret = set_voice_config(voice_id);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                        "Success: Voice config set (voice=%d)", voice_id);
            } else {
                snprintf(response, sizeof(response), 
                        "Error: Failed to set voice config");
            }
            break;
            
        case DEVCONFIG_PROMPT:
            // devconfig prompt <prompt_content>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing prompt content");
                return;
            }
            
            // ????????
            size_t prompt_len = 0;
            for (int i = 2; i < argc; i++) {
                if (i > 2) prompt_len++;
                prompt_len += strlen(argv[i]);
            }
            
            // ????????(????????)
            char *full_prompt = malloc(prompt_len + 1);
            if (!full_prompt) {
                send_response(addr, port, "Error: Memory allocation failed");
                return;
            }
            
            full_prompt[0] = '\0';
            for (int i = 2; i < argc; i++) {
                if (i > 2) strcat(full_prompt, " ");
                strcat(full_prompt, argv[i]);
            }
            
            ret = set_prompt(full_prompt, prompt_len);
            free(full_prompt);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                         "Success: Prompt configured");
            } else {
                snprintf(response, sizeof(response), 
                         "Error: Failed to configure prompt");
            }
            break;
            
        case DEVCONFIG_PERSONALITY:
            // devconfig personality <index>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing personality index");
                return;
            }
            
            char *endptr_personality;
            int personality_id = (int)strtol(argv[2], &endptr_personality, 10);
            if (*endptr_personality != '\0') {
                send_response(addr, port, "Error: Invalid personality index");
                return;
            }
            
            ret = set_personality_config(personality_id);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                         "Success: Personality config set (id=%d)", personality_id);
            } else {
                snprintf(response, sizeof(response), 
                         "Error: Failed to set personality config");
            }
            break;
            
        
        case DEVCONFIG_RELATIONSHIP:
            // devconfig relationship <index>
            if (argc < 3) {
                send_response(addr, port, "Error: Missing relationship index");
                return;
            }
            
            char *endptr_relationship;
            int relationship_id = (int)strtol(argv[2], &endptr_relationship, 10);
            if (*endptr_relationship != '\0') {
                send_response(addr, port, "Error: Invalid relationship index");
                return;
            }
            
            ret = set_relationship_config(relationship_id);
            
            if (ret == 0) {
                snprintf(response, sizeof(response), 
                         "Success: Relationship config set (id=%d)", relationship_id);
            } else {
                snprintf(response, sizeof(response), 
                         "Error: Failed to set relationship config");
            }
            break;    
// #ifdef GC9D01_SPI_LCD            
        case DEVCONFIG_WIFI_SCAN:
            printf("handle_devconfig: wifi_scan\n");
            send_wifi_list_response(addr, port);
            return;
            
        case DEVCONFIG_WIFI_CONNECT:
            if (argc < 4) {
                send_response(addr, port, "ERROR: Missing ssid or password");
                return;
            }
            printf("handle_devconfig: wifi_connect, ssid=%s\n", argv[2]);
            connect_to_wifi(argv[2], argv[3], addr, port);
            return;
// #endif        
        default:
            snprintf(response, sizeof(response), 
                     "Error: Unknown subcommand '%s'", argv[1]);
            break;
    }
    
    // 发送响应
    if (subcmd != DEVCONFIG_UNKNOWN) {
        send_response(addr, port, response);
    }
}


static int is_positive_under_1000(const char* str) {
    if (!str || !*str) return false;
    
 
    while (*str == ' ') str++;
    
   
    if (*str == '+' || *str == '-') {
        if (*str == '-') return 0;
        str++;
    }
    
    uint32_t num = 0;
    while (*str) {
        if (!isdigit(*str)) return false;
        
        num = num * 10 + (*str - '0');
        if (num >= 1000) return 0;
        
        str++;
    }
    return (num >= 0)?1:0; 
}

static uint32_t str_to_uint32(const char* str) {
    if (!str || !*str) return 0;
    
  
    while (*str == ' ') str++;
    
  
    bool negative = false;
    if (*str == '+' || *str == '-') {
        negative = (*str == '-');
        str++;
    }
    
    uint32_t result = 0;
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -result : result;
}


static void virtual_touch_init(void)
{
	g_touchdata.pressure = -1;
	printf("register virtual touch device!\r\n");
}

static void virtual_kerboard_init(void)
{
	g_kbdata.pressure = -1;
	printf("register virtual kerboard device!\r\n");
}



static void virtual_obtain_coordinates(Touch_Data* tdata)
{
	tdata->pressure = g_touchdata.pressure;
	tdata->x_pos= g_touchdata.x_pos;
	tdata->y_pos= g_touchdata.y_pos;
	g_touchdata.pressure = -1;
}

static void virtual_obtain_keyvalue(Keyboard_Data* tdata)
{
	tdata->pressure = g_kbdata.pressure;
	tdata->keyvalue = g_kbdata.keyvalue;
	g_kbdata.pressure = -1;
}


static Touch_Drv virtual_touch = {
	.touch_init = virtual_touch_init,
	.obtain_coordinates = virtual_obtain_coordinates,
}; 

static Keyboard_Drv virtual_keyvoard = {
	.keyboard_init = virtual_kerboard_init,
	.obtain_keyvalue = virtual_obtain_keyvalue,
}; 

/*
创建 UDP PCB，绑定 SHELL_PORT。
注册回调 shell_recv_callback。
调用 device_discovery_init()
*/
void udp_shell_start(void) {
    shell_pcb = udp_new();
    if (shell_pcb) {
        udp_bind(shell_pcb, IP_ADDR_ANY, SHELL_PORT);
        udp_recv(shell_pcb, shell_recv_callback, NULL);
        printf("[Shell] UDP Shell started on port %d\n", SHELL_PORT);
    }
// #ifdef GC9D01_SPI_LCD
     // 获取 WiFi 服务
    g_wifi_service = (WifiService*)get_service(WIFI_SERVICE_INDEX);
    if (g_wifi_service == NULL) {
        printf("[WiFi] Failed to get WiFi service\n");
    }
// #endif
	device_discovery_init();
	register_drv(TOUCH_DRV_INDEX,&virtual_touch);
	register_drv(KEYBOARD_DRV_INDEX,&virtual_keyvoard);
}

void udp_shell_stop(void) {
	if(shell_pcb){
		udp_remove(shell_pcb);
	}
    if(discovery_pcb){
        udp_remove(discovery_pcb);
        discovery_pcb = NULL;
    }
}

// UDP????
static void shell_recv_callback(
    void *arg, struct udp_pcb *pcb, struct pbuf *p,
    const ip_addr_t *addr, u16_t port) {
    
    if (!p || !addr || !port) return;

    pcb = pcb;
    arg = arg;
    
    // 解析命令
    char cmd_buf[p->len + 1];
    pbuf_copy_partial(p, cmd_buf, p->len, 0);
    cmd_buf[p->len] = '\0';

    // 分割命令
    int argc = 0;
    char *argv[8];
    char *token = strtok(cmd_buf, " ");
    while (token && argc < 8) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    // 执行命令
    if (argc > 0) {
        for (const shell_command *c = commands; c->name; c++) {
            if (strcmp(argv[0], c->name) == 0) {
                c->handler(addr, port, argc, argv);
                break;
            }
        }
    }

    pbuf_free(p);
}


static void handle_send_touch(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
	if (argc != 4) {
        send_response(addr, port, "USAGE: sendtouch {pressure x y} ");
        return;
    }

	if(is_positive_under_1000(argv[1])&&is_positive_under_1000(argv[2])&&is_positive_under_1000(argv[3]))
	{
		g_touchdata.pressure = str_to_uint32(argv[1]);
		g_touchdata.x_pos = str_to_uint32(argv[2]);
		g_touchdata.y_pos = str_to_uint32(argv[3]);
		send_response(addr, port,"OK");
	}else{
		send_response(addr, port,"ERROR");
	}
}

static void handle_heartbit(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
         printf("handle_heartbit \r\n");
	send_response(addr, port,"OK");
}

static void handle_send_key(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
	if (argc != 3) {
        send_response(addr, port, "USAGE: sendkb {pressure key} ");
        return;
    }

	if(is_positive_under_1000(argv[1])&&is_positive_under_1000(argv[2]))
	{
		g_kbdata.pressure = str_to_uint32(argv[1]);
		g_kbdata.keyvalue= str_to_uint32(argv[2]);
		send_response(addr, port,"OK");
	}else{
		send_response(addr, port,"ERROR");
	}
}

static void handle_send_event(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
	if (argc != 2) {
        send_response(addr, port, "USAGE: sendevent {event} ");
        return;
    }
	evt_service = wait_service(EVENT_SERVICE_INDEX);
	if(evt_service != NULL)
	{
		evt_service->send_keyevent((int)(argv[1][0]-'0'));
		send_response(addr, port,"OK");
	}else{
		send_response(addr, port,"ERROR");
	}
}

static void handle_lcd_test(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
	if (argc != 2) {
        send_response(addr, port, "USAGE: lcdtest 0 or 1");
        return;
    }
	send_response(addr, port,"OK");
}

static void handle_ifconfig(const ip_addr_t *addr, u16_t port, int argc, char **argv) {
    struct netif *netif = netif_default;
	argc = argc;
	argv = argv;
    if (!netif || !netif_is_up(netif)) {
        send_response(addr, port, "ERROR: No active network interface");
        return;
    }

    // ??IP??
    char ip_str[16], nm_str[16], gw_str[16];
    snprintf(ip_str, sizeof(ip_str), "%s", ip4addr_ntoa(&netif->ip_addr.u_addr.ip4));
    snprintf(nm_str, sizeof(nm_str), "%s", ip4addr_ntoa(&netif->netmask.u_addr.ip4));
    snprintf(gw_str, sizeof(gw_str), "%s", ip4addr_ntoa(&netif->gw.u_addr.ip4));

    // ??MAC??(?????)
    char mac_str[18];
    if (netif->hwaddr_len == 6) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
                 netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);
    } else {
        strcpy(mac_str, "N/A");
    }

	memset(response,0,256);
    // ?????
    snprintf(response, sizeof(response),
        "Network Info (%s):\n"
        "IP:      %s\n"
        "Netmask: %s\n"
        "Gateway: %s\n"
        "MAC:     %s",
        netif->name, ip_str, nm_str, gw_str, mac_str);

    send_response(addr, port, response);
}

static void send_response(const ip_addr_t *addr, u16_t port, const char *msg) {
    printf("[Shell] Sending response to %s:%d, msg: %s\n", ip4addr_ntoa(addr), port, msg);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg), PBUF_RAM);
    if (!p) 
    {
        printf("[Shell] pbuf_alloc failed!\n"); return;
    }
    memcpy(p->payload, msg, p->len);
    udp_sendto(shell_pcb, p, addr, port);
    pbuf_free(p);
}


static void handle_mkdir_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
    if (argc != 2) {
        send_response(addr, port, "USAGE: mkdir <path>");
        return;
    }

    int err = f_mkdir( argv[1]);
    send_response(addr, port, (err < 0) ? "ERROR" : "OK");
}

static void handle_ls_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
    (void)argv;
    (void)argc;
    FRESULT res;
    FILINFO fno;
    DIR dir;
    char *path = "/";
    // char fullpath[256];
    char entry[64]="";
    // ????
    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        printf("open fail\r\n");
        return;
    }
    
   // printf("path: %s\r\n", path);
    memset(response,0,512);
    // ?????
    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;  // ?????
        
        // ??.?..??
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;
            
        // ??????
        // sprintf(fullpath, "%s/%s", path, fno.fname);
        memset(entry,0,64);
        // ????/????
        if (fno.fattrib & AM_DIR) {
            //printf("dir: %s\r\n", fno.fname);
            snprintf(entry, sizeof(entry), "%-16s <dir>\n", fno.fname);
        } else {
            //printf("file: %s - size: %u bit\r\n", 
            //            fno.fname, fno.fsize);
            snprintf(entry, sizeof(entry), "%-16s %8dbit\n", 
                     fno.fname, (int)fno.fsize);
        }
        strcat(response, entry);
    }
    f_closedir(&dir);
    char final_response[strlen(ls_cmd_header) + strlen(response) + 16];
    snprintf(final_response, sizeof(final_response), "%s%s", ls_cmd_header, response);
    send_response(addr, port, final_response);
}

static void handle_rm_fs(const ip_addr_t *addr, u16_t port, 
                     int argc, char **argv) {
    if (argc != 2) {
        send_response(addr, port, "USAGE: rm <path>");
        return;
    }


    char new_path[128];
	
    if (argv[1][0] == '/') {
        snprintf(new_path, sizeof(new_path), "%s", argv[1]);
    } else {
        snprintf(new_path, sizeof(new_path), "/%s", argv[1]);
    }

    int err = f_unlink(new_path);
    send_response(addr, port, (err < 0) ? "ERROR" : "OK");
}



