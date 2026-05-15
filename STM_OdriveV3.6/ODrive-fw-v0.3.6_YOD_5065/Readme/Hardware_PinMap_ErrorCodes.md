# ODrive v3.4 硬件引脚 & 外设配置参考

**MCU：** STM32F405RGTx  
**固件：** ODrive v0.3.6 (YOD 5065 定制)  
**系统时钟：** 168 MHz  

---

## 一、GPIO 引脚分配总表

### GPIOA

| 引脚 | 宏定义 | 外设功能 | 说明 |
|------|--------|---------|------|
| PA0 | `GPIO_1` | UART4_TX | 原始命令口 TX（`cmd_parse_thread`） |
| PA1 | `GPIO_2` | UART4_RX | 原始命令口 RX（DMA 循环 64B） |
| PA2 | `GPIO_3` | USART2_TX (AF7) | 用户控制终端 TX（心跳+回复），原为 EXTI2 |
| PA3 | `GPIO_4` | USART2_RX (AF7) | 用户控制终端 RX（中断+环形缓冲区） |
| PA4 | `M1_TEMP` | ADC1_IN4 | 电机1温度传感器 ADC |
| PA5 | `AUX_I` | ADC1_IN5 | 辅助电流检测 ADC |
| PA6 | `VBUS_S` | ADC1_IN6 | 母线电压采样（分压比 19:1，48V 档） |
| PA7 | `M1_AL` | TIM1_CH1N (AF1) | 电机1 A相下桥臂 PWM |
| PA8 | `M0_AH` | TIM1_CH1 (AF1) | 电机0 A相上桥臂 PWM |
| PA9 | `M0_BH` | TIM1_CH2 (AF1) | 电机0 B相上桥臂 PWM |
| PA10 | `M0_CH` | TIM1_CH3 (AF1) | 电机0 C相上桥臂 PWM |
| PA15 | `M0_ENC_Z` | TIM2_CH1/ETR 或 Hall-C | 电机0 编码器 Z 相 / **Hall-C 输入**（M0 ENC_Z 连接器引脚） |

### GPIOB

| 引脚 | 宏定义 | 外设功能 | 说明 |
|------|--------|---------|------|
| PB0 | `M1_BL` | TIM1_CH2N (AF1) | 电机1 B相下桥臂 PWM |
| PB1 | `M1_CL` | TIM1_CH3N (AF1) | 电机1 C相下桥臂 PWM |
| PB2 | `GPIO_5` | GPIO_IN | 通用 GPIO（预留） |
| PB3 | — | Hall-C (APP) | 电机1 Hall-C / TIM4_ETR（`APP_HALL_C_PIN`） |
| PB4 | `M0_ENC_A` | TIM3_CH1 (AF2) | 电机0 编码器 A 相（或 Hall-A 复用） |
| PB5 | `M0_ENC_B` | TIM3_CH2 (AF2) | 电机0 编码器 B 相（或 Hall-B 复用） |
| PB6 | `M1_ENC_A` | TIM4_CH1 (AF2) | 电机1 编码器 A 相 / Hall-A（`APP_HALL_A_PIN`） |
| PB7 | `M1_ENC_B` | TIM4_CH2 (AF2) | 电机1 编码器 B 相 / Hall-B（`APP_HALL_B_PIN`） |
| PB8 | — | CAN1_RX (AF9) | CAN 总线接收 |
| PB9 | — | CAN1_TX (AF9) | CAN 总线发送 |
| PB10 | `AUX_L` | TIM2_CH3 (AF1) | 辅助桥臂下桥臂 PWM |
| PB11 | `AUX_H` | TIM2_CH4 (AF1) | 辅助桥臂上桥臂 PWM |
| PB12 | `EN_GATE` | GPIO_OUT | DRV8301 栅极驱动使能（高有效） |
| PB13 | `M0_AL` | TIM1_CH1N (AF1) | 电机0 A相下桥臂 PWM |
| PB14 | `M0_BL` | TIM1_CH2N (AF1) | 电机0 B相下桥臂 PWM |
| PB15 | `M0_CL` | TIM1_CH3N (AF1) | 电机0 C相下桥臂 PWM |

### GPIOC

| 引脚 | 宏定义 | 外设功能 | 说明 |
|------|--------|---------|------|
| PC0 | `M0_IB` | ADC1_IN10 | 电机0 B相电流检测 ADC |
| PC1 | `M0_IC` | ADC1_IN11 | 电机0 C相电流检测 ADC |
| PC2 | `M1_IC` | ADC1_IN12 | 电机1 C相电流检测 ADC |
| PC3 | `M1_IB` | ADC1_IN13 | 电机1 B相电流检测 ADC |
| PC4 | `AUX_TEMP` | ADC1_IN14 | 辅助温度传感器 ADC |
| PC5 | `M0_TEMP` | ADC1_IN15 | 电机0 温度传感器 ADC |
| PC6 | `M1_AH` | TIM8_CH1 (AF3) | 电机1 A相上桥臂 PWM |
| PC7 | `M1_BH` | TIM8_CH2 (AF3) | 电机1 B相上桥臂 PWM |
| PC8 | `M1_CH` | TIM8_CH3 (AF3) | 电机1 C相上桥臂 PWM |
| PC9 | `M0_DC_CAL` | GPIO_OUT | 电机0 DRV8301 DC 偏置校准触发（**内部走线到 DRV8301，不接 ENC_Z 连接器**） |
| PC10 | — | SPI3_SCK (AF6) | DRV8301 SPI 时钟 |
| PC11 | — | SPI3_MISO (AF6) | DRV8301 SPI 数据输入 |
| PC12 | — | SPI3_MOSI (AF6) | DRV8301 SPI 数据输出 |
| PC13 | `M0_nCS` | GPIO_OUT | 电机0 DRV8301 片选（低有效） |
| PC14 | `M1_nCS` | GPIO_OUT | 电机1 DRV8301 片选（低有效） |
| PC15 | `M1_DC_CAL` | GPIO_OUT | 电机1 DC 偏置校准触发 |

### GPIOD

| 引脚 | 宏定义 | 外设功能 | 说明 |
|------|--------|---------|------|
| PD2 | `nFAULT` | GPIO_IN（EXTI） | DRV8301 故障信号（低=故障，触发 ERROR_DRV_FAULT） |

---

## 二、外设配置详情

### 2.1 UART4 — 原始命令口

| 参数 | 值 |
|------|---|
| 引脚 | PA0 (TX)、PA1 (RX) |
| 对外标识 | GPIO_1 / GPIO_2 |
| 波特率 | 115200 |
| 格式 | 8N1 |
| 接收方式 | DMA 循环缓冲区，64 字节 |
| 任务 | `cmd_parse_thread`（FreeRTOS） |
| 用途 | ODrive 原始文本协议，可接 USB-TTL |

### 2.2 USART2 — 用户控制终端（本项目新增）

| 参数 | 值 |
|------|---|
| 引脚 | PA2 (TX, AF7)、PA3 (RX, AF7) |
| 对外标识 | GPIO_3 / GPIO_4 |
| 波特率 | 115200 |
| 格式 | 8N1 |
| TX 方式 | `HAL_UART_Transmit()` 阻塞（安全，因为不调 HAL RX） |
| RX 方式 | RXNE 中断 + 64 字节环形缓冲区（避免 HAL Lock Bug） |
| 中断优先级 | USART2_IRQn，抢占级 6，子级 0 |
| 任务 | `uart2_test_thread`（FreeRTOS，512 words） |
| 用途 | 心跳输出（1秒）+ `$M0,...;` 命令接收 + 命令回复 |

> **HAL Lock Bug 说明：**  
> 不可在 `huart2` 上调用 `HAL_UART_Receive()`（含超时版本）。  
> 该函数超时返回后 `RxState` 永久设为 `BUSY_RX`，导致后续 `HAL_UART_Transmit()` 返回 `HAL_BUSY`，TX 全部静默失败。

### 2.3 SPI3 — DRV8301 栅极驱动通信

| 参数 | 值 |
|------|---|
| 引脚 | PC10 (SCK)、PC11 (MISO)、PC12 (MOSI) |
| 片选 | PC13 (M0_nCS)、PC14 (M1_nCS)，软件控制，低有效 |
| 模式 | Master，全双工 |
| 数据位 | 16 bit |
| 极性/相位 | CPOL=Low，CPHA=2Edge（Mode 1） |
| 波特率 | APB1/16 = 84 MHz / 16 = **5.25 MHz** |
| 用途 | 读写 DRV8301 寄存器（增益设置、故障状态） |

### 2.4 CAN1 — CAN 总线

| 参数 | 值 |
|------|---|
| 引脚 | PB8 (RX)、PB9 (TX) |
| 预分频 | 16 |
| 时序段 | SJW=1TQ，BS1=1TQ，BS2=1TQ |
| 模式 | Normal |
| 备注 | 波特率未最终确认（BS1/BS2 配置偏小，实际使用前需重新计算） |

### 2.5 TIM1 — 电机0 三相 PWM（中心对齐，互补输出）

| 参数 | 值 |
|------|---|
| 时钟 | 168 MHz |
| 周期计数 | 10192 clocks（中心对齐，实际 PWM 频率 = 168M / (2×10192) ≈ **8.24 kHz**） |
| 死区 | 20 clocks ≈ 119 ns |
| 通道 | CH1/CH1N = A相，CH2/CH2N = B相，CH3/CH3N = C相 |
| 上桥臂 | PA8, PA9, PA10 |
| 下桥臂 | PB13, PB14, PB15 |
| 触发 ADC | TIM1 触发 ADC 电流采样（`current_meas_period = 2×10192 / 168M ≈ 121 µs`） |

### 2.6 TIM8 — 电机1 三相 PWM（中心对齐，互补输出）

| 参数 | 值 |
|------|---|
| 时钟 | 168 MHz |
| 周期计数 | 10192 clocks（同 TIM1） |
| 死区 | 20 clocks |
| 上桥臂 | PC6, PC7, PC8 |
| 下桥臂 | PB0 (M1_BL)、PB1 (M1_CL)、PA7 (M1_AL) |

### 2.7 TIM3 — 电机0 编码器接口

| 参数 | 值 |
|------|---|
| 引脚 | PB4 (ENC_A / CH1)、PB5 (ENC_B / CH2) |
| 模式 | 编码器模式（正交解码） |
| Z相 | PA15 (ENC_Z / Hall-C) |
| 备注 | 若使用 Hall 传感器（`APP_USE_HALL_SENSOR=1`），这些引脚被重配为 GPIO_IN，TIM3 编码器功能停用 |

### 2.8 TIM4 — 电机1 编码器 / Hall 接口

| 参数 | 值 |
|------|---|
| 引脚 | PB6 (ENC_A / CH1 / Hall-A)、PB7 (ENC_B / CH2 / Hall-B) |
| Z/ETR | PB3 (Hall-C / TIM4_ETR) |
| 备注 | Hall 模式下三路信号为 PB6, PB7, PB3 |

### 2.9 TIM14 — 系统时基

| 参数 | 值 |
|------|---|
| 宏 | `TIM_TIME_BASE` |
| 时钟 | APB1 = 84 MHz |
| 周期 | 4096 clocks |
| 用途 | 电流采样时序基准，`current_meas_period` 参考 |

### 2.10 TIM2 — 辅助桥臂 PWM / 电机0 编码器（复用）

| 参数 | 值 |
|------|---|
| 引脚（PWM） | PB10 (AUX_L / CH3)、PB11 (AUX_H / CH4) |
| 引脚（编码器） | PA15 (CH1/ETR) |
| 用途 | 制动电阻 PWM 输出 |

### 2.11 ADC — 电流 & 电压采样

| 通道 | 引脚 | 用途 |
|------|------|------|
| ADC1_IN10 | PC0 (`M0_IB`) | 电机0 B相电流 |
| ADC1_IN11 | PC1 (`M0_IC`) | 电机0 C相电流 |
| ADC1_IN12 | PC2 (`M1_IC`) | 电机1 C相电流 |
| ADC1_IN13 | PC3 (`M1_IB`) | 电机1 B相电流 |
| ADC1_IN6 | PA6 (`VBUS_S`) | 母线电压（19:1 分压，`HW_VERSION_VOLTAGE=48`） |
| ADC1_IN4 | PA4 (`M1_TEMP`) | 电机1 温度 NTC |
| ADC1_IN5 | PA5 (`AUX_I`) | 辅助电流 |
| ADC1_IN14 | PC4 (`AUX_TEMP`) | 辅助温度 |
| ADC1_IN15 | PC5 (`M0_TEMP`) | 电机0 温度 NTC |

> **vbus 计算：** `vbus_voltage = ADC_raw × (3.3V / 4096) × VBUS_S_DIVIDER_RATIO`  
> `HW_VERSION_VOLTAGE=48` → `VBUS_S_DIVIDER_RATIO=19.0f`（原值 24 会导致读数偏低 42%）

---

## 三、Error Code 错误码表

回复格式中最后一个字段为错误码整数值，例如 `[M0,Q,0.000,45.23,0.100,23.6,0]` 中 `err=0`。

| 值 | 名称 | 含义 | 常见原因 | 处理方法 |
|----|------|------|---------|---------|
| 0 | `ERROR_NO_ERROR` | 无错误，正常运行 | — | — |
| 1 | `ERROR_PHASE_RESISTANCE_TIMING` | 电阻校准定时器超时 | TIM1 未正确初始化 | 检查 TIM1/ADC 配置 |
| 2 | `ERROR_PHASE_RESISTANCE_MEASUREMENT_TIMEOUT` | 电阻校准等待超时 | ADC 信号未就绪 | 检查 ADC 采样触发 |
| 3 | `ERROR_PHASE_RESISTANCE_OUT_OF_RANGE` | 测量的相电阻超出合理范围 | 电机未连接；相线断路；电流过小未建立电压 | 检查电机接线；调整 `APP_CALIBRATION_CURRENT` |
| 4 | `ERROR_PHASE_INDUCTANCE_TIMING` | 电感校准定时器超时 | TIM1 问题 | 检查 TIM1 配置 |
| 5 | `ERROR_PHASE_INDUCTANCE_MEASUREMENT_TIMEOUT` | 电感校准等待超时 | ADC 采样未触发 | — |
| 6 | `ERROR_PHASE_INDUCTANCE_OUT_OF_RANGE` | 测量的相电感超出范围 | 电机参数不匹配；电流过大/过小 | 调整校准电流 |
| 7 | `ERROR_ENCODER_RESPONSE` | 编码器无响应 | 编码器未接；SPI 故障（磁编码器） | 检查编码器接线 |
| 8 | `ERROR_ENCODER_MEASUREMENT_TIMEOUT` | 编码器测量超时 | TIM3 未启动；编码器无信号 | — |
| 9 | `ERROR_ADC_FAILED` | ADC 采样失败 | ADC 未初始化；DMA 未启动 | 检查 ADC/DMA 初始化顺序 |
| 10 | `ERROR_CALIBRATION_TIMING` | 校准过程定时异常 | FreeRTOS 任务调度冲突 | 检查任务优先级 |
| 11 | `ERROR_FOC_TIMING` | FOC 控制循环定时超时 | 中断优先级冲突；任务耗时过长 | 检查中断优先级 |
| 12 | `ERROR_FOC_MEASUREMENT_TIMEOUT` | FOC 等待电流采样超时 | ADC 触发丢失 | — |
| 13 | `ERROR_SCAN_MOTOR_TIMING` | 电机扫描定时错误 | 校准阶段 TIM 配置问题 | — |
| 14 | `ERROR_FOC_VOLTAGE_TIMING` | FOC 电压计算定时错误 | — | — |
| 15 | `ERROR_GATEDRIVER_INVALID_GAIN` | DRV8301 增益设置无效 | SPI 通信失败；DRV8301 未上电 | 检查 EN_GATE（PB12）；检查 SPI3 |
| 16 | `ERROR_PWM_SRC_FAIL` | PWM 信号源异常 | TIM1/TIM8 未正常同步 | — |
| 17 | `ERROR_UNEXPECTED_STEP_SRC` | Step/Dir 信号源异常 | 步进模式相关 | — |
| 18 | `ERROR_POS_CTRL_DURING_SENSORLESS` | 无传感器模式下使用了位置控制 | 在 ROTOR_MODE_SENSORLESS 时发送 P 命令 | 改用 V 命令或切换到编码器模式 |
| 19 | `ERROR_SPIN_UP_TIMEOUT` | 开环启动超时 | 转子脱步未跟上开环旋转场；启动电流不足；负载过重 | 降低 `APP_SPIN_UP_ACCELERATION`；增大 `APP_SPIN_UP_CURRENT` |
| 20 | `ERROR_DRV_FAULT` | DRV8301 栅极驱动器故障 | 过流保护；过温；电源异常；`nFAULT`（PD2）被拉低 | 检查电源；检查电机短路；复位 DRV8301 |
| 21 | `ERROR_NOT_IMPLEMENTED_MOTOR_TYPE` | 电机类型未实现 | 使用了 MOTOR_TYPE_LOW_CURRENT（未实现） | 改用 MOTOR_TYPE_HIGH_CURRENT |
| 22 | `ERROR_ENCODER_CPR_OUT_OF_RANGE` | 编码器 CPR 超出范围 | `encoder_cpr` 配置值异常 | 检查编码器 CPR 设置 |
| 23 | `ERROR_DC_BUS_BROWNOUT` | 母线电压跌落至保护阈值以下 | 电源限流（供电能力不足）；高速制动电流过大；电源线阻抗过大 | 降低启动/制动电流；增大电源限流；检查接线电阻；`APP_DC_BUS_BROWNOUT_V=8.0f` |

---

## 四、关键参数配置（MOTOR_CONFIG=2，5065 电机）

| 参数 | 值 | 说明 |
|------|---|------|
| `APP_POLE_PAIRS` | 7 | 7 对极（14 极） |
| `APP_PM_FLUX_LINKAGE` | 2.92e-3 V/(rad/s) | 永磁磁链 = 5.51/(7×270) |
| `APP_CALIBRATION_CURRENT` | 3.0 A | 校准电流（3A 够测量，不触发电源限流） |
| `APP_SPIN_UP_CURRENT` | 6.0 A | 开环启动拖动电流 |
| `APP_SPIN_UP_ACCELERATION` | 50.0 rad/s² | 开环加速度（慢→转子跟得上） |
| `APP_SPIN_UP_TARGET_VEL` | 250.0 rad/s | 切换闭环的电角速度目标 |
| `APP_CURRENT_LIM` | 20.0 A | 软件电流上限 |
| `APP_DC_BUS_BROWNOUT_V` | 8.0 V | 欠压保护阈值 |
| `APP_WHEEL_RADIUS_M` | 0.025 m | V 命令 km/h→rad/s 换算半径 |
| `APP_KT_NM_PER_A` | 0.03535 N·m/A | 扭矩常数（60/(2π×270)） |
| `HW_VERSION_VOLTAGE` | 48 | 分压比 19:1（不可设为 24） |
| `VBUS_S_DIVIDER_RATIO` | 19.0f | 由 `HW_VERSION_VOLTAGE=48` 自动选择 |
