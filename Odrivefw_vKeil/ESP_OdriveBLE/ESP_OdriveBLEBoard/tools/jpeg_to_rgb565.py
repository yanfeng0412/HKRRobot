#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
将JPEG图片转换为RGB565格式的C数组
用于ESP32直接嵌入显示，无需JPEG解码

使用方法：
python jpeg_to_rgb565.py input.jpg output.h

生成的.h文件可以直接include到项目中
"""

import sys
from PIL import Image
import struct

def rgb888_to_rgb565(r, g, b):
    """转换RGB888到RGB565格式 (小端序)"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    rgb565 = (r5 << 11) | (g6 << 5) | b5
    # 交换字节序: 大端 -> 小端
    return ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)

def jpeg_to_rgb565_array(input_file, output_file):
    """将JPEG转换为RGB565数组"""
    
    # 打开图片
    img = Image.open(input_file)
    
    # 转换为RGB模式
    img = img.convert('RGB')
    
    width, height = img.size
    print(f"图片尺寸: {width}x{height}")
    
    # 如果图片太大，询问是否缩放
    if width > 284 or height > 240:
        print(f"警告：图片尺寸超过LCD分辨率(284x240)")
        print(f"建议缩放图片...")
        # 保持宽高比缩放
        img.thumbnail((284, 240), Image.Resampling.LANCZOS)
        width, height = img.size
        print(f"缩放后尺寸: {width}x{height}")
    
    # 获取像素数据
    pixels = img.load()
    
    # 生成变量名（从文件名）
    import os
    var_name = os.path.splitext(os.path.basename(input_file))[0]
    var_name = var_name.replace('-', '_').replace(' ', '_')
    
    # 写入头文件
    with open(output_file, 'w') as f:
        f.write(f"// Auto-generated from {input_file}\n")
        f.write(f"// Image size: {width}x{height}\n\n")
        f.write(f"#include <stdint.h>\n\n")
        
        # 写入宽高定义
        f.write(f"#define {var_name.upper()}_WIDTH {width}\n")
        f.write(f"#define {var_name.upper()}_HEIGHT {height}\n\n")
        
        # 写入RGB565数组
        f.write(f"const uint16_t {var_name}_rgb565[{width * height}] = {{\n")
        
        count = 0
        for y in range(height):
            f.write("    ")
            for x in range(width):
                r, g, b = pixels[x, y]
                rgb565 = rgb888_to_rgb565(r, g, b)
                f.write(f"0x{rgb565:04X}")
                
                count += 1
                if count < width * height:
                    f.write(", ")
                
                if count % 12 == 0:  # 每行12个数据
                    f.write("\n    ")
            
            if y < height - 1:
                f.write("\n")
        
        f.write("\n};\n")
    
    print(f"转换完成！输出文件: {output_file}")
    print(f"数组大小: {width * height * 2} bytes")
    print(f"\n使用方法：")
    print(f'  #include "{output_file}"')
    print(f'  LCD_ShowRGB565(0, 0, {var_name}_rgb565, {var_name.upper()}_WIDTH, {var_name.upper()}_HEIGHT);')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("用法: python jpeg_to_rgb565.py input.jpg output.h")
        print("示例: python jpeg_to_rgb565.py angry.jpg angry_rgb565.h")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        jpeg_to_rgb565_array(input_file, output_file)
    except Exception as e:
        print(f"错误: {e}")
        sys.exit(1)
