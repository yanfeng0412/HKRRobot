#ifndef __CST816_H
#define __CST816_H

#include "stdint.h"
#include "driver/gpio.h"
#include "Inc/iic_hal.h"

/* CST816 dev pin definitions */
#define TOUCH_RST_PIN    21    // Reset pin
#define TOUCH_INT_PIN    47    // Interrupt pin

#define I2C0_Port I2C_NUM_0 //-1Ϊ�Զ�ѡ��
#define I2C0_SDA_PIN        8
#define I2C0_SCL_PIN        9
/* Function macros */
#define TOUCH_RST_0      gpio_set_level(TOUCH_RST_PIN,0)
#define TOUCH_RST_1      gpio_set_level(TOUCH_RST_PIN,1)

/* �豸��ַ */
#define Device_Addr 0x15

/* �������Ĵ��� */
#define GestureID 0x01		// ��������������ʶ��
#define FingerNum 0x02		// ��¼������ָ����
#define XposH 0x03			// X �����λ��صļĴ�����ַ
#define XposL 0x04			// X �����λ���ֵļĴ�����ַ
#define YposH 0x05			// Y �����λ��صļĴ�����ַ
#define YposL 0x06			// Y �����λ���ֵļĴ�����ַ
#define ChipID 0xA7			// ���ʴ�����оƬ��Ψһ��ʶ���Ĵ����ĵ�ַ
#define SleepMode 0xE5		// ���ƴ�����������˳�˯��ģʽ�ļĴ�����ַ
#define MotionMask 0xEC		// ��ĳЩ�˶���ز��������λ���������
#define IrqPluseWidth 0xED	// �жϵ����������صļĴ�����ַ
#define NorScanPer 0xEE		// ����������ɨ��������صļĴ�����ַ
#define MotionSlAngle 0xEF	// �漰���˶������Ƕ����
#define LpAutoWakeTime 0xF4 // �����Զ�����ʱ��
#define LpScanTH 0xF5		// �������ĳ���ɨ����ֵ
#define LpScanWin 0xF6		// �������ĳ���ɨ�贰�����
#define LpScanFreq 0xF7		// �������ĳ���ɨ��Ƶ��
#define LpScanIdac 0xF8		// ����ɨ�����
#define AutoSleepTime 0xF9	// �������Զ�����˯��ģʽ��ʱ�����
#define IrqCtl 0xFA			// ���������жϿ������
#define AutoReset 0xFB		// �������Զ���λ���
#define LongPressTime 0xFC	// �������ĳ���ʱ��
#define IOCtl 0xFD			// �������������������
#define DisAutoSleep 0xFE	// ��ֹ�������Զ�����˯��ģʽ

/* ����������ṹ�� */
typedef struct
{
	unsigned int X_Pos;
	unsigned int Y_Pos;
} CST816_Info; // ���������

/* ����IDʶ��ѡ��*/
typedef enum
{
	NOGESTURE = 0x00,	// �޲���
	DOWNGLIDE = 0x01,	// �»�
	UPGLIDE = 0x02,		// �ϻ�
	LEFTGLIDE = 0x03,	// ��
	RIGHTGLIDE = 0x04,	// �һ�
	CLICK = 0x05,		// ���
	DOUBLECLICK = 0x0B, // ˫��
	LONGPRESS = 0x0C,	// ����
} GestureID_TypeDef;

/* ������������ѡ�� */
typedef enum
{
	M_DISABLE = 0x00,	// ����Ҫ�κ���������
	EnConLR = 0x01,		// ���һ���
	EnConUD = 0x02,		// ���»���
	EnDClick = 0x03,	// ˫��
	M_ALLENABLE = 0x07, // ��������������������
} MotionMask_TypeDef;

/* �жϵ����巢�䷽ʽѡ�� */
typedef enum
{
	OnceWLP = 0x00,	 // �������巢��
	EnMotion = 0x10, // �˶��仯
	EnChange = 0x20, // ���ĳЩ�ض��仯
	EnTouch = 0x40,	 // �����¼�
	EnTest = 0x80,	 // ����ж��Ƿ�����
} IrqCtl_TypeDef;

extern CST816_Info CST816_Instance;

/* ��������ʼ����غ��� */
void CST816_GPIO_Init(void);
void CST816_RESET(void);
void CST816_Init(void);

/* �������������� */
void CST816_Get_XY_AXIS(void);
uint8_t CST816_Get_ChipID(void);
uint8_t CST816_Get_FingerNum(void);
/* ��������д���� */
void CST816_IIC_WriteREG(uint8_t addr, uint8_t dat);
uint8_t CST816_IIC_ReadREG(unsigned char addr);

/* �������йز������ú��� */
void CST816_Config_MotionMask(uint8_t mode);
void CST816_Config_AutoSleepTime(uint8_t time);
void CST816_Config_MotionSlAngle(uint8_t x_right_y_up_angle);
void CST816_Config_NorScanPer(uint8_t Period);
void CST816_Config_IrqPluseWidth(uint8_t Width);
void CST816_Config_LpScanTH(uint8_t TH);
void CST816_Wakeup(void);
void CST816_Sleep(void);

#endif