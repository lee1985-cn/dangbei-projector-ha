# Dangbei Projector Local Control for Home Assistant 📽️

[![Hardware: ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3-blue.svg)](#)
[![Integration: Home Assistant](https://img.shields.io/badge/Integration-Home%20Assistant-41BDF5.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

[🇺🇸 English](#english) | [🇨🇳 中文说明](#chinese)

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
本项目依赖 `ESP32-BLE-Keyboard` 库。在烧录代码前，**必须**修改该库底层的描述符，否则无法实现关机：
1. 打开电脑上的库文件：`Documents\Arduino\libraries\ESP32-BLE-Keyboard\BleKeyboard.cpp`
2. 找到约 150 行的 `_hidReportDescriptor` 数组。
3. 将无用的邮件键 `0x09, 0x8A, // Usage (AL Email Reader)` 修改为标准电源键：
   ```cpp
   0x09, 0x30,          //   Usage (Power)
4. 保存文件。

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
将 ESP32 在路由器中绑定静态 IP。然后将本项目 home-assistant/ 目录下的配置文件按模块添加到你的 HA 中：

rest_commands.yaml: 定义所有的动作接口（开机、关机、音量等）。

sensor.yaml: 通过 HTTP 轮询获取真实的设备连接状态。

template.yaml: 组合为一个完美的 UI Switch 实体。

🎙️ 进阶玩法：接入小爱同学语音控制
通过 Home Assistant 的自动化脚本，我们可以将该实体暴露给智能音箱，实现“小爱同学，打开投影仪”、“小爱同学，投影仪大点声”等自然语义控制。

👉 详细小爱同学接入教程与思路复盘请参考我的博客文章

📜 许可证 (License)
本项目基于 MIT License 开源，欢迎提交 PR 或在 Issue 中讨论！