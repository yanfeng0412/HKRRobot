#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "driver/spi_master.h"
#include "driver/gpio.h"

#define USE_HORIZONTAL 2  //���ú�������������ʾ 0��1Ϊ���� 2��3Ϊ���� (改为横屏)

#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W 240
#define LCD_H 284
#else
#define LCD_W 284
#define LCD_H 240
#endif

#define LCD_SPI_PORT SPI2_HOST
#define LCD_MISO_PIN -1
#define LCD_SCK_PIN 14      // 避开 PSRAM 占用的引脚
#define LCD_MOSI_PIN 15     // 避开 PSRAM 占用的引脚
#define LCD_RES_PIN 16
#define LCD_DC_PIN 17
#define LCD_CS_PIN 18
#define LCD_BLK_PIN 48      // 避开 PSRAM 占用的引脚

//Test2 Wifi Board Pinout
// #define LCD_MISO_PIN -1
// #define LCD_SCK_PIN 14      // 避开 PSRAM 占用的引脚
// #define LCD_MOSI_PIN 13     // 避开 PSRAM 占用的引脚
// #define LCD_RES_PIN 10
// #define LCD_DC_PIN 21
// #define LCD_CS_PIN 18
// #define LCD_BLK_PIN 48      // 避开 PSRAM 占用的引脚


extern spi_device_handle_t spi;
//-----------------LCD�˿ڶ���----------------

#define LCD_RES_Clr()  gpio_set_level(LCD_RES_PIN,0)  // RES
#define LCD_RES_Set()  gpio_set_level(LCD_RES_PIN,1)

#define LCD_DC_Clr()   gpio_set_level(LCD_DC_PIN,0)  // DC
#define LCD_DC_Set()   gpio_set_level(LCD_DC_PIN,1)

#define LCD_BLK_Clr()  gpio_set_level(LCD_BLK_PIN,0)  // BLK OFF (背光高电平有效，N-MOS驱动)

// void LCD_GPIO_Init(void);//��ʼ��GPIO
// void LCD_Writ_Bus(uint8_t dat);//ģ��SPIʱ��
// void LCD_WR_DATA8(uint8_t dat);//д��һ���ֽ�
void LCD_WR_DATA(uint16_t dat);//д�������ֽ�
// void LCD_WR_REG(uint8_t dat);//д��һ��ָ��
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);//�������꺯��
void LCD_Init(void);//LCD��ʼ��
void LCD_BLK_Set(void); // 背光控制函数 (LOW=ON)
#endif




