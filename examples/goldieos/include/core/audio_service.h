#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H
enum {
	AUDIO_RING_ID_NOTIFY =0,
	AUDIO_RING_ID_POWERON,
	AUDIO_RING_ID_WAKEUP,
	AUDIO_RING_ID_SLEEP,
	AUDIO_RING_ID_ALARM,
	AUDIO_RING_ID_MAX,
};

enum {
	AUDIO_PLAY_STREAM_SYSTEM =0,
	AUDIO_PLAY_STREAM_USER,
	AUDIO_RECORD_STREAM,
	AUDIO_STREAM_MAX,
};

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct AudioService {
    void (*audio_input_config)(int);
    int (*audio_read)(void*, unsigned int);
    void (*audio_output_config)(int);
    int (*audio_write)(const void*, unsigned int);
    void (*suspend)(void);
	void (*play_start)(void);
	void (*play_stop)(void);	
	void (*record_start)(void);
	void (*record_stop)(void);
	void (*play_ring)(int,int);	
	void (*stop_ring)(void);
	void (*wait_ring)(void);
	void (*set_volume)(int, float);
	int  (*vad_detect)(short*,unsigned int);
	float (*get_volume)(int);
	void (*algostream_start)(void);
	void (*algostream_stop)(void);	
	void (*reset_playbuffer)(void);
    int (*algostream_read)(void*, unsigned int);
	unsigned int (*get_valid_length)(int *);
} AudioService;
#endif
