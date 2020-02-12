#include "sys.h"
#include "delay.h" 
#include "led.h"  
#include "usart.h" 
#include "lcd.h" 
#include "ltdc.h"   
#include "sdram.h"    
#include "key.h" 
#include "malloc.h" 
#include "w25qxx.h"    
#include "ff.h"  
#include "exfuns.h"  
#include "text.h"	   
#include "ov5640.h" 
#include "dcmi.h"  
#include "pcf8574.h"
#include "atk_qrdecode.h"
//ALIENTEK ������STM32F429������ ��չʵ��SE01
//��ά��/������ʶ��ʵ�� - �Ĵ�����
//����֧�֣�www.openedv.com
//������������ӿƼ����޹�˾ 

u16 qr_image_width;						//����ʶ��ͼ��Ŀ�ȣ�����=��ȣ�
u8 	readok=0;							//�ɼ���һ֡���ݱ�ʶ
u32 *dcmi_line_buf[2];					//����ͷ����һ��һ�ж�ȡ,�����л���  
u16 *rgb_data_buf;						//RGB565֡����buf 
u16 dcmi_curline=0;						//����ͷ�������,��ǰ�б��						

//����ͷ����DMA��������жϻص�����
void qr_dcmi_rx_callback(void)
{  
	u32 *pbuf;
	u16 i;
	pbuf=(u32*)(rgb_data_buf+dcmi_curline*qr_image_width);//��rgb_data_buf��ַƫ�Ƹ�ֵ��pbuf
	
	if(DMA2_Stream1->CR&(1<<19))//DMAʹ��buf1,��ȡbuf0
	{ 
		for(i=0;i<qr_image_width/2;i++)
		{
			pbuf[i]=dcmi_line_buf[0][i];
		} 
	}else 										//DMAʹ��buf0,��ȡbuf1
	{
		for(i=0;i<qr_image_width/2;i++)
		{
			pbuf[i]=dcmi_line_buf[1][i];
		} 
	} 
	dcmi_curline++;
}

//��ʾͼ��
void qr_show_image(u16 xoff,u16 yoff,u16 width,u16 height,u16 *imagebuf)
{
	u16 linecnt=yoff;
	
	if(lcdltdc.pwidth!=0)//RGB��
	{
		for(linecnt=0;linecnt<height;linecnt++)
		{
			LTDC_Color_Fill(xoff,linecnt+yoff,xoff+width-1,linecnt+yoff,imagebuf+linecnt*width);//RGB��,DM2D��� 
		}
		
	}else LCD_Color_Fill(xoff,yoff,xoff+width-1,yoff+height-1,imagebuf);	//MCU��,ֱ����ʾ
}

//imagewidth:<=240;����240ʱ,��240��������
//imagebuf:RGBͼ�����ݻ�����
void qr_decode(u16 imagewidth,u16 *imagebuf)
{
	static u8 bartype=0; 
	u8 *bmp;
	u8 *result=NULL;
	u16 Color;
	u16 i,j;	
	u16 qr_img_width=0;						//����ʶ������ͼ����,��󲻳���240!
	u8 qr_img_scale=0;						//ѹ����������
	
	if(imagewidth>240)
	{
		if(imagewidth%240)return ;	//����240�ı���,ֱ���˳�
		qr_img_width=240;
		qr_img_scale=imagewidth/qr_img_width;
	}else
	{
		qr_img_width=imagewidth;
		qr_img_scale=1;
	}  
	result=mymalloc(SRAMIN,1536);//����ʶ��������ڴ�
	bmp=mymalloc(SRAMCCM,qr_img_width*qr_img_width);//CCM�����ڴ�Ϊ60K��������������240*240=56K 
	mymemset(bmp,0,qr_img_width*qr_img_width);
	if(lcdltdc.pwidth==0)//MCU��,���辵��
	{ 
		for(i=0;i<qr_img_width;i++)		
		{
			for(j=0;j<qr_img_width;j++)		//��RGB565ͼƬת�ɻҶ�
			{	
				Color=*(imagebuf+((i*imagewidth)+j)*qr_img_scale); //����qr_img_scaleѹ����240*240
				*(bmp+i*qr_img_width+j)=(((Color&0xF800)>> 8)*76+((Color&0x7E0)>>3)*150+((Color&0x001F)<<3)*30)>>8;
			}		
		}
	}else	//RGB��,��Ҫ����
	{
		for(i=0;i<qr_img_width;i++)		
		{
			for(j=0;j<qr_img_width;j++)		//��RGB565ͼƬת�ɻҶ�
			{	
				Color=*(imagebuf+((i*imagewidth)+qr_img_width-j-1)*qr_img_scale);//����qr_img_scaleѹ����240*240
				*(bmp+i*qr_img_width+j)=(((Color&0xF800)>> 8)*76+((Color&0x7E0)>>3)*150+((Color&0x001F)<<3)*30)>>8;
			}		
		}		
	}
	
	atk_qr_decode(qr_img_width,qr_img_width,bmp,bartype,result);//ʶ��Ҷ�ͼƬ��ע�⣺���κ�ʱԼ0.2S��
	
	if(result[0]==0)//û��ʶ�����
	{
		bartype++;
		if(bartype>=5)bartype=0; 
	}
	else if(result[0]!=0)//ʶ������ˣ���ʾ���
	{	
		PCF8574_WriteBit(BEEP_IO,0);//�򿪷�����
		delay_ms(100);
		PCF8574_WriteBit(BEEP_IO,1);
		POINT_COLOR=BLUE; 
		LCD_Fill(0,(lcddev.height+qr_image_width)/2+20,lcddev.width,lcddev.height,BLACK);
		Show_Str(0,(lcddev.height+qr_image_width)/2+20,lcddev.width,
								(lcddev.height-qr_image_width)/2-20,(u8*)result,16,0							
						);//LCD��ʾʶ����
		printf("\r\nresult:\r\n%s\r\n",result);//���ڴ�ӡʶ���� 		
	}
	myfree(SRAMCCM,bmp);		//�ͷŻҶ�ͼbmp�ڴ�
	myfree(SRAMIN,result);	//�ͷ�ʶ����	
}  

int main(void)
{     
	float fac;							 
 	u8 key;						   
	u8 i;	
	
 	Stm32_Clock_Init(360,25,2,8);	//����ʱ��,180Mhz
	delay_init(180);			//��ʼ����ʱ���� 
	uart_init(90,115200);	//��ʼ�����ڲ�����Ϊ115200 
  LED_Init();						//��ʼ����LED���ӵ�Ӳ���ӿ�
	SDRAM_Init();					//��ʼ��SDRAM 
	LCD_Init();						//��ʼ��LCD
	KEY_Init();						//��ʼ������
	PCF8574_Init();				//��ʼ��PCF8574
	OV5640_Init();				//��ʼ��OV5640 
	W25QXX_Init();				//��ʼ��W25Q256
 	my_mem_init(SRAMIN);	//��ʼ���ڲ��ڴ��
	my_mem_init(SRAMEX);	//��ʼ���ⲿ�ڴ��
	my_mem_init(SRAMCCM);	//��ʼ��CCM�ڴ��    
	POINT_COLOR=RED; 
	LCD_Clear(BLACK); 	
	while(font_init()) 		//����ֿ�
	{	    
		LCD_ShowString(30,50,200,16,16,(u8*)"Font Error!");
		delay_ms(200);				  
		LCD_Fill(30,50,240,66,WHITE);//�����ʾ	     
		delay_ms(200);				  
	}  	 
 	Show_Str_Mid(0,0,(u8*)"������STM32F4/F7������",16,lcddev.width);	 			    	 
	Show_Str_Mid(0,20,(u8*)"��ά��/������ʶ��ʵ��",16,lcddev.width);	
	while(OV5640_Init())//��ʼ��OV5640
	{
		Show_Str(30,190,240,16,(u8*)"OV5640 ����!",16,0);
		delay_ms(200);
	    LCD_Fill(30,190,239,206,WHITE);
		delay_ms(200);
	}	
	//�Զ��Խ���ʼ��
	OV5640_RGB565_Mode();		//RGB565ģʽ 
	OV5640_Focus_Init(); 
	OV5640_Light_Mode(0);		//�Զ�ģʽ
	OV5640_Color_Saturation(3);	//ɫ�ʱ��Ͷ�0
	OV5640_Brightness(4);		//����0
	OV5640_Contrast(3);			//�Աȶ�0
	OV5640_Sharpness(33);		//�Զ����
	OV5640_Focus_Constant();//���������Խ�
	DCMI_Init();						//DCMI���� 

	qr_image_width=lcddev.width;
	if(qr_image_width>480)qr_image_width=480;//����qr_image_width����Ϊ240�ı���
	if(qr_image_width==320)qr_image_width=240;
	Show_Str(0,(lcddev.height+qr_image_width)/2+4,240,16,(u8*)"ʶ������",16,1);
	
	dcmi_line_buf[0]=mymalloc(SRAMIN,qr_image_width*2);						//Ϊ�л�����������ڴ�	
	dcmi_line_buf[1]=mymalloc(SRAMIN,qr_image_width*2);						//Ϊ�л�����������ڴ�
	rgb_data_buf=mymalloc(SRAMEX,qr_image_width*qr_image_width*2);//Ϊrgb֡���������ڴ�
	
	dcmi_rx_callback=qr_dcmi_rx_callback;//DMA���ݽ����жϻص�����
	DCMI_DMA_Init((u32)dcmi_line_buf[0],(u32)dcmi_line_buf[1],qr_image_width/2,1,1);//DCMI DMA����  
 
	fac=800/qr_image_width;	//�õ���������
	OV5640_OutSize_Set((1280-fac*qr_image_width)/2,(800-fac*qr_image_width)/2,qr_image_width,qr_image_width); 
 	DCMI_Start(); 					//��������	 
	 
	printf("SRAM IN:%d\r\n",my_mem_perused(SRAMIN));
	printf("SRAM EX:%d\r\n",my_mem_perused(SRAMEX));
	printf("SRAM CCM:%d\r\n",my_mem_perused(SRAMCCM)); 
	
	atk_qr_init();//Ϊ�㷨�����ڴ�
	
	printf("1SRAM IN:%d\r\n",my_mem_perused(SRAMIN));
	printf("1SRAM EX:%d\r\n",my_mem_perused(SRAMEX));
	printf("1SRAM CCM:%d\r\n",my_mem_perused(SRAMCCM)); 
	 while(1)
	{	
		key=KEY_Scan(0);//��֧������
		if(key)
		{ 
			OV5640_Focus_Single();  //��KEY0��KEY1��KEYUP�ֶ������Զ��Խ�
			if(key==KEY2_PRES)break;//��KEY2����ʶ��
		} 
		if(readok==1)			//�ɼ�����һ֡ͼ��
		{		
			readok=0;
			qr_show_image((lcddev.width-qr_image_width)/2,(lcddev.height-qr_image_width)/2,qr_image_width,qr_image_width,rgb_data_buf);
			qr_decode(qr_image_width,rgb_data_buf);
		}
		i++;
		if(i==20)//DS0��˸.
		{
			i=0;
			LED0=!LED0;
 		}
	}
	atk_qr_destroy();//�ͷ��㷨�ڴ�
	printf("3SRAM IN:%d\r\n",my_mem_perused(SRAMIN));
	printf("3SRAM EX:%d\r\n",my_mem_perused(SRAMEX));
	printf("3SRAM CCM:%d\r\n",my_mem_perused(SRAMCCM)); 
	while(1)
	{
		LED0=!LED0;
		delay_ms(200);
	}
}






