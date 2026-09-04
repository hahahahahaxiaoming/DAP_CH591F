# CH592 CMSIS-DAP v2 + 虚拟串口

本工程将 CherryDAP、CherryUSB 和 CherryRB 的必要部分移植到 CH592，实现一个最小的
CMSIS-DAP v2 调试器。当前启用 SWD 和标准 JTAG，不启用 SWO、MSC 和 WebUSB，同时提供一路
USB CDC ACM 虚拟串口。UART1 保留为可选固件日志串口，默认发布构建关闭日志以保证
传输性能；UART2 已通过 `USE_UART2=0` 禁用。

## 功能

- CMSIS-DAP v2.1，USB Bulk/WinUSB 传输
- SWD 调试和下载
- 标准 JTAG 调试和下载
- 目标硬件复位 nRESET（PA11，开漏输出）
- CDC0 对应 CH592 UART0
- UART2 初始化和中断代码保留，但通过 `USE_UART2=0` 排除编译且不加入 USB 复合设备
- UART1 可选输出固件日志，默认关闭
- PB23 通用活动 LED：DAP 和 CDC0 收发均闪烁
- USB Full Speed 设备
- 默认开启片内 DC/DC 转换器以降低运行功耗

## 快速编译

本工程使用 `Makefile` 直接编译，不依赖 IDE。先确认已经安装 WCH/MounRiver 的
RISC-V Embedded GCC 工具链，并确认 `riscv-none-embed-gcc` 在 `PATH` 中。

Windows PowerShell 示例：

```powershell
$env:Path = 'D:\gcc\RISC-V Embedded GCC12\bin;' + $env:Path
make clean
make
```

Windows CMD 示例：

```cmd
set "PATH=D:\gcc\RISC-V Embedded GCC12\bin;%PATH%"
where riscv-none-embed-gcc
make clean
make
```

编译完成后，输出文件在：

```text
build/daplink/DAPLink.elf
build/daplink/DAPLink.hex
build/usbdongle/USB_Dongle.elf
build/usbdongle/USB_Dongle.hex
```

如果只想编译单套固件，可执行：

```sh
make daplink
make usbdongle
```

默认构建不定义 `DEBUG`，不会初始化 UART1，`PRINT/DBG` 日志会在编译期移除。需要临时
排查问题时，可执行 `make clean` 后使用 `make DEBUG_LOG=1` 重新打开 UART1 日志。日志使用
115200 bit/s 阻塞输出，会明显降低高速 CDC 和无线传输性能，不建议在性能测试或正式固件中开启。

`make` 默认会生成两套固件；DAPLink 对应 `FIRMWARE_ROLE=1`，USB Dongle 对应
`FIRMWARE_ROLE=0`。

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
| LOG TX | PA9 | 输出 | UART1 TX，仅 `DEBUG_LOG=1` 时占用 |
| LOG RX | PA8 | 输入 | UART1 RX，仅 `DEBUG_LOG=1` 时占用 |
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
- DAPLink 只接受 RSSI 大于 -40 dBm 的首次配对请求，需要两端靠近后才能完成首次配对。
- USB Dongle 随机生成 Channel 1～39 和 64 位配对 ID，并在新信道完成二次确认。
- 配对成功后参数保存到 DataFlash；已有有效记录时不再接受其他设备的配对请求。
- 配对期间 LED 闪烁，成功后熄灭；DAP 数据传输时 LED 短暂点亮。
- 无线 DAP 使用序号、校验和、超时重传和响应缓存，避免重传时重复执行同一条目标命令。
- DAPLink 的本地 USB 已配置时只执行有线 DAP；USB 断开后才处理无线 DAP 请求。

USB Dongle 和 DAPLink 固件均枚举 CMSIS-DAP 与 CDC0 复合设备。Dongle 的 DAP 与
CDC0 数据通过 2.4 GHz 转发，不初始化本地 UART0、SWD 或 JTAG 引脚；DAPLink 收到
无线 CDC0 数据后通过 UART0 输出，并把 UART0 接收数据无线返回 Dongle。

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

## 数据通路与缓冲区大小

当前固件各级实际使用的缓冲区如下。表中的大小是每个独立缓冲区的容量，不应相加后
理解为一笔数据可以无条件连续缓存那么多；数据经过每一级时仍受到下一层处理速度限制。

| 位置 | 数量与单个大小 | 用途与说明 |
| --- | --- | --- |
| UART0 硬件 RX/TX FIFO | RX 8 字节、TX 8 字节 | CH592 硬件 FIFO；1 字节门限触发 RX 中断 |
| UART0 软件 RX 环形缓冲区 | 1024 字节 | DAPLink 的目标串口接收缓冲区，`UART0_RX_BUF_SIZE` |
| UART1 软件 RX 环形缓冲区 | 128 字节 | 调试日志串口的 RX 缓冲区；日志 TX 本身是阻塞输出 |
| UART2 软件 RX 环形缓冲区 | 配置值 1024 字节 | `USE_UART2=0`，当前不编译、不占用 RAM |
| USB CDC OUT 环形缓冲区 | 1024 字节 | 电脑发往 CDC 的数据等待无线或 UART0 处理 |
| USB CDC OUT/IN 工作缓冲区 | 各 64 字节 | USB Full Speed Bulk 端点单包大小 `USB_MPS` |
| CDC USB→RF 待发送缓冲区 | 64 字节 | 无线忙时保留当前一个 USB CDC 数据块 |
| CDC UART0→RF 待发送缓冲区 | 64 字节 | UART0 数据静默 2 ms 后聚合，每包最多 64 字节 |
| 无线 CDC 接收暂存 | 64 字节 | 当前为单包邮箱，不是多包队列 |
| 无线 DAP 请求/响应暂存 | 请求 64 字节、响应 64 字节 | Dongle 与 DAPLink 之间的单条 DAP 命令数据 |
| 无线重复响应缓存 | 64 字节 | DAP 请求重传时避免重复执行目标操作 |
| CMSIS-DAP USB 请求队列 | 8 × 64 字节 | 共 512 字节，`DAP_PACKET_COUNT × DAP_PACKET_SIZE` |
| CMSIS-DAP USB 响应队列 | 8 × 64 字节 | 共 512 字节，与请求队列独立 |
| USB 控制请求缓冲区 | 256 字节 | EP0 标准/类控制请求，非 CDC 数据流缓冲区 |
| USB 端点 DMA 缓冲区 | 每个方向 64 字节 | EP1～EP3 的 OUT/IN 各自拥有 64 字节底层 DMA 空间 |
| BLE/TMOS 内存堆 | 6144 字节 | CH59x 库与 TMOS 使用，不是串口数据缓冲区 |

CH592 RF 库的 `RxMaxlen` 和 `TxMaxlen` 都设置为 251 字节，它表示底层 RF PHY
允许的最大完整包长，不等于本工程的 CDC 缓冲区大小。当前无线协议将业务载荷固定为
64 字节；协议头、64 字节载荷和校验合计后，`wireless_packet_t` 实际为 88 字节，发送时
每次发送完整的 88 字节结构。因此当前 CDC/DAP 的单个无线业务包最大仍是 64 字节，
并没有使用满 RF 硬件的 251 字节能力。

当前 CDC 无线接收侧只有一个 64 字节单包邮箱，也还没有 CDC ACK/窗口流控。1024 字节
环形缓冲区可以吸收短时间突发，但持续发送速度超过无线链路处理速度时仍可能溢出；日志会
输出 `[USB CDC OUT OVERFLOW]`。需要完全可靠的持续高速传输时，还需实现多包队列、ACK、
超时重传和主机侧背压。

## UART 波特率分频与误差

系统时钟固定为 60 MHz。WCH UART 驱动使用整数分频，代码等价于：

```text
分频值       = round(60,000,000 / (8 × 设置波特率))
实际波特率   = 60,000,000 / (8 × 分频值)
误差         = (实际波特率 - 设置波特率) / 设置波特率 × 100%
```

常见设置在当前 60 MHz 系统时钟下的结果如下：

| 设置值 (bps) | 分频值 | 实际值 (bps) | 误差 |
| ---: | ---: | ---: | ---: |
| 1,200 | 6250 | 1,200.00 | 0.000% |
| 2,400 | 3125 | 2,400.00 | 0.000% |
| 4,800 | 1563 | 4,798.46 | -0.032% |
| 9,600 | 781 | 9,603.07 | +0.032% |
| 14,400 | 521 | 14,395.39 | -0.032% |
| 19,200 | 391 | 19,181.59 | -0.096% |
| 28,800 | 260 | 28,846.15 | +0.160% |
| 38,400 | 195 | 38,461.54 | +0.160% |
| 57,600 | 130 | 57,692.31 | +0.160% |
| 76,800 | 98 | 76,530.61 | -0.351% |
| 115,200 | 65 | 115,384.62 | +0.160% |
| 128,000 | 59 | 127,118.64 | -0.689% |
| 230,400 | 33 | 227,272.73 | -1.357% |
| 250,000 | 30 | 250,000.00 | 0.000% |
| 256,000 | 29 | 258,620.69 | +1.024% |
| 460,800 | 16 | 468,750.00 | +1.725% |
| 500,000 | 15 | 500,000.00 | 0.000% |
| 576,000 | 13 | 576,923.08 | +0.160% |
| 750,000 | 10 | 750,000.00 | 0.000% |
| 921,600 | 8 | 937,500.00 | +1.725% |
| 1,000,000 | 8 | 937,500.00 | **-6.250%** |
| 1,500,000 | 5 | 1,500,000.00 | **0.000%** |
| 2,000,000 | 4 | 1,875,000.00 | **-6.250%** |
| 2,500,000 | 3 | 2,500,000.00 | 0.000% |

这解释了为什么外部串口设置 1 Mbit/s 时可能完全不能通信，而 1.5 Mbit/s 反而正常：
CH592 在 1 Mbit/s 设置下实际输出/采样 937500 bit/s，单端误差已经达到 -6.25%，通常超过
异步 UART 能可靠容忍的范围；1.5 Mbit/s 则可由 60 MHz 精确整除，误差为零。若通信两端
都是同样时钟和同样分频算法的 CH592，双方设置 1 Mbit/s 后可能因为都实际运行在 937500
bit/s 而互相通信；但连接常规 USB 转串口或其他 MCU 时，对端通常会产生接近真正的
1,000,000 bit/s，于是出现较大的相对误差和帧错误。

一般建议让通信双方的总相对误差控制在约 2% 以内；具体容限还受对端 UART 采样方式、
晶振误差、数据帧长度和信号质量影响。当前 60 MHz 配置优先推荐 115200、250000、500000、
750000、1500000 或 2500000；921600/460800 的误差约 1.725%，通常可用但余量较小；
不建议使用当前分频下误差达到 6.25% 的 1000000 和 2000000。

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
