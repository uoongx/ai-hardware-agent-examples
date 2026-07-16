#ifndef GOLDIE_INIT_H
#define GOLDIE_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

// �����ʼ����������
typedef void (*goldie_init_func_t)(void);

// ����ȫ��ע�ắ��
void register_init_function(goldie_init_func_t func,char* name);

// ������ʼ���������ú���
void call_all_init_functions(void);

// ����ע��� - ʹ�ù��캯��������main֮ǰ�Զ�ע��
#define GOLDIE_INIT_CALL_(func) \
    static void _goldie_init_##func(void) __attribute__((constructor)); \
    static void _goldie_init_##func(void) { \
        register_init_function(func,#func); \
    }

#ifdef __cplusplus
}
#endif

#endif // GOLDIE_INIT_H

