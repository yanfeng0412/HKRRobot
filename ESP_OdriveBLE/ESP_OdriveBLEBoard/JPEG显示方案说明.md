# JPEG图片显示解决方案

由于TJpgDec库需要完整实现（代码量较大），这里提供两种替代方案：

## 方案1：转换为RGB565格式（推荐）

### 步骤：

1. **安装Python PIL库**
```bash
pip install pillow
```

2. **转换图片**
```bash
cd C:\Users\feng\Desktop\P169H002
python tools\jpeg_to_rgb565.py main\images\angry.jpg main\Inc\angry_rgb565.h
python tools\jpeg_to_rgb565.py main\images\haqi.jpg main\Inc\haqi_rgb565.h
python tools\jpeg_to_rgb565.py main\images\idle.jpg main\Inc\idle_rgb565.h
```

3. **创建显示函数** (在lcd.c中)
```c
void LCD_ShowRGB565(uint16_t x, uint16_t y, const uint16_t *rgb565_data, 
                    uint16_t width, uint16_t height)
{
    LCD_Address_Set(x, y, x + width - 1, y + height - 1);
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.tx_buffer = rgb565_data;
    t.length = width * height * 16;
    t.user = (void *)1;
    spi_device_transmit(spi, &t);
}
```

4. **在main.c中使用**
```c
#include "angry_rgb565.h"
#include "haqi_rgb565.h"
#include "idle_rgb565.h"

void play_rgb565_images(void) {
    while(1) {
        LCD_ShowRGB565(0, 0, idle_rgb565, IDLE_WIDTH, IDLE_HEIGHT);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        LCD_ShowRGB565(0, 0, haqi_rgb565, HAQI_WIDTH, HAQI_HEIGHT);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        LCD_ShowRGB565(0, 0, angry_rgb565, ANGRY_WIDTH, ANGRY_HEIGHT);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

## 方案2：使用ESP-IDF的esp_jpeg组件（需要ESP-IDF 5.0+）

如果你的ESP-IDF版本支持，可以在menuconfig中启用：
```bash
idf.py menuconfig
# Component config -> ESP JPEG Decoder
```

## 方案3：继续使用表情系统

当前代码已经回退到使用原来的触摸交互表情系统：
```c
Robot_Demo_Interactive();
```

这个系统包含14种表情和触摸手势控制，无需图片解码。

## 建议

**推荐使用方案1（RGB565）**，因为：
- ✅ 不需要解码，直接显示
- ✅ 速度最快
- ✅ 代码简单
- ✅ 无需额外库

缺点是文件会比JPEG大（但Flash空间通常足够）。
