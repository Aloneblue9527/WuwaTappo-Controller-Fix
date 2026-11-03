# WuwaTappo Controller Fix

> 🎮 鸣潮桌宠（WuwaTappo）手柄按键计数修复补丁 · V2  
> Controller compatibility patch for WuwaTappo

[![Platform](https://img.shields.io/badge/Platform-Windows-blue?logo=windows)](https://www.microsoft.com/windows)
[![Game](https://img.shields.io/badge/Game-鸣潮%20Wuthering%20Waves-6f42c1)](https://mc.kurogames.com/)
[![Version](https://img.shields.io/badge/Version-V2-orange)]()
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

---

## 📖 简介

本补丁用于修复 **鸣潮桌宠（WuwaTappo）** 在部分手柄上无法统计按键与十字键点击数的问题。

原版桌宠底层仅调用了微软 Xbox 专属的 `XInputGetState` 接口，导致 **PS5 DualSense、PS4、Switch Pro** 等走 DirectInput / HID 协议的手柄，在进入《鸣潮》或切换窗口后无法录入按键数据。本补丁通过兼容层自动识别手柄类型，让所有手柄都能正常工作。

> ✅ 自动兼容所有类型手柄（含 Xbox 原生手柄）  
> ✅ V2 支持 0~15 全槽位扫描 + 状态合并  
> ✅ 不修改 `WuwaTappo.exe` 本体  
> ✅ 不修改系统注册表

---

## ✨ 特性

### 🔄 自动兼容非 Xbox 手柄

如果使用的是 **PS5 (DualSense)、PS4、Switch Pro** 等手柄：

> 当系统原版 XInput 返回 `1167`（未连接）时，补丁会自动调用系统自带的 `winmm.dll!joyGetPosEx`，读取手柄的 **14 个按键**和**十字键（dwPOV）**，并实时转换成桌宠能识别的 XInput 信号。

### 🎮 不影响 Xbox 原生手柄

如果使用的是 **Xbox 手柄**：

> 补丁在 `XInputGetState` 中会优先调用系统原版的 `xinput1_4.dll`。一旦检测到原生 Xbox 手柄返回 `0 (ERROR_SUCCESS)`，就会直接原样返回原版数据，**完全不影响 Xbox 手柄用户**。

### 🕹️ 摇杆防刷屏设计

补丁源码在 `convert` 函数中**只映射了 14 个数字按键**（含 ❌ / ⭕ / ⬜ / 🔺、肩键 L1/R1、扳机键 L2/R2、L3/R3、Start/Back 等）和**十字键**，并故意将左右摇杆轴保持在**居中归零状态**。

> 因此玩家在《鸣潮》里推摇杆跑图时**不会疯狂触发拍桌**，只有按按键和十字键时才会正常计数。

---

## 🚀 V2 更新说明

V2 版本在 V1 的基础上解决了两个最常见的“次日失效”问题：

### 1️⃣ 突破 0 号槽位限制

原本只读 0 号槽位的 `read_joystick`，升级为**自动遍历 0~15 全部 16 个 WinMM 槽位**，并将所有已连接控制器的按键状态**按位合并**（`wButtons |= ...`）。

> 哪怕玩家的真实手柄排在 2 号、3 号槽位，或者后台插着磁轴键盘 / 虚拟手柄，都能同时读取，**不再需要拔掉多余设备**。

### 2️⃣ 修复原版 XInput 短路问题

即使原版 `g_real_get(0, state)` 返回了 `ERROR_SUCCESS`（比如后台挂着空置的虚拟 Xbox 手柄，或 Steam 接管了输入），也不再直接退出，而是把**原版 XInput 读到的按键与 WinMM 读到的按键合并输出**，彻底杜绝被虚拟手柄或 Steam 屏蔽。

> V2 已从根本上解决玩家因重启电脑、多外设抢占槽位或开启 Steam 导致的次日失效问题。

---

## 📦 安装方法

1. **彻底退出**正在运行的鸣潮桌宠（右下角托盘 → 右键 → 退出）。
2. 将压缩包内的以下两个文件，一起解压 / 复制到桌宠的安装文件夹内（即包含 `WuwaTappo.exe` 和 `uninstall.exe` 的文件夹）：
   - `xinput1_4.dll`（V2 桥接补丁）
   - `xinput1_4_system.dll`（系统原版配套转发库）
3. 重新双击启动 `WuwaTappo.exe`。

完成后即可在《鸣潮》**全屏及任意窗口**下正常统计手柄按键与十字键点击数。

---

## ⚠️ 注意事项

### 1️⃣ 文件说明

| 文件 | 说明 |
| --- | --- |
| `xinput1_4.dll` | V2 桥接补丁，包含 0~15 全槽位扫描 + 状态合并逻辑 |
| `xinput1_4_system.dll` | 微软官方原版 `XInput1_4.dll`（文件版本 10.0.26100.9278），用于接收其余导出函数的转发（如 `XInputEnable` 及序号 #100~#109） |

> 两个文件**必须一起放入**桌宠目录，缺一不可。

### 2️⃣ 旧版 Windows 10 兼容性

本仓库提供的 `xinput1_4_system.dll` 提取自 **Windows 11（Build 10.0.26100.9278）**，内部依赖 `ext-ms-win-mininput-inputhost-l1-1-1.dll`、`InputUap.dll` 等较新的系统接口。

- **Windows 11 玩家**：直接下载两个文件即可使用。
- **旧版 Windows 10 玩家**：若放入后无效或无法加载，可只保留本仓库的 `xinput1_4.dll`，然后将你自己电脑上的  
  `C:\Windows\System32\xinput1_4.dll`  
  复制到桌宠目录，并**重命名为 `xinput1_4_system.dll`** 即可。

### 3️⃣ 卸载方法

本补丁不会修改 `WuwaTappo.exe` 本体或系统注册表。如需移除补丁（或桌宠官方后续更新已修复此问题）：

> 退出桌宠后，将安装目录下的 `xinput1_4.dll` 和 `xinput1_4_system.dll` 删除即可恢复原状。

---

## 🔍 问题原因

| 项目 | 说明 |
| --- | --- |
| **现象** | PS4 / PS5 / Switch Pro 手柄在《鸣潮》全屏下无响应 |
| **根因** | 原版桌宠底层仅调用微软 Xbox 专属的 `XInputGetState` 接口 |
| **影响** | 走 DirectInput / HID 协议的手柄在进入游戏或切换窗口后无法录入按键数据 |
| **解决** | 通过 `winmm.dll!joyGetPosEx` 读取 DirectInput 数据并转换为 XInput 信号（V2 进一步合并全槽位状态） |

---

## 🧾 更新日志

### V2
- 新增 0~15 全槽位 WinMM 扫描，按键状态按位合并
- 修复原版 XInput 返回 `ERROR_SUCCESS` 时短路退出的问题
- 解决多外设抢占槽位 / Steam 接管 / 重启电脑后的次日失效问题
- 不再需要手动拔掉虚拟手柄或磁轴键盘

### V1
- 首次发布：为非 Xbox 手柄补充 WinMM 读取并转换为 XInput 信号

---

## 📄 免责声明

本项目为个人兴趣开发的兼容性补丁，与鸣潮官方及 WuwaTappo 作者无关。使用前请自行备份相关文件，因使用本补丁造成的任何问题由使用者自行承担。

---

<p align="center">
  <i>如果这个补丁帮到了你，欢迎点一个 ⭐ Star 支持一下！</i>
</p>
