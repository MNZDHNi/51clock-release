#include <REGX52.H>

sbit DS1302_SCLK = P3^6;
sbit DS1302_CE = P3^5;
sbit DS1302_IO = P3^4;

//写command表
#define DS1302_SECOND 0x80
#define DS1302_MINUTE 0x82
#define DS1302_HOUR   0x84
#define DS1302_DATE   0x86
#define DS1302_MONTH  0x88
#define DS1302_DAY    0x8A
#define DS1302_YEAR   0x8C
#define DS1302_WP     0x8E

//必须用 signed char：Keil C51 的 char 默认无符号，界面上"减到 0 再 -1"的
//下溢判断(if(x < 0))依赖有符号语义，否则会回绕成 255。
signed char DS1302_Time[] = {25,1,1,0,0,0,3};    //年月日 时分秒 星期

void DS1302_Init(void)
{
    DS1302_CE = 0;
    DS1302_SCLK = 0;
}

//仅供本文件内部使用的底层读写
static void DS1302_WriteByte(unsigned char Command, unsigned char Data)
{
	unsigned char i;
    DS1302_CE = 1;
    
    for(i = 0; i < 8; i++)
    {
        DS1302_IO = Command & (0x01 << i);    //0000 0001
        DS1302_SCLK = 1;
        DS1302_SCLK = 0;
    }
    
    for(i = 0; i < 8; i++)
    {
        DS1302_IO = Data & (0x01 << i);
        DS1302_SCLK = 1;
        DS1302_SCLK = 0;
    }
    DS1302_CE = 0;
}

static unsigned char DS1302_ReadByte(unsigned char Command)
{
    unsigned char i, Data = 0x00;
    Command |= 0x01;    //将写command切换为读
    DS1302_CE = 1;
    
    for(i = 0; i < 8; i++)
    {
        DS1302_IO = Command & (0x01 << i);
        DS1302_SCLK = 0;    //和写入时相反，SCLk时序相关
        DS1302_SCLK = 1;
    }
    
    DS1302_IO = 1;    //释放 IO 线：8051 准双向口读引脚前必须先把锁存器置 1，
                      //否则内部下拉管仍导通，会和 DS1302 的输出对拉。

    for(i = 0; i < 8; i++)
    {
        DS1302_SCLK = 1;    //重复置1，过滤
        DS1302_SCLK = 0;
        if(DS1302_IO) {Data |= (0x01 << i);}
    }
     DS1302_CE = 0;
     DS1302_IO = 0;
     
     return Data;
}

void DS1302_SetTime(void)
{
    DS1302_WriteByte(DS1302_WP,0x00);    //关闭写保护
    DS1302_WriteByte(DS1302_YEAR, DS1302_Time[0]/10*16+DS1302_Time[0]%10);
    DS1302_WriteByte(DS1302_MONTH, DS1302_Time[1]/10*16+DS1302_Time[1]%10);
    DS1302_WriteByte(DS1302_DATE, DS1302_Time[2]/10*16+DS1302_Time[2]%10);
    DS1302_WriteByte(DS1302_HOUR, DS1302_Time[3]/10*16+DS1302_Time[3]%10);
    DS1302_WriteByte(DS1302_MINUTE, DS1302_Time[4]/10*16+DS1302_Time[4]%10);
    DS1302_WriteByte(DS1302_SECOND, DS1302_Time[5]/10*16+DS1302_Time[5]%10);
    DS1302_WriteByte(DS1302_DAY, DS1302_Time[6]/10*16+DS1302_Time[6]%10);
    DS1302_WriteByte(DS1302_WP,0x80);    //打开写保护
}

void DS1302_ReadTime(void)
{
    unsigned char Temp;
    Temp = DS1302_ReadByte(DS1302_YEAR);
    DS1302_Time[0] = Temp/16*10+Temp%16;
    Temp = DS1302_ReadByte(DS1302_MONTH);
    DS1302_Time[1] = Temp/16*10+Temp%16;
    Temp = DS1302_ReadByte(DS1302_DATE);
    DS1302_Time[2] = Temp/16*10+Temp%16;
    //bit7 是 12/24 小时制选择位，置位表示芯片处于 12 小时制。
    //屏蔽掉再按 BCD 解析，避免 24 小时制下被误读成比实际大 80 的值。
    Temp = DS1302_ReadByte(DS1302_HOUR) & 0x7F;
    DS1302_Time[3] = Temp/16*10+Temp%16;
    Temp = DS1302_ReadByte(DS1302_MINUTE);
    DS1302_Time[4] = Temp/16*10+Temp%16;
    Temp = DS1302_ReadByte(DS1302_SECOND);
    DS1302_Time[5] = Temp/16*10+Temp%16;
    Temp = DS1302_ReadByte(DS1302_DAY);
    DS1302_Time[6] = Temp/16*10+Temp%16;
}

//判断 DS1302 是否已有一个可用的时间：
//  1) 秒寄存器 bit7(CH 位)为 0 —— 振荡器在走，说明芯片已被初始化过；
//  2) 读出的月/日落在合法范围。
//用于替代"每次上电都写初值"的做法，避免覆盖电池维持的真实时间。
unsigned char DS1302_IsRunning(void)
{
    unsigned char Second, Month, Date;

    Second = DS1302_ReadByte(DS1302_SECOND);
    if(Second & 0x80) {return 0;}           //CH=1：振荡器停止 / 芯片全新

    Month = DS1302_ReadByte(DS1302_MONTH);
    Date  = DS1302_ReadByte(DS1302_DATE);
    Month = Month/16*10 + Month%16;
    Date  = Date/16*10 + Date%16;

    if(Month < 1 || Month > 12) {return 0;}
    if(Date  < 1 || Date  > 31) {return 0;}

    return 1;
}