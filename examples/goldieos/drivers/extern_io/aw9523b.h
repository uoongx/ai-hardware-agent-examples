#ifndef AW9523B_H
#define AW9523B_H
#define AW9523B_ADDR  0x5b  // 7bit addr
// AW9523B 寄存器定义
#define AW9523B_REG_INPUT0     0x00  // PORT0输入状态寄存器
#define AW9523B_REG_INPUT1     0x01  // PORT1输入状态寄存器  
#define AW9523B_REG_OUTPUT0    0x02  // PORT0输出状态寄存器
#define AW9523B_REG_OUTPUT1    0x03  // PORT1输出状态寄存器
#define AW9523B_REG_CONFIG0    0x04  // PORT0方向配置寄存器 (0:输出, 1:输入)
#define AW9523B_REG_CONFIG1    0x05  // PORT1方向配置寄存器
#define AW9523B_REG_INT0       0x06  // PORT0中断使能寄存器
#define AW9523B_REG_INT1       0x07  // PORT1中断使能寄存器
#define AW9523B_REG_ID         0x10  // 芯片ID寄存器
#define AW9523B_REG_CTL        0x11  // 全局控制寄存器
#define AW9523B_REG_LED_MODE0  0x12  // PORT0 LED模式控制
#define AW9523B_REG_LED_MODE1  0x13  // PORT1 LED模式控制

#define AW9523B_CHIP_ID       0x23   // 芯片ID值
#endif
