# ODrive UART 通信协议规范

> **机器可读版本说明**
> 本文件供上位机 AI / 解析程序直接使用。
> 所有帧格式均以 BNF + 示例双重描述；字段顺序固定，以逗号分隔，无空格。
> 版本：v1.1  更新日期：2026-05-13

---

## 1. 物理层

| 参数 | 值 |
|------|----|
| 接口 | USART2（调试/主控串口） |
| 波特率 | 115200 baud |
| 帧格式 | 8N1，无硬件流控 |
| TX 引脚 | PA2（板上 GPIO_3） |
| RX 引脚 | PA3（板上 GPIO_4） |
| 共地 | 必须与上位机共地 |

> UART4（PA0/PA1）为备用调试口，协议相同。

---

## 2. 帧结构总览

```
方向           帧格式
PC → Board    $<CH>,<CMD>,<VAL>;\n
Board → PC    $ST,<fields...>;\r\n      （主动上报帧）
Board → PC    [<CH>,<CMD>,<fields...>]  （命令回复帧）
```

- 帧起始：`$`（0x24）
- 帧结束：`;`（0x3B）
- 换行：`\r\n`
- 字段分隔：`,`（0x2C）
- 无前导/尾随空格
- 所有浮点数保留 1~2 位小数

---

## 3. 下行命令帧（PC → Board）

### 3.1 BNF

```
CMD_FRAME  ::= "$" CHANNEL "," MODE "," VALUE ";" NEWLINE
CHANNEL    ::= "M0" | "M1"
MODE       ::= "V" | "P" | "T" | "I" | "H" | "Q" | "E"
VALUE      ::= FLOAT
FLOAT      ::= ["-"] DIGITS ["." DIGITS]
NEWLINE    ::= "\n"   (0x0A)
```

### 3.2 模式说明

| MODE | 功能 | VALUE 单位 | 备注 |
|------|------|-----------|------|
| `V`  | 速度控制 | km/h | `APP_WHEEL_RADIUS_M=0.025m`；设为 0 则直接 rad/s |
| `P`  | 位置控制 | 度 (°) | — |
| `T`  | 力矩控制 | N·m | Kt=0.03535 N·m/A（5065）；上限受 `APP_CURRENT_LIM` |
| `I`  | 电流控制 | A | 直接设定 Iq |
| `H`  | 急停 | — | VALUE 填 0，立即停止并锁定 |
| `Q`  | 查询 | — | VALUE 填 0，返回当前状态回复帧 |
| `E`  | 清错/复位 | — | VALUE 填 0，清除 error，重新校准 |

### 3.3 示例

```
$M0,V,5.0;       速度控制，5.0 km/h
$M0,V,-5.0;      反转
$M1,V,10.0;      M1 速度控制
$M0,I,3.0;       M0 电流 3A
$M0,T,0.5;       M0 力矩 0.5 N·m
$M0,P,90.0;      M0 位置 90°
$M0,H,0;         M0 急停
$M1,H,0;         M1 急停
$M0,Q,0;         查询 M0 状态
$M0,E,0;         清除 M0 错误并复位
```

---

## 4. 上行状态上报帧（Board → PC，主动）

### 4.1 触发规则

| 情况 | 触发条件 | 最高速率 |
|------|---------|---------|
| **Case 1（运动中）** | M0 或 M1 检测到 Hall 边沿变化 | 100 Hz（最短间隔 10 ms） |
| **Case 2（静止）** | 连续 500 ms 无 Hall 变化 | 1 Hz |

> Case 1 激活时 Case 2 自动屏蔽；静止 500 ms 后自动切换到 Case 2。

### 4.2 帧格式

```
$ST,<vb>,<f>,<M0h>,<M0c>,<M0v>,<M0i>,<M0e>,<M0s>,<M1h>,<M1c>,<M1v>,<M1i>,<M1e>,<M1s>;\r\n
```

### 4.3 字段定义（固定位置，索引从 0 起）

| 索引 | 字段 | 类型 | 单位 | 描述 |
|------|------|------|------|------|
| 0 | `vb` | float | V | VBUS 母线电压 |
| 1 | `f` | int | — | DRV8301 故障码（0=正常；共用 nFAULT，取 M0 值） |
| 2 | `M0h` | uint | — | M0 Hall 状态（1–6 有效；0 或 7 表示无效/错误） |
| 3 | `M0c` | int | — | M0 累计 Hall 计数（无溢出递增/递减） |
| 4 | `M0v` | float | rad/s | M0 机械角速度（正=正转；由 Hall PLL 估算） |
| 5 | `M0i` | float | A | M0 Iq 实测电流 |
| 6 | `M0e` | int | — | M0 错误码（见第 6 节） |
| 7 | `M0s` | uint | — | M0 状态位（bit0=校准完成 bit1=控制使能） |
| 8 | `M1h` | uint | — | M1 Hall 状态（同 M0h） |
| 9 | `M1c` | int | — | M1 累计 Hall 计数 |
| 10 | `M1v` | float | rad/s | M1 机械角速度 |
| 11 | `M1i` | float | A | M1 Iq 实测电流 |
| 12 | `M1e` | int | — | M1 错误码 |
| 13 | `M1s` | uint | — | M1 状态位 |

#### M0s / M1s 状态位解码

| 值 | bit1(enable) | bit0(calib) | 含义 |
|----|:---:|:---:|------|
| 0 | 0 | 0 | 未校准，未使能 |
| 1 | 0 | 1 | 已校准，未使能（Halt 后） |
| 2 | 1 | 0 | 使能中，校准未完成（校准过程中） |
| 3 | 1 | 1 | 正常运行 |

### 4.4 示例帧

```
$ST,23.6,0,3,12,45.2,1.23,0,3,5,8,22.1,0.45,0,1;\r\n
```

解读：VBUS=23.6V，无故障，M0 Hall=3 已转 12 格 速度=45.2rad/s 电流=1.23A 无错误 正常运行，M1 Hall=5 已转 8 格 速度=22.1rad/s 电流=0.45A 无错误 已校准未使能。

---

## 5. 上行命令回复帧（Board → PC，应答）

由 `$M0,Q,0;` 或其他命令触发，格式：

```
[<CH>,<MODE>,<vel_rad/s>,<pos_deg>,<Iq_A>,<Vbus_V>,<error>]
```

示例：
```
[M0,V,45.200,12.30,1.230,23.60,0]
  │   │  │      │    │     │    └─ 错误码
  │   │  │      │    │     └───── VBUS [V]
  │   │  │      │    └─────────── Iq [A]
  │   │  │      └───────────────── 位置 [°]
  │   │  └───────────────────────── 速度 [rad/s]
  │   └──────────────────────────── 当前控制模式
  └──────────────────────────────── 通道
```

---

## 6. 错误码对照表

| 值 | 宏名 | 含义 |
|----|------|------|
| 0 | `ERROR_NO_ERROR` | 无错误 |
| 1 | `ERROR_PHASE_RESISTANCE_TIMING` | 相电阻测量时序错误 |
| 2 | `ERROR_PHASE_RESISTANCE_MEASUREMENT_TIMEOUT` | 相电阻测量超时 |
| 3 | `ERROR_PHASE_RESISTANCE_OUT_OF_RANGE` | 相电阻超出范围 |
| 4 | `ERROR_PHASE_INDUCTANCE_TIMING` | 相电感测量时序错误 |
| 5 | `ERROR_PHASE_INDUCTANCE_MEASUREMENT_TIMEOUT` | 相电感测量超时 |
| 6 | `ERROR_PHASE_INDUCTANCE_OUT_OF_RANGE` | 相电感超出范围 |
| 7 | `ERROR_ENCODER_RESPONSE` | 编码器无响应 |
| 8 | `ERROR_ENCODER_MEASUREMENT_TIMEOUT` | 编码器测量超时 |
| 9 | `ERROR_ADC_FAILED` | ADC 采样失败 |
| 10 | `ERROR_CALIBRATION_TIMING` | 校准时序错误 |
| 11 | `ERROR_FOC_TIMING` | FOC 时序错误（ADC 信号超时） |
| 12 | `ERROR_FOC_MEASUREMENT_TIMEOUT` | FOC 测量超时 |
| 19 | `ERROR_SPIN_UP_TIMEOUT` | 无感起转超时 |
| 20 | `ERROR_DRV_FAULT` | DRV8301 驱动器故障 |

> 出现错误后发送 `$M0,E,0;` 清除并重新校准。

---

## 7. 上位机解析示例

### Python

```python
import serial, time

ser = serial.Serial('COM3', 115200, timeout=1)

def parse_st(line: str) -> dict | None:
    """解析主动上报帧 $ST,...;"""
    line = line.strip()
    if not (line.startswith('$ST,') and line.endswith(';')):
        return None
    f = line[4:-1].split(',')
    if len(f) != 14:
        return None
    return {
        'vbus':  float(f[0]),
        'fault': int(f[1]),
        'M0': {
            'hall':  int(f[2]),
            'count': int(f[3]),
            'vel':   float(f[4]),   # rad/s
            'iq':    float(f[5]),   # A
            'error': int(f[6]),
            'status':int(f[7]),
        },
        'M1': {
            'hall':  int(f[8]),
            'count': int(f[9]),
            'vel':   float(f[10]),
            'iq':    float(f[11]),
            'error': int(f[12]),
            'status':int(f[13]),
        },
    }

def send_cmd(ch: str, mode: str, val: float):
    ser.write(f'${ch},{mode},{val:.2f};\n'.encode())

# 发送速度命令
send_cmd('M0', 'V', 5.0)

# 读取上报
for raw in ser:
    data = parse_st(raw.decode(errors='ignore'))
    if data:
        print(f"M0 vel={data['M0']['vel']:.1f} rad/s  Iq={data['M0']['iq']:.2f} A")
```

---

## 8. Hall 传感器引脚（当前固件 APP_USE_HALL_SENSOR=1）

| 电机 | Hall-A | Hall-B | Hall-C |
|------|--------|--------|--------|
| M0 | PB4 (M0_ENC_A) | PB5 (M0_ENC_B) | PC9 (M0_DC_CAL pad) |
| M1 | PB6 (M1_ENC_A) | PB7 (M1_ENC_B) | PC15 (M1_DC_CAL pad) |

---

## 9. 快速调试流程

```
1. $M0,Q,0;          确认通信正常，观察回复帧
2. $M0,I,3.0;        小电流验证转向（观察 M0v 符号）
3. $M0,H,0;          急停
4. $M0,V,5.0;        速度环低速测试（5 km/h）
5. $M0,E,0;          如有错误：清除 → 自动重新校准
```
5. 急停：`$M0,H,0;`

---

## 5065 无感 FOC 首次上电测试序列

> 前提：`MOTOR_CONFIG=2`，`APP_USE_HALL_SENSOR=0`（无感模式），母线电压 24–36 V。
> 每步观察回复中的 `error` 字段，非 0 立即急停排查。

### 第 1 步 — 确认通信
```
$M0,Q,0;
```
预期回复（示例，Vbus≈24V）：
```
[M0,Q,0.000,0.00,0.000,24.20,0]
```

### 第 2 步 — 小电流开环验证（电机是否响应）
> 无感估算器尚未收敛，转子可能抖动/轻微转动，属正常现象。
```
$M0,I,2.0;
```
```
$M0,I,0.0;
```

### 第 3 步 — 速度环低速起转（≈ 40 rad/s = 54 rpm）
> 起转电流 10 A，加速度 300 rad/s²，目标 300 rad/s；
> 发送 3.6 km/h = 40 rad/s，低于起转目标，估算器应能锁相。
```
$M0,V,3.6;
```
预期回复（锁相后约 0.5 s）：
```
[M0,V,39.8,xx.xx,2.1,24.10,0]
```
急停：
```
$M0,H,0;
```

### 第 4 步 — 速度逐级提升
```
$M0,V,7.2;
```
（≈ 80 rad/s = 110 rpm，稳定后继续）
```
$M0,V,18.0;
```
（≈ 200 rad/s = 273 rpm）
```
$M0,V,27.0;
```
（≈ 300 rad/s = 409 rpm，接近起转目标速，估算器完全收敛后可继续加速）

### 第 5 步 — 反转验证
```
$M0,V,-7.2;
```
急停：
```
$M0,H,0;
```

### 第 6 步 — 力矩模式验证（电流限制 20 A，最大约 0.71 N·m）
```
$M0,T,0.35;
```
```
$M0,T,0.0;
```
```
$M0,H,0;
```

### 第 7 步 — 位置模式验证（弧度，无感模式精度有限）
```
$M0,P,90.0;
```
```
$M0,P,0.0;
```
```
$M0,H,0;
```

---

## 常见问题排查

| 现象 | 可能原因 | 处理 |
|------|---------|------|
| error=23 | 母线欠压 | 检查电源电压 ≥ 8V |
| error=20 | DRV8301 驱动器故障 | 检查供电/散热，复位电源 |
| error=9  | ADC 故障 | 检查电流传感器供电 |
| 电机嗡嗡不转 | 估算器未锁相 | 适当增大 `APP_SPIN_UP_CURRENT` 或降低 `APP_SPIN_UP_ACCELERATION` |
| 转速有抖动 | `vel_gain` / `vel_integrator_gain` 需调整 | 先调 vel_gain，再调积分项 |
| 反转后失控 | `pm_flux_linkage` 偏差 | 用电机实测 R/L 重新计算 |





