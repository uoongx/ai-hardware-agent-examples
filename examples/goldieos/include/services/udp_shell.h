#ifndef INCLUDE_UDP_SHELL_H
#define INCLUDE_UDP_SHELL_H
//#include "lwip/udp.h"
//#include "stdlib.h"
//#include "lwip/nettool/misc.h"

#include "service_manager.h"
#include "driver_manager.h"
#include "lwip/netif.h"


typedef enum {
    DEVCONFIG_UNKNOWN = 0,
    DEVCONFIG_NAME,
    DEVCONFIG_GENDER,
    DEVCONFIG_VOICE,
    DEVCONFIG_PROMPT,
    DEVCONFIG_PERSONALITY,    
    DEVCONFIG_RELATIONSHIP,
    DEVCONFIG_WIFI_SCAN, 
    DEVCONFIG_WIFI_CONNECT
} devconfig_subcmd_t;


#define SHELL_PORT 555
typedef void (*cmd_handler)(const ip_addr_t *addr, u16_t port, 
                           int argc, char **argv);

typedef struct {
    const char *name;
    cmd_handler handler;
} shell_command;

void udp_shell_start(void);
void udp_shell_stop(void);
#endif
