# Dangbei Projector Local Control for Home Assistant 📽️

[![Hardware: ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3-blue.svg)](#)
[![Integration: Home Assistant](https://img.shields.io/badge/Integration-Home%20Assistant-41BDF5.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

[🇺🇸 English](#english) | [🇨🇳 中文说明](#chinese)

> **➡️ 新项目：** 本项目的投影仪部分已被 **[dangbei-netlea-hub](https://github.com/lee1985-cn/dangbei-netlea-hub)** 完整涵盖 —— 一块 ESP32-C3 同时控制投影仪 **和** 尼特利变频过滤桶，并带预编译固件可直接刷。
> 如果你只有投影仪、不想折腾编译，直接用那边的 Release 更省事；本项目保留作历史版本与补丁说明。

<a name="english"></a>
## 🇺🇸 English Overview

A 100% local, zero-latency integration solution for Dangbei projectors in Home Assistant, powered by an **ESP32-C3**.

Due to the strict deep-sleep mechanism of Dangbei projectors, standard BLE keyboard simulators fail to wake up the device. This project utilizes a **Split-Boot Architecture** (RTC memory state persistence) to seamlessly switch between a raw ESP-IDF BLE Beacon mode (for hardware wake-up) and a standard BleKeyboard mode (for power-off and media control), ensuring stable and idempotent operations.

**Core Features:**
* **Absolute Wake-up**: Replays the factory original BLE beacon packet to bypass the hardware deep-sleep filter.
* **Zero Identity Loss**: Forces the `NimBLE` stack globally and explicitly mounts NVS flash to preserve Bonding keys across power losses.
* **Split-Boot Design**: Prevents NVS memory pollution by physically isolating the wake-up and control modes via soft reboots.

For detailed installation and Home Assistant YAML configurations, please refer to the Chinese documentation below.

---

<a name="chinese"></a>
## 🇨🇳 当贝投影仪纯本地 HA 接入方案 (ESP32-C3)

这是一个基于 ESP32-C3 的硬核本地控制网关，旨在彻底解决当贝投影仪在 Home Assistant 中的“休眠无法唤醒”痛点。

本项目不仅实现了开/关机、音量调节的纯本地局域网控制，更通过底层的“双系统软重启架构”和 NVS 闪存固化，做到了**断电不丢签、控制高可用、状态防干扰**的运维级稳定性。

### 🌟 核心特性
1. **魔法唤醒（无视深度休眠）**：抓取并重放原厂遥控器的绝对唤醒信标包，直接触发主板硬件中断。
2. **双态隔离架构**：利用 ESP32-C3 的 `RTC_NOINIT_ATTR` 内存特性，在“纯净信标模式”与“常态控制模式”间通过软重启切换，杜绝蓝牙协议栈的内存互相污染。
3. **断电防失忆（NVS 固化）**：全局强制接管 `NimBLE` 协议栈并挂载闪存，确保配对密钥（Bonding Keys）永久保存，无论拔电多久，通电即秒连。
4. **防吞键与掩码修复**：修复了底层库的位掩码冲突（解决关机变音量加的问题），并内置唤醒链路的激活序列，确保关机指令 100% 触达。

### 🛠️ 准备工作：必须的外科手术级修改
本项目依赖 `ESP32-BLE-Keyboard` 库。在烧录代码前，**必须**修改该库底层的 HID 描述符，否则无法实现关机：

1. 打开电脑上的库文件：`Documents\Arduino\libraries\ESP32-BLE-Keyboard\BleKeyboard.cpp`
2. 找到 `_hidReportDescriptor[]` 数组（在文件**第 33 行**附近开始），定位到 **第 80 行** 的媒体键区：
   ```cpp
   USAGE(1),           0xB7,          //   USAGE (Stop)                ; bit 2: 4
   ```
3. 把 `0xB7`（Stop）改成 `0x30`（Power）：
   ```cpp
   USAGE(1),           0x30,          //   USAGE (Power)
   ```
4. 保存文件。

> **为什么是 bit 2 而不是 bit 7**
> `main.cpp` 里 `const MediaKeyReport KEY_PROJ_POWER = {0x04, 0x00};` 用的是 **bit 2**（0x04）。
> 在库的媒体键描述符里，bit 0 = Scan Next Track、bit 1 = Scan Previous Track、**bit 2 = Stop (0xB7)**、bit 3 = Play/Pause、bit 7 = Mail (0x8A)。
> 所以必须改 **bit 2 的 Stop**，改成 bit 7 的 Mail 会导致关机键失效。
>
> 改错位置的典型症状：按关机变成「音量 +」或者完全没反应。


🚀 部署指南
1. 固件烧录
使用 Arduino IDE 或 VS Code (PlatformIO) 打开 src/main.cpp。

修改代码顶部的 ssid 和 password 为你家的局域网 WiFi。

将代码烧录进 ESP32-C3 模块。

2. 唯一一次蓝牙配对
使用原厂遥控器打开投影仪。

进入投影仪的 设置 -> 蓝牙设置。

彻底删除 以前残留的任何 Dangbei_ESP32 设备。

重新搜索并连接 Dangbei_ESP32。完成这一次配对后，ESP32 即可随意断电移动，永久不掉签。

3. Home Assistant 配置
将 ESP32 在路由器中绑定静态 IP（本项目示例使用 `10.0.0.54`，请改成你自己的）。然后将本项目 `src/home-assistant/` 目录下的配置文件按模块添加到你的 HA 中：

`rest_commands.yaml`: 定义所有的动作接口（开机、关机、音量等）。

`sensor.yaml`: 通过 HTTP 轮询获取真实的设备连接状态。

`template.yaml`: 组合为一个完美的 UI Switch 实体。

`automations.yaml`: 可选的自动化示例。

> ⚠️ 这几个 YAML 里的 IP 需要替换成你实际的 ESP32 地址，否则实体全部不可用。

🎙️ 进阶玩法：接入小爱同学语音控制
通过 Home Assistant 的自动化脚本，我们可以将该实体暴露给智能音箱，实现“小爱同学，打开投影仪”、“小爱同学，投影仪大点声”等自然语义控制。

[👉 详细小爱同学接入教程与思路复盘请参考我的博客文章](https://blog.lida1985.eu.org/archives/xiao-ai-tong-xue-home-assistant-yi-jian-yu-yin-kong-zhi-pve-su-zhu-ji-ji-xu-ni-ji-kai-guan-ji)

📜 许可证 (License)
本项目基于 MIT License 开源，欢迎提交 PR 或在 Issue 中讨论！