#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cst816.h"
#include "lcd_init.h"

#define TOUCH_OFFSET_Y 5
#define REVERSE 0

CST816_Info CST816_Instance;

iic_bus_t CST816_dev =
	{
		.sda_pin = I2C0_SDA_PIN,
		.scl_pin = I2C0_SCL_PIN,
};
/*
*********************************************************************************************************
*	�� �� ��: CST816_GPIO_Init
*	����˵��: CST816 GPIO�ڳ�ʼ��
*	��    �Σ�none
*	�� �� ֵ: none
*********************************************************************************************************
*/
void CST816_GPIO_Init(void)
{
    printf("CST816_GPIO_Init: Configuring RST pin (GPIO%d)...\n", TOUCH_RST_PIN);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_RST_PIN),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&io_conf);

    TOUCH_RST_1;
    
    printf("CST816_GPIO_Init: Initializing I2C pins (SDA=GPIO%d, SCL=GPIO%d)...\n", 
           I2C0_SDA_PIN, I2C0_SCL_PIN);

    /* Initialize I2C pins */
    IICInit(&CST816_dev);
    
    printf("CST816_GPIO_Init: GPIO initialization complete\n");
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Init
*	����˵��: CST816��ʼ��
*	��    �Σ�none
*	�� �� ֵ: none
*********************************************************************************************************
*/
void CST816_Init(void)
{
	printf("CST816_Init: Starting touch initialization...\n");
	
	// GPIO和I2C初始化
	CST816_GPIO_Init();
	printf("CST816_Init: GPIO and I2C initialized\n");
	
	// 硬件复位
	CST816_RESET();
	printf("CST816_Init: Hardware reset completed\n");
	
	// 增加延迟等待芯片启动
	vTaskDelay(pdMS_TO_TICKS(200));
	
	// 尝试读取芯片ID验证（多次重试）
	uint8_t chip_id = 0xFF;
	for(int i = 0; i < 5; i++) {
		chip_id = CST816_Get_ChipID();
		printf("CST816_Init: Read attempt %d, Chip ID = 0x%02X\n", i+1, chip_id);
		
		if(chip_id != 0xFF && chip_id != 0x00) {
			break;  // 读取成功
		}
		vTaskDelay(pdMS_TO_TICKS(50));
	}
	
	if(chip_id == 0xFF || chip_id == 0x00) {
		printf("CST816_Init: ERROR - Failed to communicate with touch chip!\n");
		printf("CST816_Init: Possible issues:\n");
		printf("  1. I2C wiring (SDA=GPIO%d, SCL=GPIO%d)\n", I2C0_SDA_PIN, I2C0_SCL_PIN);
		printf("  2. Touch screen power supply\n");
		printf("  3. CST816 chip malfunction\n");
		printf("  4. I2C address conflict (using 0x%02X)\n", Device_Addr);
		return;  // 初始化失败，直接返回
	} else {
		printf("CST816_Init: Touch chip detected successfully (ID=0x%02X)\n", chip_id);
	}
	
	// 配置自动休眠时间
	CST816_Config_AutoSleepTime(5);
	printf("CST816_Init: Auto sleep time configured\n");
	
	// 延迟确保配置生效
	vTaskDelay(pdMS_TO_TICKS(50));
	
	printf("CST816_Init: Initialization complete\n");
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_IIC_ReadREG
*	����˵��: ��ȡ�����������Ĵ���������
*	��    �Σ�reg���Ĵ�����ַ
*	�� �� ֵ: ���ؼĴ����洢������
*********************************************************************************************************
*/
uint8_t CST816_IIC_ReadREG(uint8_t addr)
{
	return IIC_Read_One_Byte(&CST816_dev, Device_Addr, addr);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_IIC_WriteREG
*	����˵��: �������ļĴ���д������
*	��    �Σ�addr���Ĵ�����ַ
*						dat:	д�������
*	�� �� ֵ: ���ؼĴ����洢������
*********************************************************************************************************
*/
void CST816_IIC_WriteREG(uint8_t addr, uint8_t dat)
{
	IIC_Write_One_Byte(&CST816_dev, Device_Addr, addr, dat);
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_RESET
*	����˵��: ��������λ
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_RESET(void)
{
    TOUCH_RST_0;
    vTaskDelay(pdMS_TO_TICKS(10));
    TOUCH_RST_1;
    vTaskDelay(pdMS_TO_TICKS(100));
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_READ_X
*	����˵��: ��ȡ�������ڴ���ʱ������ֵ
*	��    �Σ���
*	�� �� ֵ: �� �����ݴ洢��CST816_Instance�ṹ���У�
*********************************************************************************************************
*/
void CST816_Get_XY_AXIS(void)
{
	uint8_t DAT[4];
	IIC_Read_Multi_Byte(&CST816_dev, Device_Addr, XposH, 4, DAT);
	CST816_Instance.X_Pos = ((DAT[0] & 0x0F) << 8) | DAT[1];				  //(temp[0]&0X0F)<<4|
	CST816_Instance.Y_Pos = (((DAT[2] & 0x0F) << 8) | DAT[3]) + TOUCH_OFFSET_Y; //(temp[2]&0X0F)<<4|
#if REVERSE
	// CST816_Instance.X_Pos = LCD_W -1 - CST816_Instance.X_Pos;
	CST816_Instance.Y_Pos = LCD_H -1 - CST816_Instance.Y_Pos;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Get_FingerNum
*	����˵��: ��ȡ����������ָ��������,0xFFΪ˯��
*	��    �Σ���
*	�� �� ֵ: ����оƬID
*********************************************************************************************************
*/
uint8_t CST816_Get_FingerNum(void)
{
	return CST816_IIC_ReadREG(FingerNum);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Get_ChipID
*	����˵��: ��ȡ��������оƬID
*	��    �Σ���
*	�� �� ֵ: ����оƬID
*********************************************************************************************************
*/
uint8_t CST816_Get_ChipID(void)
{
	return CST816_IIC_ReadREG(ChipID);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_MotionMask
*	����˵��: ʹ�������������������һ������������»�����˫����
*	��    �Σ�mode��ģʽ(5��)
*	�� �� ֵ: ��
*	ע    �⣺ʹ������������������Ӧʱ��
*********************************************************************************************************
*/
void CST816_Config_MotionMask(uint8_t mode)
{
	CST816_IIC_WriteREG(MotionMask, mode);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_AutoSleepTime
*	����˵��: �涨time���޴������Զ�����͹���ģʽ
*	��    �Σ�time��ʱ��(s)
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Config_AutoSleepTime(uint8_t time)
{
	CST816_IIC_WriteREG(AutoSleepTime, time);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Sleep
*	����˵��: ����˯�ߣ��޴������ѹ���
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Sleep(void)
{
	CST816_IIC_WriteREG(SleepMode, 0x03);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Wakeup
*	����˵��: ����
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Wakeup(void)
{
	CST816_RESET();
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_MotionSlAngle
*	����˵��: ���Ƽ�⻬�������Ƕȿ��ơ�Angle=tan(c)*10 cΪ��x��������Ϊ��׼�ĽǶȡ�
*	��    �Σ�x_right_y_up_angle���Ƕ�ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Config_MotionSlAngle(uint8_t x_right_y_up_angle)
{
	CST816_IIC_WriteREG(MotionSlAngle, x_right_y_up_angle);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_NorScanPer
*	����˵��: �������ټ���������ú�����
*						��ֵ��Ӱ�쵽LpAutoWakeTime��AutoSleepTime��
*						��λ10ms����ѡֵ��1��30��Ĭ��ֵΪ1��
*	��    �Σ�Period������ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Config_NorScanPer(uint8_t Period)
{
	if (Period >= 30)
		Period = 30;
	CST816_IIC_WriteREG(NorScanPer, Period);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_IrqPluseWidth
*	����˵��: �жϵ���������������ú���
*	��    �Σ�Period������ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Config_IrqPluseWidth(uint8_t Width)
{
	if (Width >= 200)
		Width = 200;
	CST816_IIC_WriteREG(IrqPluseWidth, Width);
}

/*
*********************************************************************************************************
*	�� �� ��: CST816_Config_NorScanPer
*	����˵��: �͹���ɨ�軽���������ú�����ԽСԽ������Ĭ��ֵ48
*	��    �Σ�TH������ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void CST816_Config_LpScanTH(uint8_t TH)
{
	CST816_IIC_WriteREG(LpScanTH, TH);
}