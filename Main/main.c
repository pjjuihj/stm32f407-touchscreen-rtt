#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"

/*********************************************************************************
*********************启明欣欣 STM32F407应用开发板(高配版)*************************
**********************************************************************************
* 文件名称: 例程3 按键使用主函数main()                                           *
* 文件简述：按键实验                                                             *
* 创建日期：2017.08.30                                                           *
* 版    本：V1.0                                                                 *
* 作    者：Clever                                                               *
* 说    明：按键控制LED亮灭与蜂鸣器开断                                          *
* 淘宝店铺：https://shop125046348.taobao.com                                     *
* 声    明：本例程代码仅用于学习参考                                             *
**********************************************************************************
*********************************************************************************/

int main(void)
{ 
  HAL_Init();                    	//初始化HAL库    
  Stm32_Clock_Init(336,8,2,7);  	//设置时钟,168Mhz
	delay_init();     //延时函数初始化
	LED_Init();				//LED初始化
	BEEP_Init();      //蜂鸣器初始化
	KEY_Init();       //按键初始化
 	LCD_Init();           //初始化LCD FSMC接口和显示驱动
	Touch_Init();				//触摸屏初始化
 	BRUSH_COLOR=RED;    //设置字体为红色 
	LCD_DisplayString(10,10,16,"Illuminati STM32");	
  LCD_DisplayString(20,40,24,"Author:Clever");
	LCD_DisplayString(30,80,24,"19.TOUCH TEST");

	delay_ms(1000);
	
 	Clear_Screen();	       //先清屏 	
  if(lcd_id==0x9341)
	   R_Touch_test();     //电阻屏画板测试测试
	else if(lcd_id==0x1963)
	   C_Touch_test(); 		 //电容屏画板测试测试		
}

