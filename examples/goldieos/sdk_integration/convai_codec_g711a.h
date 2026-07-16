#ifndef CONVAI_CODEC_G711A_H
#define CONVAI_CODEC_G711A_H

/**
 * @file convai_codec_g711a.h
 * @brief G.711 A-law codec interface.
 *
 * G.711 A-law compresses 16-bit PCM to 8-bit, with 2:1 compression ratio.
 * Uses logarithmic companding algorithm, suitable for voice transmission.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode 16-bit PCM to G.711 A-law.
 *
 * @param pcm     [in]  Input PCM data (2 bytes per sample, signed 16-bit).
 * @param pcm_len [in]  Input data length in bytes (must be multiple of 2).
 * @param out     [out] Output buffer.
 * @param out_cap [in]  Output buffer capacity.
 * @param out_len [out] Actual output length in bytes.
 * @return 0 on success, -1 on invalid parameters.
 */
/**
 * @brief Encode PCM16 (planar stereo) to G.711 A-law.
 *
 * Input format: planar stereo [L0, L1, ..., L(n-1), R0, R1, ..., R(n-1)]
 * Output: interleaved A-law [L0, R0, L1, R1, ...]
 *
 * @param pcm      [in]  Input PCM data (planar format).
 * @param pcm_len  [in]  Input data length in bytes (must be multiple of 2).
 * @param channels [in]  Number of channels (default 2 for stereo).
 * @param out      [out] Output buffer.
 * @param out_cap  [in]  Output buffer capacity.
 * @param out_len  [out] Actual output length in bytes.
 * @return 0 on success, -1 on invalid parameters.
 */
int convai_g711a_encode(const uint8_t *pcm, size_t pcm_len, int channels,
                        uint8_t *out, size_t out_cap,
                        size_t *out_len);

/**
 * @brief Decode G.711 A-law to 16-bit PCM.
 *
 * @param encoded  [in]  Input G.711 A-law data.
 * @param enc_len  [in]  Input data length in bytes.
 * @param pcm      [out] Output PCM buffer.
 * @param pcm_cap  [in]  Output buffer capacity.
 * @param pcm_len  [out] Actual output length in bytes.
 * @return 0 on success, -1 on invalid parameters.
 */
int convai_g711a_decode(const uint8_t *encoded, size_t enc_len,
                        uint8_t *pcm, size_t pcm_cap,
                        size_t *pcm_len);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_CODEC_G711A_H */
