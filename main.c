#include <REGX52.H>
#include "Delay.h"
#include "Key.h"
#include "DS1302.h"
#include "LCD1602.h"
#include "Time.h"

unsigned char KeyNum, MODE, TimeSetSelect, TimeSetFlashFlag;  //键值 模式值 时间位 时间闪烁标志位

//闹钟值定义
unsigned char clock[5]; //年月日 时分秒
char Alarm;             //是否有闹钟
int CheakFlag;          //判断标志位
sbit FMQ = P2^5;        //定义蜂鸣器引脚

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
unsigned char StopCount;            //秒表中断计数器
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
void FMQBeep(unsigned int time);    //蜂鸣器发声
int Cheak();                        //检测闹钟时间是否达到

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

        //输入合法性判断
        if(DS1302_Time[0] > 99) {DS1302_Time[0] = 0;}
        if(DS1302_Time[1] > 12) {DS1302_Time[1] = 1;}

        if(DS1302_Time[1] == 1 || DS1302_Time[1] == 3 || DS1302_Time[1] == 5 || DS1302_Time[1] == 7 ||
           DS1302_Time[1] == 8 || DS1302_Time[1] == 10 || DS1302_Time[1] == 12)
        {
            if(DS1302_Time[2] > 31) {DS1302_Time[2] = 1;}
        }
        else if(DS1302_Time[1] == 4 || DS1302_Time[1] == 6 || DS1302_Time[1] == 9 || DS1302_Time[1] == 11)
        {
            if(DS1302_Time[2] > 30) {DS1302_Time[2] = 1;}
        }
        else if(DS1302_Time[1] == 2)
        {
            if(DS1302_Time[0] % 4 == 0)
            {
                if(DS1302_Time[2] > 29) {DS1302_Time[2] = 1;}
            }
            else
            {
                if(DS1302_Time[2] > 28) {DS1302_Time[2] = 1;}
            }
        }

        if(DS1302_Time[3] > 23) {DS1302_Time[3] = 0;}
        if(DS1302_Time[4] > 59) {DS1302_Time[4] = 0;}
        if(DS1302_Time[5] > 59) {DS1302_Time[5] = 0;}
    }
    if(KeyNum == 4)
    {
        DS1302_Time[TimeSetSelect]--;

        //输入合法性判断
        if(DS1302_Time[0] < 0) {DS1302_Time[0] = 99;}
        if(DS1302_Time[1] < 1) {DS1302_Time[1] = 12;}

        if(DS1302_Time[1] == 1 || DS1302_Time[1] == 3 || DS1302_Time[1] == 5 || DS1302_Time[1] == 7 ||
           DS1302_Time[1] == 8 || DS1302_Time[1] == 10 || DS1302_Time[1] == 12)
        {
            if(DS1302_Time[2] < 1) {DS1302_Time[2] = 31;}
            if(DS1302_Time[2] > 31) {DS1302_Time[2] = 1;}
        }
        else if(DS1302_Time[1] == 4 || DS1302_Time[1] == 6 || DS1302_Time[1] == 9 || DS1302_Time[1] == 11)
        {
            if(DS1302_Time[2] < 1) {DS1302_Time[2] = 30;}
            if(DS1302_Time[2] > 30) {DS1302_Time[2] = 1;}
        }
        else if(DS1302_Time[1] == 2)
        {
            if(DS1302_Time[0] % 4 == 0)
            {
                if(DS1302_Time[2] < 1) {DS1302_Time[2] = 29;}
                if(DS1302_Time[2] > 29) {DS1302_Time[2] = 1;}
            }
            else
            {
                if(DS1302_Time[2] < 1) {DS1302_Time[2] = 28;}
                if(DS1302_Time[2] > 28) {DS1302_Time[2] = 1;}
            }
        }

        if(DS1302_Time[3] < 0) {DS1302_Time[3] = 23;}
        if(DS1302_Time[4] < 0) {DS1302_Time[4] = 59;}
        if(DS1302_Time[5] < 0) {DS1302_Time[5] = 59;}
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
    //设置闹钟时间
    if(KeyNum == 2)
    {
        if(!Alarm)
        {
            Alarm = 1;  //有闹钟

            //赋值clock
            clock[0] = DS1302_Time[0];
            clock[1] = DS1302_Time[1];
            clock[2] = DS1302_Time[2];
            clock[3] = DS1302_Time[3];
            clock[4] = DS1302_Time[4];
            clock[5] = DS1302_Time[5];
        }
        else
        {
            TimeSetSelect++;
            TimeSetSelect %= 6;
        }
    }
    if(KeyNum == 3)
    {
        clock[TimeSetSelect]++;

        //输入合法性一般判断
        if(clock[0] > 99) {clock[0] = 0;}
        if(clock[1] > 12) {clock[1] = 1;}

        if(clock[1] == 1 || clock[1] == 3 || clock[1] == 5 || clock[1] == 7 ||
           clock[1] == 8 || clock[1] == 10 || clock[1] == 12)
        {
            if(clock[2] > 31) {clock[2] = 1;}
        }
        else if(clock[1] == 4 || clock[1] == 6 || clock[1] == 9 || clock[1] == 11)
        {
            if(clock[2] > 30) {clock[2] = 1;}
        }
        else if(clock[1] == 2)
        {
            if(clock[0] % 4 == 0)
            {
                if(clock[2] > 29) {clock[2] = 1;}
            }
            else
            {
                if(clock[2] > 28) {clock[2] = 1;}
            }
        }

        if(clock[3] > 23) {clock[3] = 0;}
        if(clock[4] > 59) {clock[4] = 0;}
        if(clock[5] > 59) {clock[5] = 0;}

        //闹钟时间边界判断
        if(clock[0] < DS1302_Time[0])
        {clock[0] = DS1302_Time[0];}

        if(clock[0] == DS1302_Time[0] && clock[1] < DS1302_Time[1])
        {clock[1] = DS1302_Time[1];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] < DS1302_Time[2])
        {clock[2] = DS1302_Time[2];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] < DS1302_Time[3])
        {clock[3] = DS1302_Time[3];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] == DS1302_Time[3] &&
           clock[4] < DS1302_Time[4])
        {clock[4] = DS1302_Time[4];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] == DS1302_Time[3] &&
           clock[4] == DS1302_Time[4] && clock[5] < DS1302_Time[5])
        {clock[5] = DS1302_Time[5];}
    }
    if(KeyNum == 4)
    {
        clock[TimeSetSelect]--;       

        //一般边界判断
        if(clock[0] < 0) {clock[0] = 99;}
        if(clock[1] < 1) {clock[1] = 12;}

        if(clock[1] == 1 || clock[1] == 3 || clock[1] == 5 || clock[1] == 7 ||
           clock[1] == 8 || clock[1] == 10 || clock[1] == 12)
        {
            if(clock[2] < 1) {clock[2] = 31;}
            if(clock[2] > 31) {clock[2] = 1;}
        }
        else if(clock[1] == 4 || clock[1] == 6 || clock[1] == 9 || clock[1] == 11)
        {
            if(clock[2] < 1) {clock[2] = 30;}
            if(clock[2] > 30) {clock[2] = 1;}
        }
        else if(clock[1] == 2)
        {
            if(clock[0] % 4 == 0)
            {
                if(clock[2] < 1) {clock[2] = 29;}
                if(clock[2] > 29) {clock[2] = 1;}
            }
            else
            {
                if(clock[2] < 1) {clock[2] = 28;}
                if(clock[2] > 28) {clock[2] = 1;}
            }
        }

        if(clock[3] < 0) {clock[3] = 23;}
        if(clock[4] < 0) {clock[4] = 59;}
        if(clock[5] < 0) {clock[5] = 59;} 
    

        //闹钟时间边界判断
        if(clock[0] < DS1302_Time[0])
        {clock[0] = DS1302_Time[0];}

        if(clock[0] == DS1302_Time[0] && clock[1] < DS1302_Time[1])
        {clock[1] = DS1302_Time[1];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] < DS1302_Time[2])
        {clock[2] = DS1302_Time[2];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] < DS1302_Time[3])
        {clock[3] = DS1302_Time[3];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] == DS1302_Time[3] &&
           clock[4] < DS1302_Time[4])
        {clock[4] = DS1302_Time[4];}

        if(clock[0] == DS1302_Time[0] && clock[1] == DS1302_Time[1] &&
           clock[2] == DS1302_Time[2] && clock[3] == DS1302_Time[3] &&
           clock[4] == DS1302_Time[4] && clock[5] < DS1302_Time[5])
        {clock[5] = DS1302_Time[5];}
    }

    if(Alarm)   //进入闹钟设置，开始闪烁
    {
    //闹钟时间实时显示，已写入 clock[]
    if(TimeSetSelect == 0 && TimeSetFlashFlag == 1) {LCD_ShowString(1,1,"  ");}
    else {LCD_ShowNum(1,1,clock[0],2);}
    if(TimeSetSelect == 1 && TimeSetFlashFlag == 1) {LCD_ShowString(1,4,"  ");}
    else {LCD_ShowNum(1,4,clock[1],2);}
    if(TimeSetSelect == 2 && TimeSetFlashFlag == 1) {LCD_ShowString(1,7,"  ");}
    else {LCD_ShowNum(1,7,clock[2],2);}
    if(TimeSetSelect == 3 && TimeSetFlashFlag == 1) {LCD_ShowString(2,1,"  ");}
    else {LCD_ShowNum(2,1,clock[3],2);}
    if(TimeSetSelect == 4 && TimeSetFlashFlag == 1) {LCD_ShowString(2,4,"  ");}
    else {LCD_ShowNum(2,4,clock[4],2);}
    if(TimeSetSelect == 5 && TimeSetFlashFlag == 1) {LCD_ShowString(2,7,"  ");}
    else {LCD_ShowNum(2,7,clock[5],2);}
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
    //进入累积前，清空StopCount
    if(!StopWatchAddFlag)
    {StopCount = 0;}
    
    //秒表累积
    if(StopWatchAddFlag)
    {
        NowTime.StopSecond_2 += StopCount;
        StopCount = 0;

        if(NowTime.StopSecond_2 > 9)
        {
            NowTime.StopSecond_2 = (NowTime.StopSecond_2 - 10);
            NowTime.StopSecond_1++;

            if(NowTime.StopSecond_1 == 10)
            {
                NowTime.StopSecond_1 = 0;
                NowTime.StopSecond_0++;

                if(NowTime.StopSecond_0 == 60)
                {
                    NowTime.StopSecond_0 = 0;
                    NowTime.StopMinute++;

                    if(NowTime.StopMinute == 60)
                    {
                        NowTime.StopMinute = 0;
                        NowTime.StopHour++;

                        if(NowTime.StopHour == 100)
                        {
                            //显示超出边界
                        }
                    }
                }
            }
        }
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
    Timer0_Init();
    Timer2_Init();

    //初始化时钟模块
    DS1302_Init();
    DS1302_SetTime();

    //初始化LCD模块
    LCD_Init();
    LCD_ShowString(1,1,"  -  -   MODE");
    LCD_ShowString(2,1,"  :  :   SHOW");

    while(1)
    {
        //change MODE
        KeyNum = Key();
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
        CheakFlag = Cheak();
        if(CheakFlag && Alarm && (MODE == 0))
        {
            CheakFlag = 0;
            Alarm = 0;

            LCD_ShowString(1,1,"  -  -   MODE");
            LCD_ShowString(2,1,"  :  :   BELL ");

            LCD_ShowNum(1,1,clock[0],2);
            LCD_ShowNum(1,4,clock[1],2);
            LCD_ShowNum(1,7,clock[2],2);
            LCD_ShowNum(2,1,clock[3],2);
            LCD_ShowNum(2,4,clock[4],2);
            LCD_ShowNum(2,7,clock[5],2);

            FMQBeep(1500);

            MODE = 0;   //进入 TimeShow
            //初始化模式显示
            LCD_ShowString(1,1,"  -  -   MODE");
            LCD_ShowString(2,1,"  :  :   SHOW ");
            //清零秒表模式
            StopWatchFlag = 0;
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
void Timer0_ISR(void) interrupt 1 
{
    static unsigned int T0Count;

    TH0 = 0xFC;        
    TL0 = 0x67;
    
    T0Count++;
    if(T0Count >= 500)
    {
        T0Count = 0;
        //中断内容
        TimeSetFlashFlag = !TimeSetFlashFlag;   //控制时间闪烁
    }
}

void Timer1_ISR(void) interrupt 3 
{
    TH1 = 0xFC;        
    TL1 = 0x67;
    
    FMQ = !FMQ;
}

void Timer2_ISR(void) interrupt 5 
{
    static unsigned int T1Count;
    TF2 = 0;           
 
    T1Count++;
    if(T1Count >= 10)
    {
        T1Count = 0;
        StopCount++;
    }
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

void FMQBeep(unsigned int time)
{
    Timer1_Init();

    Delay(time);

    TR1 = 0;
    FMQ = 0;
}

int Cheak()
{
    DS1302_ReadTime();
    if(DS1302_Time[0] == clock[0] && DS1302_Time[1] == clock[1] &&
       DS1302_Time[2] == clock[2] && DS1302_Time[3] == clock[3] &&
       DS1302_Time[4] == clock[4] && DS1302_Time[5] == clock[5])
    {return 1;}
    else
    {return 0;}
}