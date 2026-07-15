#ifndef __GOLDIE_ALGO_WK_CAPI_H
#define __GOLDIE_ALGO_WK_CAPI_H
#ifdef __cplusplus
extern "C" {
#endif
int wk_algo_init(const unsigned char* wk_model,uint8_t* tensor_arena,int tensor_arena_size);
int wk_algo_process(float* input_data);
#ifdef __cplusplus
}
#endif

#endif

