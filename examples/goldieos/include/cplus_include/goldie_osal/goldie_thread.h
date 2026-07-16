#ifndef HIHIOE_THREAD_H
#define HIHIOE_THREAD_H

#include <functional>
#define MAX_NAME_SIZE 24
// 定义线程函数类型
typedef int (*thd_func)(void* data);

class Goldie_Thread {
public:
    // 修改ThreadFunc以匹配thd_func的签名
    using ThreadFunc = std::function<int(void*)>;
    
    // 构造函数 - 需要传入数据和栈大小
    Goldie_Thread(thd_func func, void* data, int stack_size);
    
    // 简化构造函数，使用默认栈大小
    Goldie_Thread(thd_func func, void* data);
    
    // 支持std::function的构造函数
    template<typename Function>
    Goldie_Thread(Function&& func, void* data, int stack_size = 0x500);
    
    ~Goldie_Thread();

private:
    void createThread();
    // 修改返回类型为int以匹配C函数期望
    static int threadEntry(void* arg);

private:
    ThreadFunc m_func;
    void* m_user_data;  // 需要存储用户数据
    int m_stack_size;
	char m_name[MAX_NAME_SIZE];
};

// 模板构造函数的实现
template<typename Function>
Goldie_Thread::Goldie_Thread(Function&& func, void* data, int stack_size) 
    : m_stack_size(stack_size), m_user_data(data) {
    m_func = std::forward<Function>(func);
    createThread();
}

#endif // HIHIOE_THREAD_H



