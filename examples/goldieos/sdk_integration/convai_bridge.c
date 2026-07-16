/**
 * @file convai_bridge.c
 * @brief Connects goldieos apps to the ConvAI SDK public API.
 *
 * Manages engine lifecycle, session state, and background audio recording.
 * Transport/protocol are stubs; key API calls are logged to stdout.
 */
#include "convai_bridge.h"
#include "convai_config.h"
#include "service_manager.h"
#include "goldie_osal.h"
#include "audio_service.h"
#include "convai_codec_g711a.h"
#include "ringbuffer.h"
#include "../platform/convai_platform_ws63.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- internal state ---- */
static convai_engine_t          g_engine    = NULL;
static convai_status_e          g_status    = CONVAI_STATUS_IDLE;
static int                      g_started   = 0;
static convai_bridge_status_cb  g_status_cb  = NULL;
static convai_bridge_event_cb   g_event_cb   = NULL;
static convai_bridge_message_cb g_message_cb = NULL;

/* ---- startup config (set by settings UI, consumed by start) ---- */
#define STARTUP_CONFIG_MAX  2048
static char g_startup_config[STARTUP_CONFIG_MAX] = {0};

#define INFO_CONFIG_MAX  2048

/* ---- audio recording state ---- */
typedef struct {
    void *audio_service;               /* AudioService* */
    int   sample_rate;
    int   channels;
    int   bits_per_sample;
    int   running;                     /* flag to stop recording thread */
    void *thread_handle;               /* goldie thread handle */
} audio_source_t;

static audio_source_t g_audio_src = {0};

/* ---- audio dump for debugging (desktop only, no FS on embedded) ---- */
#ifndef __EMBEDDED__
static FILE *g_dump_file       = NULL;
static long   g_dump_data_bytes = 0;
#define AUDIO_DUMP_PATH     "audio_dump.wav"
#endif

/* ---- playback ring buffer (producer-consumer bridge) ---- */
#define PLAYBACK_RING_SIZE      8000   /* 500ms @ 8kHz mono 16bit */
#define PLAYBACK_PRIME_THRESHOLD  480   /* 160ms — enough to cover initial network jitter */
static uint8_t      g_ring_data[PLAYBACK_RING_SIZE];
static RingBuffer   g_playback_ring;

/* ---- playback thread state ---- */

static uint8_t g_pcm_decode_buf[1024];  /* 用于 on_audio 回调 */
static char g_json_copy_buf[2048];       /* 用于 on_message_data 回调 */

enum {
    PLAYBACK_IDLE,      /* HW stopped, waiting for next response */
    PLAYBACK_PRIMING,   /* buffering initial data before play_start */
    PLAYBACK_PLAYING,   /* actively consuming and writing to HW */
};
static int g_playback_state = PLAYBACK_IDLE;

static int      g_playback_running = 0;   /* thread exit flag */
static void    *g_playback_thread  = NULL;

#define AUDIO_RECORD_BUF_SIZE  640   /* 40ms @ 8kHz mono 16bit = 640 bytes (double-buffered) */
#define AUDIO_FRAME_MS          20

#ifndef __EMBEDDED__
/* Write a placeholder WAV header; sizes will be patched on stop. */
static int dump_wav_header(FILE *f, int sample_rate, int channels, int bits)
{
    /* RIFF header */
    fwrite("RIFF", 1, 4, f);
    uint32_t riff_size = 0;  /* placeholder */
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1; /* PCM */
    fwrite(&audio_format, 2, 1, f);
    uint16_t ch = (uint16_t)channels;
    fwrite(&ch, 2, 1, f);
    uint32_t sr = (uint32_t)sample_rate;
    fwrite(&sr, 4, 1, f);
    uint32_t byte_rate = (uint32_t)(sample_rate * channels * bits / 8);
    fwrite(&byte_rate, 4, 1, f);
    uint16_t block_align = (uint16_t)(channels * bits / 8);
    fwrite(&block_align, 2, 1, f);
    uint16_t bps = (uint16_t)bits;
    fwrite(&bps, 2, 1, f);

    /* data chunk header */
    fwrite("data", 1, 4, f);
    uint32_t data_size = 0;  /* placeholder */
    fwrite(&data_size, 4, 1, f);

    fflush(f);
    return 0;
}

/* Patch the WAV header with actual data size. */
static void dump_wav_finalize(FILE *f, long total_data_bytes)
{
    if (!f) return;

    /* RIFF chunk size = 4 + 24 + 8 + data_size = 36 + data_size */
    uint32_t riff_size = (uint32_t)(36 + total_data_bytes);
    fseek(f, 4, SEEK_SET);
    fwrite(&riff_size, 4, 1, f);

    /* data chunk size */
    fseek(f, 40, SEEK_SET);
    uint32_t data_size = (uint32_t)total_data_bytes;
    fwrite(&data_size, 4, 1, f);

    fclose(f);
}
#endif /* __EMBEDDED__ */

static int audio_record_thread(void *arg)
{
    (void)arg;
    audio_source_t *s = &g_audio_src;
    AudioService *audio = (AudioService *)s->audio_service;

    if (!audio || !audio->audio_read) {
        printf("[convai_bridge] ERROR: no audio_read available\n");
        return -1;
    }

    uint8_t *buf = (uint8_t *)goldie_malloc(AUDIO_RECORD_BUF_SIZE);
    uint8_t *planar_buf = (uint8_t *)goldie_malloc(AUDIO_RECORD_BUF_SIZE);
    uint8_t *g711_buf = (uint8_t *)goldie_malloc(AUDIO_RECORD_BUF_SIZE);
    if (!buf || !planar_buf || !g711_buf) {
        printf("[convai_bridge] ERROR: buffer malloc failed\n");
        goldie_free(buf);
        goldie_free(planar_buf);
        goldie_free(g711_buf);
        return -1;
    }

    /* Start audio capture */
    if (audio->record_start) audio->record_start();
    printf("[convai_bridge] audio recording started (sr=%d)\n", s->sample_rate);

    while (s->running) {
        int len = audio->audio_read(buf, AUDIO_RECORD_BUF_SIZE);
        if (len > 0) {
            /*
             * GoldieOS audio hardware provides stereo interleaved PCM (L/R).
             * We need planar format [L0, L1, ...] [R0, R1, ...] for cloud AEC.
             * Both channels are silent frames.
             */
            int sample_count = len / (int)sizeof(short);   /* total 16-bit samples */
            int frame_count = sample_count / 2;            /* stereo frames (L/R pairs) */
            int16_t *samples = (int16_t *)buf;

            /* Rearrange to planar format: [L0, L1, ... R0, R1, ...] */
            int16_t *planar_samples = (int16_t *)planar_buf;
            for (int i = 0; i < frame_count; i++) {
                /* Left channel = mic data */
                planar_samples[i] = samples[i * 2];
                /* Right channel = silent (zero) for cloud AEC */
                planar_samples[frame_count + i] = 0;
            }

            /* Write mono PCM to debug dump file (desktop only) */
#ifndef __EMBEDDED__
            if (g_dump_file) {
                fwrite(planar_buf, 1, (size_t)len, g_dump_file);
                g_dump_data_bytes += (size_t)len;
            }
#endif

            /* Encode planar stereo PCM → G.711A before sending to SDK */
            size_t  g711_len = 0;
            int enc_ret = convai_g711a_encode(planar_buf, (size_t)len, 2,
                                              g711_buf, AUDIO_RECORD_BUF_SIZE,
                                              &g711_len);
            if (enc_ret != 0 || g711_len == 0) {
                printf("[convai_bridge] WARNING: g711 encode failed\n");
            } else {
                convai_audio_frame_info_t info;
                memset(&info, 0, sizeof(info));
                info.data_type = CONVAI_AUDIO_DATA_TYPE_G711A;

                convai_bridge_send_audio(g711_buf, g711_len, &info);
            }
        } else {
            goldie_msleep(10);
        }
    }

    goldie_free(buf);
    goldie_free(planar_buf);
    goldie_free(g711_buf);
    printf("[convai_bridge] audio recording thread stopped\n");
    return 0;
}

/* ===================================================================
 *  Playback thread — three-state consumer.
 *
 *  State machine (driven by g_playback_state, set by on_status):
 *    IDLE    – HW stopped, waiting for next response
 *    PRIMING – buffering initial data before play_start (once per response)
 *    PLAYING – paced consumption: 1 chunk/tick, 10ms interval
 * =================================================================== */

static void playback_write(AudioService *audio, const uint8_t *buf, unsigned int len)
{
    if (audio && audio->audio_write) {
        audio->audio_write(buf, len);
    }
}

static int playback_thread_func(void *arg)
{
    (void)arg;
    AudioService *audio = (AudioService *)g_audio_src.audio_service;

    const int sr = g_audio_src.sample_rate > 0 ? g_audio_src.sample_rate : 8000;

    printf("[convai_bridge] playback thread started (sr=%d)\n", sr);

    uint8_t *buf = (uint8_t *)goldie_malloc(1024);
    int len = 0;
    
    int prev_state = PLAYBACK_IDLE;
    while (g_playback_running) {

        switch (g_playback_state) {
            /* ---- HW stopped, waiting for next response ---- */
            case PLAYBACK_IDLE: {
                /*
                * Only drain + stop on the transition INTO idle,
                * not every 50ms tick.
                */
                if (prev_state != PLAYBACK_IDLE) {
                    int d;
                    while ((d = ring_buffer_bulk_read_noblock(&g_playback_ring,
                                                            buf, 1024)) > 0) {
                        playback_write(audio, buf, (unsigned int)d);
                    }
                    /* play_stop is idempotent — safe to call even if already stopped */
                    if (audio && audio->play_stop) {
                        audio->play_stop();
                    }
                    printf("[convai_bridge] playback HW stopped\n");
                }
                goldie_msleep(10);
                break;
            }

            /* ---- Buffering initial data before play_start ---- */
            case PLAYBACK_PRIMING:
                if (g_playback_ring.count < PLAYBACK_PRIME_THRESHOLD) {
                    goldie_msleep(10);
                    break;
                }
                if (audio && audio->audio_output_config) {
                    audio->audio_output_config(sr);
                }
                if (audio && audio->play_start) {
                    audio->play_start();
                }
                g_playback_state = PLAYBACK_PLAYING;
                printf("[convai_bridge] playback HW started (sr=%d, primed=%u bytes)\n",
                    sr, (unsigned int)g_playback_ring.count);
                /* fall through to PLAYING */

            /* ---- Paced consumption: 1 chunk/tick, 10ms interval ---- */
            case PLAYBACK_PLAYING: {
                int d;
                while ((d = ring_buffer_bulk_read_noblock(&g_playback_ring,
                                                        buf, 1024)) > 0) {
                    playback_write(audio, buf, (unsigned int)d);
                }
                goldie_msleep(10);
                break;
            }
            default: break;
        }
        prev_state = g_playback_state;
    }

    /* ---- Thread exiting ---- */
    g_playback_state = PLAYBACK_IDLE;

    /* Drain remaining, stop HW, finalize dump files */
    {
        int d;
        while ((d = ring_buffer_bulk_read_noblock(&g_playback_ring,
                                                   buf, 1024)) > 0) {
            playback_write(audio, buf, (unsigned int)d);
        }
    }

    if (audio && audio->play_stop) {
        audio->play_stop();
    }
    
    goldie_free(buf);
    printf("[convai_bridge] playback thread stopped\n");
    return 0;
}

static void start_playback_thread(void)
{
    if (g_playback_running) return;

    /* Init the ring buffer (mutex is initialised by ring_buffer_init) */
    ring_buffer_init(&g_playback_ring);
    g_playback_ring.buffer     = g_ring_data;
    g_playback_ring.buffer_len = PLAYBACK_RING_SIZE;

    g_playback_running = 1;
    g_playback_state = PLAYBACK_IDLE;

    g_playback_thread = goldie_thread_create(
        playback_thread_func, NULL, "convai_playback", 0x2000);
    if (g_playback_thread) {
        goldie_thread_set_priority(g_playback_thread, 21);
    }
    printf("[convai_bridge] playback thread created\n");
}

static void stop_playback_thread(void)
{
    if (!g_playback_running) return;

    g_playback_running = 0;

    if (g_playback_thread) {
        goldie_thread_destroy(g_playback_thread);
        g_playback_thread = NULL;
    }

    /* Reset ring buffer (thread is gone, no contention) */
    ring_buffer_reset(&g_playback_ring);

    printf("[convai_bridge] playback thread destroyed\n");
}

static void start_audio_recording(void)
{
    if (!g_audio_src.audio_service) {
        printf("[convai_bridge] no audio source set, skipping recording\n");
        return;
    }
    if (g_audio_src.running) return;

#ifndef __EMBEDDED__
    /* Open debug dump files (desktop only) */
    // g_dump_file = fopen(AUDIO_DUMP_PATH, "wb");
    if (g_dump_file) {
        dump_wav_header(g_dump_file,
                        g_audio_src.sample_rate ? g_audio_src.sample_rate : 8000,
                        g_audio_src.channels ? g_audio_src.channels : 1,
                        g_audio_src.bits_per_sample ? g_audio_src.bits_per_sample : 16);
        g_dump_data_bytes = 0;
        printf("[convai_bridge] audio dump file opened: %s\n", AUDIO_DUMP_PATH);
    } else {
        printf("[convai_bridge] WARNING: cannot open dump file %s\n", AUDIO_DUMP_PATH);
    }
#endif

    g_audio_src.running = 1;
    g_audio_src.thread_handle = goldie_thread_create(
        audio_record_thread, NULL, "convai_audio", 0x2000);
    if (g_audio_src.thread_handle) {
        goldie_thread_set_priority(g_audio_src.thread_handle, 22);
    }
    printf("[convai_bridge] AUTO mode: recording started\n");

    /* Start the playback thread (persistent; drains ring buffer on demand) */
    start_playback_thread();
}

static void stop_audio_recording(void)
{
    if (!g_audio_src.running) return;
    g_audio_src.running = 0;
    if (g_audio_src.thread_handle) {
        goldie_thread_destroy(g_audio_src.thread_handle);
        g_audio_src.thread_handle = NULL;
    }
    /* Stop audio capture */
    AudioService *audio = (AudioService *)g_audio_src.audio_service;
    if (audio && audio->record_stop) audio->record_stop();

#ifndef __EMBEDDED__
    /* Finalize debug dump file (desktop only) */
    if (g_dump_file) {
        dump_wav_finalize(g_dump_file, g_dump_data_bytes);
        printf("[convai_bridge] audio dump saved: %s (%ld bytes PCM data)\n",
               AUDIO_DUMP_PATH, g_dump_data_bytes);
        g_dump_file = NULL;
        g_dump_data_bytes = 0;
    }
#endif

    printf("[convai_bridge] audio recording stopped\n");
    
    /* Stop the playback thread */
    stop_playback_thread();
}

/* ---- default config fallbacks (used when config file is absent) ---- */
#define BRIDGE_DEFAULT_BOT_ID         "your_agent_id"       // ← 替换为实际的 agent_id
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"     // ← 替换为实际的 product_id
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"    // ← 替换为实际的 product_key
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret" // ← 替换为实际的 product_secret
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"    // ← 替换为实际的 device_name
#define DEFAULT_STARTUP_CONFIG \
    "{" \
            "\"config\":{" \
                "\"llm_config\":{" \
                    "\"system_messages\":[" \
                        "\"你的名字叫小荷，你可以帮小朋友解决小烦恼哦。\"" \
                    "]" \
                "}," \
                "\"tts_config\":{" \
                    "\"provider_params\":{" \
                        "\"audio\":{" \
                            "\"voice_type\":\"Chinese (Mandarin)_Warm_Girl\"" \
                        "}" \
                    "}" \
                "}" \
            "}" \
        "}"

/* Return config value, or @p fallback if not set. */
static const char *cfg_or(const char *key, const char *fallback)
{
    const char *v = convai_config_get(key);
    return v ? v : fallback;
}

/**
 * Build the create-time JSON config string, filling in values from the
 * config file where available, otherwise using hardcoded defaults.
 *
 * Writes into the supplied buffer and returns a pointer to it.
 */
static const char *bridge_build_config_json(char *buf, size_t buf_size)
{
    int n = snprintf(buf, buf_size,
        "{"
            "\"info\":{"
                "\"product_id\":\"%s\","
                "\"product_key\":\"%s\","
                "\"product_secret\":\"%s\","
                "\"device_name\":\"%s\""
            "},"
            "\"ws\":{"
                "\"audio\":{"
                    "\"codec\":%d"
                "}"
            "}"
        "}", 
        cfg_or("product_id",      BRIDGE_DEFAULT_PRODUCT_ID),
        cfg_or("product_key",     BRIDGE_DEFAULT_PRODUCT_KEY),
        cfg_or("product_secret",  BRIDGE_DEFAULT_PRODUCT_SECRET),
        cfg_or("device_name",     BRIDGE_DEFAULT_DEVICE_NAME),
        0
    );
    (void)n; /* truncation is acceptable — engine will reject malformed JSON */
    return buf;
}

/* ===================================================================
 *  SDK callbacks
 * =================================================================== */

static void on_event(convai_engine_t e, convai_event_t *ev, void *ud)
{
    (void)e; (void)ud;
    const char *info = NULL;
    switch (ev->code) {
    case CONVAI_EV_CONNECTED:
        info = ev->data.details ? ev->data.details : "";
        printf("[convai_bridge] EVENT: CONNECTED (%s)\n", info);
        break;
    case CONVAI_EV_DISCONNECTED:
        info = ev->data.details ? ev->data.details : "";
        printf("[convai_bridge] EVENT: DISCONNECTED (reason=%s)\n", info);
        break;
    case CONVAI_EV_FAILED:
        /* ev->data.details contains complete JSON: {"code": "...", "message": "..."} */
        printf("[convai_bridge] EVENT: FAILED %s\n", ev->data.details);
        break;
    default: break;
    }
    if (g_event_cb) g_event_cb(ev->code, info);
}

static void on_status(convai_engine_t e, convai_status_e s, void *ud)
{
    (void)e; (void)ud;
    g_status = s;
    if (g_status_cb) g_status_cb(s);
    const char *str = "?";
    switch (g_status) {
        case CONVAI_STATUS_IDLE:          str = "IDLE"; break;
        case CONVAI_STATUS_LISTENING:     str = "LISTENING"; break;
        case CONVAI_STATUS_THINKING:      str = "THINKING"; break;
        case CONVAI_STATUS_ANSWERING:     str = "ANSWERING"; break;
        case CONVAI_STATUS_INTERRUPTED:   str = "INTERRUPTED"; break;
        case CONVAI_STATUS_ANSWER_FINISHED: str = "ANSWER_FINISH"; break;
    }

    printf("[STATUS] %s\n", str);

    /*
     * State-driven playback:
     *   ANSWERING    → prime then paced consumption
     *   INTERRUPTED  → stop + drop stale audio
     *   other states → stop, but drain remaining frames naturally
     */
    if (s == CONVAI_STATUS_ANSWERING) {
        g_playback_state = PLAYBACK_PRIMING;
    } else {
        if (s == CONVAI_STATUS_INTERRUPTED) {
            g_playback_state = PLAYBACK_IDLE;
            ring_buffer_reset(&g_playback_ring);
        }
    }
}

static void on_audio(convai_engine_t e, const void *data, size_t len,
                     const convai_audio_frame_info_t *info, void *ud)
{
    (void)e; (void)ud; (void)info;

    size_t  pcm_len = 0;
    int dec_ret = convai_g711a_decode((const uint8_t *)data, len,
                                      g_pcm_decode_buf, sizeof(g_pcm_decode_buf), &pcm_len);
    if (dec_ret != 0 || pcm_len == 0) {
        printf("[convai_bridge] WARNING: g711 decode failed (ret=%d pcm_len=%zu)\n",
               dec_ret, pcm_len);
        return;
    }

    /*
     * Push decoded PCM into the ring buffer (non-blocking).
     * If the buffer is full, data is dropped — this is acceptable for
     * real-time TTS; the playback thread will catch up.
     */
    printf("pcm input-----------------------------write=%d, count=%u bytes\r\n",
        pcm_len, (unsigned int)g_playback_ring.count);
    int written = ring_buffer_bulk_write_noblock(&g_playback_ring,
                                                  g_pcm_decode_buf,
                                                  (unsigned int)pcm_len);
    (void)written;
}

static void on_message_data(convai_engine_t e, const void *data, size_t len,
                           const convai_message_info_t *info, void *ud)
{
    (void)ud;

    printf("[convai_bridge] MESSAGE: %.*s (binary=%d)\n",
           (int)len, (const char *)data, info ? info->is_binary : 0);

    /* Pass raw JSON to the app layer via generic callback */
    if (g_message_cb && !(info && info->is_binary)) {
        size_t copy_len = len < sizeof(g_json_copy_buf) - 1 ? len : sizeof(g_json_copy_buf) - 1;
        memcpy(g_json_copy_buf, data, copy_len);
        g_json_copy_buf[copy_len] = '\0';
        g_message_cb(g_json_copy_buf);
    }
}

/* ===================================================================
 *  Public API
 * =================================================================== */

void convai_bridge_init(void)
{
    if (g_engine) {
        printf("[convai_bridge] already initialized\n");
        return;
    }
    
    // FIXME: WS63 platform——open file
    /* Initialise config from file alongside the executable */
    // if (convai_config_init() != 0) {
    //     printf("[convai_bridge] WARNING: no config file, using defaults\n");
    // }

    char config_json[2048];
    const char *cfg = bridge_build_config_json(config_json, sizeof(config_json));

    printf("[convai_bridge] using config:\n%s\n", cfg);

    /* Initialize platform abstraction layer */
    int retPlat = convai_platform_ws63_init();
    printf("[convai_bridge] convai_platform_ws63_init ret: %d", retPlat);

    convai_event_handler_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.on_convai_event        = on_event;
    cb.on_convai_conversation_status = on_status;
    cb.on_convai_audio_data          = on_audio;
    cb.on_convai_message_data        = on_message_data;

    int ret = convai_create(&g_engine, cfg, &cb, NULL);
    if (ret != CONVAI_OK) {
        printf("[convai_bridge] ERROR: convai_create failed: %s\n", convai_err_2_str(ret));
        g_engine = NULL;
        return;
    }

    printf("[convai_bridge] engine created\n");
    printf("[convai_bridge] SDK version: %s\n", convai_get_version());

    register_service(CONVAI_BRIDGE_SERVICE_INDEX, g_engine);
    printf("[convai_bridge] registered at service index %d\n",
           CONVAI_BRIDGE_SERVICE_INDEX);
}

void convai_bridge_set_audio_source(convai_audio_source_t *src,
                                    int sr, int ch, int bits)
{
    if (!src) {
        printf("[convai_bridge] audio source cleared\n");
        memset(&g_audio_src, 0, sizeof(g_audio_src));
        return;
    }
    g_audio_src.audio_service  = src;
    g_audio_src.sample_rate    = sr;
    g_audio_src.channels       = ch;
    g_audio_src.bits_per_sample = bits;
    printf("[convai_bridge] audio source set: sr=%d ch=%d bits=%d\n", sr, ch, bits);
}

/* ---- startup config helpers ---- */

void convai_bridge_set_startup_config(const char *config)
{
    if (config) {
        strncpy(g_startup_config, config, sizeof(g_startup_config) - 1);
        g_startup_config[sizeof(g_startup_config) - 1] = '\0';
        printf("[convai_bridge] startup config saved (%zu bytes)\n", strlen(g_startup_config));
    }
}

const char *convai_bridge_get_startup_config(void)
{
    return g_startup_config[0] ? g_startup_config : NULL;
}

int convai_bridge_start(void)
{
    if (!g_engine) {
        printf("[convai_bridge] ERROR: not initialized\n");
        return -1;
    }
    if (g_started) {
        printf("[convai_bridge] already started\n");
        return 0;
    }

    convai_opt_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.mode     = CONVAI_MODE_WS;
    opt.agent_id = cfg_or("agent_id", BRIDGE_DEFAULT_BOT_ID);
    opt.params   = g_startup_config[0] ? g_startup_config : DEFAULT_STARTUP_CONFIG;

    printf("[convai_bridge] START: agent_id=%s, params=%s\n",
           opt.agent_id, opt.params);

    int ret = convai_start(g_engine, &opt);
    if (ret == CONVAI_OK) {
        g_started = 1;
        g_status  = CONVAI_STATUS_LISTENING;
        printf("[convai_bridge] start OK (status=LISTENING)\n");
        if (g_status_cb) g_status_cb(g_status);

        /* Begin background microphone recording */
        start_audio_recording();
    } else {
        printf("[convai_bridge] start FAILED: %s\n", convai_err_2_str(ret));
    }
    return ret;
}

int convai_bridge_stop(void)
{
    if (!g_engine) return -1;
    if (!g_started) return 0;

    /* Stop recording before stopping engine */
    stop_audio_recording();

    printf("[convai_bridge] STOP\n");
    int ret = convai_stop(g_engine);
    g_started = 0;
    g_status  = CONVAI_STATUS_IDLE;
    if (g_status_cb) g_status_cb(g_status);
    if (ret != CONVAI_OK) {
        printf("[convai_bridge] stop FAILED: %s\n", convai_err_2_str(ret));
    }
    return ret;
}

int convai_bridge_restart(void)
{
    printf("[convai_bridge] RESTART\n");
    convai_bridge_stop();
    goldie_msleep(100);
    return convai_bridge_start();
}

convai_engine_t convai_bridge_get_engine(void)     { return g_engine; }
convai_status_e convai_bridge_get_status(void)     { return g_status; }
int convai_bridge_is_speaking(void)                { return (g_status == CONVAI_STATUS_ANSWERING); }

int convai_bridge_send_audio(const uint8_t *data, size_t len,
                             const convai_audio_frame_info_t *info)
{
    if (!g_engine || !g_started) return -1;
    return convai_send_audio(g_engine, data, len, info);
}

void convai_bridge_on_status(convai_bridge_status_cb cb)   { g_status_cb  = cb; }
void convai_bridge_on_event(convai_bridge_event_cb cb)     { g_event_cb   = cb; }
void convai_bridge_on_message(convai_bridge_message_cb cb) { g_message_cb = cb; }