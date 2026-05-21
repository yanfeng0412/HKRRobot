#include "lcd_demo.h"
#include "lcd.h"
#include "cst816.h"
#include "stdlib.h"
#include "pic.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

typedef enum
{
    STATE_LOGO,
    STATE_TEXT,
    STATE_IMAGE,
    STATE_COLOR_FULL,
    STATE_COLOR_BAR,
    STATE_GRAYSCALE,
    STATE_COUNTDOWN,
    STATE_HANDWRITING
} AppState;

AppState g_state = STATE_LOGO;
uint32_t g_state_timer = 0;
uint8_t g_img_index = 0;
uint8_t color_full_index = 0;
uint8_t g_countdown = 3;
extern const uint8_t gImage_logo[];

uint8_t IsTouchInButton(uint16_t x, uint16_t y);

void LCD_DEMO(void)
{
    printf("\n=== LCD DEMO Starting ===\n");
    printf("Screen size: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    printf("Pin Configuration:\n");
    printf("  SCK:  GPIO%d\n", LCD_SCK_PIN);
    printf("  MOSI: GPIO%d\n", LCD_MOSI_PIN);
    printf("  CS:   GPIO%d\n", LCD_CS_PIN);
    printf("  DC:   GPIO%d\n", LCD_DC_PIN);
    printf("  RES:  GPIO%d\n", LCD_RES_PIN);
    printf("  BLK:  GPIO%d\n\n", LCD_BLK_PIN);
    
    LCD_Init();
    printf("LCD_DEMO: LCD Init completed\n");
    
    CST816_Init();
    printf("LCD_DEMO: Touch Init completed\n");
    
    // 填充黑色并打开背光
    printf("LCD_DEMO: Filling screen with BLACK...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLACK);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    printf("LCD_DEMO: Turning ON backlight...\n");
    LCD_BLK_Set();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 测试填充白色
    printf("LCD_DEMO: Testing WHITE screen...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, WHITE);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试填充红色
    printf("LCD_DEMO: Testing RED screen...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, RED);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试填充绿色
    printf("LCD_DEMO: Testing GREEN screen...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, GREEN);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试填充蓝色
    printf("LCD_DEMO: Testing BLUE screen...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLUE);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试彩色条纹
    printf("LCD_DEMO: Testing color bars...\n");
    LCD_Fill(0, 0, SCREEN_WIDTH/4 - 1, SCREEN_HEIGHT - 1, RED);
    LCD_Fill(SCREEN_WIDTH/4, 0, SCREEN_WIDTH/2 - 1, SCREEN_HEIGHT - 1, GREEN);
    LCD_Fill(SCREEN_WIDTH/2, 0, SCREEN_WIDTH*3/4 - 1, SCREEN_HEIGHT - 1, BLUE);
    LCD_Fill(SCREEN_WIDTH*3/4, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, WHITE);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    printf("LCD_DEMO: Basic tests complete, starting demo loop...\n");

    static uint16_t lastX, lastY;
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        CST816_Get_XY_AXIS(); // ���´�������
        switch (g_state)
        {
        case STATE_LOGO:
            LCD_ShowPicture(0, 29, 239, 219, gImage_logo);

            // if (get_tick() - g_state_timer > LOGO_DURATION)
            // {
            vTaskDelay(pdMS_TO_TICKS(LOGO_DURATION));
            LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLACK);
            g_state = STATE_TEXT;
            // g_state_timer = get_tick();
            // }
            break;

        case STATE_TEXT:
            LCD_ShowString(20, 50, (const uint8_t *)"STM32 Display", WHITE, BLACK, 24, 0);
            LCD_ShowString(30, 100, (const uint8_t *)"Multi-Size Text", BLUE, BLACK, 16, 0);
            LCD_ShowChinese(80, 150, (uint8_t *)"����Һ��", RED, BLACK, 32, 0);

            // if (get_tick() - g_state_timer > TEXT_DURATION)
            // {
            vTaskDelay(pdMS_TO_TICKS(TEXT_DURATION));
            LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLACK);
            g_state = STATE_COLOR_FULL;
            // g_state_timer = get_tick();
            // }
            break;

        case STATE_IMAGE:
            // switch (g_img_index)
            // {
            //     // case 0: LCD_ShowPicture(0, 0, 239, 279, gImage_img1); break;
            //     // case 1: LCD_ShowPicture(0, 0, 239, 279, gImage_img2); break;
            //     // case 2: LCD_ShowPicture(0, 0, 239, 171, gImage_img3); break;
            // }

            // if (get_tick() - g_state_timer > IMAGE_INTERVAL)
            // {
            //     if (++g_img_index > 2)
            //     {
            //         g_state = STATE_COLOR_BAR;
            //         g_img_index = 0;
            //     }
            //     LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLACK);
            //     g_state_timer = get_tick();
            // }
            break;
        case STATE_COLOR_FULL:
            switch (color_full_index)
            {
            case 0:
                LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RED);
                break;
            case 1:
                LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GREEN);
                break;
            case 2:
                LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLUE);
                break;
            case 3:
                LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
                break;
            case 4:
                LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
                break;
            }

            // if (get_tick() - g_state_timer > COLOR_FULL_INTERVAL)
            // {
            vTaskDelay(pdMS_TO_TICKS(COLOR_FULL_INTERVAL));
            if (++color_full_index > 4)
            {
                g_state = STATE_COLOR_BAR;
                color_full_index = 0;
            }
            // g_state_timer = get_tick();
            // }
            break;
        case STATE_COLOR_BAR:
            DrawColorBars();
            // if (get_tick() - g_state_timer > EFFECT_DURATION)
            // {
            vTaskDelay(pdMS_TO_TICKS(EFFECT_DURATION));
            LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
            g_state = STATE_GRAYSCALE;
            // g_state_timer = get_tick();
            // }
            break;

        case STATE_GRAYSCALE:
            DrawGrayscale();
            // if (get_tick() - g_state_timer > EFFECT_DURATION)
            // {
            vTaskDelay(pdMS_TO_TICKS(EFFECT_DURATION));
            LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
            g_state = STATE_HANDWRITING;
            // g_state_timer = get_tick();
            DrawClearButton();
            // }
            break;

        case STATE_COUNTDOWN:
            // LCD_ShowIntNum(100, 120, g_countdown, 1, RED, BLACK, 32);
            // if (get_tick() - g_state_timer > 1000)
            // {
            //     if (--g_countdown == 0)
            //     {
            //         LCD_Fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
            //         DrawClearButton();
            //         g_state = STATE_HANDWRITING;
            //     }
            //     g_state_timer = get_tick();
            // }
            break;

        case STATE_HANDWRITING:
            // ������ͼ
            if (CST816_Get_FingerNum() > 0)
            {
                if (lastX != 0xFFFF && lastY != 0xFFFF)
                {
                    // ʹ��Bresenham�㷨����
                    LCD_DrawThickLine(lastX, lastY, CST816_Instance.X_Pos, CST816_Instance.Y_Pos, WHITE, 2);
                }
                lastX = CST816_Instance.X_Pos;
                lastY = CST816_Instance.Y_Pos;
                if (IsTouchInButton(lastX, lastY))
                {
                    LCD_Fill(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, BLACK);
                    DrawClearButton();
                }
            }
            else
            {
                lastX = lastY = 0xFFFF; // ��ָ̧��ʱ����
            }

            // ����ʱ������CPUռ�ù���
            vTaskDelay(pdMS_TO_TICKS(2));
            break;
        }
    }
}

uint8_t IsTouchInButton(uint16_t x, uint16_t y)
{
    return (x >= SCREEN_WIDTH - BTN_WIDTH) &&
           (y >= SCREEN_HEIGHT - BTN_HEIGHT);
}
