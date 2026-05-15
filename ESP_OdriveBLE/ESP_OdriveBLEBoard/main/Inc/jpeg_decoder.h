#ifndef __JPEG_DECODER_H
#define __JPEG_DECODER_H

#include <stdint.h>

// 从内存中的JPEG数据显示
void LCD_ShowJPEG(uint16_t x, uint16_t y, const uint8_t *jpeg_data, uint32_t jpeg_size);

// 从文件系统加载JPEG并显示
void LCD_ShowJPEG_FromFile(uint16_t x, uint16_t y, const char *filename);

// 循环播放嵌入的JPEG图片（默认2秒间隔）
void JPEG_PlayLoop(void);

// 循环播放JPEG图片（自定义间隔时间）
void JPEG_PlayLoop_WithDelay(uint32_t delay_ms);

#endif
