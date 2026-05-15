#include "lcd_init.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

spi_device_handle_t spi = NULL;

static const spi_bus_config_t buscfg = {
	.miso_io_num = LCD_MISO_PIN,
	.mosi_io_num = LCD_MOSI_PIN,
	.sclk_io_num = LCD_SCK_PIN,
	.quadwp_io_num = -1,
	.quadhd_io_num = -1,
	.max_transfer_sz = 284 * 240 * 2, // 全屏缓冲区大小
};

static const spi_device_interface_config_t devcfg = {
	.clock_speed_hz = 26*1000*1000,  // 26MHz for ESP32S3
	.mode = 0,					// SPIģʽ0
	.spics_io_num = LCD_CS_PIN, // CS����
	.queue_size = 7,			// ����ϣ���ܹ�һ���Ŷ�7������
	.flags = SPI_DEVICE_HALFDUPLEX,
};

static void LCD_GPIO_Init(void)
{
	esp_err_t ret;

	gpio_config_t io_conf = {
		.pin_bit_mask = ((1ULL << LCD_RES_PIN) | (1ULL << LCD_DC_PIN) | (1ULL << LCD_BLK_PIN)),
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};

	gpio_config(&io_conf);
	
	// 测试GPIO是否正常工作
	printf("Testing GPIO outputs:\n");
	gpio_set_level(LCD_BLK_PIN, 1);
	printf("  BLK (GPIO%d) = HIGH\n", LCD_BLK_PIN);
	vTaskDelay(pdMS_TO_TICKS(100));
	gpio_set_level(LCD_BLK_PIN, 0);
	printf("  BLK (GPIO%d) = LOW\n", LCD_BLK_PIN);
	
	// ��ʼ��SPI����
	printf("Initializing SPI bus on port %d...\n", LCD_SPI_PORT);
	ret = spi_bus_initialize(LCD_SPI_PORT, &buscfg, SPI_DMA_CH_AUTO);
	if (ret == ESP_OK) {
		printf("SPI bus initialized successfully\n");
	} else {
		printf("SPI bus init FAILED: %d\n", ret);
	}
	assert(ret == ESP_OK);
	
	ret = spi_bus_add_device(LCD_SPI_PORT, &devcfg, &spi);
	if (ret == ESP_OK) {
		printf("SPI device added successfully\n");
	} else {
		printf("SPI device add FAILED: %d\n", ret);
	}
	assert(ret == ESP_OK);

	LCD_RES_Set();
	LCD_DC_Set();
	// 初始化时不要立即打开背光，等LCD初始化完成后再打开
}

// 添加一个函数来设置背光 (低电平有效)
void LCD_BLK_Set(void)
{
	gpio_set_level(LCD_BLK_PIN, 1);  // LOW = ON
	printf("LCD_BLK_Set: Backlight turned ON (LOW)\n");
}
/******************************************************************************
	  ����˵����LCD��������д�뺯��
	  ������ݣ�dat  Ҫд��Ĵ�������
	  ����ֵ��  ��
******************************************************************************/
static inline void LCD_Writ_Bus(uint8_t dat)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8;
	t.tx_buffer = &dat;

	ret = spi_device_polling_transmit(spi, &t); // Transmit!
	assert(ret == ESP_OK);
}

/******************************************************************************
	  ����˵����LCDд������
	  ������ݣ�dat д�������
	  ����ֵ��  ��
******************************************************************************/
static inline void LCD_WR_DATA8(uint8_t dat)
{
	LCD_DC_Set(); // 数据模式
	LCD_Writ_Bus(dat);
}

/******************************************************************************
	  ����˵����LCDд������
	  ������ݣ�dat д�������
	  ����ֵ��  ��
******************************************************************************/
inline void LCD_WR_DATA(uint16_t dat)
{
	esp_err_t ret;
	spi_transaction_t t;
	LCD_DC_Set(); // 数据模式
	
	memset(&t, 0, sizeof(t));
	uint8_t tx_buf[2] = {
		(dat >> 8) & 0xFF, // ���ֽ�
		dat & 0xFF		   // ���ֽ�
	};
	t.length = 16;

	t.tx_buffer = tx_buf;

	ret = spi_device_polling_transmit(spi, &t); // Transmit!
	assert(ret == ESP_OK);
}

/******************************************************************************
	  ����˵����LCDд������
	  ������ݣ�dat д�������
	  ����ֵ��  ��
******************************************************************************/
static inline void LCD_WR_REG(uint8_t dat)
{
	LCD_DC_Clr(); // д����
	LCD_Writ_Bus(dat);
	LCD_DC_Set();
}

// 读取LCD ID用于诊断
static void LCD_Read_ID(void)
{
	esp_err_t ret;
	spi_transaction_t t;
	uint8_t rx_data[4] = {0};
	
	LCD_DC_Clr();
	LCD_Writ_Bus(0x04); // Read Display ID
	LCD_DC_Set();
	
	memset(&t, 0, sizeof(t));
	t.length = 32;
	t.rxlength = 32;
	t.rx_buffer = rx_data;
	t.flags = SPI_TRANS_USE_RXDATA;
	
	ret = spi_device_polling_transmit(spi, &t);
	if (ret == ESP_OK) {
		printf("LCD ID: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
			rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
	} else {
		printf("LCD ID read failed: %d\n", ret);
	}
}

/******************************************************************************
	  ����˵����������ʼ�ͽ�����ַ
	  ������ݣ�x1,x2 �����е���ʼ�ͽ�����ַ
				y1,y2 �����е���ʼ�ͽ�����ַ
	  ����ֵ��  ��
******************************************************************************/
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	if (USE_HORIZONTAL == 0)
	{
		LCD_WR_REG(0x2a); //�е�ַ����
		LCD_WR_DATA(x1 );
		LCD_WR_DATA(x2 );
		LCD_WR_REG(0x2b); //�е�ַ����
		LCD_WR_DATA(y1 + 20);
		LCD_WR_DATA(y2 + 20);
		LCD_WR_REG(0x2c); //������д
	}
	else if (USE_HORIZONTAL == 1)
	{
		LCD_WR_REG(0x2a); //�е�ַ����
		LCD_WR_DATA(x1);
		LCD_WR_DATA(x2);
		LCD_WR_REG(0x2b); //�е�ַ����
		LCD_WR_DATA(y1 + 80);
		LCD_WR_DATA(y2 + 80);
		LCD_WR_REG(0x2c); //������д
	}
	else if (USE_HORIZONTAL == 2)
	{
		LCD_WR_REG(0x2a); //�е�ַ����
		LCD_WR_DATA(x1 + 20);
		LCD_WR_DATA(x2 + 20);
		LCD_WR_REG(0x2b); //�е�ַ����
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c); //������д
	}
	else
	{
		LCD_WR_REG(0x2a); //�е�ַ����
		LCD_WR_DATA(x1 + 80);
		LCD_WR_DATA(x2 + 80);
		LCD_WR_REG(0x2b); //�е�ַ����
		LCD_WR_DATA(y1);
		LCD_WR_DATA(y2);
		LCD_WR_REG(0x2c); //������д
	}
}

void LCD_Init(void)
{
	printf("LCD_Init: Starting LCD initialization...\n");
	LCD_GPIO_Init(); // ��ʼ��GPIO
	printf("LCD_Init: GPIO initialized\n");

	gpio_set_level(LCD_BLK_PIN, 1); // 先关闭背光 (HIGH = OFF)
	printf("LCD_Init: Backlight OFF (HIGH)\n");

	// 硬件复位
	printf("LCD_Init: Hardware reset...\n");
	LCD_RES_Clr();
	vTaskDelay(pdMS_TO_TICKS(10));
	LCD_RES_Set();
	vTaskDelay(pdMS_TO_TICKS(120));  // 增加复位后等待时间
	printf("LCD_Init: Reset complete\n");

	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x11);				// Sleep out
	vTaskDelay(pdMS_TO_TICKS(120)); // Delay 120ms
	printf("LCD_Init: Sleep out sent\n");
	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x36);
	if (USE_HORIZONTAL == 0)
		LCD_WR_DATA8(0x00);
	else if (USE_HORIZONTAL == 1)
		LCD_WR_DATA8(0xC0);
	else if (USE_HORIZONTAL == 2)
		LCD_WR_DATA8(0xA0);  // 修改为0xA0实现屏幕上下颠倒
	else
		LCD_WR_DATA8(0xA0);

	LCD_WR_REG(0x3A);
	LCD_WR_DATA8(0x05);

	LCD_WR_REG(0xB2);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x33);

	LCD_WR_REG(0xB7);
	LCD_WR_DATA8(0x35);

	LCD_WR_REG(0xBB);
	LCD_WR_DATA8(0x32); // Vcom=1.35V

	LCD_WR_REG(0xC2);
	LCD_WR_DATA8(0x01);

	LCD_WR_REG(0xC3);
	LCD_WR_DATA8(0x15); // GVDD=4.8V  ��ɫ���

	LCD_WR_REG(0xC4);
	LCD_WR_DATA8(0x20); // VDV, 0x20:0v

	LCD_WR_REG(0xC6);
	LCD_WR_DATA8(0x0F); // 0x0F:60Hz

	LCD_WR_REG(0xD0);
	LCD_WR_DATA8(0xA4);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0xE0);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x0E);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x05);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x48);
	LCD_WR_DATA8(0x17);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x34);

	LCD_WR_REG(0xE1);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x0E);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x48);
	LCD_WR_DATA8(0x17);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x34);
	LCD_WR_REG(0x21);

	LCD_WR_REG(0x29); // Display ON
	vTaskDelay(pdMS_TO_TICKS(20));
	printf("LCD_Init: Display ON command sent\n");
	
	// 尝试读取LCD ID
	printf("LCD_Init: Reading LCD ID...\n");
	LCD_Read_ID();
	
	printf("LCD_Init: LCD initialization complete!\n");
}
