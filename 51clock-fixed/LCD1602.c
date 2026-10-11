      
#include <REGX52.H>

//引脚配置：
//注意：P2^5 与 main.c 中的蜂鸣器 sbit FMQ 是同一根物理引脚(硬件冲突)。
//      LCD 只写不读，RW 恒为 0；蜂鸣时主循环不会访问 LCD，
//      且 FMQBeepStop() 会把该脚拉低，故实际可正常工作。改动 LCD 时序时务必留意。
sbit LCD_RS=P2^6;
sbit LCD_RW=P2^5;   // 与蜂鸣器 FMQ 共用 P2.5
sbit LCD_EN=P2^7;
#define LCD_DataPort P0

void LCD_Delay()
{
	unsigned char i, j;

	i = 2;
	j = 239;
	do
	{
		while (--j);
	} while (--i);
}

void LCD_WriteCommand(unsigned char Command)
{
	LCD_RS=0;
	LCD_RW=0;
	LCD_DataPort=Command;
	LCD_EN=1;
	LCD_Delay();
	LCD_EN=0;
	LCD_Delay();
}

void LCD_WriteData(unsigned char Data)
{
	LCD_RS=1;
	LCD_RW=0;
	LCD_DataPort=Data;
	LCD_EN=1;
	LCD_Delay();
	LCD_EN=0;
	LCD_Delay();
}

void LCD_SetCursor(unsigned char Line,unsigned char Column)
{
	if(Line==1)
	{
		LCD_WriteCommand(0x80|(Column-1));
	}
	else if(Line==2)
	{
		LCD_WriteCommand(0x80|(Column-1+0x40));
	}
}

void LCD_Init()
{
	LCD_WriteCommand(0x38);//八位数据接口，两行显示，5*7点阵
	LCD_WriteCommand(0x0c);//显示开，光标关，闪烁关
	LCD_WriteCommand(0x06);//数据读写操作后，光标自动加一，画面不动
	LCD_WriteCommand(0x01);//光标复位，清屏
}

void LCD_ShowString(unsigned char Line,unsigned char Column,char *String)
{
	unsigned char i;
	LCD_SetCursor(Line,Column);
	for(i=0;String[i]!='\0';i++)
	{
		LCD_WriteData(String[i]);
	}
}

void LCD_ShowNum(unsigned char Line,unsigned char Column,unsigned int Number,unsigned char Length)
{
	unsigned char i;
	unsigned char Buf[5];	//unsigned int 最多 5 位

	if(Length > 5) {Length = 5;}

	LCD_SetCursor(Line,Column);

	//先按低位到高位取出各位数字...
	for(i = Length; i > 0; i--)
	{
		Buf[i-1] = Number % 10;
		Number /= 10;
	}
	//...再按显示顺序写出。共 Length 次除法，无需反复计算 10 的幂。
	for(i = 0; i < Length; i++)
	{
		LCD_WriteData(Buf[i] + '0');
	}
}