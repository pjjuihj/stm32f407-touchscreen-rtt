#include "touch.h"
#include "ft5426.h"
#include "xpt2046.h"
#include "lcd.h"
#include "led.h"
#include "log.h"

/*********************************************************************************
*********************�������� STM32F407Ӧ�ÿ�����(�����)*************************
**********************************************************************************
* �ļ�����: touch.c                                                              *
* �ļ����������������Գ���                                                       *
* �������ڣ�2018.08.30                                                           *
* ��    ����V1.0                                                                 *
* ��    �ߣ�Clever                                                               *
* ˵    ����                                                                     * 
**********************************************************************************
*********************************************************************************/

//��������ʼ��
void Touch_Init(void)
{
	if(lcd_id==0x9341)
	{
	  XPT2046_Init();
	}
	else if(lcd_id==0x1963)
	{
	  FT5426_Init();
	}
}

//�����Ļ
void Clear_Screen(void)
{
	LCD_Clear(WHITE);//����   
 	BRUSH_COLOR=BLUE;//��������Ϊ��ɫ 
	LCD_DisplayString(lcd_width-40,lcd_height-18,16,"Clear");//��ʾ��������
  BRUSH_COLOR=RED;//���û�����ɫ 
}

/****************************************************************************
* ��    ��: u8 Draw_Point(u16 x,u16 y,u16 color)
* ��    �ܣ���һ����(4*4�ĵ�)	
* ��ڲ�����x,y:����
            color:��ɫ
* ���ز���: ��  
* ˵    ����        
****************************************************************************/
void Draw_Point(u16 x,u16 y,u16 color)
{	    	
	u8 i=0;

	BRUSH_COLOR=color;
	for(i=0;i<4;i++)
	{
	 LCD_DrawPoint(x,y+i);
	 LCD_DrawPoint(x+1,y+i);
	 LCD_DrawPoint(x+2,y+i);
	 LCD_DrawPoint(x+3,y+i);
	} 	  	
}	

//���败�������Ժ���
void R_Touch_test(void)
{
	u8 i=0;	  
	while(1)
	{
		XPT2046_Scan(0); 		 
	
		 	if(Xdown<lcd_width&&Ydown<lcd_height)
			{
				if(Xdown>(lcd_width-40)&&Ydown>lcd_height-18)
				{
					Clear_Screen();
					Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_INFO, "Screen cleared");
				}
				else
				{
					Draw_Point(Xdown,Ydown,RED);
					Log_Write(LOG_MODULE_TOUCH, LOG_LEVEL_DEBUG,
					          "Touch: x=%d, y=%d", Xdown, Ydown);
				}
			} 
    			
		i++;
		if(i%20==0)
			LED0=!LED0;
	}
}

//���ݴ��������Ժ���
void C_Touch_test(void)
{
	u8 i=0;	  	    

	while(1)
	{
		FT5426_Scan();
	 
		if(x[0]>(lcd_width-40)&&y[0]>lcd_height-24)
			{
				Clear_Screen();//����
			}

		i++;
		if(i%20==0)
			 LED0=!LED0;
	}	
}

