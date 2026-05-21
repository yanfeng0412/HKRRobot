#ifndef __LCD_H
#define __LCD_H

#include "lcd_init.h"

void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);  // ָ�����������ɫ
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);                                 // ��ָ��λ�û�һ����
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);      // ��ָ��λ�û�һ����
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color); // ��ָ��λ�û�һ������
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);                      // ��ָ��λ�û�һ��Բ
void LCD_FillCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);

void LCD_ShowChinese(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);      // ��ʾ���ִ�
void LCD_ShowChinese12x12(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ��ʾ����12x12����
void LCD_ShowChinese16x16(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ��ʾ����16x16����
void LCD_ShowChinese24x24(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ��ʾ����24x24����
void LCD_ShowChinese32x32(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ��ʾ����32x32����

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);        // ��ʾһ���ַ�
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ��ʾ�ַ���
uint32_t mypow(uint8_t m, uint8_t n);                                                                                 // ����
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);      // ��ʾ��������
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);      // ��ʾ��λС������

void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const void *pic); // ��ʾͼƬ

void DrawColorBars(void);
void DrawGrayscale(void);
void DrawClearButton(void);

void DrawThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color);
void LCD_DrawThickLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint8_t size);

// 机器人表情函数
void Robot_DrawFace(uint8_t expression);
void Robot_DrawFace_At(uint8_t expression, uint16_t center_x, uint16_t center_y);
void Robot_Demo(void);
void Robot_Demo_Interactive(void);

// RGB565图片显示
void LCD_ShowRGB565(uint16_t x, uint16_t y, const uint16_t *rgb565_data, uint16_t width, uint16_t height);
void RGB565_PlayLoop(const uint16_t *images[], const uint16_t widths[], const uint16_t heights[], 
                     const char *names[], uint8_t num_images, uint32_t delay_ms);
void RGB565_PlayLoop_WithDelays(const uint16_t *images[], const uint16_t widths[], const uint16_t heights[], 
                                const char *names[], const uint32_t delays[], uint8_t num_images);
void RGB565_TouchInteractive(const uint16_t *idle_img, uint16_t idle_w, uint16_t idle_h,
                             const uint16_t *closeeye_img, uint16_t closeeye_w, uint16_t closeeye_h,
                             const uint16_t *haqi_img, uint16_t haqi_w, uint16_t haqi_h,
                             const uint16_t *angry_img, uint16_t angry_w, uint16_t angry_h);
void RGB565_DualModeInteractive(
    const uint16_t *robot_idle, uint16_t robot_idle_w, uint16_t robot_idle_h,
    const uint16_t *robot_closeeye, uint16_t robot_closeeye_w, uint16_t robot_closeeye_h,
    const uint16_t *robot_smile, uint16_t robot_smile_w, uint16_t robot_smile_h,
    const uint16_t *robot_haqi, uint16_t robot_haqi_w, uint16_t robot_haqi_h,
    const uint16_t *robot_angry, uint16_t robot_angry_w, uint16_t robot_angry_h,
    const uint16_t *ynf_idle, uint16_t ynf_idle_w, uint16_t ynf_idle_h,
    const uint16_t *ynf_haqi, uint16_t ynf_haqi_w, uint16_t ynf_haqi_h,
    const uint16_t *ynf_angry, uint16_t ynf_angry_w, uint16_t ynf_angry_h);

// ������ɫ
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40      // ��ɫ
#define BRRED 0XFC07      // �غ�ɫ
#define GRAY 0X8430       // ��ɫ
#define DARKBLUE 0X01CF   // ����ɫ
#define LIGHTBLUE 0X7D7C  // ǳ��ɫ
#define GRAYBLUE 0X5458   // ����ɫ
#define LIGHTGREEN 0X841F // ǳ��ɫ
#define LGRAY 0XC618      // ǳ��ɫ(PANNEL),���屳��ɫ
#define LGRAYBLUE 0XA651  // ǳ����ɫ(�м����ɫ)
#define LBBLUE 0X2B12     // ǳ����ɫ(ѡ����Ŀ�ķ�ɫ)

/* �궨�� */
#define LOGO_DURATION 3000
#define TEXT_DURATION 2000
#define IMAGE_INTERVAL 2000
#define COLOR_FULL_INTERVAL 1000
#define EFFECT_DURATION 3000
#define BTN_WIDTH 60
#define BTN_HEIGHT 30
#define SCREEN_WIDTH LCD_W
#define SCREEN_HEIGHT LCD_H

#define RGB(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define COLOR_BAR_NUM 6                          // 6����ɫ
#define BAR_WIDTH (SCREEN_WIDTH / COLOR_BAR_NUM) // �Զ���������

#endif
