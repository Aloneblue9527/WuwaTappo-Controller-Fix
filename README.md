# WuwaTappo Controller Fix

> 🎮 鸣潮桌宠（WuwaTappo）手柄按键计数修复补丁  
> Controller compatibility patch for WuwaTappo

[![Platform](https://img.shields.io/badge/Platform-Windows-blue?logo=windows)](https://www.microsoft.com/windows)
[![Game](https://img.shields.io/badge/Game-鸣潮%20Wuthering%20Waves-6f42c1)](https://mc.kurogames.com/)
[![Version](https://img.shields.io/badge/Version-V2-orange)]()
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

---

## 📖 简介

本补丁用于修复 **鸣潮桌宠（WuwaTappo）** 在部分手柄上无法统计按键与摇杆点击数的问题。

原版桌宠底层仅调用了微软 Xbox 专属的 `XInputGetState` 接口，导致 **PS5 DualSense、PS4、Switch Pro** 等走 DirectInput / HID 协议的手柄，在进入《鸣潮》或切换窗口后无法录入按键数据。本补丁通过兼容层自动识别手柄类型，让所有手柄都能正常工作。

> ✅ 自动兼容所有类型手柄（含 Xbox 原生手柄）  
> ✅ 支持全槽位扫描 + 状态合并  
> ✅ 不修改 `WuwaTappo.exe` 本体  
> ✅ 不修改系统注册表

---

## ✨ 特性

### 🔄 自动兼容非 Xbox 手柄

如果使用的是 **PS5 (DualSense)、PS4、Switch Pro** 等手柄：

> 当系统原版 XInput 返回 `1167`（未连接）时，补丁会自动调用系统自带的 `winmm.dll!joyGetPosEx`，读取手柄的 **按键**和**摇杆**，并实时转换成桌宠能识别的 XInput 信号。

### 🎮 不影响 Xbox 原生手柄

如果使用的是 **Xbox 手柄**：

> 补丁在 `XInputGetState` 中会优先调用系统原版的 `xinput1_4.dll`。一旦检测到原生 Xbox 手柄返回 `0 (ERROR_SUCCESS)`，就会直接原样返回原版数据，**完全不影响 Xbox 手柄用户**。

---

## 📦 安装方法

1. **彻底退出**正在运行的鸣潮桌宠（右下角托盘 → 右键 → 退出）。
2. 仅需将`xinput1_4.dll`文件放入桌宠安装文件夹内即可
3. 重新双击启动 `WuwaTappo.exe`。

完成后即可在《鸣潮》**全屏及任意窗口**下正常统计手柄按键与摇杆点击数。

---

### 3️⃣ 卸载方法

本补丁不会修改 `WuwaTappo.exe` 本体或系统注册表。如需移除补丁（或桌宠官方后续更新已修复此问题）：

> 退出桌宠后，将安装目录下的 `xinput1_4.dll` 删除即可恢复原状。

---

## 🔍 问题原因

| 项目 | 说明 |
| --- | --- |
| **现象** | PS4 / PS5 / Switch Pro 手柄在《鸣潮》全屏下无响应 |
| **根因** | 原版桌宠底层仅调用微软 Xbox 专属的 `XInputGetState` 接口 |
| **影响** | 走 DirectInput / HID 协议的手柄在进入游戏或切换窗口后无法录入按键数据 |
| **解决** | 通过 `winmm.dll!joyGetPosEx` 读取 DirectInput 数据并转换为 XInput 信号（V2 进一步合并全槽位状态） |

---

## 📄 免责声明

本项目为个人兴趣开发的兼容性补丁，与鸣潮官方及 WuwaTappo 作者无关。使用前请自行备份相关文件，因使用本补丁造成的任何问题由使用者自行承担。

---

<p align="center">
  <i>如果这个补丁帮到了你，欢迎点一个 ⭐ Star 支持一下！</i>
</p>
