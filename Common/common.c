#include "common.h"

/*********************************************************************************
************************�������� STM32F407���Ŀ�����******************************
**********************************************************************************
* �ļ�����: common.c                                                             *
* �ļ���������������������õĹ����ļ�                                           *
* �������ڣ�2015.03.03                                                           *
* ��    ����V1.0                                                                 *
* ��    �ߣ�Clever                                                               *
* ˵    ���������������Ͷ��塢IO��λ���塢λ�ζ�������ʱ��������                 * 
**********************************************************************************
*********************************************************************************/	  

/****************************************************************************
* ��    ��: void GPIO_group_OUT(_gpio_group *group,u16 outdata)
* ��    �ܣ�ʹ������16��IO�����һ��16λ���������
* ��ڲ�����*group�� ����16��IO��ΪԪ�صĽṹ��ָ��
            outdata: 16λ�����ֵ
* ���ز�������
* ˵    ����outdata�Ӹ�λ��ʼ��ֵ
****************************************************************************/
void GPIO_group_OUT(_gpio_group *group,u16 outdata)
{
  u8 t;
	for(t=0;t<16;t++)
    {               
        if((outdata&0x8000)>>15)  
				{
						switch(t)
						{
								case 0:	   group->data15=1; break;
								case 1:	   group->data14=1; break;
								case 2:	   group->data13=1; break;
								case 3:	   group->data12=1; break;
								case 4:	   group->data11=1; break;
								case 5:	   group->data10=1; break;
								case 6:	   group->data9=1;  break;
								case 7:	   group->data8=1;  break;
								case 8:	   group->data7=1;  break;
								case 9:	   group->data6=1;  break;
								case 10:	 group->data5=1;  break;
								case 11:	 group->data4=1;  break;
								case 12:	 group->data3=1;  break;
								case 13:	 group->data2=1;  break;
								case 14:	 group->data1=1;  break;
								case 15:	 group->data0=1;  break;
						}
				}
				else
				{
				  switch(t)
						{
								case 0:	   group->data15=0; break;
								case 1:	   group->data14=0; break;
								case 2:	   group->data13=0; break;
								case 3:	   group->data12=0; break;
								case 4:	   group->data11=0; break;
								case 5:	   group->data10=0; break;
								case 6:	   group->data9=0;  break;
								case 7:	   group->data8=0;  break;
								case 8:	   group->data7=0;  break;
								case 9:	   group->data6=0;  break;
								case 10:	 group->data5=0;  break;
								case 11:	 group->data4=0;  break;
								case 12:	 group->data3=0;  break;
								case 13:	 group->data2=0;  break;
								case 14:	 group->data1=0;  break;
								case 15:	 group->data0=0;  break;
						}
				}
     outdata<<=1; 	
	  }
}

/****************************************************************************
* ��    ��: void GPIO_bits_OUT(GPIO_TypeDef* GPIOx, u8 start_bit, u8 bit_size,u16 outdata)
* ��    �ܣ�λ�β���ʵ�֣�ͬһIO�ڵļ�λ�����������
* ��ڲ�����* GPIOx��  ��Ӧ��IO��
*           start_bit: �����������ʼλ
*           bit_size:  Ҫ���������λ��
* ���ز�������
* ˵    ����start_bit: 0~14
            bit_size:  1~16 
            bit_size<=16-start_bit
****************************************************************************/
void GPIO_bits_OUT(GPIO_TypeDef* GPIOx, u8 start_bit, u8 bit_size,u16 outdata)
{
  u8 i=0;
	u16 bu1=0;u16 middata=1;

	if( bit_size>(16-start_bit) ) 
     bit_size=16-start_bit;
	
	i=start_bit;
	if(i>0)
		 {
			 while(i--)
         { bu1+=middata; middata*=2;}
		 }
   
	 GPIOx->ODR&=(  ( (0xffff<<bit_size) <<start_bit ) |bu1   ); 
	 GPIOx->ODR|=(outdata<<start_bit);		 
}

/*****************************************************************************
**********************���´���ο����ϣ�����ѧϰ�ο�**************************
*****************************************************************************/
/****************************************************************************
* ��    ��: void Stm32_Clock_Init(u32 plln,u32 pllm,u32 pllp,u32 pllq)
* ��    �ܣ�ʱ��ϵͳ���ú���
* ��ڲ�����Fvco:VCOƵ��
            SYSCLK:ϵͳʱ��Ƶ��
            Fusb:USB,SDIO,RNG�ȵ�ʱ��Ƶ��
            Fs:PLL����ʱ��Ƶ��,������HSI,HSE��. 
            plln:��PLL��Ƶϵ��(PLL��Ƶ),ȡֵ��Χ:64~432.
            pllm:��PLL����ƵPLL��Ƶϵ��(PLL֮ǰ�ķ�Ƶ),ȡֵ��Χ:2~63.
            pllp:ϵͳʱ�ӵ���PLL��Ƶϵ��(PLL֮��ķ�Ƶ),ȡֵ��Χ:2,4,6,8.(������4��ֵ!)
            pllq:USB/SDIO/������������ȵ���PLL��Ƶϵ��(PLL֮��ķ�Ƶ),ȡֵ��Χ:2~15.
* ���ز�������
* ˵    ����Fvco=Fs*(plln/pllm);
            SYSCLK=Fvco/pllp=Fs*(plln/(pllm*pllp));
            Fusb=Fvco/pllq=Fs*(plln/(pllm*pllq));�ⲿ����Ϊ8M��ʱ��,�Ƽ�ֵ:plln=336,pllm=8,pllp=2,pllq=7.
            �õ�:Fvco=8*(336/8)=336Mhz
            SYSCLK=336/2=168Mhz
            Fusb=336/7=48Mhz
****************************************************************************/
void Stm32_Clock_Init(u32 plln,u32 pllm,u32 pllp,u32 pllq)
{
    HAL_StatusTypeDef ret = HAL_OK;
    RCC_OscInitTypeDef RCC_OscInitStructure; 
    RCC_ClkInitTypeDef RCC_ClkInitStructure;
    
    __HAL_RCC_PWR_CLK_ENABLE(); //ʹ��PWRʱ��
    
    RCC_OscInitStructure.OscillatorType=RCC_OSCILLATORTYPE_HSE;    //ʱ��ԴΪHSE
    RCC_OscInitStructure.HSEState=RCC_HSE_ON;                      //��HSE
    RCC_OscInitStructure.PLL.PLLState=RCC_PLL_ON;//��PLL
    RCC_OscInitStructure.PLL.PLLSource=RCC_PLLSOURCE_HSE;//PLLʱ��Դѡ��HSE
    RCC_OscInitStructure.PLL.PLLM=pllm; //��PLL����ƵPLL��Ƶϵ��(PLL֮ǰ�ķ�Ƶ),ȡֵ��Χ:2~63.
    RCC_OscInitStructure.PLL.PLLN=plln; //��PLL��Ƶϵ��(PLL��Ƶ),ȡֵ��Χ:64~432.  
    RCC_OscInitStructure.PLL.PLLP=pllp; //ϵͳʱ�ӵ���PLL��Ƶϵ��(PLL֮��ķ�Ƶ),ȡֵ��Χ:2,4,6,8.(������4��ֵ!)
    RCC_OscInitStructure.PLL.PLLQ=pllq; //USB/SDIO/������������ȵ���PLL��Ƶϵ��(PLL֮��ķ�Ƶ),ȡֵ��Χ:2~15.
    ret=HAL_RCC_OscConfig(&RCC_OscInitStructure);//��ʼ��
	
    if(ret!=HAL_OK) while(1);
    
    //ѡ��PLL��Ϊϵͳʱ��Դ��������HCLK,PCLK1��PCLK2
    RCC_ClkInitStructure.ClockType=(RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStructure.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;//����ϵͳʱ��ʱ��ԴΪPLL
    RCC_ClkInitStructure.AHBCLKDivider=RCC_SYSCLK_DIV1;//AHB��Ƶϵ��Ϊ1
    RCC_ClkInitStructure.APB1CLKDivider=RCC_HCLK_DIV4; //APB1��Ƶϵ��Ϊ4
    RCC_ClkInitStructure.APB2CLKDivider=RCC_HCLK_DIV2; //APB2��Ƶϵ��Ϊ2
    ret=HAL_RCC_ClockConfig(&RCC_ClkInitStructure,FLASH_LATENCY_5);//ͬʱ����FLASH��ʱ����Ϊ5WS��Ҳ����6��CPU���ڡ�

    if(ret!=HAL_OK) while(1);

    //STM32F405x/407x/415x/417x Z�汾������֧��Ԥȡ����
    if (HAL_GetREVID() == 0x1001)
    {
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();  //ʹ��flashԤȡ
    }
}


//THUMBָ� support WFI instruction
void WFI_SET(void)
{
	__asm volatile("WFI");
}
// Disable all interrupts (except fault and NMI)
void INTX_DISABLE(void)
{
	__asm volatile("CPSID I");
}
// Enable all interrupts
void INTX_ENABLE(void)
{
	__asm volatile("CPSIE I");
}
// Set Main Stack Pointer
void MSR_MSP(u32 addr)
{
	// 简单实现，直接赋值
	*(volatile uint32_t *)0xE000ED08 = addr;
}

//����ϵͳ�δ�ʱ����д����ʱ����

static u8  fac_us=0; //us��ʱ������			   

/****************************************************************************
* ��    ��: delay_init()
* ��    �ܣ���ʱ������ʼ��
* ��ڲ�������
* ���ز�������
* ˵    ����
****************************************************************************/
void delay_init()
{
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	fac_us=SYSCLK;
	/* 启用DWT周期计数器 */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}								    

/****************************************************************************
* ��    ��: void delay_us(u32 nus)
* ��    �ܣ���ʱnus
* ��ڲ�����Ҫ��ʱ��΢����
* ���ز�������
* ˵    ����nus��ֵ,��Ҫ����798915us
****************************************************************************/
void delay_us(u32 nus)
{
	/* NOP循环延时 + DWT超时保护 */
	u32 i;
	u32 limit = nus * 21;
	if(limit > 1000000) limit = 1000000;  /* 防止溢出 */
	for(i = 0; i < limit; i++) { __NOP(); }
}

/****************************************************************************
* ��    ��: void delay_ms(u16 nms)
* ��    �ܣ���ʱnms
* ��ڲ�����Ҫ��ʱ�ĺ�����
* ���ز�������
* ˵    ���� 
****************************************************************************/
void delay_ms(u16 nms)
{
	u32 i;
	for(i=0;i<nms;i++) delay_us(1000);
} 

			 
