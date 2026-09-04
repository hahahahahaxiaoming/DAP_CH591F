# CH592 CMSIS-DAP v2 + 双虚拟串口

本工程将 CherryDAP、CherryUSB 和 CherryRB 的必要部分移植到 CH592，实现一个最小的
CMSIS-DAP v2 调试器。当前启用 SWD 和标准 JTAG，不启用 SWO、MSC 和 WebUSB，同时提供一路
USB CDC ACM 虚拟串口。UART1 保留为固件日志串口，UART2 已通过 `USE_UART2=0` 禁用。

## 功能

- CMSIS-DAP v2.1，USB Bulk/WinUSB 传输
- SWD 调试和下载
- 标准 JTAG 调试和下载
- 目标硬件复位 nRESET（PA11，开漏输出）
- CDC0 对应 CH592 UART0
- UART2 初始化和中断代码保留，但通过 `USE_UART2=0` 排除编译且不加入 USB 复合设备
- UART1 输出固件日志
- PB23 通用活动 LED：DAP 和 CDC0 收发均闪烁
- USB Full Speed 设备
- 默认开启片内 DC/DC 转换器以降低运行功耗

## 引脚分配

`SWCLK`、`SWDIO`、`nRESET`、`TDI`、`TDO` 和活动 LED 的端口、引脚可在
`HAL/include/board.h` 中修改。端口宏填写 `A` 或 `B`，UART 和 USB 引脚保持固定。

| 功能 | CH592 引脚 | 方向（相对 CH592） | 说明 |
| --- | --- | --- | --- |
| USB D- | PB10 | 双向 | CH592 固定 USB 引脚 |
| USB D+ | PB11 | 双向 | CH592 固定 USB 引脚，使用内部上拉 |
| SWCLK | PA12 | 输出 | 连接目标芯片 SWCLK |
| SWDIO | PA13 | 双向 | 连接目标芯片 SWDIO |
| nRESET | PA11 | 开漏输出 | 低电平复位目标芯片，目标侧需要上拉 |
| CDC0 TX | PB7 | 输出 | UART0 TX，连接外部设备 RX |
| CDC0 RX | PB4 | 输入 | UART0 RX，连接外部设备 TX |
| 预留 UART2 TX | PB23 | 输出 | `USE_UART2=0`，当前不占用、不枚举 CDC1 |
| 预留 UART2 RX | PB22 | 输入 | `USE_UART2=0`，当前不占用、不枚举 CDC1 |
| LOG TX | PA9 | 输出 | UART1 TX，默认日志口 |
| LOG RX | PA8 | 输入 | UART1 RX，当前日志功能通常不需要连接 |
| 活动 LED | PB23 | 输出 | 默认高电平点亮，DAP 和 CDC0 收发共用 |

目标 SWD 接口至少应引出 `SWCLK`、`SWDIO`、`GND` 和 `VTref`，建议同时连接
`nRESET`。VTref 当前不接入 ADC，仅用于确认调试器与目标板使用兼容的 IO 电压。

USB D+/D-
走线应短、等长，是否串联 22 Ω 左右电阻按 PCB 和信号质量决定。CH592 与目标芯片必须共地。

PB14/PB15 建议保留给 CH592 自身的 WCH 调试接口，不用于目标 SWD。

标准 JTAG 与 SWD 默认使用 PA11～PA15：

| JTAG 信号 | CH592 引脚 | 方向（相对 CH592） |
| --- | --- | --- |
| nRESET | PA11 | 开漏输出，目标侧需要上拉 |
| nTRST | 不连接 | 通过 TMS/TCK 序列复位 JTAG TAP |
| TCK | PA12 | 输出 |
| TMS | PA13 | 输出/输入，与 SWDIO 共用 |
| TDI | PA14 | 输出 |
| TDO | PA15 | 输入 |

此分配的 SWD 使用 PA12/PA13，PA11 专用于目标 nRESET，建议目标侧设置约 10 kΩ
上拉。JTAG 不使用独立 nTRST，CMSIS-DAP 通过 TMS/TCK 序列复位 TAP。

## USB 接口和端点

设备枚举为一个复合 USB 设备：

| 接口 | 端点 | 类型 |
| --- | --- | --- |
| CMSIS-DAP v2 | EP1 OUT / EP1 IN | Bulk / WinUSB |
| CDC0 控制 | EP2 IN | Interrupt |
| CDC0 数据 | EP3 OUT / EP3 IN | Bulk |

为兼容性验证，当前按参考工程使用 DAPLink VID/PID `0D28:0204`，定义在
`APP/dap_main.c`。其中 VID `0D28` 属于 Arm，量产产品必须换成自己合法取得的 VID/PID。

CMSIS-DAP v2 的 WinUSB 接口使用标准设备接口 GUID：

```text
{CDB3B5AD-293B-4663-AA36-1AAE46463776}
```

该 GUID 通过 Microsoft OS 2.0 `DeviceInterfaceGUIDs` 属性下发，供 Keil 等调试软件发现探针。

## 编译

需要 WCH/MounRiver RISC-V Embedded GCC 工具链。命令行构建：

```sh
make clean
make
```

如果编译器不在 PATH，可在 PowerShell 中临时设置：

```powershell
$env:Path = 'D:\gcc\RISC-V Embedded GCC12\bin;' + $env:Path
make clean
make
```

```cmd
set "PATH=D:\gcc\RISC-V Embedded GCC12\bin;%PATH%"
where riscv-none-embed-gcc
make clean
make
```

输出文件：

```text
build/daplink/DAPLink.elf
build/daplink/DAPLink.hex
build/usbdongle/USB_Dongle.elf
build/usbdongle/USB_Dongle.hex
```

`make` 一次生成 DAPLink 与 USB Dongle 两套固件；也可分别执行 `make daplink`
或 `make usbdongle`。DAPLink 对应 `FIRMWARE_ROLE=1`，USB Dongle 对应
`FIRMWARE_ROLE=0`。MounRiver 工程默认生成 DAPLink 固件。

## 无线配对与传输

- 没有有效配对记录时，双方在 Channel 0 工作，USB Dongle 每 100 ms 发送一次配对请求。
- DAPLink 只接受 RSSI 大于 -50 dBm 的首次配对请求。
- USB Dongle 随机生成 Channel 1～39 和 64 位配对 ID，并在新信道完成二次确认。
- 配对成功后参数保存到 DataFlash；已有有效记录时不再接受其他设备的配对请求。
- 配对期间 LED 闪烁，成功后熄灭；DAP 数据传输时 LED 短暂点亮。
- 无线 DAP 使用序号、校验和、超时重传和响应缓存，避免重传时重复执行同一条目标命令。
- DAPLink 的本地 USB 已配置时只执行有线 DAP；USB 断开后才处理无线 DAP 请求。

USB Dongle 固件只枚举 CMSIS-DAP WinUSB 接口，不初始化 UART0、SWD 或 JTAG 引脚。
DAPLink 固件继续枚举 CMSIS-DAP 与 CDC0 复合设备。

MounRiver Studio 的 `.cproject` 已加入 DAP、CherryUSB 和 CherryRB 头文件路径。若 IDE
仍使用旧的 `obj` 自动生成文件，请执行一次 Clean Project，让 IDE 重新生成构建文件。

## 使用

1. 烧录 `build/CH592_DAP.hex`。
2. 将 PB10/PB11 接到 USB 接口。
3. SWD 模式将 PA12、PA13、PA11、GND 接到目标板 SWCLK、SWDIO、nRESET、GND；JTAG
   模式按上方 PA11～PA15 表连接。
4. Windows 应显示一个 WinUSB CMSIS-DAP 接口和一个串口。
5. OpenOCD、pyOCD、Keil 或其他支持 CMSIS-DAP v2 的工具选择该探针即可。
6. 虚拟串口连接 UART0（PB7/PB4）；UART2（PB23/PB22）当前停用。

CDC 主机修改波特率时，固件会更新 UART0 波特率。当前实现固定使用 8 位
数据，串口奇偶校验和停止位的动态切换尚未实现。

UART1 日志会输出 UART0 的波特率设置和实际收发数据：

```text
[UART0 CFG] 115200 baud, 8 data, parity=0, stop=0
[UART0 TX] 3 byte: 41 42 43
[UART0 RX] 3 byte: 31 32 33
```

其中 `TX` 表示 USB CDC 发往 UART0，`RX` 表示 UART0 收到后发往 USB CDC。
UART0、UART1 的接收 FIFO 均配置为 1 字节触发中断；收到数据后立即写入
CherryRB，以降低短数据包的接收延迟。

DAPLink 和 USB Dongle 的活动 LED 均默认使用 PB23、高电平点亮，配置彼此独立。
在 `HAL/include/board.h` 中分别修改 `DAPLINK_LED_PORT/PIN/ACTIVE_HIGH` 和
`USB_DONGLE_LED_PORT/PIN/ACTIVE_HIGH` 即可；`ACTIVE_HIGH` 为 `1` 表示高电平点亮，
为 `0` 表示低电平点亮。固件会根据 `FIRMWARE_ROLE` 自动选用对应角色的 LED。

## 目录

```text
APP/dap_main.c                  USB 复合设备与 8 深度 DAP/CDC 数据通路
APP/RF_PHY.c                    CH592 2.4 GHz 基础收发驱动
APP/wireless_dap.c              配对状态机和无线 CMSIS-DAP 可靠传输
APP/include/wireless_dap.h      固件角色与无线 DAP 接口
HAL/include/board.h             DAP 调试信号和活动 LED 的端口、引脚配置
DAP/                            CMSIS-DAP v2.1 协议实现
HAL/uart.c                      UART0/UART1 驱动及已禁用的 UART2 预留代码
HAL/activity_led.c              按固件角色选择的活动 LED
HAL/flash_save.c                无线配对参数的 DataFlash 持久化
ThirdParty/CherryUSB/           CherryUSB Device 栈及 CH58x/CH59x USBFS 端口
ThirdParty/CherryRB/            USB 到 UART 的环形缓冲区
Makefile                        可复现的命令行构建入口
```

## 上游项目

- [CherryDAP](https://github.com/cherry-embedded/CherryDAP)
- [CherryUSB](https://github.com/cherry-embedded/CherryUSB)
- [CherryRB](https://github.com/cherry-embedded/CherryRB)

引入的第三方代码保留其原始版权头和许可证要求。CherryDAP、CherryUSB、CherryRB 和
CMSIS-DAP 相关代码均采用 Apache-2.0 许可证，发布产品前请保留相应版权与许可证文件。

## 当前验证状态

工程已经完成全量编译和链接验证。USB 枚举、双串口收发、SWD 时序和不同目标芯片上的
下载稳定性仍需要在实际 CH592 硬件上验证。首次上板建议先确认 USB 枚举，再测试 CDC0
回环，最后从较低 SWD/JTAG 时钟开始测试目标芯片连接。
