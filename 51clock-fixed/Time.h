#ifndef __TIME_H__
#define __TIME_H__

//定时器重载值：0xFC67 = 65536-64615，即 921 个机器周期 ≈ 1ms @ 11.0592MHz 晶振。
//若你的板子是 12MHz 晶振，1ms 应为 0xFC18，否则秒表和闪烁都会快约 8%。
#define TIMER_1MS_H 0xFC
#define TIMER_1MS_L 0x67

void Timer0_Init(void);
void Timer1_Init(void);
void Timer2_Init(void);

#endif