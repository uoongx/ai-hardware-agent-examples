/* ---------- service_manager.h ---------- */
#ifndef INCLUDE_SERVICE_MANAGER_
#define INCLUDE_SERVICE_MANAGER_
#include "goldie_osal.h"
#include "event_service.h"
#include "input_service.h"
#include "wifi_service.h"
/* cloud_service.h removed — apps use convai_bridge */
#include "audio_service.h"
#include "sdcard_service.h"
#include "aualgo_service.h"
#include "ntp_service.h"
#include "alarm_service.h" 
#ifdef SUPPORT_SLE
#include "sle_sdp_service.h"
#endif
enum SERVICE_INDEX{
	AUDIO_SERVICE_INDEX =0,
	EVENT_SERVICE_INDEX,
	WIFI_SERVICE_INDEX,
	SDCARD_SERVICE_INDEX,
	CLOUD_SERVICE_INDEX,
	AUALGO_SERVICE_INDEX,
	SLE_SDP_SERVICE_INDEX,
	NTP_SERVICE_INDEX,
	ALARM_SERVICE_INDEX,
	SLE_WTP_SERVICE_INDEX,
	RESERVE_SERVICE_INDEX_4,
	RESERVE_SERVICE_INDEX_3,
	RESERVE_SERVICE_INDEX_2,
	RESERVE_SERVICE_INDEX_1,
	RESERVE_SERVICE_INDEX_0,
	MAX_SERVICE_INDEX,
};


void register_service(int service_index, void* service);
void* wait_service(int service_index);
void* get_service(int service_index);

#endif


