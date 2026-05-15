# ESP32 JPEG图片显示使用说明

## 1. 已添加的文件
- `main/Src/jpeg_decoder.c` - JPEG解码实现
- `main/Inc/jpeg_decoder.h` - 头文件

## 2. 使用方法

### 方法1：从内存显示JPEG（嵌入式数组）

```c
#include "jpeg_decoder.h"

// 1. 将JPEG文件转换为C数组（使用xxd或在线工具）
const uint8_t my_image_jpg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, // JPEG文件数据...
    // ... 你的JPEG数据
};

// 2. 在代码中显示
void app_main(void) {
    LCD_Init();
    
    // 在(0, 0)位置显示JPEG
    LCD_ShowJPEG(0, 0, my_image_jpg, sizeof(my_image_jpg));
}
```

### 方法2：从文件系统加载（需要SPIFFS或SD卡）

```c
#include "jpeg_decoder.h"
#include "esp_spiffs.h"

void app_main(void) {
    // 挂载SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_vfs_spiffs_register(&conf);
    
    LCD_Init();
    
    // 从SPIFFS加载并显示
    LCD_ShowJPEG_FromFile(0, 0, "/spiffs/image.jpg");
}
```

## 3. 准备JPEG图片

### 转换为C数组（Python脚本）
```python
# jpeg_to_array.py
with open('your_image.jpg', 'rb') as f:
    data = f.read()

print('const uint8_t image_jpg[] = {')
for i in range(0, len(data), 16):
    chunk = data[i:i+16]
    hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
    print(f'    {hex_str},')
print('};')
print(f'const uint32_t image_jpg_size = {len(data)};')
```

运行：
```bash
python jpeg_to_array.py > my_image.h
```

然后在代码中：
```c
#include "my_image.h"
LCD_ShowJPEG(0, 0, image_jpg, image_jpg_size);
```

## 4. 性能优化建议

- **图片尺寸**：建议不超过284x240（你的LCD尺寸）
- **压缩质量**：使用中等质量（60-80%），平衡大小和质量
- **内存管理**：大图片会占用大量RAM，建议分块显示

## 5. 示例代码

```c
// 在main.c中测试
#include "jpeg_decoder.h"

// 包含你的图片数组
#include "test_image.h"

void test_jpeg_display(void) {
    LCD_Init();
    LCD_Fill(0, 0, LCD_W-1, LCD_H-1, BLACK); // 清屏
    
    // 显示JPEG图片
    LCD_ShowJPEG(0, 0, test_image_jpg, test_image_jpg_size);
    
    printf("JPEG显示测试完成\n");
}
```

## 6. 故障排查

- **解码失败**：检查JPEG文件是否损坏，尝试用图片查看器打开
- **内存不足**：减小图片尺寸或使用PSRAM
- **显示不正常**：确认LCD初始化正确，检查SPI配置
- **颜色不对**：JPEG解码器输出RGB565格式，与LCD要求一致

## 7. 高级功能

如需缩放功能，可以使用`JPG_SCALE_2X`, `JPG_SCALE_4X`, `JPG_SCALE_8X`：

```c
// 在jpeg_decoder.c中修改解码参数
esp_jpg_decode(jpeg_size, JPG_SCALE_2X, jpeg_data, ...);  // 缩小2倍
```
