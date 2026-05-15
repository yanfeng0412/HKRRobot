#include "lcd.h"
#include "lcd_init.h"
#include "lcdfont.h"
#include "cst816.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAX_BUFFER_SIZE 256		// ���ݿ���RAM����
#define MAX_ALLOWED_DISTANCE 50 // ����

/******************************************************************************
	  ����˵������ָ�����������ɫ
	  ������ݣ�xsta,ysta   ��ʼ����
				xend,yend   ��ֹ����
								color       Ҫ������ɫ
	  ����ֵ��  ��
******************************************************************************/
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));

	uint32_t pixelCount = (xend - xsta + 1) * (yend - ysta + 1);
	uint16_t color_buf = (uint16_t)((color << 8) | (color >> 8));

	printf("LCD_Fill: area(%d,%d)-(%d,%d) pixels=%lu color=0x%04X\n", 
		xsta, ysta, xend, yend, pixelCount, color);

	LCD_Address_Set(xsta, ysta, xend, yend); // ������ʾ��Χ

	uint16_t *buffer = (uint16_t *)malloc(MAX_BUFFER_SIZE * sizeof(uint16_t));
	if (!buffer) {
		printf("LCD_Fill: malloc failed!\n");
		return;
	}

	uint32_t remaining = pixelCount;
	// ��仺������ÿ�������ɸߵ��ֽ���ɣ�
	for (uint32_t i = 0; i < MAX_BUFFER_SIZE; i++)
	{
		buffer[i] = color_buf; // ���ֽ�
	}

	LCD_DC_Set(); // 设置为数据模式
	t.tx_buffer = buffer;

	while (remaining > 0)
	{
		uint32_t chunk_size = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
		t.length = chunk_size * sizeof(uint16_t) * 8;
		ret = spi_device_polling_transmit(spi, &t); // Transmit!
		if (ret != ESP_OK) {
			printf("LCD_Fill: SPI transmit failed, ret=%d\n", ret);
		}
		assert(ret == ESP_OK);
		remaining -= chunk_size;
	}

	free(buffer);
	printf("LCD_Fill: completed\n");
}

/******************************************************************************
	  ����˵������ָ��λ�û���
	  ������ݣ�x,y ��������
				color �����ɫ
	  ����ֵ��  ��
******************************************************************************/
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
	LCD_Address_Set(x, y, x, y); // ���ù��λ��
	LCD_WR_DATA(color);
}

/******************************************************************************
	  ����˵��������
	  ������ݣ�x1,y1   ��ʼ����
				x2,y2   ��ֹ����
				color   �ߵ���ɫ
	  ����ֵ��  ��
******************************************************************************/
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	uint16_t t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	delta_x = x2 - x1; // ������������
	delta_y = y2 - y1;
	uRow = x1; // �����������
	uCol = y1;
	if (delta_x > 0)
		incx = 1; // ���õ�������
	else if (delta_x == 0)
		incx = 0; // ��ֱ��
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	if (delta_y > 0)
		incy = 1;
	else if (delta_y == 0)
		incy = 0; // ˮƽ��
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y)
		distance = delta_x; // ѡȡ��������������
	else
		distance = delta_y;
	for (t = 0; t < distance + 1; t++)
	{
		LCD_DrawPoint(uRow, uCol, color); // ����
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance)
		{
			xerr -= distance;
			uRow += incx;
		}
		if (yerr > distance)
		{
			yerr -= distance;
			uCol += incy;
		}
	}
}

// ���ݴ�����ר�в���
// ��ˮƽ��
// x0,y0:����
// len:�߳���
// color:��ɫ
void gui_draw_hline(uint16_t x0, uint16_t y0, uint16_t len, uint16_t color)
{
	if (len == 0)
		return;
	LCD_DrawLine(x0, y0, x0 + len - 1, y0, color);
}

void gui_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
	uint32_t i;
	uint32_t imax = (r * 724) >> 10; // r * 707/1000 �� r * 724/1024
	uint32_t sqmax = r * r + (r >> 1);
	uint32_t x = r;
	uint32_t i_squared = 1; // 1^2 = 1

	gui_draw_hline(x0 - r, y0, 2 * r, color);

	for (i = 1; i < imax + 1; i++)
	{
		if ((i_squared + x * x) > sqmax)
		{
			if (x > imax)
			{
				gui_draw_hline(x0 - i + 1, y0 + x, 2 * (i - 1), color);
				gui_draw_hline(x0 - i + 1, y0 - x, 2 * (i - 1), color);
			}
			x--;
		}
		// �����ڲ���
		gui_draw_hline(x0 - x, y0 + i, 2 * x, color);
		gui_draw_hline(x0 - x, y0 - i, 2 * x, color);

		i_squared += (i << 1) + 1; // ������һ��i��ƽ��
	}
}

/******************************************************************************
	  ����˵����������
	  ������ݣ�x1,y1   ��ʼ����
				x2,y2   ��ֹ����
				color   �ߵ���ɫ
				size �ߵĿ���(����)
	  ����ֵ��  ��
******************************************************************************/
void LCD_DrawThickLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint8_t size)
{
	if (size == 1)
	{
		LCD_DrawLine(x1, y1, x2, y2, color);
		return;
	}

	// ���ٱ߽���
	if (x1 < size || x2 < size || y1 < size || y2 < size)
		return;

	int16_t dx = x2 - x1;
	int16_t dy = y2 - y1;

	// ���پ����飨���ƣ�
	uint16_t abs_dx = dx > 0 ? dx : -dx;
	uint16_t abs_dy = dy > 0 ? dy : -dy;
	if ((abs_dx > MAX_ALLOWED_DISTANCE) || (abs_dy > MAX_ALLOWED_DISTANCE))
	{
		return;
	}
	//
	//     // ������ֱ�ߵ����
	//    if(dx == 0) {
	//        // ���ƴ�ֱ�ߵ�����
	//        for(uint8_t i = 0; i < thickness; i++) {
	//            LCD_DrawLine(x1 + i - thickness/2, y1, x2 + i - thickness/2, y2, color);
	//        }
	//        // �������˵İ�Բ
	//        LCD_DrawCircle(x1, y1, thickness/2, color, 1); // ���Բ
	//        LCD_DrawCircle(x2, y2, thickness/2, color, 1); // �յ�Բ
	//        return;
	//    }
	//
	//    // ����ˮƽ�ߵ����
	//    if(dy == 0) {
	//        // ����ˮƽ�ߵ�����
	//        for(uint8_t i = 0; i < thickness; i++) {
	//            LCD_DrawLine(x1, y1 + i - thickness/2, x2, y2 + i - thickness/2, color);
	//        }
	//        // �������˵İ�Բ
	//        LCD_DrawCircle(x1, y1, thickness/2, color, 1); // ���Բ
	//        LCD_DrawCircle(x2, y2, thickness/2, color, 1); // �յ�Բ
	//        return;
	//    }
	//
	//    // �����ߵĴ�ֱ����
	//    float nx = -dy;
	//    float ny = dx;
	//
	//    // ��һ��
	//	int16_t gcd_val = gcd(abs(nx), abs(ny));
	//    if(gcd_val != 0) {
	//        nx /= gcd_val;
	//        ny /= gcd_val;
	//    }
	//
	//    // ����ƫ����
	//    float offset = (thickness - 1) / 2.0f;
	//
	//    // ���ƶ���ƽ�����γɿ�������
	//    for(uint8_t i = 0; i < thickness; i++) {
	//        float currOffset = i - offset;
	//        int16_t x1_offset = x1 + (int16_t)(nx * currOffset);
	//        int16_t y1_offset = y1 + (int16_t)(ny * currOffset);
	//        int16_t x2_offset = x2 + (int16_t)(nx * currOffset);
	//        int16_t y2_offset = y2 + (int16_t)(ny * currOffset);
	//
	//        LCD_DrawLine(x1_offset, y1_offset, x2_offset, y2_offset, color);
	//    }
	//
	//    // �������˵�Բ�ζ˵�
	//    LCD_DrawCircle(x1, y1, thickness/2, color, 1); // ���Բ
	//    LCD_DrawCircle(x2, y2, thickness/2, color, 1); // �յ�Բ

	uint16_t t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	if (x1 < size || x2 < size || y1 < size || y2 < size)
		return;
	delta_x = x2 - x1; // ������������
	delta_y = y2 - y1;
	uRow = x1;
	uCol = y1;
	if (delta_x > 0)
		incx = 1; // ���õ�������
	else if (delta_x == 0)
		incx = 0; // ��ֱ��
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	if (delta_y > 0)
		incy = 1;
	else if (delta_y == 0)
		incy = 0; // ˮƽ��
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y)
		distance = delta_x; // ѡȡ��������������
	else
		distance = delta_y;
	for (t = 0; t <= distance + 1; t++) // �������
	{
		gui_fill_circle(uRow, uCol, size, color); // ����
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance)
		{
			xerr -= distance;
			uRow += incx;
		}
		if (yerr > distance)
		{
			yerr -= distance;
			uCol += incy;
		}
	}
}

void DrawThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color)
{
	// �߽���
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x0 >= SCREEN_WIDTH)
		x0 = SCREEN_WIDTH - 1;
	if (y0 >= SCREEN_HEIGHT)
		y0 = SCREEN_HEIGHT - 1;
	if (x1 >= SCREEN_WIDTH)
		x1 = SCREEN_WIDTH - 1;
	if (y1 >= SCREEN_HEIGHT)
		y1 = SCREEN_HEIGHT - 1;

	// �򻯰���߻���
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy, e2;

	for (;;)
	{
		// ���ֵ�
		for (int i = -thickness; i <= thickness; i++)
		{
			for (int j = -thickness; j <= thickness; j++)
			{
				if (x0 + i >= 0 && x0 + i < SCREEN_WIDTH &&
					y0 + j >= 0 && y0 + j < SCREEN_HEIGHT)
				{
					LCD_DrawPoint(x0 + i, y0 + j, color);
				}
			}
		}

		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy)
		{
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx)
		{
			err += dx;
			y0 += sy;
		}
	}
}

/******************************************************************************
	  ����˵����������
	  ������ݣ�x1,y1   ��ʼ����
				x2,y2   ��ֹ����
				color   ���ε���ɫ
	  ����ֵ��  ��
******************************************************************************/
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	LCD_DrawLine(x1, y1, x2, y1, color);
	LCD_DrawLine(x1, y1, x1, y2, color);
	LCD_DrawLine(x1, y2, x2, y2, color);
	LCD_DrawLine(x2, y1, x2, y2, color);
}

/******************************************************************************
	  ����˵������Բ
	  ������ݣ�x0,y0   Բ������
				r       �뾶
				color   Բ����ɫ
	  ����ֵ��  ��
******************************************************************************/
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
	int a, b;
	a = 0;
	b = r;
	while (a <= b)
	{
		LCD_DrawPoint(x0 - b, y0 - a, color); // 3
		LCD_DrawPoint(x0 + b, y0 - a, color); // 0
		LCD_DrawPoint(x0 - a, y0 + b, color); // 1
		LCD_DrawPoint(x0 - a, y0 - b, color); // 2
		LCD_DrawPoint(x0 + b, y0 + a, color); // 4
		LCD_DrawPoint(x0 + a, y0 - b, color); // 5
		LCD_DrawPoint(x0 + a, y0 + b, color); // 6
		LCD_DrawPoint(x0 - b, y0 + a, color); // 7
		a++;
		if ((a * a + b * b) > (r * r)) // �ж�Ҫ���ĵ��Ƿ��Զ
		{
			b--;
		}
	}
}

/******************************************************************************
	  ����˵������ʾ���ִ�
	  ������ݣ�x,y��ʾ����
				*s Ҫ��ʾ�ĺ��ִ�
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ� ��ѡ 16 24 32
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChinese(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	while (*s != 0)
	{
		if (sizey == 12)
			LCD_ShowChinese12x12(x, y, s, fc, bc, sizey, mode);
		else if (sizey == 16)
			LCD_ShowChinese16x16(x, y, s, fc, bc, sizey, mode);
		else if (sizey == 24)
			LCD_ShowChinese24x24(x, y, s, fc, bc, sizey, mode);
		else if (sizey == 32)
			LCD_ShowChinese32x32(x, y, s, fc, bc, sizey, mode);
		else
			return;
		s += 2;
		x += sizey;
	}
}

/******************************************************************************
	  ����˵������ʾ����12x12����
	  ������ݣ�x,y��ʾ����
				*s Ҫ��ʾ�ĺ���
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChinese12x12(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	uint8_t i, j, m = 0;
	uint16_t k;
	uint16_t HZnum;		  // ������Ŀ
	uint16_t TypefaceNum; // һ���ַ���ռ�ֽڴ�С
	uint16_t x0 = x;
	TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;

	HZnum = sizeof(tfont12) / sizeof(typFNT_GB12); // ͳ�ƺ�����Ŀ
	for (k = 0; k < HZnum; k++)
	{
		if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1)))
		{
			LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
			for (i = 0; i < TypefaceNum; i++)
			{
				for (j = 0; j < 8; j++)
				{
					if (!mode) // �ǵ��ӷ�ʽ
					{
						if (tfont12[k].Msk[i] & (0x01 << j))
							LCD_WR_DATA(fc);
						else
							LCD_WR_DATA(bc);
						m++;
						if (m % sizey == 0)
						{
							m = 0;
							break;
						}
					}
					else // ���ӷ�ʽ
					{
						if (tfont12[k].Msk[i] & (0x01 << j))
							LCD_DrawPoint(x, y, fc); // ��һ����
						x++;
						if ((x - x0) == sizey)
						{
							x = x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue; // ���ҵ���Ӧ�����ֿ������˳�����ֹ��������ظ�ȡģ����Ӱ��
	}
}

/******************************************************************************
	  ����˵������ʾ����16x16����
	  ������ݣ�x,y��ʾ����
				*s Ҫ��ʾ�ĺ���
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChinese16x16(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	uint8_t i, j, m = 0;
	uint16_t k;
	uint16_t HZnum;		  // ������Ŀ
	uint16_t TypefaceNum; // һ���ַ���ռ�ֽڴ�С
	uint16_t x0 = x;
	TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
	HZnum = sizeof(tfont16) / sizeof(typFNT_GB16); // ͳ�ƺ�����Ŀ
	for (k = 0; k < HZnum; k++)
	{
		if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
		{
			LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
			for (i = 0; i < TypefaceNum; i++)
			{
				for (j = 0; j < 8; j++)
				{
					if (!mode) // �ǵ��ӷ�ʽ
					{
						if (tfont16[k].Msk[i] & (0x01 << j))
							LCD_WR_DATA(fc);
						else
							LCD_WR_DATA(bc);
						m++;
						if (m % sizey == 0)
						{
							m = 0;
							break;
						}
					}
					else // ���ӷ�ʽ
					{
						if (tfont16[k].Msk[i] & (0x01 << j))
							LCD_DrawPoint(x, y, fc); // ��һ����
						x++;
						if ((x - x0) == sizey)
						{
							x = x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue; // ���ҵ���Ӧ�����ֿ������˳�����ֹ��������ظ�ȡģ����Ӱ��
	}
}

/******************************************************************************
	  ����˵������ʾ����24x24����
	  ������ݣ�x,y��ʾ����
				*s Ҫ��ʾ�ĺ���
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChinese24x24(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	uint8_t i, j, m = 0;
	uint16_t k;
	uint16_t HZnum;		  // ������Ŀ
	uint16_t TypefaceNum; // һ���ַ���ռ�ֽڴ�С
	uint16_t x0 = x;
	TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
	HZnum = sizeof(tfont24) / sizeof(typFNT_GB24); // ͳ�ƺ�����Ŀ
	for (k = 0; k < HZnum; k++)
	{
		if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
		{
			LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
			for (i = 0; i < TypefaceNum; i++)
			{
				for (j = 0; j < 8; j++)
				{
					if (!mode) // �ǵ��ӷ�ʽ
					{
						if (tfont24[k].Msk[i] & (0x01 << j))
							LCD_WR_DATA(fc);
						else
							LCD_WR_DATA(bc);
						m++;
						if (m % sizey == 0)
						{
							m = 0;
							break;
						}
					}
					else // ���ӷ�ʽ
					{
						if (tfont24[k].Msk[i] & (0x01 << j))
							LCD_DrawPoint(x, y, fc); // ��һ����
						x++;
						if ((x - x0) == sizey)
						{
							x = x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue; // ���ҵ���Ӧ�����ֿ������˳�����ֹ��������ظ�ȡģ����Ӱ��
	}
}

/******************************************************************************
	  ����˵������ʾ����32x32����
	  ������ݣ�x,y��ʾ����
				*s Ҫ��ʾ�ĺ���
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChinese32x32(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	uint8_t i, j, m = 0;
	uint16_t k;
	uint16_t HZnum;		  // ������Ŀ
	uint16_t TypefaceNum; // һ���ַ���ռ�ֽڴ�С
	uint16_t x0 = x;
	TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
	HZnum = sizeof(tfont32) / sizeof(typFNT_GB32); // ͳ�ƺ�����Ŀ
	for (k = 0; k < HZnum; k++)
	{
		if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
		{
			LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
			for (i = 0; i < TypefaceNum; i++)
			{
				for (j = 0; j < 8; j++)
				{
					if (!mode) // �ǵ��ӷ�ʽ
					{
						if (tfont32[k].Msk[i] & (0x01 << j))
							LCD_WR_DATA(fc);
						else
							LCD_WR_DATA(bc);
						m++;
						if (m % sizey == 0)
						{
							m = 0;
							break;
						}
					}
					else // ���ӷ�ʽ
					{
						if (tfont32[k].Msk[i] & (0x01 << j))
							LCD_DrawPoint(x, y, fc); // ��һ����
						x++;
						if ((x - x0) == sizey)
						{
							x = x0;
							y++;
							break;
						}
					}
				}
			}
		}
		continue; // ���ҵ���Ӧ�����ֿ������˳�����ֹ��������ظ�ȡģ����Ӱ��
	}
}

/******************************************************************************
	  ����˵������ʾ�����ַ�
	  ������ݣ�x,y��ʾ����
				num Ҫ��ʾ���ַ�
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	uint8_t temp, sizex, t, m = 0;
	uint16_t i, TypefaceNum; // һ���ַ���ռ�ֽڴ�С
	uint16_t x0 = x;
	sizex = sizey / 2;
	TypefaceNum = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
	num = num - ' ';									 // �õ�ƫ�ƺ��ֵ
	LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1); // ���ù��λ��
	for (i = 0; i < TypefaceNum; i++)
	{
		if (sizey == 12)
			temp = ascii_1206[num][i]; // ����6x12����
		else if (sizey == 16)
			temp = ascii_1608[num][i]; // ����8x16����
		else if (sizey == 24)
			temp = ascii_2412[num][i]; // ����12x24����
		else if (sizey == 32)
			temp = ascii_3216[num][i]; // ����16x32����
		else
			return;
		for (t = 0; t < 8; t++)
		{
			if (!mode) // �ǵ���ģʽ
			{
				if (temp & (0x01 << t))
					LCD_WR_DATA(fc);
				else
					LCD_WR_DATA(bc);
				m++;
				if (m % sizex == 0)
				{
					m = 0;
					break;
				}
			}
			else // ����ģʽ
			{
				if (temp & (0x01 << t))
					LCD_DrawPoint(x, y, fc); // ��һ����
				x++;
				if ((x - x0) == sizex)
				{
					x = x0;
					y++;
					break;
				}
			}
		}
	}
}

/******************************************************************************
	  ����˵������ʾ�ַ���
	  ������ݣ�x,y��ʾ����
				*p Ҫ��ʾ���ַ���
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
				mode:  0�ǵ���ģʽ  1����ģʽ
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
	while (*p != '\0')
	{
		LCD_ShowChar(x, y, *p, fc, bc, sizey, mode);
		x += sizey / 2;
		p++;
	}
}

/******************************************************************************
	  ����˵������ʾ����
	  ������ݣ�m������nָ��
	  ����ֵ��  ��
******************************************************************************/
uint32_t mypow(uint8_t m, uint8_t n)
{
	uint32_t result = 1;
	while (n--)
		result *= m;
	return result;
}

/******************************************************************************
	  ����˵������ʾ��������
	  ������ݣ�x,y��ʾ����
				num Ҫ��ʾ��������
				len Ҫ��ʾ��λ��
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
	uint8_t t, temp;
	uint8_t enshow = 0;
	uint8_t sizex = sizey / 2;
	for (t = 0; t < len; t++)
	{
		temp = (num / mypow(10, len - t - 1)) % 10;
		if (enshow == 0 && t < (len - 1))
		{
			if (temp == 0)
			{
				LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey, 0);
				continue;
			}
			else
				enshow = 1;
		}
		LCD_ShowChar(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
	}
}

/******************************************************************************
	  ����˵������ʾ��λС������
	  ������ݣ�x,y��ʾ����
				num Ҫ��ʾС������
				len Ҫ��ʾ��λ��
				fc �ֵ���ɫ
				bc �ֵı���ɫ
				sizey �ֺ�
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
	uint8_t t, temp, sizex;
	uint16_t num1;
	sizex = sizey / 2;
	num1 = num * 100;
	for (t = 0; t < len; t++)
	{
		temp = (num1 / mypow(10, len - t - 1)) % 10;
		if (t == (len - 2))
		{
			LCD_ShowChar(x + (len - 2) * sizex, y, '.', fc, bc, sizey, 0);
			t++;
			len += 1;
		}
		LCD_ShowChar(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
	}
}

/******************************************************************************
	  ����˵������ʾͼƬ
	  ������ݣ�x,y�������
				length ͼƬ����
				width  ͼƬ����
				pic[]  ͼƬ����
	  ����ֵ��  ��
******************************************************************************/
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const void *pic)
{
	esp_err_t ret;
	spi_transaction_t t;
	const uint8_t *pic_bytes = (const uint8_t *)pic;
	memset(&t, 0, sizeof(t));

	// ������ʾ����Ľ�������
	uint16_t x_end = x + length;
	uint16_t y_end = y + width;

	// ������ʾ��Χ
	LCD_Address_Set(x, y, x_end, y_end);

	// 计算像素总数
	uint32_t pixelCount = length * width;

	// ����ͼƬ�������ֽ��� (����ÿ������2�ֽ�)
	uint32_t dataSize = pixelCount * 2;

	// �ֿ鴫�����
	uint32_t remaining = dataSize;
	uint32_t offset = 0;

	LCD_DC_Set();

	// �ֿ鴫��ͼƬ����
	while (remaining > 0)
	{
		uint32_t chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
		t.length = chunkSize * 8;

		t.tx_buffer = pic_bytes + offset;
		ret = spi_device_polling_transmit(spi, &t); // Transmit!
		assert(ret == ESP_OK);

		offset += chunkSize;
		remaining -= chunkSize;
	}
}

/* ������ɫ�� */
void DrawColorBars(void)
{
	uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA};
	for (int i = 0; i < 6; i++)
	{
		LCD_Fill(i * 40, 0, ((i + 1) * 40) - 1, SCREEN_HEIGHT - 1, colors[i]);
	}
}

/* ���ƻҶȽ��� */
void DrawGrayscale(void)
{
#define GRAY_LEVELS 24
	const uint16_t level_width = SCREEN_WIDTH / GRAY_LEVELS;
	const uint16_t remainder = SCREEN_WIDTH % GRAY_LEVELS;

	for (uint8_t n = 0; n < GRAY_LEVELS; n++)
	{
		// ���㵱ǰ�Ҷȼ�����Χ
		uint16_t start_x = n * level_width;
		uint16_t end_x = start_x + level_width - 1;

		// �����������أ��������һ��
		if (n == GRAY_LEVELS - 1)
		{
			end_x += remainder;
		}

		// ����24���Ҷ�ֵ (0~23 �� 0~255)
		uint8_t gray = (n * 255) / (GRAY_LEVELS - 1);

		// ����RGB565��ɫ����ȷ����ʵ��RGB�꣩
		uint16_t color = RGB(gray, gray, gray);

		// ���Ҷȴ�����
		LCD_Fill(start_x, 0, end_x, SCREEN_HEIGHT - 1, color);
	}
}

/* ����������ť */
void DrawClearButton(void)
{
	LCD_Fill(SCREEN_WIDTH - BTN_WIDTH, SCREEN_HEIGHT - BTN_HEIGHT,
			 SCREEN_WIDTH, SCREEN_HEIGHT, GRAY);
	LCD_ShowString(SCREEN_WIDTH - BTN_WIDTH + 5, SCREEN_HEIGHT - BTN_HEIGHT + 8,
				   (const uint8_t *)"Clear", BLACK, GRAY, 16, 0);
}

/**
 * @brief  ��ָ��λ�����һ��Բ
 * @param  x0,y0: Բ������
 * @param  r: Բ�İ뾶
 * @param  color: �����ɫ
 * @retval ��
 */
void LCD_FillCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
	if (r == 0)
		return;

	int16_t x = r;
	int16_t y = 0;
	int16_t err = 0;

	while (x >= y)
	{
		// ���ˮƽ��
		LCD_Fill(x0 - x, y0 + y, x0 + x, y0 + y, color);
		LCD_Fill(x0 - y, y0 + x, x0 + y, y0 + x, color);
		LCD_Fill(x0 - x, y0 - y, x0 + x, y0 - y, color);
		LCD_Fill(x0 - y, y0 - x, x0 + y, y0 - x, color);

		if (err <= 0)
		{
			y += 1;
			err += 2 * y + 1;
		}
		if (err > 0)
		{
			x -= 1;
			err -= 2 * x + 1;
		}
	}
}

/******************************************************************************
      辅助函数：在缓冲区中绘制矩形
******************************************************************************/
static void DrawRect_Buffer(uint16_t *buffer, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	for(uint16_t y = y1; y <= y2 && y < SCREEN_HEIGHT; y++) {
		for(uint16_t x = x1; x <= x2 && x < SCREEN_WIDTH; x++) {
			buffer[y * SCREEN_WIDTH + x] = color;
		}
	}
}

/******************************************************************************
      辅助函数：在缓冲区中绘制圆（空心）
******************************************************************************/
static void DrawCircle_Buffer(uint16_t *buffer, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t thickness)
{
	// 绘制多个半径的圆来实现粗线条
	for(uint8_t t = 0; t < thickness; t++) {
		int16_t current_r = r - t;
		if(current_r <= 0) break;
		
		int16_t x = current_r;
		int16_t y = 0;
		int16_t err = 0;

		while (x >= y) {
			// 8个对称点
			if(x0 + x < SCREEN_WIDTH && y0 + y < SCREEN_HEIGHT) buffer[(y0 + y) * SCREEN_WIDTH + (x0 + x)] = color;
			if(x0 + y < SCREEN_WIDTH && y0 + x < SCREEN_HEIGHT) buffer[(y0 + x) * SCREEN_WIDTH + (x0 + y)] = color;
			if(x0 - y >= 0 && y0 + x < SCREEN_HEIGHT) buffer[(y0 + x) * SCREEN_WIDTH + (x0 - y)] = color;
			if(x0 - x >= 0 && y0 + y < SCREEN_HEIGHT) buffer[(y0 + y) * SCREEN_WIDTH + (x0 - x)] = color;
			if(x0 - x >= 0 && y0 - y >= 0) buffer[(y0 - y) * SCREEN_WIDTH + (x0 - x)] = color;
			if(x0 - y >= 0 && y0 - x >= 0) buffer[(y0 - x) * SCREEN_WIDTH + (x0 - y)] = color;
			if(x0 + y < SCREEN_WIDTH && y0 - x >= 0) buffer[(y0 - x) * SCREEN_WIDTH + (x0 + y)] = color;
			if(x0 + x < SCREEN_WIDTH && y0 - y >= 0) buffer[(y0 - y) * SCREEN_WIDTH + (x0 + x)] = color;
			
			if (err <= 0) {
				y += 1;
				err += 2 * y + 1;
			}
			if (err > 0) {
				x -= 1;
				err -= 2 * x + 1;
			}
		}
	}
}

/******************************************************************************
      辅助函数：在缓冲区中绘制实心圆
******************************************************************************/
static void FillCircle_Buffer(uint16_t *buffer, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
	for(int16_t y = -r; y <= r; y++) {
		for(int16_t x = -r; x <= r; x++) {
			if(x*x + y*y <= r*r) {
				int16_t px = x0 + x;
				int16_t py = y0 + y;
				if(px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
					buffer[py * SCREEN_WIDTH + px] = color;
				}
			}
		}
	}
}

/******************************************************************************
      眼睛类型枚举
******************************************************************************/
typedef enum {
	EYE_SQUARE,      // [] 方形眼睛 - 无表情
	EYE_CIRCLE,      // O 圆形眼睛 - 疑惑
	EYE_CLOSED,      // - 闭眼 - 不开心
	EYE_SLEEPY,      // _ 半闭眼 - 哈气
	EYE_EQUAL,       // = 双横线 - 开心/满足
	EYE_AT,          // @ 圆形带圈 - 惊讶/震惊
	EYE_CHEVRON_IN,  // > < 内尖括号 - 雀跃
	EYE_CHEVRON_UP,  // ^ ^ 上尖括号 - 开心
	EYE_WINK_LEFT,   // + < 左眼闭右眼睁 - Wink
	EYE_WINK_RIGHT,  // > + 右眼闭左眼睁 - Wink
	EYE_ANGRY,       // = = 双横线生气眼 - 生气(配#号)
	EYE_SLEEPING     // - - 闭眼睡觉 - 休眠ZZZ
} EyeType;

typedef enum {
	MOUTH_DOT,       // . 小点 - 无表情
	MOUTH_OPEN,      // O 张开 - 哈气
	MOUTH_LINE,      // - 平线 - 无表情/疑惑
	MOUTH_SAD,       // ^ 倒V - 不开心
	MOUTH_M,         // m 反W形 - 纠结/无言
	MOUTH_W_UP,      // w 正W形 - 开心/可爱
	MOUTH_SLEEP      // o 小圆 - 睡觉
} MouthType;

/******************************************************************************
      绘制眼睛函数
******************************************************************************/
static void DrawEye(uint16_t *buffer, uint16_t x, uint16_t y, EyeType type, uint16_t color)
{
	switch(type) {
		case EYE_SQUARE: // 方形眼睛
			DrawRect_Buffer(buffer, x - 10, y - 15, x + 10, y + 15, color);
			break;
			
		case EYE_CIRCLE: // 圆形眼睛
			FillCircle_Buffer(buffer, x, y, 18, color);
			break;
			
		case EYE_CLOSED: // 闭眼横线
			DrawRect_Buffer(buffer, x - 18, y - 2, x + 18, y + 2, color);
			break;
			
		case EYE_SLEEPY: // 半闭眼
			DrawRect_Buffer(buffer, x - 15, y + 10, x + 15, y + 18, color);
			break;
			
		case EYE_EQUAL: // = 双横线眼睛
			DrawRect_Buffer(buffer, x - 15, y - 8, x + 15, y - 4, color);
			DrawRect_Buffer(buffer, x - 15, y + 4, x + 15, y + 8, color);
			break;
			
		case EYE_AT: // @ 圆圈眼睛
			DrawCircle_Buffer(buffer, x, y, 18, color, 3);
			FillCircle_Buffer(buffer, x, y, 8, color);
			break;
			
		case EYE_CHEVRON_IN: // > 或 < (根据位置)
			// 画 > 或 < 形状
			if(x < SCREEN_WIDTH / 2) { // 左眼画 >
				for(int i = 0; i < 16; i++) {
					int y_offset = i * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y - 14 + y_offset, x - 5 + i, y - 11 + y_offset, color);
				}
				for(int i = 0; i < 16; i++) {
					int y_offset = (16 - i) * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y + y_offset, x - 5 + i, y + 3 + y_offset, color);
				}
			} else { // 右眼画 <
				for(int i = 0; i < 16; i++) {
					int y_offset = (16 - i) * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y - 14 + y_offset, x - 5 + i, y - 11 + y_offset, color);
				}
				for(int i = 0; i < 16; i++) {
					int y_offset = i * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y + y_offset, x - 5 + i, y + 3 + y_offset, color);
				}
			}
			break;
			
		case EYE_CHEVRON_UP: // ^ 上尖号
			// 画倒V形
			for(int i = 0; i < 14; i++) {
				int y_offset = i * 10 / 14;
				DrawRect_Buffer(buffer, x - 14 + i, y + y_offset, x - 11 + i, y + 3 + y_offset, color);
			}
			for(int i = 0; i < 14; i++) {
				int y_offset = (14 - i) * 10 / 14;
				DrawRect_Buffer(buffer, x + i, y + y_offset, x + 3 + i, y + 3 + y_offset, color);
			}
			break;
			
		case EYE_WINK_LEFT: // 左眼+ 右眼<
			if(x < SCREEN_WIDTH / 2) { // 左眼画+
				DrawRect_Buffer(buffer, x - 12, y - 3, x + 12, y + 3, color);
				DrawRect_Buffer(buffer, x - 3, y - 12, x + 3, y + 12, color);
			} else { // 右眼画<
				for(int i = 0; i < 16; i++) {
					int y_offset = (16 - i) * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y - 14 + y_offset, x - 5 + i, y - 11 + y_offset, color);
				}
				for(int i = 0; i < 16; i++) {
					int y_offset = i * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y + y_offset, x - 5 + i, y + 3 + y_offset, color);
				}
			}
			break;
			
		case EYE_WINK_RIGHT: // 左眼> 右眼+
			if(x < SCREEN_WIDTH / 2) { // 左眼画>
				for(int i = 0; i < 16; i++) {
					int y_offset = i * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y - 14 + y_offset, x - 5 + i, y - 11 + y_offset, color);
				}
				for(int i = 0; i < 16; i++) {
					int y_offset = (16 - i) * 14 / 16;
					DrawRect_Buffer(buffer, x - 8 + i, y + y_offset, x - 5 + i, y + 3 + y_offset, color);
				}
			} else { // 右眼画+
				DrawRect_Buffer(buffer, x - 12, y - 3, x + 12, y + 3, color);
				DrawRect_Buffer(buffer, x - 3, y - 12, x + 3, y + 12, color);
			}
			break;
			
		case EYE_ANGRY: // == 生气的双横线
			DrawRect_Buffer(buffer, x - 18, y - 8, x + 18, y - 4, color);
			DrawRect_Buffer(buffer, x - 18, y + 4, x + 18, y + 8, color);
			break;
			
		case EYE_SLEEPING: // 睡觉闭眼
			DrawRect_Buffer(buffer, x - 18, y - 2, x + 18, y + 2, color);
			break;
	}
}

/******************************************************************************
      绘制嘴巴函数
******************************************************************************/
static void DrawMouth(uint16_t *buffer, uint16_t x, uint16_t y, MouthType type, uint16_t color)
{
	switch(type) {
		case MOUTH_DOT: // 小点
			DrawRect_Buffer(buffer, x - 4, y - 3, x + 4, y + 3, color);
			break;
			
		case MOUTH_OPEN: // 张开的O型
			DrawCircle_Buffer(buffer, x, y, 15, color, 4);
			break;
			
		case MOUTH_LINE: // 平线
			DrawRect_Buffer(buffer, x - 20, y - 2, x + 20, y + 2, color);
			break;
			
		case MOUTH_SAD: // 倒V型（不开心）
			// 左边斜线
			for(int i = 0; i < 15; i++) {
				int y_offset = i * 8 / 15;
				DrawRect_Buffer(buffer, x - 15 + i, y - y_offset, x - 13 + i, y + 2 - y_offset, color);
			}
			// 右边斜线
			for(int i = 0; i < 15; i++) {
				int y_offset = (15 - i) * 8 / 15;
				DrawRect_Buffer(buffer, x + i, y - y_offset, x + 2 + i, y + 2 - y_offset, color);
			}
			break;
			
		case MOUTH_M: // M形嘴巴（纠结/无言） - 反W形(两个倒V)
			// 第一个V（左侧）
			// 左下斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = i * 8 / 12;
				DrawRect_Buffer(buffer, x - 22 + i, y - y_offset, x - 20 + i, y + 2 - y_offset, color);
			}
			// 左上斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = (12 - i) * 8 / 12;
				DrawRect_Buffer(buffer, x - 10 + i, y - y_offset, x - 8 + i, y + 2 - y_offset, color);
			}
			
			// 第二个V（右侧）
			// 右下斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = i * 8 / 12;
				DrawRect_Buffer(buffer, x + 2 + i, y - y_offset, x + 4 + i, y + 2 - y_offset, color);
			}
			// 右上斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = (12 - i) * 8 / 12;
				DrawRect_Buffer(buffer, x + 14 + i, y - y_offset, x + 16 + i, y + 2 - y_offset, color);
			}
			break;
			
		case MOUTH_W_UP: // W形嘴巴（开心） - 正W形(两个正V)
			// 第一个正V（左侧）- 从上到下到上
			// 左下斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = (12 - i) * 8 / 12;
				DrawRect_Buffer(buffer, x - 22 + i, y - y_offset, x - 20 + i, y + 2 - y_offset, color);
			}
			// 左上斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = i * 8 / 12;
				DrawRect_Buffer(buffer, x - 10 + i, y - y_offset, x - 8 + i, y + 2 - y_offset, color);
			}
			
			// 第二个正V（右侧）
			// 右下斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = (12 - i) * 8 / 12;
				DrawRect_Buffer(buffer, x + 2 + i, y - y_offset, x + 4 + i, y + 2 - y_offset, color);
			}
			// 右上斜线
			for(int i = 0; i < 12; i++) {
				int y_offset = i * 8 / 12;
				DrawRect_Buffer(buffer, x + 14 + i, y - y_offset, x + 16 + i, y + 2 - y_offset, color);
			}
			break;
			
		case MOUTH_SLEEP: // 睡觉小圆嘴
			FillCircle_Buffer(buffer, x, y, 6, color);
			break;
	}
}

/******************************************************************************
      快速显示帧缓冲
******************************************************************************/
static void DisplayFrame(uint16_t *framebuffer, uint32_t total_pixels)
{
	// 转换字节序
	for(uint32_t i = 0; i < total_pixels; i++) {
		uint16_t color = framebuffer[i];
		framebuffer[i] = (color << 8) | (color >> 8);
	}
	
	LCD_Address_Set(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
	LCD_DC_Set();
	
	// 分块传输
	spi_transaction_t t;
	const uint32_t chunk_size = 2048;
	uint32_t remaining = total_pixels;
	uint32_t offset = 0;
	
	while(remaining > 0) {
		uint32_t current_chunk = (remaining > chunk_size) ? chunk_size : remaining;
		memset(&t, 0, sizeof(t));
		t.length = current_chunk * 16;
		t.tx_buffer = &framebuffer[offset];
		spi_device_polling_transmit(spi, &t);
		offset += current_chunk;
		remaining -= current_chunk;
	}
	
	// 恢复字节序
	for(uint32_t i = 0; i < total_pixels; i++) {
		uint16_t color = framebuffer[i];
		framebuffer[i] = (color << 8) | (color >> 8);
	}
}

/******************************************************************************
      眨眼动画 - 从当前状态到闭眼再到目标状态
******************************************************************************/
static void BlinkTransition(uint16_t center_x, uint16_t center_y, 
                           EyeType from_eye, EyeType to_eye,
                           MouthType from_mouth, MouthType to_mouth,
                           uint16_t color)
{
	uint32_t total_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
	uint16_t left_eye_x = center_x - 50;
	uint16_t right_eye_x = center_x + 50;
	uint16_t eye_y = center_y - 10;
	uint16_t mouth_y = center_y + 45;
	
	// 眨眼分为3个阶段：闭眼过程 -> 完全闭眼 -> 睁眼到新状态
	const int close_frames = 3;  // 闭眼帧数
	const int open_frames = 3;   // 睁眼帧数
	
	// 阶段1: 闭眼过程（眼睛逐渐变扁）
	for(int frame = 0; frame < close_frames; frame++) {
		uint16_t *framebuffer = (uint16_t *)malloc(total_pixels * sizeof(uint16_t));
		if(!framebuffer) return;
		
		// 清空背景
		for(uint32_t i = 0; i < total_pixels; i++) {
			framebuffer[i] = BLACK;
		}
		
		// 绘制逐渐变扁的眼睛
		float progress = (float)(frame + 1) / close_frames;
		int eye_height = 30 * (1.0f - progress); // 眼睛高度逐渐减小
		
		if(eye_height > 2) {
			// 根据原始眼型绘制变扁的眼睛
			if(from_eye == EYE_SQUARE) {
				DrawRect_Buffer(framebuffer, left_eye_x - 10, eye_y - eye_height/2, 
				              left_eye_x + 10, eye_y + eye_height/2, color);
				DrawRect_Buffer(framebuffer, right_eye_x - 10, eye_y - eye_height/2, 
				              right_eye_x + 10, eye_y + eye_height/2, color);
			} else if(from_eye == EYE_CIRCLE) {
				// 圆形变扁平
				for(int16_t y = -eye_height/2; y <= eye_height/2; y++) {
					int width = 18 * sqrt(1.0f - (float)(y*y)/(eye_height*eye_height/4));
					DrawRect_Buffer(framebuffer, left_eye_x - width, eye_y + y, 
					              left_eye_x + width, eye_y + y + 1, color);
					DrawRect_Buffer(framebuffer, right_eye_x - width, eye_y + y, 
					              right_eye_x + width, eye_y + y + 1, color);
				}
			} else {
				// 其他眼型直接画扁矩形
				DrawRect_Buffer(framebuffer, left_eye_x - 18, eye_y - eye_height/2, 
				              left_eye_x + 18, eye_y + eye_height/2, color);
				DrawRect_Buffer(framebuffer, right_eye_x - 18, eye_y - eye_height/2, 
				              right_eye_x + 18, eye_y + eye_height/2, color);
			}
		}
		
		// 保持原嘴巴
		DrawMouth(framebuffer, center_x, mouth_y, from_mouth, WHITE);
		
		DisplayFrame(framebuffer, total_pixels);
		free(framebuffer);
		vTaskDelay(pdMS_TO_TICKS(40)); // 每帧40ms
	}
	
	// 阶段2: 完全闭眼（画一条线）
	{
		uint16_t *framebuffer = (uint16_t *)malloc(total_pixels * sizeof(uint16_t));
		if(framebuffer) {
			for(uint32_t i = 0; i < total_pixels; i++) {
				framebuffer[i] = BLACK;
			}
			
			// 闭眼横线
			DrawRect_Buffer(framebuffer, left_eye_x - 18, eye_y - 2, left_eye_x + 18, eye_y + 2, color);
			DrawRect_Buffer(framebuffer, right_eye_x - 18, eye_y - 2, right_eye_x + 18, eye_y + 2, color);
			
			// 嘴巴可能变化
			if(from_mouth != to_mouth) {
				DrawMouth(framebuffer, center_x, mouth_y, to_mouth, WHITE);
			} else {
				DrawMouth(framebuffer, center_x, mouth_y, from_mouth, WHITE);
			}
			
			DisplayFrame(framebuffer, total_pixels);
			free(framebuffer);
			vTaskDelay(pdMS_TO_TICKS(60)); // 闭眼停留60ms
		}
	}
	
	// 阶段3: 睁开到新状态
	for(int frame = 0; frame < open_frames; frame++) {
		uint16_t *framebuffer = (uint16_t *)malloc(total_pixels * sizeof(uint16_t));
		if(!framebuffer) return;
		
		for(uint32_t i = 0; i < total_pixels; i++) {
			framebuffer[i] = BLACK;
		}
		
		// 绘制逐渐睁开的眼睛
		float progress = (float)(frame + 1) / open_frames;
		
		if(progress < 1.0f) {
			// 部分睁开
			int eye_height = 30 * progress;
			
			if(to_eye == EYE_SQUARE) {
				DrawRect_Buffer(framebuffer, left_eye_x - 10, eye_y - eye_height/2, 
				              left_eye_x + 10, eye_y + eye_height/2, color);
				DrawRect_Buffer(framebuffer, right_eye_x - 10, eye_y - eye_height/2, 
				              right_eye_x + 10, eye_y + eye_height/2, color);
			} else if(to_eye == EYE_CIRCLE) {
				for(int16_t y = -eye_height/2; y <= eye_height/2; y++) {
					int width = 18 * sqrt(1.0f - (float)(y*y)/(eye_height*eye_height/4));
					DrawRect_Buffer(framebuffer, left_eye_x - width, eye_y + y, 
					              left_eye_x + width, eye_y + y + 1, color);
					DrawRect_Buffer(framebuffer, right_eye_x - width, eye_y + y, 
					              right_eye_x + width, eye_y + y + 1, color);
				}
			} else if(to_eye == EYE_SLEEPY) {
				// 半闭眼逐渐出现
				int sleepy_height = 8 * progress;
				DrawRect_Buffer(framebuffer, left_eye_x - 15, eye_y + 10 + (8-sleepy_height), 
				              left_eye_x + 15, eye_y + 18, color);
				DrawRect_Buffer(framebuffer, right_eye_x - 15, eye_y + 10 + (8-sleepy_height), 
				              right_eye_x + 15, eye_y + 18, color);
			} else {
				DrawRect_Buffer(framebuffer, left_eye_x - 18, eye_y - eye_height/2, 
				              left_eye_x + 18, eye_y + eye_height/2, color);
				DrawRect_Buffer(framebuffer, right_eye_x - 18, eye_y - eye_height/2, 
				              right_eye_x + 18, eye_y + eye_height/2, color);
			}
		} else {
			// 完全睁开 - 画最终状态
			DrawEye(framebuffer, left_eye_x, eye_y, to_eye, color);
			DrawEye(framebuffer, right_eye_x, eye_y, to_eye, color);
		}
		
		DrawMouth(framebuffer, center_x, mouth_y, to_mouth, WHITE);
		
		DisplayFrame(framebuffer, total_pixels);
		free(framebuffer);
		vTaskDelay(pdMS_TO_TICKS(40));
	}
}

/******************************************************************************
      机器人表情绘制函数 - 使用组合方式（指定位置）
      expression: 0=无表情, 1=哈气, 2=疑惑, 3=不开心, etc.
      center_x, center_y: 脸部中心位置
******************************************************************************/
void Robot_DrawFace_At(uint8_t expression, uint16_t center_x, uint16_t center_y)
{
	// 定义颜色
	uint16_t PINK = RGB(255, 150, 200);
	
	// 分配全屏缓冲区
	uint32_t total_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
	uint16_t *framebuffer = (uint16_t *)malloc(total_pixels * sizeof(uint16_t));
	
	if(!framebuffer) {
		printf("Failed to allocate framebuffer!\n");
		return;
	}
	
	// 清空缓冲区为黑色
	for(uint32_t i = 0; i < total_pixels; i++) {
		framebuffer[i] = BLACK;
	}
	
	// 定义眼睛和嘴巴位置
	uint16_t left_eye_x = center_x - 50;
	uint16_t right_eye_x = center_x + 50;
	uint16_t eye_y = center_y - 10;
	uint16_t mouth_y = center_y + 45;
	
	// 根据表情类型组合绘制
	EyeType eye_type;
	MouthType mouth_type;
	
	switch(expression) {
		case 0: // 无表情: [] - []
			eye_type = EYE_SQUARE;
			mouth_type = MOUTH_LINE;
			break;
			
		case 1: // 哈气: _ O _
			eye_type = EYE_SLEEPY;
			mouth_type = MOUTH_OPEN;
			break;
			
		case 2: // 疑惑: O - O
			eye_type = EYE_CIRCLE;
			mouth_type = MOUTH_LINE;
			break;
			
		case 3: // 不开心: - ^ -
			eye_type = EYE_CLOSED;
			mouth_type = MOUTH_SAD;
			break;
			
		case 4: // 生气: - - - (带红色#在左上角)
			eye_type = EYE_CLOSED;
			mouth_type = MOUTH_LINE;
			// 在左上角画红色#号
			DrawRect_Buffer(framebuffer, 15, 15, 18, 30, RED);
			DrawRect_Buffer(framebuffer, 22, 15, 25, 30, RED);
			DrawRect_Buffer(framebuffer, 12, 19, 28, 22, RED);
			DrawRect_Buffer(framebuffer, 12, 24, 28, 27, RED);
			break;
			
		case 5: // = w = 开心
			eye_type = EYE_EQUAL;
			mouth_type = MOUTH_W_UP;
			break;
			
		case 6: // @ w @ 惊讶开心
			eye_type = EYE_AT;
			mouth_type = MOUTH_W_UP;
			break;
			
		case 7: // @ - @ 惊讶
			eye_type = EYE_AT;
			mouth_type = MOUTH_LINE;
			break;
			
		case 8: // > < m 雀跃纠结
			eye_type = EYE_CHEVRON_IN;
			mouth_type = MOUTH_M;
			break;
			
		case 9: // ^ ^ w 超开心
			eye_type = EYE_CHEVRON_UP;
			mouth_type = MOUTH_W_UP;
			break;
			
		case 10: // + < 左眼Wink
			eye_type = EYE_WINK_LEFT;
			mouth_type = MOUTH_LINE;
			break;
			
		case 11: // > + 右眼Wink
			eye_type = EYE_WINK_RIGHT;
			mouth_type = MOUTH_LINE;
			break;
			
		case 12: // = = # 生气
			eye_type = EYE_ANGRY;
			mouth_type = MOUTH_LINE;
			// 在右上角画红色#号
			DrawRect_Buffer(framebuffer, SCREEN_WIDTH - 35, 15, SCREEN_WIDTH - 32, 30, RED);
			DrawRect_Buffer(framebuffer, SCREEN_WIDTH - 28, 15, SCREEN_WIDTH - 25, 30, RED);
			DrawRect_Buffer(framebuffer, SCREEN_WIDTH - 38, 19, SCREEN_WIDTH - 22, 22, RED);
			DrawRect_Buffer(framebuffer, SCREEN_WIDTH - 38, 24, SCREEN_WIDTH - 22, 27, RED);
			break;
			
		case 13: // ZZZ 休眠
			eye_type = EYE_SLEEPING;
			mouth_type = MOUTH_SLEEP;
			// 画ZZZ符号（右上方）
			for(int z = 0; z < 3; z++) {
				int base_x = center_x + 60 + z * 15;
				int base_y = center_y - 50 + z * 12;
				int size = 10 - z * 2;
				// Z的横线
				DrawRect_Buffer(framebuffer, base_x, base_y, base_x + size, base_y + 2, WHITE);
				DrawRect_Buffer(framebuffer, base_x, base_y + size, base_x + size, base_y + size + 2, WHITE);
				// Z的斜线
				for(int i = 0; i <= size; i++) {
					DrawRect_Buffer(framebuffer, base_x + size - i, base_y + i, base_x + size - i + 2, base_y + i + 2, WHITE);
				}
			}
			break;
			
		default:
			eye_type = EYE_SQUARE;
			mouth_type = MOUTH_DOT;
			break;
	}
	
	// 绘制两只眼睛
	DrawEye(framebuffer, left_eye_x, eye_y, eye_type, PINK);
	DrawEye(framebuffer, right_eye_x, eye_y, eye_type, PINK);
	
	// 绘制嘴巴
	DrawMouth(framebuffer, center_x, mouth_y, mouth_type, WHITE);
	
	// 显示帧
	DisplayFrame(framebuffer, total_pixels);
	free(framebuffer);
}

/******************************************************************************
      机器人表情绘制函数 - 默认中心位置
******************************************************************************/
void Robot_DrawFace(uint8_t expression)
{
	Robot_DrawFace_At(expression, 142, 120);
}

/******************************************************************************
      机器人表情演示函数 - 带眨眼动画的切换（自动循环）
******************************************************************************/
void Robot_Demo(void)
{
	printf("\n=== Robot Face Demo with Blink Animation ===\n");
	printf("Expressions:\n");
	printf("  0 = 无表情 ([] - [])\n");
	printf("  1 = 哈气 (_ O _)\n");
	printf("  2 = 疑惑 (O - O)\n");
	printf("  3 = 不开心 (- ^ -)\n");
	printf("  4 = 生气 (- - - with #)\n");
	printf("  5 = 开心 (= w =)\n");
	printf("  6 = 惊讶开心 (@ w @)\n");
	printf("  7 = 惊讶 (@ - @)\n");
	printf("  8 = 雀跃纠结 (> < m)\n");
	printf("  9 = 超开心 (^ ^ w)\n");
	printf("  10 = 左眼Wink (+ <)\n");
	printf("  11 = 右眼Wink (> +)\n");
	printf("  12 = 生气 (= = #)\n\n");
	
	EyeType eye_types[] = {EYE_SQUARE, EYE_SLEEPY, EYE_CIRCLE, EYE_CLOSED, EYE_CLOSED, EYE_EQUAL, EYE_AT, EYE_AT, EYE_CHEVRON_IN, EYE_CHEVRON_UP, EYE_WINK_LEFT, EYE_WINK_RIGHT, EYE_ANGRY};
	MouthType mouth_types[] = {MOUTH_LINE, MOUTH_OPEN, MOUTH_LINE, MOUTH_SAD, MOUTH_LINE, MOUTH_W_UP, MOUTH_W_UP, MOUTH_LINE, MOUTH_M, MOUTH_W_UP, MOUTH_LINE, MOUTH_LINE, MOUTH_LINE};
	
	uint16_t center_x = 142;
	uint16_t center_y = 120;
	
	uint8_t current_expr = 0;
	
	// 显示第一个表情
	printf("Drawing initial expression: %d\n", current_expr);
	Robot_DrawFace(current_expr);
	vTaskDelay(pdMS_TO_TICKS(2000));
	
	while(1) {
		uint8_t next_expr = (current_expr + 1) % 13;
		
		printf("Blinking transition: %d -> %d\n", current_expr, next_expr);
		
		// 眨眼切换到下一个表情
		BlinkTransition(center_x, center_y,
		               eye_types[current_expr], eye_types[next_expr],
		               mouth_types[current_expr], mouth_types[next_expr],
		               RGB(255, 150, 200));
		
		current_expr = next_expr;
		
		// 保持新表情一段时间
		vTaskDelay(pdMS_TO_TICKS(10000));
		
		// 随机眨眼（不改变表情）
		if((esp_random() % 100) < 30) {  // 30%概率随机眨眼
			printf("Random blink\n");
			BlinkTransition(center_x, center_y,
			               eye_types[current_expr], eye_types[current_expr],
			               mouth_types[current_expr], mouth_types[current_expr],
			               RGB(255, 150, 200));
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}
}

/******************************************************************************
      状态机定义
******************************************************************************/
typedef enum {
	STATE_IDLE,          // 静止状态
	STATE_TRACKING,      // 跟随状态
	STATE_DODGING,       // 闪避状态
	STATE_AUTO_CHANGE,   // 自动切换状态
	STATE_SLEEPING       // 休眠状态
} RobotState;

typedef enum {
	GESTURE_NONE,
	GESTURE_TAP,         // 点击
	GESTURE_DRAG,        // 拖拽
	GESTURE_SWIPE_FAST,  // 快速滑动
	GESTURE_LONG_PRESS   // 长按
} GestureType;

/******************************************************************************
      触摸分析器 - 精确识别手势
******************************************************************************/
typedef struct {
	bool is_touching;
	uint16_t start_x, start_y;
	uint16_t last_x, last_y;
	uint32_t touch_start_time;
	uint32_t last_update_time;
	uint16_t total_distance;
	uint16_t velocity;  // 像素/100ms
} TouchAnalyzer;

static GestureType analyze_gesture(TouchAnalyzer *analyzer, uint32_t current_time) {
	if(!analyzer->is_touching) {
		return GESTURE_NONE;
	}
	
	uint32_t duration = current_time - analyzer->touch_start_time;
	uint16_t total_distance = abs(analyzer->last_x - analyzer->start_x) + abs(analyzer->last_y - analyzer->start_y);
	
	// 计算瞬时速度 (像素/秒)
	uint32_t time_diff = current_time - analyzer->touch_start_time;
	if(time_diff > 50) {
		analyzer->velocity = (total_distance * 1000) / time_diff;
	}
	
	// 手势识别逻辑（优先级从高到低）
	if(duration > 50 && analyzer->velocity > 300) {
		// 快速移动 -> 快速滑动（300像素/秒）
		return GESTURE_SWIPE_FAST;
	}
	else if(duration > 1000 && total_distance < 10) {
		// 长时间按住不动 -> 长按
		return GESTURE_LONG_PRESS;
	}
	else if(duration > 100 && total_distance > 10) {
		// 持续移动 -> 拖拽（降低阈值到10px）
		return GESTURE_DRAG;
	}
	else if(duration < 400 && total_distance < 10) {
		// 短时间轻触 -> 点击（提高到400ms）
		return GESTURE_TAP;
	}
	
	return GESTURE_NONE;
}

/******************************************************************************
      交互式表情演示 - 状态机版本
      基于状态机的智能交互系统，精确手势识别
******************************************************************************/
void Robot_Demo_Interactive(void)
{
	printf("\n=== State Machine Based Interactive Robot ===\n");
	printf("触摸分析:\n");
	printf("  点击(<200ms, <8px) -> 切换表情\n");
	printf("  拖拽(>100ms, >15px) -> 跟随手指\n");
	printf("  快速滑动(速度>50px/100ms) -> 闪避\n");
	printf("  长按(>1000ms) -> 重置\n\n");
	
	// 表情数据
	EyeType eye_types[] = {EYE_SQUARE, EYE_SLEEPY, EYE_CIRCLE, EYE_CLOSED, EYE_CLOSED, EYE_EQUAL, EYE_AT, EYE_AT, EYE_CHEVRON_IN, EYE_CHEVRON_UP, EYE_WINK_LEFT, EYE_WINK_RIGHT, EYE_ANGRY, EYE_SLEEPING};
	MouthType mouth_types[] = {MOUTH_LINE, MOUTH_OPEN, MOUTH_LINE, MOUTH_SAD, MOUTH_LINE, MOUTH_W_UP, MOUTH_W_UP, MOUTH_LINE, MOUTH_M, MOUTH_W_UP, MOUTH_LINE, MOUTH_LINE, MOUTH_LINE, MOUTH_SLEEP};
	
	// 状态变量
	RobotState current_state = STATE_IDLE;
	uint8_t current_expr = 0;
	uint16_t center_x = 138;
	uint16_t center_y = 120;
	
	// 触摸分析器
	TouchAnalyzer touch_analyzer = {0};
	GestureType gesture = GESTURE_NONE;
	
	// 时间记录
	uint32_t last_touch_time = 0;
	uint32_t last_expr_change = 0;
	uint32_t state_enter_time = 0;
	
	// 自动切换相关
	uint8_t idle_expr_index = 0;
	uint8_t idle_expressions[] = {0, 1, 2, 5, 9}; // 平和表情序列
	
	// 显示初始表情
	Robot_DrawFace_At(current_expr, center_x, center_y);
	uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	last_touch_time = current_time;
	last_expr_change = current_time;
	state_enter_time = current_time;
	
	vTaskDelay(pdMS_TO_TICKS(500));
	
	while(1) {
		current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		
		// 读取触摸信息
		CST816_Get_XY_AXIS();
		uint8_t finger_num = CST816_Get_FingerNum();
		bool has_touch = (finger_num > 0);
		
		// 更新触摸分析器
		if(has_touch) {
			if(!touch_analyzer.is_touching) {
				// 触摸开始
				touch_analyzer.is_touching = true;
				touch_analyzer.start_x = CST816_Instance.X_Pos;
				touch_analyzer.start_y = CST816_Instance.Y_Pos;
				touch_analyzer.touch_start_time = current_time;
				touch_analyzer.total_distance = 0;
				touch_analyzer.velocity = 0;
				printf("触摸开始: (%d, %d)\n", touch_analyzer.start_x, touch_analyzer.start_y);
			}
			touch_analyzer.last_x = CST816_Instance.X_Pos;
			touch_analyzer.last_y = CST816_Instance.Y_Pos;
			touch_analyzer.last_update_time = current_time;
			last_touch_time = current_time;
		}
		else if(touch_analyzer.is_touching) {
			// 触摸结束 - 最后检查一次手势（捕获快速滑动）
			uint32_t duration = current_time - touch_analyzer.touch_start_time;
			uint16_t total_distance = abs(touch_analyzer.last_x - touch_analyzer.start_x) + 
			                          abs(touch_analyzer.last_y - touch_analyzer.start_y);
			
			// 快速滑动检测
			if(duration > 50 && duration < 500 && total_distance > 40) {
				gesture = GESTURE_SWIPE_FAST;
				printf("触摸结束: 持续%lums, 距离%dpx [快速滑动]\n", duration, total_distance);
			}
			else {
				printf("触摸结束: 持续%lums, 距离%dpx\n", duration, total_distance);
			}
			
			touch_analyzer.is_touching = false;
		}
		
		// 分析手势
		GestureType gesture = analyze_gesture(&touch_analyzer, current_time);
		
		// ===== 状态机逻辑 =====
		RobotState next_state = current_state;
		
		// 全局优先处理快速滑动（闪避）
		if(gesture == GESTURE_SWIPE_FAST && current_state != STATE_DODGING) {
			next_state = STATE_DODGING;
			printf("[状态] %s -> DODGING (快速滑动, 速度=%d)\n", 
			       (current_state == STATE_IDLE) ? "IDLE" : "TRACKING",
			       touch_analyzer.velocity);
		}
		
		switch(current_state) {
			case STATE_IDLE:
				if(gesture == GESTURE_DRAG) {
					next_state = STATE_TRACKING;
					printf("[状态] IDLE -> TRACKING (拖拽)\n");
				}
				else if(gesture == GESTURE_TAP) {
					// 点击切换表情
					idle_expr_index = (idle_expr_index + 1) % 5;
					uint8_t new_expr = idle_expressions[idle_expr_index];
					if(new_expr != current_expr) {
						BlinkTransition(center_x, center_y,
						               eye_types[current_expr], eye_types[new_expr],
						               mouth_types[current_expr], mouth_types[new_expr],
						               RGB(255, 150, 200));
						current_expr = new_expr;
						last_expr_change = current_time;
						printf("[动作] 点击切换表情: %d\n", current_expr);
					}
				}
				else if(!has_touch && (current_time - last_touch_time > 5000)) {
					next_state = STATE_AUTO_CHANGE;
					printf("[状态] IDLE -> AUTO_CHANGE (5秒无触摸)\n");
				}
				break;
				
			case STATE_TRACKING:
				if(!has_touch) {
					next_state = STATE_IDLE;
					printf("[状态] TRACKING -> IDLE (触摸结束)\n");
				}
				else if(gesture == GESTURE_DRAG) {
					// 继续跟随
					uint16_t touch_x = CST816_Instance.X_Pos;
					if(touch_x > 60 && touch_x < SCREEN_WIDTH - 60) {
						// 平滑移动
						int16_t dx = touch_x - center_x;
						if(abs(dx) > 3) {
							center_x += (dx > 0) ? 8 : -8;
						}
						
						// 根据手指方向显示表情
						uint8_t target_expr = 5; // 默认开心
						if(touch_x < center_x - 20) {
							// 手指在左边，眼睛看左
							target_expr = 10; // 左眼眨
						}
						else if(touch_x > center_x + 20) {
							// 手指在右边，眼睛看右
							target_expr = 11; // 右眼眨
						}
						else if(abs(touch_x - center_x) < 15) {
							// 手指在中间，显示好奇
							target_expr = 2; // 圆圈眼睛
						}
						
						if(current_expr != target_expr && (current_time - last_expr_change > 500)) {
							current_expr = target_expr;
							last_expr_change = current_time;
						}
						
						Robot_DrawFace_At(current_expr, center_x, center_y);
					}
				}
				break;
				
			case STATE_DODGING:
				// 闪避状态持续600ms后返回IDLE
				if(current_time - state_enter_time > 600) {
					next_state = STATE_IDLE;
					printf("[状态] DODGING -> IDLE (动画完成)\n");
				}
				break;
				
			case STATE_AUTO_CHANGE:
				if(has_touch) {
					next_state = STATE_IDLE;
					printf("[状态] AUTO_CHANGE -> IDLE (触摸)\n");
				}
				else if(current_time - last_touch_time > 30000) {
					next_state = STATE_SLEEPING;
					printf("[状态] AUTO_CHANGE -> SLEEPING (30秒无交互)\n");
				}
				else if(current_time - last_expr_change > 3000) {
					// 每3秒切换表情
					idle_expr_index = (idle_expr_index + 1) % 5;
					uint8_t new_expr = idle_expressions[idle_expr_index];
					if(new_expr != current_expr) {
						BlinkTransition(center_x, center_y,
						               eye_types[current_expr], eye_types[new_expr],
						               mouth_types[current_expr], mouth_types[new_expr],
						               RGB(255, 150, 200));
						current_expr = new_expr;
						last_expr_change = current_time;
						printf("[动作] 自动切换表情: %d\n", current_expr);
					}
				}
				break;
				
			case STATE_SLEEPING:
				if(has_touch || gesture != GESTURE_NONE) {
					// 唤醒
					next_state = STATE_IDLE;
					current_expr = 7; // 惊讶
					BlinkTransition(center_x, center_y,
					               EYE_SLEEPING, eye_types[current_expr],
					               MOUTH_SLEEP, mouth_types[current_expr],
					               RGB(255, 150, 200));
					last_expr_change = current_time;
					printf("[状态] SLEEPING -> IDLE (唤醒)\n");
				}
				break;
		}
		
		// 状态转换处理
		if(next_state != current_state) {
			current_state = next_state;
			state_enter_time = current_time;
			
			// 进入新状态的初始化
			switch(current_state) {
				case STATE_DODGING: {
					// 执行闪避动画
					uint8_t dodge_expr = 12; // 生气表情 >_<
					current_expr = dodge_expr;
					
					// 计算滑动方向和距离
					int16_t dx = touch_analyzer.last_x - touch_analyzer.start_x;
					int16_t dy = touch_analyzer.last_y - touch_analyzer.start_y;
					
					// 判断主要方向（水平还是垂直）
					int16_t dodge_offset = 35;
					uint16_t dodge_x = center_x;
					uint16_t dodge_y = center_y;
					
					if(abs(dx) > abs(dy)) {
						// 水平滑动 -> 水平闪避
						if(dx > 0) {
							// 向右滑 -> 向左闪避
							dodge_x = center_x - dodge_offset;
							if(dodge_x < 70) dodge_x = 70;
						} else {
							// 向左滑 -> 向右闪避
							dodge_x = center_x + dodge_offset;
							if(dodge_x > 214) dodge_x = 214;
						}
						printf("[闪避] 水平方向, dx=%d\n", dx);
					} else {
						// 垂直滑动 -> 垂直闪避
						if(dy > 0) {
							// 向下滑 -> 向上闪避
							dodge_y = center_y - dodge_offset;
							if(dodge_y < 80) dodge_y = 80;
						} else {
							// 向上滑 -> 向下闪避
							dodge_y = center_y + dodge_offset;
							if(dodge_y > 160) dodge_y = 160;
						}
						printf("[闪避] 垂直方向, dy=%d\n", dy);
					}
					
					// 快速闪避
					for(int i = 0; i < 6; i++) {
						if(center_x < dodge_x) center_x += 7;
						else if(center_x > dodge_x) center_x -= 7;
						if(center_y < dodge_y) center_y += 7;
						else if(center_y > dodge_y) center_y -= 7;
						Robot_DrawFace_At(current_expr, center_x, center_y);
						vTaskDelay(pdMS_TO_TICKS(20));
					}
					
					// 恢复原位
					vTaskDelay(pdMS_TO_TICKS(250));
					for(int i = 0; i < 6; i++) {
						if(center_x < 142) center_x += 7;
						else if(center_x > 142) center_x -= 7;
						if(center_y < 120) center_y += 7;
						else if(center_y > 120) center_y -= 7;
						Robot_DrawFace_At(current_expr, center_x, center_y);
						vTaskDelay(pdMS_TO_TICKS(20));
					}
					center_x = 142;
					center_y = 120;
					break;
				}
				
				case STATE_SLEEPING:
					current_expr = 13; // ZZZ
					BlinkTransition(center_x, center_y,
					               eye_types[0], EYE_SLEEPING,
					               mouth_types[0], MOUTH_SLEEP,
					               RGB(255, 150, 200));
					Robot_DrawFace_At(current_expr, center_x, center_y);
					break;
					
				default:
					break;
			}
		}
		
		// 位置回中（非跟随状态）
		if(current_state != STATE_TRACKING && current_state != STATE_DODGING && center_x != 142) {
			if(center_x < 142) center_x += 3;
			else if(center_x > 142) center_x -= 3;
			if(abs(center_x - 142) <= 3) center_x = 142;
			Robot_DrawFace_At(current_expr, center_x, center_y);
		}
		
		// 随机眨眼（非睡眠状态）
		if(current_state != STATE_SLEEPING && (esp_random() % 400) < 2) {
			BlinkTransition(center_x, center_y,
			               eye_types[current_expr], eye_types[current_expr],
			               mouth_types[current_expr], mouth_types[current_expr],
			               RGB(255, 150, 200));
		}
		
		// 更新频率：50ms
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

/******************************************************************************
      显示RGB565格式图片
      支持分块传输，避免超出SPI硬件限制
******************************************************************************/
void LCD_ShowRGB565(uint16_t x, uint16_t y, const uint16_t *rgb565_data, 
                    uint16_t width, uint16_t height)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));

	// 设置显示窗口
	LCD_Address_Set(x, y, x + width - 1, y + height - 1);
	
	// 计算像素总数
	uint32_t total_pixels = width * height;
	
	// 分块传输数据（RGB565数据已经是正确格式，直接传输）
	uint32_t remaining = total_pixels;
	uint32_t offset = 0;
	const uint32_t MAX_CHUNK = 4096;  // 每次最多传输4096个像素
	
	LCD_DC_Set();  // 设置为数据模式
	
	while(remaining > 0) {
		uint32_t chunk_size = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;
		
		t.length = chunk_size * 16;  // 位数 (16位/像素)
		t.tx_buffer = rgb565_data + offset;
		
		ret = spi_device_polling_transmit(spi, &t);
		if(ret != ESP_OK) {
			printf("LCD_ShowRGB565: SPI传输失败, ret=%d\n", ret);
			break;
		}
		
		offset += chunk_size;
		remaining -= chunk_size;
	}
}

/******************************************************************************
      循环播放RGB565图片
******************************************************************************/
void RGB565_PlayLoop(const uint16_t *images[], const uint16_t widths[], 
                     const uint16_t heights[], const char *names[], 
                     uint8_t num_images, uint32_t delay_ms)
{
	printf("\n=== 开始循环播放RGB565图片 ===\n");
	printf("共 %d 张图片\n", num_images);
	
	uint8_t current_index = 0;
	
	// 清屏
	LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
	
	while(1) {
		printf("显示: %s (%dx%d)\n", 
		       names[current_index], 
		       widths[current_index], 
		       heights[current_index]);
		
		// 显示图片（居中）
		int16_t x_offset = ((int16_t)LCD_W - (int16_t)widths[current_index]) / 2;
		if(x_offset < 0) x_offset = 0;
		uint16_t y_offset = (LCD_H - heights[current_index]) / 2;
		
		LCD_ShowRGB565((uint16_t)x_offset, y_offset, 
		              images[current_index], 
		              widths[current_index], 
		              heights[current_index]);
		
		vTaskDelay(pdMS_TO_TICKS(delay_ms));
		
		current_index = (current_index + 1) % num_images;
	}
}

/******************************************************************************
      循环播放RGB565图片 - 带自定义延迟
******************************************************************************/
void RGB565_PlayLoop_WithDelays(const uint16_t *images[], const uint16_t widths[], 
                                const uint16_t heights[], const char *names[], 
                                const uint32_t delays[], uint8_t num_images)
{
	printf("\n=== 开始循环播放RGB565图片（自定义延迟） ===\n");
	printf("共 %d 张图片\n", num_images);
	
	uint8_t current_index = 0;
	
	// 清屏
	LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
	
	while(1) {
		printf("显示: %s (%dx%d), 延迟: %lums\n", 
		       names[current_index], 
		       widths[current_index], 
		       heights[current_index],
		       delays[current_index]);
		
		// 显示图片（居中）
		int16_t x_offset = ((int16_t)LCD_W - (int16_t)widths[current_index]) / 2;
		if(x_offset < 0) x_offset = 0;
		uint16_t y_offset = (LCD_H - heights[current_index]) / 2;
		
		LCD_ShowRGB565((uint16_t)x_offset, y_offset, 
		              images[current_index], 
		              widths[current_index], 
		              heights[current_index]);
		
		vTaskDelay(pdMS_TO_TICKS(delays[current_index]));
		
		current_index = (current_index + 1) % num_images;
	}
}

/******************************************************************************
      触摸交互式RGB565图片切换
      待机：idle和closeeye随机切换
      触摸：显示haqi和angry来回切换
******************************************************************************/
void RGB565_TouchInteractive(const uint16_t *idle_img, uint16_t idle_w, uint16_t idle_h,
                             const uint16_t *closeeye_img, uint16_t closeeye_w, uint16_t closeeye_h,
                             const uint16_t *haqi_img, uint16_t haqi_w, uint16_t haqi_h,
                             const uint16_t *angry_img, uint16_t angry_w, uint16_t angry_h)
{
	printf("\n=== 触摸交互式图片显示 ===\n");
	printf("待机：idle <-> closeeye 随机切换\n");
	printf("触摸：haqi <-> angry 来回切换\n\n");
	
	typedef enum {
		STATE_IDLE,      // 待机状态
		STATE_TOUCHED    // 触摸状态
	} DisplayState;
	
	DisplayState state = STATE_IDLE;
	uint32_t last_display_time = 0;
	uint32_t last_touch_time = 0;
	bool show_idle = true;  // true=idle, false=closeeye
	uint8_t touch_toggle_count = 0;  // 触摸时的切换计数
	bool show_haqi = true;  // true=haqi, false=angry
	
	// 清屏
	LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
	
	// 显示初始图片
	int16_t x_offset = ((int16_t)LCD_W - (int16_t)idle_w) / 2;
	if(x_offset < 0) x_offset = 0;
	uint16_t y_offset = (LCD_H - idle_h) / 2;
	LCD_ShowRGB565((uint16_t)x_offset, y_offset, idle_img, idle_w, idle_h);
	last_display_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	
	while(1) {
		uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		
		// 读取触摸状态
		CST816_Get_XY_AXIS();
		uint8_t finger_num = CST816_Get_FingerNum();
		bool has_touch = (finger_num > 0);
		
		if(has_touch) {
			last_touch_time = current_time;
		}
		
		// 状态切换逻辑
		DisplayState new_state = state;
		
		if(state == STATE_IDLE && has_touch) {
			// 从待机进入触摸状态
			new_state = STATE_TOUCHED;
			touch_toggle_count = 0;
			show_haqi = true;
			printf("[状态] IDLE -> TOUCHED\n");
		}
		else if(state == STATE_TOUCHED && !has_touch && (current_time - last_touch_time > 2000)) {
			// 触摸结束2秒后返回待机
			new_state = STATE_IDLE;
			show_idle = true;
			printf("[状态] TOUCHED -> IDLE\n");
		}
		
		// 状态改变时立即更新显示
		if(new_state != state) {
			state = new_state;
			last_display_time = current_time;
			
			if(state == STATE_IDLE) {
				// 显示idle
				x_offset = ((int16_t)LCD_W - (int16_t)idle_w) / 2;
				if(x_offset < 0) x_offset = 0;
				y_offset = (LCD_H - idle_h) / 2;
				LCD_ShowRGB565((uint16_t)x_offset, y_offset, idle_img, idle_w, idle_h);
				printf("显示: idle\n");
			}
			else {
				// 显示haqi
				x_offset = ((int16_t)LCD_W - (int16_t)haqi_w) / 2;
				if(x_offset < 0) x_offset = 0;
				y_offset = (LCD_H - haqi_h) / 2;
				LCD_ShowRGB565((uint16_t)x_offset, y_offset, haqi_img, haqi_w, haqi_h);
				printf("显示: haqi\n");
			}
		}
		
		// 根据状态更新显示
		if(state == STATE_IDLE) {
			// 待机状态：随机切换idle和closeeye
			uint32_t idle_interval;
			if(show_idle) {
				idle_interval = 2000 + (esp_random() % 3000);  // idle显示2-5秒随机
			} else {
				idle_interval = 100 + (esp_random() % 50);   // closeeye快速眨眼100-150ms
			}
			
			if(current_time - last_display_time > idle_interval) {
				if(show_idle) {
					// 切换到closeeye（眨眼）
					x_offset = ((int16_t)LCD_W - (int16_t)closeeye_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - closeeye_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, closeeye_img, closeeye_w, closeeye_h);
					printf("显示: closeeye (眨眼)\n");
					show_idle = false;
					last_display_time = current_time;
				}
				else {
					// 切换回idle
					x_offset = ((int16_t)LCD_W - (int16_t)idle_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - idle_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, idle_img, idle_w, idle_h);
					printf("显示: idle\n");
					show_idle = true;
					last_display_time = current_time;
				}
			}
		}
		else if(state == STATE_TOUCHED) {
			// 触摸状态：haqi和angry来回切换
			uint32_t touch_interval = 1200;  // 1.2秒切换
			
			if(current_time - last_display_time > touch_interval && touch_toggle_count < 6) {
				if(show_haqi) {
					// 切换到angry
					x_offset = ((int16_t)LCD_W - (int16_t)angry_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - angry_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, angry_img, angry_w, angry_h);
					printf("显示: angry\n");
					show_haqi = false;
				}
				else {
					// 切换到haqi
					x_offset = ((int16_t)LCD_W - (int16_t)haqi_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - haqi_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, haqi_img, haqi_w, haqi_h);
					printf("显示: haqi\n");
					show_haqi = true;
				}
				
				last_display_time = current_time;
				touch_toggle_count++;
			}
			
			// 如果用户持续触摸，重置计数继续切换
			if(has_touch) {
				if(touch_toggle_count >= 6) {
					touch_toggle_count = 0;
				}
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(50));  // 50ms检查一次
	}
}

/******************************************************************************
      双模式交互式RGB565图片切换（机器人模式 + YNF模式）
      
      机器人模式：
        - 待机：idle/smile随机切换，时不时眨眼(closeeye)，5-10秒随机切换
        - 触摸：angry和haqi来回切换
        
      YNF模式：
        - 待机：ynfidle显示
        - 触摸：ynfangry和ynfhaqi来回切换
        
      滑动手势：在两种模式间切换
******************************************************************************/
void RGB565_DualModeInteractive(
    // 机器人模式图片
    const uint16_t *robot_idle, uint16_t robot_idle_w, uint16_t robot_idle_h,
    const uint16_t *robot_closeeye, uint16_t robot_closeeye_w, uint16_t robot_closeeye_h,
    const uint16_t *robot_smile, uint16_t robot_smile_w, uint16_t robot_smile_h,
    const uint16_t *robot_haqi, uint16_t robot_haqi_w, uint16_t robot_haqi_h,
    const uint16_t *robot_angry, uint16_t robot_angry_w, uint16_t robot_angry_h,
    // YNF模式图片
    const uint16_t *ynf_idle, uint16_t ynf_idle_w, uint16_t ynf_idle_h,
    const uint16_t *ynf_haqi, uint16_t ynf_haqi_w, uint16_t ynf_haqi_h,
    const uint16_t *ynf_angry, uint16_t ynf_angry_w, uint16_t ynf_angry_h)
{
	printf("\n=== 双模式交互式图片显示 ===\n");
	printf("机器人模式：idle/smile切换 + 眨眼，触摸显示angry/haqi\n");
	printf("YNF模式：ynfidle，触摸显示ynfangry/ynfhaqi\n");
	printf("滑动手势：切换模式\n\n");
	
	typedef enum {
		MODE_ROBOT,   // 机器人模式
		MODE_YNF      // YNF模式
	} DisplayMode;
	
	typedef enum {
		STATE_IDLE,      // 待机状态
		STATE_TOUCHED    // 触摸状态
	} DisplayState;
	
	DisplayMode current_mode = MODE_ROBOT;
	DisplayState state = STATE_IDLE;
	
	uint32_t last_display_time = 0;
	uint32_t last_touch_time = 0;
	uint32_t last_gesture_time = 0;
	
	// 机器人模式状态
	uint8_t robot_idle_type = 0;  // 0=idle, 1=smile
	bool show_closeeye = false;
	uint8_t robot_touch_toggle = 0;  // 触摸切换计数
	bool show_robot_haqi = true;
	
	// YNF模式状态
	uint8_t ynf_touch_toggle = 0;
	bool show_ynf_haqi = true;
	
	// 手势检测 (留用于未来扩展)
	
	// 清屏
	LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
	
	// 显示初始图片（机器人idle）
	int16_t x_offset = ((int16_t)LCD_W - (int16_t)robot_idle_w) / 2;
	if(x_offset < 0) x_offset = 0;
	uint16_t y_offset = (LCD_H - robot_idle_h) / 2;
	LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_idle, robot_idle_w, robot_idle_h);
	last_display_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	printf("初始显示: Robot Idle\n");
	
	while(1) {
		uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		
		// 读取触摸状态
		CST816_Get_XY_AXIS();
		uint8_t finger_num = CST816_Get_FingerNum();
		uint8_t gesture = CST816_IIC_ReadREG(GestureID);
		
		// 检查I2C通信是否正常（0xFF表示读取失败）
		static uint8_t i2c_error_count = 0;
		if(finger_num == 0xFF && gesture == 0xFF) {
			i2c_error_count++;
			if(i2c_error_count == 10) {  // 只报告一次
				printf("[错误] 触摸屏I2C通信失败！请检查:\n");
				printf("  1. I2C接线 (SDA=GPIO%d, SCL=GPIO%d)\n", I2C0_SDA_PIN, I2C0_SCL_PIN);
				printf("  2. 触摸屏电源\n");
				printf("  3. CST816芯片是否正常\n");
			}
			finger_num = 0;  // 将无效数据清零
			gesture = NOGESTURE;
		} else {
			i2c_error_count = 0;
		}
		
		bool has_touch = (finger_num > 0 && finger_num < 10);  // 有效手指数：1-9
		
		// 调试：输出触摸和手势信息（仅在有有效触摸时）
		static uint32_t last_debug_time = 0;
		if(has_touch && (current_time - last_debug_time > 500)) {
			printf("[触摸] 手指数: %d, 手势: 0x%02X, 模式: %s\n", 
			       finger_num, gesture, (current_mode == MODE_ROBOT) ? "Robot" : "YNF");
			last_debug_time = current_time;
		}
		
		// 检测滑动手势（切换模式）
		if(gesture == UPGLIDE || gesture == DOWNGLIDE || 
		   gesture == LEFTGLIDE || gesture == RIGHTGLIDE) {
			printf("[手势检测] 检测到滑动手势: 0x%02X\n", gesture);
			if(current_time - last_gesture_time > 1000) {  // 防抖1秒
				// 切换模式
				current_mode = (current_mode == MODE_ROBOT) ? MODE_YNF : MODE_ROBOT;
				state = STATE_IDLE;  // 重置为待机状态
				
				// 清屏
				LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
				
				if(current_mode == MODE_ROBOT) {
					printf("[模式切换] YNF -> Robot\n");
					robot_idle_type = 0;
					show_closeeye = false;
					x_offset = ((int16_t)LCD_W - (int16_t)robot_idle_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - robot_idle_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_idle, robot_idle_w, robot_idle_h);
				} else {
					printf("[模式切换] Robot -> YNF\n");
					x_offset = ((int16_t)LCD_W - (int16_t)ynf_idle_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - ynf_idle_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, ynf_idle, ynf_idle_w, ynf_idle_h);
				}
				
				last_display_time = current_time;
				last_gesture_time = current_time;
				last_touch_time = current_time;
			}
		}
		
		if(has_touch) {
			last_touch_time = current_time;
		}
		
		// 状态切换逻辑
		DisplayState new_state = state;
		
		if(state == STATE_IDLE && has_touch && gesture == NOGESTURE) {
			// 从待机进入触摸状态（非滑动手势）
			new_state = STATE_TOUCHED;
			if(current_mode == MODE_ROBOT) {
				robot_touch_toggle = 0;
				show_robot_haqi = true;
			} else {
				ynf_touch_toggle = 0;
				show_ynf_haqi = true;
			}
			printf("[状态] IDLE -> TOUCHED\n");
		}
		else if(state == STATE_TOUCHED && !has_touch && (current_time - last_touch_time > 2000)) {
			// 触摸结束2秒后返回待机
			new_state = STATE_IDLE;
			printf("[状态] TOUCHED -> IDLE\n");
		}
		
		// 状态改变时立即更新显示
		if(new_state != state) {
			state = new_state;
			last_display_time = current_time;
			
			if(state == STATE_IDLE) {
				if(current_mode == MODE_ROBOT) {
					// 显示robot idle
					robot_idle_type = 0;
					show_closeeye = false;
					x_offset = ((int16_t)LCD_W - (int16_t)robot_idle_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - robot_idle_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_idle, robot_idle_w, robot_idle_h);
					printf("显示: Robot Idle\n");
				} else {
					// 显示ynf idle
					x_offset = ((int16_t)LCD_W - (int16_t)ynf_idle_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - ynf_idle_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, ynf_idle, ynf_idle_w, ynf_idle_h);
					printf("显示: YNF Idle\n");
				}
			}
			else {
				if(current_mode == MODE_ROBOT) {
					// 显示robot haqi
					x_offset = ((int16_t)LCD_W - (int16_t)robot_haqi_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - robot_haqi_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_haqi, robot_haqi_w, robot_haqi_h);
					printf("显示: Robot Haqi\n");
				} else {
					// 显示ynf haqi
					x_offset = ((int16_t)LCD_W - (int16_t)ynf_haqi_w) / 2;
					if(x_offset < 0) x_offset = 0;
					y_offset = (LCD_H - ynf_haqi_h) / 2;
					LCD_ShowRGB565((uint16_t)x_offset, y_offset, ynf_haqi, ynf_haqi_w, ynf_haqi_h);
					printf("显示: YNF Haqi\n");
				}
			}
		}
		
		// ========== 机器人模式逻辑 ==========
		if(current_mode == MODE_ROBOT) {
			if(state == STATE_IDLE) {
				// 待机状态：idle/smile随机切换，时不时眨眼
				uint32_t idle_interval;
				
				if(show_closeeye) {
					// 眨眼快速结束 100-150ms
					idle_interval = 100 + (esp_random() % 50);
				} else {
					// idle或smile显示 5-10秒
					idle_interval = 5000 + (esp_random() % 5000);
				}
				
				if(current_time - last_display_time > idle_interval) {
					if(show_closeeye) {
						// 眨眼结束，切回idle或smile
						show_closeeye = false;
						
						if(robot_idle_type == 0) {
							x_offset = ((int16_t)LCD_W - (int16_t)robot_idle_w) / 2;
							if(x_offset < 0) x_offset = 0;
							y_offset = (LCD_H - robot_idle_h) / 2;
							LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_idle, robot_idle_w, robot_idle_h);
							printf("显示: Robot Idle\n");
						} else {
							x_offset = ((int16_t)LCD_W - (int16_t)robot_smile_w) / 2;
							if(x_offset < 0) x_offset = 0;
							y_offset = (LCD_H - robot_smile_h) / 2;
							LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_smile, robot_smile_w, robot_smile_h);
							printf("显示: Robot Smile\n");
						}
						last_display_time = current_time;
					} else {
						// 决定是眨眼还是切换表情
						uint32_t action = esp_random() % 10;
						
						if(action < 3) {  // 30%概率眨眼
							show_closeeye = true;
							x_offset = ((int16_t)LCD_W - (int16_t)robot_closeeye_w) / 2;
							if(x_offset < 0) x_offset = 0;
							y_offset = (LCD_H - robot_closeeye_h) / 2;
							LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_closeeye, robot_closeeye_w, robot_closeeye_h);
							printf("显示: Robot Closeeye (眨眼)\n");
						} else {
							// 70%概率切换idle/smile
							robot_idle_type = (robot_idle_type == 0) ? 1 : 0;
							
							if(robot_idle_type == 0) {
								x_offset = ((int16_t)LCD_W - (int16_t)robot_idle_w) / 2;
								if(x_offset < 0) x_offset = 0;
								y_offset = (LCD_H - robot_idle_h) / 2;
								LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_idle, robot_idle_w, robot_idle_h);
								printf("显示: Robot Idle\n");
							} else {
								x_offset = ((int16_t)LCD_W - (int16_t)robot_smile_w) / 2;
								if(x_offset < 0) x_offset = 0;
								y_offset = (LCD_H - robot_smile_h) / 2;
								LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_smile, robot_smile_w, robot_smile_h);
								printf("显示: Robot Smile\n");
							}
						}
						last_display_time = current_time;
					}
				}
			}
			else if(state == STATE_TOUCHED) {
				// 触摸状态：haqi和angry来回切换
				uint32_t touch_interval = 1200;  // 1.2秒切换
				
				if(current_time - last_display_time > touch_interval && robot_touch_toggle < 6) {
					if(show_robot_haqi) {
						// 切换到angry
						x_offset = ((int16_t)LCD_W - (int16_t)robot_angry_w) / 2;
						if(x_offset < 0) x_offset = 0;
						y_offset = (LCD_H - robot_angry_h) / 2;
						LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_angry, robot_angry_w, robot_angry_h);
						printf("显示: Robot Angry\n");
						show_robot_haqi = false;
					} else {
						// 切换到haqi
						x_offset = ((int16_t)LCD_W - (int16_t)robot_haqi_w) / 2;
						if(x_offset < 0) x_offset = 0;
						y_offset = (LCD_H - robot_haqi_h) / 2;
						LCD_ShowRGB565((uint16_t)x_offset, y_offset, robot_haqi, robot_haqi_w, robot_haqi_h);
						printf("显示: Robot Haqi\n");
						show_robot_haqi = true;
					}
					
					last_display_time = current_time;
					robot_touch_toggle++;
				}
				
				// 持续触摸重置计数
				if(has_touch && robot_touch_toggle >= 6) {
					robot_touch_toggle = 0;
				}
			}
		}
		// ========== YNF模式逻辑 ==========
		else {
			if(state == STATE_IDLE) {
				// YNF待机状态：只显示ynfidle，不需要切换
				// 保持当前显示即可
			}
			else if(state == STATE_TOUCHED) {
				// YNF触摸状态：ynfhaqi和ynfangry来回切换
				uint32_t touch_interval = 1200;  // 1.2秒切换
				
				if(current_time - last_display_time > touch_interval && ynf_touch_toggle < 6) {
					if(show_ynf_haqi) {
						// 切换到ynfangry
						x_offset = ((int16_t)LCD_W - (int16_t)ynf_angry_w) / 2;
						if(x_offset < 0) x_offset = 0;
						y_offset = (LCD_H - ynf_angry_h) / 2;
						LCD_ShowRGB565((uint16_t)x_offset, y_offset, ynf_angry, ynf_angry_w, ynf_angry_h);
						printf("显示: YNF Angry\n");
						show_ynf_haqi = false;
					} else {
						// 切换到ynfhaqi
						x_offset = ((int16_t)LCD_W - (int16_t)ynf_haqi_w) / 2;
						if(x_offset < 0) x_offset = 0;
						y_offset = (LCD_H - ynf_haqi_h) / 2;
						LCD_ShowRGB565((uint16_t)x_offset, y_offset, ynf_haqi, ynf_haqi_w, ynf_haqi_h);
						printf("显示: YNF Haqi\n");
						show_ynf_haqi = true;
					}
					
					last_display_time = current_time;
					ynf_touch_toggle++;
				}
				
				// 持续触摸重置计数
				if(has_touch && ynf_touch_toggle >= 6) {
					ynf_touch_toggle = 0;
				}
			}
		}
		
		vTaskDelay(pdMS_TO_TICKS(20));  // 20ms检查一次，提高触摸响应速度
	}
}
