/*
 * VAD调试工具 - 输出各个频率带的能量值
 */

#include "vad/vad_core.h"
#include "vad/vad_filterbank.h"
#include "vad/vad_sp.h"
#include <stdio.h>

// Declare GmmProbability function from vad_core.c
int16_t GmmProbability(VadInstT* self, int16_t* features,
                       int16_t total_power, size_t frame_length);

// 调试版本的VAD处理函数
int WebRtcVad_ProcessDebug(VadInstT* self, const int16_t* frame, 
                          size_t frame_length, int16_t* features_out) {
    int16_t features[kNumChannels];
    int16_t total_power;
    
    size_t len;
    int16_t speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)

    // Wideband: Downsample signal before doing VAD
    WebRtcVad_Downsampling(frame, speechNB, self->downsampling_filter_states,
                           frame_length);
    
    len = frame_length/2;
    // 计算特征
    total_power = WebRtcVad_CalculateFeatures(self, speechNB, len, features);
    
    // 输出能量值到外部数组
    for (int i = 0; i < kNumChannels; i++) {
        features_out[i] = features[i];
    }
    
    // 继续正常VAD处理
    return GmmProbability(self, features, total_power, len);
}

// 打印能量值信息
void PrintVadEnergyInfo(int16_t* features, int vad_decision) {
    const char* band_names[] = {
        "80-250Hz",
        "250-500Hz", 
        "500-1000Hz",
        "1000-2000Hz",
        "2000-3000Hz",
        "3000-4000Hz"
    };
    
    printf("VAD能量分析 - 决策: %s\r\n", vad_decision ? "语音" : "噪声");
    printf("频带         能量值(Q4)            实际能量(dB)\r\n");
    printf("--------------------------------------------\r\n");
    
    for (int i = 0; i < kNumChannels; i++) {
        // 将Q4转换为实际dB值: actual = value / 16.0
        int actual_dB = (features[i] / 16.0f)*100;
        printf("%s      %d        %d(dB)\n", band_names[i], features[i], actual_dB);
    }
    printf("--------------------------------------------\r\n");
}

// 详细的VAD分析函数
void AnalyzeVadPerformance(VadInstT* self, const int16_t* frame, 
                          size_t frame_length, int expected_result) {
    int16_t features[kNumChannels];
    int16_t total_power;
    int vad_decision;
    
    size_t len;
    int16_t speechNB[240]; // Downsampled speech frame: 480 samples (30ms in WB)

    // Wideband: Downsample signal before doing VAD
    WebRtcVad_Downsampling(frame, speechNB, self->downsampling_filter_states,
                           frame_length);

    len = frame_length / 2;

    // 计算特征
    total_power = WebRtcVad_CalculateFeatures(self, speechNB, len, features);
    
    // 执行VAD决策
    vad_decision = GmmProbability(self, features, total_power, len);
    
    // 输出详细分析
    printf("=== VAD详细分析 ===\n");
    printf("帧长度: %zu, 总能量: %d\n", len, total_power);
    printf("预期结果: %s, 实际结果: %s\n", 
           expected_result ? "语音" : "噪声",
           vad_decision ? "语音" : "噪声");
    
    if (vad_decision != expected_result) {
        printf("*** 检测错误! ***\n");
    }
    
    PrintVadEnergyInfo(features, vad_decision);
    
    // 输出GMM模型状态
    printf("GMM模型状态:\n");
    for (int ch = 0; ch < kNumChannels; ch++) {
        printf("通道%d - 噪声均值: %d, 语音均值: %d\n", 
               ch, self->noise_means[ch], self->speech_means[ch]);
    }
    printf("\n");
}
