#include <REGX52.H>
#include "Delay.h"
#include "Key.h"
#include "DS1302.h"
#include "LCD1602.h"
#include "Time.h"

unsigned char KeyNum, MODE, TimeSetSelect;    //键值 模式值 时间位
volatile unsigned char TimeSetFlashFlag;      //时间闪烁标志位(中断与主循环共用，需 volatile)

//闹钟值定义
//注意：必须用 signed char。Keil C51 中 char 默认为无符号，
//      "减到 0 再 -1" 会回绕成 255，原来的 if(x < 0) 边界判断永远不会成立。
signed char AlarmTime[6];   //年 月 日 时 分 秒（原为 clock[5]，下标用到 0~5 需要 6 个元素，否则越界写会踩坏 Alarm）
unsigned char Alarm;        //是否有闹钟
int CheakFlag;              //判断标志位
//注意：P2^5 同时被 LCD1602 用作 LCD_RW，硬件上是同一根引脚。
//      蜂鸣时不得访问 LCD；FMQBeepStop 会把该脚拉低，等价于重新置 LCD_RW=0。
sbit FMQ = P2^5;            //定义蜂鸣器引脚

//闹钟响铃(非阻塞)：主循环只置 BellFlag 并给 BellRemain 赋时长，
//剩余毫秒数在 Timer2 的 1ms 中断里递减，因此响铃期间按键和显示都保持可用。
#define ALARM_BEEP_MS 1500
unsigned char BellFlag;             //响铃中标记
volatile unsigned int BellRemain;   //响铃剩余毫秒(0 = 未响铃)

//秒表
struct StopTime
{
    unsigned char StopHour;     //时
    unsigned char StopMinute;   //分
    unsigned char StopSecond_0; //秒
    unsigned char StopSecond_1; //十分秒
    unsigned char StopSecond_2; //百分秒
};
struct StopTime NowTime;            //秒表实时时间
struct StopTime StopTimeArray[10];  //回看值
unsigned char StopTimeNum;          //回看值数
unsigned char StopTimeNumSelect;    //回看值位
volatile unsigned int StopCount;    //秒表中断计数器(中断写、主循环读；16位可容忍长卡顿，8位会在 2.55s 后回绕丢时间)
unsigned char StopWatchFlag;        //秒表显示标记位
unsigned char StopTimeFlag;         //显示内容(实时/回看)标记位
unsigned char StopWatchAddFlag;     //秒表累计状态位

/* MODE.JSON
{
    "Name":"MODE",
    "MODE":{
        "0":"TimeShow",
        "1":"TimeSet",
        "2":"AlarmSet",
        "3":"StopWatch"
    }
}
*/

//函数声明部分
void ClearStopNum();                //清除秒表存储
void FMQBeepStart(void);            //开始蜂鸣(非阻塞)
void FMQBeepStop(void);             //停止蜂鸣
int Cheak();                        //检测闹钟时间是否达到
unsigned char DaysInMonth(signed char Year, signed char Month);  //当月最大天数
void Time_Clamp(signed char *t);                                 //把 年 月 日 时 分 秒 钳回合法范围
void Alarm_ClampToNow(signed char *t);                           //闹钟不得早于当前时间

void TimeShow() //MODE 0 显示时间
{
    DS1302_ReadTime();
    LCD_ShowNum(1,1,DS1302_Time[0],2);
    LCD_ShowNum(1,4,DS1302_Time[1],2);
    LCD_ShowNum(1,7,DS1302_Time[2],2);
    LCD_ShowNum(2,1,DS1302_Time[3],2);
    LCD_ShowNum(2,4,DS1302_Time[4],2);
    LCD_ShowNum(2,7,DS1302_Time[5],2);
}

void TimeSet()  //MODE 1 设置时间
{
    if(KeyNum == 2)
    {
        TimeSetSelect++;
        TimeSetSelect %= 6; //越界清零
    }
    if(KeyNum == 3)
    {
        DS1302_Time[TimeSetSelect]++;
        Time_Clamp(DS1302_Time);    //输入合法性判断
    }
    if(KeyNum == 4)
    {
        DS1302_Time[TimeSetSelect]--;
        Time_Clamp(DS1302_Time);    //输入合法性判断
    }
    
    //实时显示，未经过DS1302
    if(TimeSetSelect == 0 && TimeSetFlashFlag == 1) {LCD_ShowString(1,1,"  ");}
    else {LCD_ShowNum(1,1,DS1302_Time[0],2);}
    if(TimeSetSelect == 1 && TimeSetFlashFlag == 1) {LCD_ShowString(1,4,"  ");}
    else {LCD_ShowNum(1,4,DS1302_Time[1],2);}
    if(TimeSetSelect == 2 && TimeSetFlashFlag == 1) {LCD_ShowString(1,7,"  ");}
    else {LCD_ShowNum(1,7,DS1302_Time[2],2);}
    if(TimeSetSelect == 3 && TimeSetFlashFlag == 1) {LCD_ShowString(2,1,"  ");}
    else {LCD_ShowNum(2,1,DS1302_Time[3],2);}
    if(TimeSetSelect == 4 && TimeSetFlashFlag == 1) {LCD_ShowString(2,4,"  ");}
    else {LCD_ShowNum(2,4,DS1302_Time[4],2);}
    if(TimeSetSelect == 5 && TimeSetFlashFlag == 1) {LCD_ShowString(2,7,"  ");}
    else {LCD_ShowNum(2,7,DS1302_Time[5],2);}
    
    //LCD_ShowNum(2,10,TimeSetSelect,2);
}

void AlarmSet() //MODE 2 设置闹钟
{
    //取当前时间，作为闹钟"不得早于现在"的比较基准。
    //本模式不走 TimeShow()，DS1302_Time 若不主动刷新，会一直停留在进入本模式前的旧值。
    DS1302_ReadTime();

    //设置闹钟时间
    if(KeyNum == 2)
    {
        if(!Alarm)
        {
            Alarm = 1;  //有闹钟

            //赋值AlarmTime
            AlarmTime[0] = DS1302_Time[0];
            AlarmTime[1] = DS1302_Time[1];
            AlarmTime[2] = DS1302_Time[2];
            AlarmTime[3] = DS1302_Time[3];
            AlarmTime[4] = DS1302_Time[4];
            AlarmTime[5] = DS1302_Time[5];
        }
        else
        {
            TimeSetSelect++;
            TimeSetSelect %= 6;
        }
    }
    if(KeyNum == 3)
    {
        AlarmTime[TimeSetSelect]++;

        Time_Clamp(AlarmTime);          //输入合法性一般判断
        Alarm_ClampToNow(AlarmTime);    //闹钟时间边界判断
    }
    if(KeyNum == 4)
    {
        AlarmTime[TimeSetSelect]--;

        Time_Clamp(AlarmTime);          //输入合法性一般判断
        Alarm_ClampToNow(AlarmTime);    //闹钟时间边界判断
    }

    if(Alarm)   //进入闹钟设置，开始闪烁
    {
    //闹钟时间实时显示，已写入 AlarmTime[]
    if(TimeSetSelect == 0 && TimeSetFlashFlag == 1) {LCD_ShowString(1,1,"  ");}
    else {LCD_ShowNum(1,1,AlarmTime[0],2);}
    if(TimeSetSelect == 1 && TimeSetFlashFlag == 1) {LCD_ShowString(1,4,"  ");}
    else {LCD_ShowNum(1,4,AlarmTime[1],2);}
    if(TimeSetSelect == 2 && TimeSetFlashFlag == 1) {LCD_ShowString(1,7,"  ");}
    else {LCD_ShowNum(1,7,AlarmTime[2],2);}
    if(TimeSetSelect == 3 && TimeSetFlashFlag == 1) {LCD_ShowString(2,1,"  ");}
    else {LCD_ShowNum(2,1,AlarmTime[3],2);}
    if(TimeSetSelect == 4 && TimeSetFlashFlag == 1) {LCD_ShowString(2,4,"  ");}
    else {LCD_ShowNum(2,4,AlarmTime[4],2);}
    if(TimeSetSelect == 5 && TimeSetFlashFlag == 1) {LCD_ShowString(2,7,"  ");}
    else {LCD_ShowNum(2,7,AlarmTime[5],2);}
    }
}

void StopWatch()    //MODE 3 秒表
{
    if(KeyNum == 2) //开始计时(清零并重新计时)
    {
        //清空实时时间
        NowTime.StopHour = 0;
        NowTime.StopMinute = 0;
        NowTime.StopSecond_0 = 0;
        NowTime.StopSecond_1 = 0;
        NowTime.StopSecond_2 = 0;

        //清空 回看记录 回看值 回看选择位
        ClearStopNum();

        //初始化状态
        StopWatchFlag = 1;
        StopTimeFlag = 0;
        StopWatchAddFlag = 1;

        //清空中断计数，保证从 0 开始计时
        EA = 0;
        StopCount = 0;
        EA = 1;
    }
    if(KeyNum == 3) //暂停/运行
    {
        StopWatchAddFlag = !StopWatchAddFlag;
    }
    if(KeyNum == 4) //运行/添加回看  暂停/回看列表 无记录不回看
    {
        if(StopWatchAddFlag)
        {
            if(StopTimeNum < 10)
            {
                //写入回看值数组
                StopTimeArray[StopTimeNum].StopHour = NowTime.StopHour;
                StopTimeArray[StopTimeNum].StopMinute = NowTime.StopMinute;
                StopTimeArray[StopTimeNum].StopSecond_0 = NowTime.StopSecond_0;
                StopTimeArray[StopTimeNum].StopSecond_1 = NowTime.StopSecond_1;
                StopTimeArray[StopTimeNum].StopSecond_2 = NowTime.StopSecond_2;

                StopTimeNum++;
            }
        }
        else{
            if(StopTimeNum > 0)
            {
                if(StopTimeFlag == 0)   {StopTimeFlag = 1;}
                else if(StopTimeNumSelect == (StopTimeNum - 1))
                {
                    StopTimeNumSelect = 0;
                    StopTimeFlag = 0;
                } 
                else    {StopTimeNumSelect++;}
            }
        }
    }
    //暂停时清空计数，恢复运行后从 0 重新累积
    if(!StopWatchAddFlag)
    {StopCount = 0;}

    //秒表累积
    if(StopWatchAddFlag)
    {
        unsigned int Ticks;

        // StopCount 是 16 位变量，读取+清零必须原子，否则会被中断撕裂
        EA = 0;
        Ticks = StopCount;
        StopCount = 0;
        EA = 1;

        // 1 个 tick = 10ms = 1 百分秒，按位依次进位
        // 百分秒(0~9)
        Ticks += NowTime.StopSecond_2;
        NowTime.StopSecond_2 = Ticks % 10;  Ticks /= 10;
        // 十分秒(0~9)
        Ticks += NowTime.StopSecond_1;
        NowTime.StopSecond_1 = Ticks % 10;  Ticks /= 10;
        // 秒(0~59)
        Ticks += NowTime.StopSecond_0;
        NowTime.StopSecond_0 = Ticks % 60;  Ticks /= 60;
        // 分(0~59)
        Ticks += NowTime.StopMinute;
        NowTime.StopMinute = Ticks % 60;    Ticks /= 60;
        // 时(0~99)
        Ticks += NowTime.StopHour;
        NowTime.StopHour = Ticks % 100;
    }

    //实时显示 时分秒/10/100
    if(StopWatchFlag)
    {
        if(StopTimeFlag)    //回看显示
        {
            LCD_ShowNum(1,1,StopTimeArray[StopTimeNumSelect].StopHour,2);
            LCD_ShowNum(1,4,StopTimeArray[StopTimeNumSelect].StopMinute,2);
            LCD_ShowNum(2,1,StopTimeArray[StopTimeNumSelect].StopSecond_0,2);
            LCD_ShowNum(2,4,StopTimeArray[StopTimeNumSelect].StopSecond_1,1);
            LCD_ShowNum(2,5,StopTimeArray[StopTimeNumSelect].StopSecond_2,1);

            //显示回看选择位
            LCD_ShowString(2,7,"T");
            LCD_ShowNum(2,8,StopTimeNumSelect,1);
        }
        else    //实时显示
        {
            LCD_ShowNum(1,1,NowTime.StopHour,2);
            LCD_ShowNum(1,4,NowTime.StopMinute,2);
            LCD_ShowNum(2,1,NowTime.StopSecond_0,2);
            LCD_ShowNum(2,4,NowTime.StopSecond_1,1);
            LCD_ShowNum(2,5,NowTime.StopSecond_2,1);

            //清空回看选择位
            LCD_ShowString(2,7,"  ");
        }
    }
}

void main()
{
    //初始化定时器
    //Timer0_Init();
    Timer2_Init();

    //初始化时钟模块
    //原代码上电无条件 DS1302_SetTime()，会把 RTC 重置为初值，
    //使 DS1302 掉电保持走时完全失效。改为仅在检测到未初始化时写入。
    DS1302_Init();
    if(DS1302_IsRunning())  //已有合法时间(电池维持)，直接沿用
    {
        DS1302_ReadTime();
    }
    else                    //首次上电或数据非法，写入初值
    {
        DS1302_SetTime();
    }

    //初始化LCD模块
    LCD_Init();
    LCD_ShowString(1,1,"  -  -   MODE");
    LCD_ShowString(2,1,"  :  :   SHOW");

    while(1)
    {
        //change MODE
        KeyNum = Key();

        //闹钟响铃中：蜂鸣由 Timer1 中断产生，剩余时长由 Timer2 中断递减。
        //主循环不再被 Delay 卡住，所以响铃期间走时和按键都保持正常。
        if(BellFlag)
        {
            if(BellRemain == 0 || KeyNum != 0)  //到点，或按任意键提前停止
            {
                FMQBeepStop();
                BellFlag = 0;
                BellRemain = 0;

                MODE = 0;                       //回到时间显示
                LCD_ShowString(1,1,"  -  -   MODE");
                LCD_ShowString(2,1,"  :  :   SHOW ");
                StopWatchFlag = 0;              //清零秒表模式
            }

            TimeShow();     //响铃期间照常走时
            continue;       //本轮按键已用于停止响铃，不再参与模式切换
        }

        if(KeyNum == 1)
        {
            if(MODE == 0){
                MODE = 1;   //进入 TimeSet

                //初始化模式显示
                LCD_ShowString(1,1,"  -  -   MODE");
                LCD_ShowString(2,1,"  :  :   SET ");

                //初始化时间位
                TimeSetSelect = 0;
            }
            else if(MODE == 1){
                MODE = 2;   //进入 AlarmSet
                
                //将新设置的时间写入DS1302
                DS1302_SetTime();

                //初始化模式显示
                LCD_ShowString(1,1,"  -  -   MODE");
                LCD_ShowString(2,1,"  :  :   ALARM");

                //初始化数字显示
                LCD_ShowNum(1,1,DS1302_Time[0],2);
                LCD_ShowNum(1,4,DS1302_Time[1],2);
                LCD_ShowNum(1,7,DS1302_Time[2],2);
                LCD_ShowNum(2,1,DS1302_Time[3],2);
                LCD_ShowNum(2,4,DS1302_Time[4],2);
                LCD_ShowNum(2,7,DS1302_Time[5],2);

                //初始化时间位
                TimeSetSelect = 0;    
            }
            else if (MODE == 2){
                MODE = 3;   //进入 StopWatch

                //初始化模式显示
                LCD_ShowString(1,1,"00:00    MODE");
                LCD_ShowString(2,1,"00.00    S-W  ");
            }
            else if (MODE == 3){
                MODE = 0;   //进入 TimeShow

                //初始化模式显示
                LCD_ShowString(1,1,"  -  -   MODE");
                LCD_ShowString(2,1,"  :  :   SHOW");

                //清零秒表模式
                StopWatchFlag = 0;
            }
        }

        //闹钟检测
        if(Alarm && (MODE == 0))
        {
            CheakFlag = Cheak();

            if(CheakFlag)
            {
                CheakFlag = 0;
                Alarm = 0;

                //进入响铃状态。数字部分紧接着由下方的 TimeShow() 刷新，
                //这里只改行尾的模式标签(TimeShow 不写这两列)。
                LCD_ShowString(1,1,"  -  -   MODE");
                LCD_ShowString(2,1,"  :  :   BELL ");

                BellFlag = 1;
                FMQBeepStart();

                EA = 0;                     //BellRemain 是 16 位，赋值时避免被中断打断
                BellRemain = ALARM_BEEP_MS;
                EA = 1;
            }
        }
        
        //MODE 显示
        switch(MODE)
        {
            case 0: TimeShow(); break;
            case 1: TimeSet();  break;
            case 2: AlarmSet(); break;
            case 3: StopWatch();break;
        }
    }
}

//中断部分
// void Timer0_ISR(void) interrupt 1 
// {
//     static unsigned int T0Count;

//     TH0 = 0xFC;        
//     TL0 = 0x67;
    
//     T0Count++;
//     if(T0Count >= 500)
//     {
//         T0Count = 0;
//         //中断内容
//         TimeSetFlashFlag = !TimeSetFlashFlag;   //控制时间闪烁
//     }
// }

void Timer1_ISR(void) interrupt 3
{
    TH1 = TIMER_1MS_H;
    TL1 = TIMER_1MS_L;

    FMQ = !FMQ;
}

void Timer2_ISR(void) interrupt 5 
{
    static unsigned int T0Count;
    static unsigned int T1Count;

    TF2 = 0;    
    
    T0Count++;
    if(T0Count >= 500)
    {
        T0Count = 0;
        TimeSetFlashFlag = !TimeSetFlashFlag;
    }    
 
    T1Count++;
    if(T1Count >= 10)
    {
        T1Count = 0;
        StopCount++;
    }

    //响铃倒计时：本中断 1ms 一次
    if(BellRemain > 0) {BellRemain--;}
}

//函数定义部分
void ClearStopNum()
{
	unsigned char i;
	
    StopTimeNum = 0;
    StopTimeNumSelect = 0;

    for(i = 0; i < 10; i++)
    {
        StopTimeArray[i].StopHour = 0;
        StopTimeArray[i].StopMinute = 0;
        StopTimeArray[i].StopSecond_0 = 0;
        StopTimeArray[i].StopSecond_1 = 0;
        StopTimeArray[i].StopSecond_2 = 0;
    }
}

//当月最大天数。Year 为两位年份(00 表示 2000 年)，沿用原代码的 %4 闰年规则。
unsigned char DaysInMonth(signed char Year, signed char Month)
{
    if(Month == 1 || Month == 3 || Month == 5 || Month == 7 ||
       Month == 8 || Month == 10 || Month == 12)
    {return 31;}

    if(Month == 4 || Month == 6 || Month == 9 || Month == 11)
    {return 30;}

    if(Month == 2)
    {
        if(Year % 4 == 0) {return 29;}
        return 28;
    }

    return 31;  //月已先钳到 1~12，正常走不到这里
}

//把 年 月 日 时 分 秒 统一钳回合法范围。
//调用前已对目标位做过 ++/--，这里修正整个数组（改月份会连带修正日）。
//两种回绕都处理：上溢取最小值(如 分 60→0)，下溢取最大值(如 分 -1→59)。
void Time_Clamp(signed char *t)
{
    unsigned char MaxDay;

    if(t[0] > 99) {t[0] = 0;}        //年 0~99
    if(t[0] < 0)  {t[0] = 99;}

    if(t[1] > 12) {t[1] = 1;}        //月 1~12
    if(t[1] < 1)  {t[1] = 12;}

    MaxDay = DaysInMonth(t[0], t[1]);
    if(t[2] > MaxDay) {t[2] = 1;}    //日 1~当月天数
    if(t[2] < 1)      {t[2] = MaxDay;}

    if(t[3] > 23) {t[3] = 0;}        //时 0~23
    if(t[3] < 0)  {t[3] = 23;}

    if(t[4] > 59) {t[4] = 0;}        //分 0~59
    if(t[4] < 0)  {t[4] = 59;}

    if(t[5] > 59) {t[5] = 0;}        //秒 0~59
    if(t[5] < 0)  {t[5] = 59;}
}

//闹钟不得早于当前时间。从年开始逐位比较：
//比当前小的位抬到当前值，比当前大的位即可停止(高位已决定闹钟在未来)。
//与原代码 6 段递进 if 等价。
void Alarm_ClampToNow(signed char *t)
{
    unsigned char i;

    for(i = 0; i < 6; i++)
    {
        if(t[i] < DS1302_Time[i])      {t[i] = DS1302_Time[i];}
        else if(t[i] > DS1302_Time[i]) {break;}
    }
}

//开始/停止蜂鸣。发声由 Timer1 中断翻转 P2^5 产生，这里不做任何延时，
//主循环可以继续处理按键和刷新显示。
void FMQBeepStart(void)
{
    Timer1_Init();
}

void FMQBeepStop(void)
{
    TR1 = 0;
    // P2^5 与 LCD_RW 是同一根引脚：拉低既停止蜂鸣，也等价于恢复 LCD_RW=0(写模式)
    FMQ = 0;
}

int Cheak()
{
    //取最新的 RTC 值再比较，避免用到上一轮循环的旧时间(闹钟会晚一拍)
    DS1302_ReadTime();
    if(DS1302_Time[0] == AlarmTime[0] && DS1302_Time[1] == AlarmTime[1] &&
       DS1302_Time[2] == AlarmTime[2] && DS1302_Time[3] == AlarmTime[3] &&
       DS1302_Time[4] == AlarmTime[4] && DS1302_Time[5] == AlarmTime[5])
    {return 1;}
    else
    {return 0;}
}