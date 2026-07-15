#include "goldie_osal.h"
extern void register_sle_drv(void);

static void platform_init_entry(void){
	register_sle_drv();
}
GOLDIE_INIT_CALL_(platform_init_entry);
