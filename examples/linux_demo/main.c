/**
 * @file main.c
 * @brief Linux demo – minimal ConvAI SDK usage example.
 *
 * Demonstrates the complete engine lifecycle:
 *   create → start → send messages/audio → poll events → stop → destroy
 *
 * Compile (standalone, without CMake):
 *   gcc -std=c99 -I../../include -I../../src main.c -L../../build/src \
 *       -lconvai_sdk -lpthread -o linux_demo
 *
 * Or use the CMake build:
 *   mkdir build && cd build && cmake .. && make
 */

#include "convai/convai_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/* ---- demo context ---- */

static int g_running = 1;

static void on_signal(int sig)
{
    (void)sig;
    printf("\n[DEMO] Signal received, shutting down...\n");
    g_running = 0;
}

/* ---- typed callbacks ---- */

static void on_convai_event(convai_engine_t engine,
                            convai_event_t *event, void *user_data)
{
    (void)engine;
    (void)user_data;

    switch (event->type) {
    case CONVAI_EVENT_CONNECTED:
        printf("[EVENT] Connected to server: %s\n",
               event->data.connected.server_info
                   ? event->data.connected.server_info : "(unknown)");
        break;
    case CONVAI_EVENT_DISCONNECTED:
        printf("[EVENT] Disconnected (graceful=%d): %s\n",
               event->data.disconnected.graceful,
               event->data.disconnected.reason
                   ? event->data.disconnected.reason : "no reason");
        g_running = 0;
        break;
    case CONVAI_EVENT_ERROR:
        printf("[EVENT] Error (code=%d): %s\n",
               event->data.error.code,
               event->data.error.message
                   ? event->data.error.message
                   : convai_err_to_str(event->data.error.code));
        break;
    default:
        printf("[EVENT] Unknown event type: %d\n", (int)event->type);
        break;
    }
    fflush(stdout);
}

static void on_conversation_status(convai_engine_t engine,
                                   convai_status_e status, void *user_data)
{
    (void)engine;
    (void)user_data;
    printf("[STATUS] Agent status: %d\n", (int)status);
    fflush(stdout);
}

static void on_audio_data(convai_engine_t engine,
                          const void *data_ptr, size_t data_len,
                          const convai_audio_frame_info_t *info_ptr,
                          void *user_data)
{
    (void)engine;
    (void)data_ptr;
    (void)user_data;
    printf("[AUDIO] Received %zu bytes, data_type=%d\n",
           data_len,
           info_ptr ? (int)info_ptr->data_type : -1);
    fflush(stdout);
}

static void on_message_data(convai_engine_t engine,
                            const void *data_ptr, size_t data_len,
                            const convai_message_info_t *info_ptr,
                            void *user_data)
{
    (void)engine;
    (void)user_data;
    printf("[MESSAGE] %.*s (binary=%d)\n",
           (int)data_len, (const char *)data_ptr,
           info_ptr ? info_ptr->is_binary : 0);
    fflush(stdout);
}

/* ---- main ---- */

int main(int argc, char **argv)
{
    int ret;

    printf("========================================\n");
    printf("  ConvAI SDK – Linux Demo\n");
    printf("  Version: 0.1.0\n");
    printf("========================================\n\n");

    /* install signal handlers */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    /* ---- 1. Create engine ---- */

    convai_event_callback_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.on_convai_event        = on_convai_event;
    cb.on_conversation_status = on_conversation_status;
    cb.on_audio_data          = on_audio_data;
    cb.on_message_data        = on_message_data;

    convai_engine_t engine = convai_create("{\"log_level\":\"info\"}",
                                           cb, NULL);
    if (!engine) {
        printf("[DEMO] convai_create failed\n");
        return EXIT_FAILURE;
    }
    printf("[DEMO] Engine created successfully\n");

    /* ---- 2. Start engine ---- */

    convai_options_t options;
    memset(&options, 0, sizeof(options));
    options.agent_id    = "demo-agent";
    options.config_json = "{\"voice\":\"default\"}";

    ret = convai_start(engine, &options);
    if (ret != CONVAI_OK) {
        printf("[DEMO] convai_start failed: %s (code %d)\n",
               convai_err_to_str(ret), ret);
        convai_destroy(engine);
        return EXIT_FAILURE;
    }
    printf("[DEMO] Engine started\n");

    /* ---- 3. Send a text message ---- */

    const char *hello = "Hello, ConvAI!";
    ret = convai_send_message_data(engine, hello, strlen(hello));
    if (ret != CONVAI_OK) {
        printf("[DEMO] send_message failed: %s (code %d)\n",
               convai_err_to_str(ret), ret);
    } else {
        printf("[DEMO] Sent message: \"%s\"\n", hello);
    }

    /* ---- 4. Send a dummy audio frame ---- */

    /* Generate a 20 ms silent frame: 16 kHz, mono, 16-bit = 640 bytes */
    size_t frame_bytes = 640;
    uint8_t *silence = (uint8_t *)calloc(1, frame_bytes);
    if (silence) {
        convai_audio_frame_info_t audio_info;
        memset(&audio_info, 0, sizeof(audio_info));
        audio_info.data_type = CONVAI_AUDIO_DATA_TYPE_G711A;
        audio_info.commit    = 0;

        ret = convai_send_audio_data(engine, silence, frame_bytes,
                                     &audio_info);
        if (ret != CONVAI_OK) {
            printf("[DEMO] send_audio failed: %s (code %d)\n",
                   convai_err_to_str(ret), ret);
        } else {
            printf("[DEMO] Sent audio frame: %zu bytes\n", frame_bytes);
        }
        free(silence);
    }

    /* ---- 5. Wait for events (SDK poll thread runs internally) ---- */

    printf("[DEMO] Running (press Ctrl+C to exit)...\n");
    while (g_running) {
        sleep(1);
    }

    /* ---- 6. Stop engine ---- */

    ret = convai_stop(engine);
    if (ret != CONVAI_OK) {
        printf("[DEMO] convai_stop failed: %s (code %d)\n",
               convai_err_to_str(ret), ret);
    } else {
        printf("[DEMO] Engine stopped\n");
    }

    /* ---- 8. Destroy engine ---- */

    ret = convai_destroy(engine);
    if (ret != CONVAI_OK) {
        printf("[DEMO] convai_destroy failed: %s (code %d)\n",
               convai_err_to_str(ret), ret);
    } else {
        printf("[DEMO] Engine destroyed\n");
    }

    printf("[DEMO] Exit\n");
    return EXIT_SUCCESS;
}
