# MKLink 调试命令速查

## 环境

| 项目         | 值                                       |
| ------------ | ---------------------------------------- |
| MCU          | PY32F003x8 (Cortex-M0+)                  |
| SWD 时钟上限 | 200kHz                                   |
| 调试口       | COM10 (SWD)                              |
| TTL/RS485 口 | COM9                                     |
| 磁盘         | F:\ (MicroLink)                          |
| FLM          | PY32F0xx_64.FLM                          |
| HEX          | Projects\...\MDK-ARM\Output\Debug\TC.hex |
| AXF          | Projects\...\MDK-ARM\Output\Debug\TC.axf |

## 连接与烧录

```bash
# 发现端口和磁盘
python -m mklink discover

# 测试连接
python -m mklink test --port COM10

# 烧录（指定 HEX）
python -m mklink flash --port COM10 --hex "Projects/.../TC.hex"

# 烧录（从 .mklink/project_info.json 自动读取）
python -m mklink flash
```

## RTT 日志输出

```bash
# 基础 RTT 捕获（默认 10 秒）
python -m mklink rtt --port COM10 --duration 15

# RTT + 浏览器可视化
python -m mklink rtt --visualize --duration 30
```

> **注意**: RTT 需要固件主动调用 `SEGGER_RTT_printf()`，调试器自身不会注入代码。DEBUG 宏打开后才会有丰富的 RTT 打印。

## SuperWatch — SWD 实时变量观测

**不需要接线、不需要改代码**，直接通过 SWD 读取芯片 RAM 中的变量值。

```bash
# 列出所有可观测变量
python -m mklink symbols --source "Projects/.../TC.axf"

# 采样指定变量（JSON 输出）
python -m mklink superwatch g_chip_temp,monitor_state,g_time_counts,g_result_r,cell_status \
  --source "Projects/.../TC.axf" \
  --port COM10 \
  --period 0.2 \
  --duration 15

# 可视化波形（浏览器实时图表）
python -m mklink superwatch g_chip_temp,monitor_state,g_time_counts,g_result_r,cell_status \
  --source "Projects/.../TC.axf" \
  --port COM10 \
  --period 0.2 \
  --visualize \
  --duration 60
```

### 可视化快捷键

| 按键       | 功能                  |
| ---------- | --------------------- |
| `Space`  | 暂停/恢复更新         |
| `L`      | 显示/隐藏原始日志面板 |
| 点击曲线名 | 切换显示/隐藏单条曲线 |

### 常用选项

| 选项                 | 说明                 | 示例          |
| -------------------- | -------------------- | ------------- |
| `--period`         | 采样间隔（秒）       | `0.2` = 5Hz |
| `--duration`       | 总时长（秒），0=持续 | `60`        |
| `--visualize`      | 启动浏览器仪表盘     | —            |
| `--no-browser`     | 不自动打开浏览器     | —            |
| `--port-http 8888` | 固定 HTTP 端口       | —            |
| `--max-points 500` | 图表最大数据点数     | —            |

### 支持的类型

变量名支持基础类型和 `struct.field` 格式，也可混合内存映射寄存器（如 `SCB.CFSR`）。

## 串口调试（COM9）

TTL/RS485 口的通用 UART 调试，独立于 SWD 接口。

```bash
# 列出可用串口（自动排除 MKLink 虚拟串口）
python -m mklink serial list

# 交互式终端
python -m mklink serial open --port COM9 --baud 115200

# HEX 模式 + 协议解析
python -m mklink serial open --port COM9 --baud 9600 --mode hex --profile my_protocol.json

# 带日志记录
python -m mklink serial open --port COM9 --baud 115200 --log uart_log.txt

# 发送 HEX 数据
python -m mklink serial send --port COM9 --baud 115200 --hex "AA55010300"

# 无头日志模式（定长采集）
python -m mklink serial log --port COM9 --baud 115200 --output data.csv --format csv --duration 60

# Web Dashboard（协议帧解析）
python -m mklink serial dashboard --port COM9 --baud 115200 --profile protocol.json
```

**终端快捷键**: `Ctrl+Q` 退出、`Ctrl+H` 切换 HEX/ASCII 模式、`Ctrl+L` 清屏。

### 协议 Profile 管理

```bash
# 从 C 头文件自动生成协议 Profile
python -m mklink serial profile detect --source inc/uart_protocol.h
python -m mklink serial profile generate --source inc/uart_protocol.h --output .mklink/serial_profile.json

# 查看 Profile
python -m mklink serial profile show --profile .mklink/serial_profile.json
```

## 远程 GUI

```bash
# 启动远程调试服务器（REST API + WebSocket）
python -m mklink serve --port 8765

# 一键启动 Web GUI（FastAPI 后端 + Vue 前端）
python -m mklink gui --port 8765 --device-port COM10

# 不自动打开浏览器
python -m mklink gui --no-browser
```

GUI 提供 **配置页**（COM 口选择、MCU 配置）和 **仪表盘页**（RTT View、烧录、SuperWatch、串口、Modbus）。

## 首次调试的变量速查

本次会话确认板子正常运行时观察到的关键变量：

| 变量                   | 说明             | 正常值示例         |
| ---------------------- | ---------------- | ------------------ |
| `g_time_counts`      | 系统 tick 计数器 | 持续递增           |
| `monitor_state`      | 监控状态机       | 0→2→4→1→0 循环 |
| `g_result_r`         | 内阻计算结果     | 0（未接电池）      |
| `g_chip_temp`        | MCU 内部温度     | 0.0（未使能）      |
| `cell_status`        | 电池状态位       | 257                |
| `g_result_vol_value` | 电压计算结果     | —                 |
| `g_leaking_vol`      | 漏电电压         | —                 |
| `uwTick`             | SysTick 计数值   | 持续递增           |

完整变量列表通过 `python -m mklink symbols --source <axf>` 获取。

## 故障排查

| 现象                               | 原因              | 解决                             |
| ---------------------------------- | ----------------- | -------------------------------- |
| `Failed to write file,addr: 0x0` | SWD 时钟过高      | PY32F003 降到 200kHz             |
| `握手超时，设备可能处于流模式`   | MKLink 卡在流模式 | 拔插 USB 重连                    |
| 磁盘检测失败                       | 卷标不匹配        | 已修 discovery.py 兼容 MicroLink |
| `IDCODE 读取失败`                | 固件无此命令      | 已修 flash.py 跳过               |
| RTT 无输出                         | 固件未调用 printf | 开 DEBUG 宏或加打印              |
