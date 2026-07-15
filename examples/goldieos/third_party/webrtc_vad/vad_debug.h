/*
 * VAD调试工具头文件
 */

#ifndef VAD_DEBUG_H_
#define VAD_DEBUG_H_

#include "vad/vad_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 调试版本的VAD处理函数，输出能量特征
 * @param self VAD实例
 * @param frame 输入音频帧
 * @param frame_length 帧长度
 * @param features_out 输出能量特征数组（6个频带）
 * @return VAD决策结果
 */
int WebRtcVad_ProcessDebug(VadInstT* self, const int16_t* frame, 
                          size_t frame_length, int16_t* features_out);

/**
 * 打印能量值信息
 * @param features 能量特征数组
 * @param vad_decision VAD决策结果
 */
void PrintVadEnergyInfo(int16_t* features, int vad_decision);

/**
 * 详细的VAD性能分析
 * @param self VAD实例
 * @param frame 输入音频帧
 * @param frame_length 帧长度
 * @param expected_result 预期结果（用于验证）
 */
void AnalyzeVadPerformance(VadInstT* self, const int16_t* frame, 
                          size_t frame_length, int expected_result);

#ifdef __cplusplus
}
#endif

#endif  // VAD_DEBUG_H_