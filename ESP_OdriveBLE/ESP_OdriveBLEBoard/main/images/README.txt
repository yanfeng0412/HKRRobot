将你的JPEG图片文件放到这个文件夹中

例如：
- image1.jpg
- image2.jpg
- logo.jpg

然后在 main/CMakeLists.txt 中添加：
EMBED_FILES images/你的图片.jpg

编译时会自动嵌入到固件中。


命令示例
(base) C:\Users\feng\Desktop\P169H002>python tools\jpeg_to_rgb565.py main\images\ynfcloseeye.jpg main\Inc\ynfcloseeye_rgb565.h
图片尺寸: 466x386
警告：图片尺寸超过LCD分辨率(284x240)
建议缩放图片...
缩放后尺寸: 284x235
转换完成！输出文件: main\Inc\ynfcloseeye_rgb565.h
数组大小: 133480 bytes