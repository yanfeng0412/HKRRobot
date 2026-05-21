#include "lcd.h"
#include "lcd_init.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tjpgd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"

// 声明嵌入的图片
extern const uint8_t angry_jpg_start[] asm("_binary_angry_jpg_start");
extern const uint8_t angry_jpg_end[] asm("_binary_angry_jpg_end");
extern const uint8_t haqi_jpg_start[] asm("_binary_haqi_jpg_start");
extern const uint8_t haqi_jpg_end[] asm("_binary_haqi_jpg_end");
extern const uint8_t idle_jpg_start[] asm("_binary_idle_jpg_start");
extern const uint8_t idle_jpg_end[] asm("_binary_idle_jpg_end");

extern spi_device_handle_t spi;

// JPEG输入结构
typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t index;
} JPEG_INPUT;

// JPEG输出结构
typedef struct {
    uint16_t x;
    uint16_t y;
} JPEG_OUTPUT;

static JPEG_OUTPUT g_jpeg_output;

// JPEG输入函数
static size_t jpeg_input_func(JDEC *jd, uint8_t *buff, size_t ndata)
{
    JPEG_INPUT *input = (JPEG_INPUT *)jd->device;
    
    if (buff) {
        size_t remain = input->size - input->index;
        if (ndata > remain) ndata = remain;
        memcpy(buff, input->data + input->index, ndata);
        input->index += ndata;
        return ndata;
    } else {
        if (ndata > input->size - input->index) {
            ndata = input->size - input->index;
        }
        input->index += ndata;
        return ndata;
    }
}

// JPEG输出函数
static size_t jpeg_output_func(JDEC *jd, void *bitmap, JRECT *rect)
{
    uint16_t *src = (uint16_t *)bitmap;
    uint16_t w = rect->right - rect->left + 1;
    uint16_t h = rect->bottom - rect->top + 1;
    uint16_t x = g_jpeg_output.x + rect->left;
    uint16_t y = g_jpeg_output.y + rect->top;
    
    // 设置显示窗口
    LCD_Address_Set(x, y, x + w - 1, y + h - 1);
    
    // 发送数据
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.tx_buffer = src;
    t.length = w * h * 16;
    t.user = (void *)1;
    spi_device_transmit(spi, &t);
    
    return 1;
}

/******************************************************************************
      JPEG解码并显示
******************************************************************************/
void LCD_ShowJPEG(uint16_t x, uint16_t y, const uint8_t *jpeg_data, uint32_t jpeg_size)
{
    JDEC jd;
    void *work = malloc(3100);
    
    if (!work) {
        printf("内存分配失败\n");
        return;
    }
    
    JPEG_INPUT input = {jpeg_data, jpeg_size, 0};
    g_jpeg_output.x = x;
    g_jpeg_output.y = y;
    
    JRESULT res = jd_prepare(&jd, jpeg_input_func, work, 3100, &input);
    if (res != JDR_OK) {
        printf("JPEG准备失败: %d\n", res);
        free(work);
        return;
    }
    
    printf("JPEG尺寸: %dx%d\n", jd.width, jd.height);
    
    res = jd_decomp(&jd, jpeg_output_func, 0);
    
    if (res == JDR_OK) {
        printf("JPEG显示完成\n");
    } else {
        printf("JPEG解码失败: %d\n", res);
    }
    
    free(work);
}

/******************************************************************************
      从文件加载JPEG
******************************************************************************/
void LCD_ShowJPEG_FromFile(uint16_t x, uint16_t y, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("无法打开文件: %s\n", filename);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *jpeg_buf = (uint8_t *)malloc(file_size);
    if (!jpeg_buf) {
        printf("内存分配失败\n");
        fclose(f);
        return;
    }
    
    fread(jpeg_buf, 1, file_size, f);
    fclose(f);
    
    LCD_ShowJPEG(x, y, jpeg_buf, file_size);
    free(jpeg_buf);
}

/******************************************************************************
      循环播放JPEG图片
******************************************************************************/
void JPEG_PlayLoop(void)
{
    printf("\n=== 开始循环播放JPEG图片 ===\n");
    
    typedef struct {
        const uint8_t *start;
        const uint8_t *end;
        const char *name;
    } ImageInfo;
    
    ImageInfo images[] = {
        {idle_jpg_start, idle_jpg_end, "idle"},
        {haqi_jpg_start, haqi_jpg_end, "haqi"},
        {angry_jpg_start, angry_jpg_end, "angry"}
    };
    
    uint8_t num_images = sizeof(images) / sizeof(images[0]);
    uint8_t current_index = 0;
    
    LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
    
    while(1) {
        ImageInfo *img = &images[current_index];
        uint32_t img_size = img->end - img->start;
        
        printf("[%s] ", img->name);
        LCD_ShowJPEG(0, 0, img->start, img_size);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        current_index = (current_index + 1) % num_images;
    }
}

/******************************************************************************
      自定义间隔播放
******************************************************************************/
void JPEG_PlayLoop_WithDelay(uint32_t delay_ms)
{
    printf("\n=== 循环播放 (间隔%lums) ===\n", delay_ms);
    
    typedef struct {
        const uint8_t *start;
        const uint8_t *end;
        const char *name;
    } ImageInfo;
    
    ImageInfo images[] = {
        {idle_jpg_start, idle_jpg_end, "idle"},
        {haqi_jpg_start, haqi_jpg_end, "haqi"},
        {angry_jpg_start, angry_jpg_end, "angry"}
    };
    
    uint8_t num_images = 3;
    uint8_t idx = 0;
    
    LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
    
    while(1) {
        uint32_t size = images[idx].end - images[idx].start;
        printf("[%s] ", images[idx].name);
        LCD_ShowJPEG(0, 0, images[idx].start, size);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        idx = (idx + 1) % num_images;
    }
}
