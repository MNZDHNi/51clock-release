#ifndef __DS1302_H__
#define __DS1302_H__

extern signed char DS1302_Time[];   //年月日 时分秒 星期（有符号，依赖下溢判断）

void DS1302_Init(void);
void DS1302_SetTime(void);
void DS1302_ReadTime(void);
unsigned char DS1302_IsRunning(void);   //RTC 是否已有可用时间

#endif