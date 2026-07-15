#ifndef CONVAI_EVENT_H
#define CONVAI_EVENT_H

#include "convai_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file convai_event.h
 * @brief Unified event model for the ConvAI SDK.
 *
 * All asynchronous notifications flow through the event callback that
 * the application registers in convai_create().
 */

/* ---- event code enumeration ---- */

typedef enum {
    CONVAI_EV_CONNECTED = 0,
    CONVAI_EV_DISCONNECTED,
    CONVAI_EV_FAILED,
    CONVAI_EV_UPDATED
} convai_event_code_e;

/* ---- unified event structure ---- */

/**
 * @brief Unified event delivered to the application callback.
 *
 * The struct is valid only for the duration of the callback.
 */
typedef struct {
    convai_event_code_e code;
    union {
        const char *details;
    } data;
} convai_event_t;

/* ---- callback interface ---- */

typedef struct {
    /**
     * @brief Engine lifecycle & error events.
     * @param engine    Engine handle.
     * @param event     Event payload.
     * @param user_data Opaque user pointer from convai_create.
     */
    void (*on_convai_event)(convai_engine_t engine,
                            convai_event_t *event,
                            void *user_data);

    /**
     * @brief Conversation-status change notification.
     * @param engine    Engine handle.
     * @param status    New agent status.
     * @param user_data Opaque user pointer from convai_create.
     */
    void (*on_convai_conversation_status)(convai_engine_t engine,
                                          convai_status_e status,
                                          void *user_data);

    /**
     * @brief Incoming audio data.
     * @param engine    Engine handle.
     * @param data_ptr  Raw audio buffer.
     * @param data_len  Buffer length in bytes.
     * @param info_ptr  Audio format descriptor.
     * @param user_data Opaque user pointer from convai_create.
     */
    void (*on_convai_audio_data)(convai_engine_t engine,
                                 const void *data_ptr,
                                 size_t data_len,
                                 const convai_audio_frame_info_t *info_ptr,
                                 void *user_data);

    /**
     * @brief Incoming text message.
     * @param engine    Engine handle.
     * @param data_ptr  Message payload.
     * @param data_len  Payload length in bytes.
     * @param info_ptr  Message descriptor.
     * @param user_data Opaque user pointer from convai_create.
     */
    void (*on_convai_message_data)(convai_engine_t engine,
                                   const void *data_ptr,
                                   size_t data_len,
                                   const convai_message_info_t *info_ptr,
                                   void *user_data);

} convai_event_handler_t;

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_EVENT_H */