#include <REGX52.H>

void Timer0_Init(void) 
{
    TMOD &= 0xF0;      // 清除 T0 设置
    TMOD |= 0x01;      // T0 模式 1（16位定时）
    TH0 = 0xFC;        // 1ms 初值高8位
    TL0 = 0x67;        // 1ms 初值低8位
    ET0 = 1;           // 允许 T0 中断
    TR0 = 1;           // 启动 T0
    EA = 1;            // 开总中断
}

void Timer1_Init(void) {
    TMOD &= 0x0F;      // 清除 T1 设置
    TMOD |= 0x10;      // T1 模式 1（16位定时）
    TH1 = 0xFC;        // 1ms 初值高8位
    TL1 = 0x67;        // 1ms 初值低8位
    ET1 = 1;           // 允许 T1 中断
    TR1 = 1;           // 启动 T1
    EA = 1;            // 开总中断
}

void Timer2_Init(void) {
    // 设置自动重载值
    RCAP2H = 0xFC;     // 1ms 重载值高8位
    RCAP2L = 0x67;     // 1ms 重载值低8位
    
    // 初始化计数器
    TH2 = 0xFC;
    TL2 = 0x67;
    
    T2CON = 0x00;      // 使用内部定时，自动重载模式
    ET2 = 1;           // 允许 T2 中断
    TR2 = 1;           // 启动 T2
    EA = 1;            // 开总中断
}

/*
// T0 中断服务函数
void Timer0_ISR(void) interrupt 1 {
    TH0 = 0xFC;        // 重装初值
    TL0 = 0x67;
    // 这里添加1ms定时任务代码
}

// T1 中断服务函数
void Timer1_ISR(void) interrupt 3 {
    TH1 = 0xFC;        // 重装初值
    TL1 = 0x67;
    // 这里添加1ms定时任务代码
}

// T2 中断服务函数
void Timer2_ISR(void) interrupt 5 {
    TF2 = 0;           // T2 必须手动清除中断标志
    // 自动重载模式下无需手动重装初值
    // 这里添加1ms定时任务代码
}
*/