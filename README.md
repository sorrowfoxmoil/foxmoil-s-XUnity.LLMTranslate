# XUnity LLM Translator GUI

<div align="center">

<h1>
  <a href="README_US.md">English</a> | <a href="README.md">中文</a>
</h1>

</div>

<div align="center">

<img src="https://img.shields.io/badge/license-MIT-green" height="25">  
<img src="https://img.shields.io/badge/Qt-6.x-blue" height="25">  
<img src="https://img.shields.io/badge/C++-17-orange" height="25">
<img src="https://img.shields.io/badge/Platform-Windows-lightgrey" height="25">

</div>

---

## 🇨🇳 中文版本

### 📖 简介
**XUnity LLM Translator GUI** 是一款专为 Unity 游戏设计的本地翻译中转工具。它作为一个轻量级的本地 HTTP 转发服务器，主要用于将 **XUnity.AutoTranslator** 的请求桥接至各大语言模型（如 Grok, DeepSeek, OpenAI, Gemini, Ollama 等）。

本项目基于 **C++17 / Qt** 编写，主要目的是在保证较低响应延迟和较高并发请求处理能力的同时，提供一个可视化的配置管理界面。

---

### 🛠️ 核心功能

#### 🎨 双模式界面 (UI)
- **经典与流光模式**：提供两种 UI 风格供自由切换。
  - **经典模式 (Classic)**：设计简明，去除了复杂的过渡动画与渲染，适合配置较老的电脑使用。
  - **流光模式 (Modern)**：注重了过渡动画、毛玻璃模糊（Glassmorphism）与光影细节，适合较新的电脑使用。（*注：得益于 Qt 底层的硬件加速，实际测试中两种模式的性能占用差异极小，可按个人审美自由选择。*）
- **内置术语编辑器**：支持在界面内直接侧滑展开术语表（`_Substitutions.txt`）并进行编辑，带有基础的原文/译文语法高亮。

#### 🧠 翻译逻辑处理
- **多行打包路由 (Batch Mode)**：开启后，程序将尝试自动修改游戏目录下的 `Config.ini`，伪装为 Google 节点路由至本地端口，以支持多行文本的并发打包翻译。关闭服务时程序会自动还原该配置。
- **防上下文污染 (Anti-Bleed)**：内置本地转义处理，在发送给大模型前保护换行符 `[LF]` 与游戏富文本标签（如 `<T_0>`）。要求模型将传入的多行文本视为独立碎片处理，减少大模型因“自行脑补”前文而产生的幻觉或逻辑连读污染。
- **术语自进化 (RAG)**：在翻译过程中自动读取设定的术语表作为上下文参考，并尝试提取未记录的名词补充到文件中。

#### 🚀 并发与服务控制
- **异步处理**：基于 `httplib` 实现的底层线程池，支持 64~256 的并发线程数。
- **API 轮询**：支持填入多个以逗号分隔的 API Key，程序会在请求时进行轮询调用。
- **动态热重载**：在服务运行期间，可直接修改模型名称、API Key 或系统提示词，配置将在下一次请求时实时生效。

#### 🛡️ 状态监测与容错
- **HUD 悬浮窗**：可切换至迷你 HUD 窗口，提供简单的三色状态灯（绿/青/红）指示当前工作状态，并统计 Token 消耗。
- **错误映射与超时**：针对 API 请求设置了 10~40 秒的强制超时机制，防止游戏因长连接无响应而卡死；对常见的 HTTP 状态码（401, 429 等）提供了直观的中文建议。

---

### 🚀 快速开始

#### 方式一：标准模式 (逐行处理)
1. 运行本程序，填入 API 并点击 **测试配置**，测试通过后点击 **启动服务**。
2. 手动编辑游戏目录下的 `AutoTranslator/Config.ini` 文件：
   ```ini
   [Service]
   Endpoint=CustomTranslate
   
   [Custom]
   Url=http://localhost:6800  # 此处端口需与 GUI 中设置的端口保持一致
   ```

#### 方式二：打包模式 (多行并发 - 推荐于文本密集型 UI)
1. 在本程序主界面勾选 **📦 多行模式 (Batching)**。
2. 确保在界面中选择了正确的术语表（`.txt`）路径，程序需要依赖此路径反向定位游戏的 `Config.ini`。
3. 点击 **启动服务**（程序将自动修改并接管游戏的配置文件）。
4. 运行游戏。退出本程序时，被修改的游戏配置会自动恢复。

---

### 📂 代码结构概述

```text
src/
├── main.cpp                   # 应用入口、UI 切换与过渡动画控制
├── MainWindow.cpp/h           # 经典模式主界面与业务中枢
├── ModernWindow.cpp/h         # 流光模式主界面
├── TranslationServer.cpp/h    # HTTP 服务器、API 交互与重试逻辑
├── XuaConfigHijacker.h        # 游戏配置文件自动修改与还原组件
├── GlossaryManager.h          # 术语表文件读写与 RAG 注入逻辑
├── RegexManager.h             # 前置/后置正则表达式处理
├── HudWindow.cpp/h            # HUD 悬浮状态窗
├── ModernUI.h                 # 流光组件库 (包含内置编辑器、渲染代理)
├── ConfigManager.cpp/h        # 配置文件 (config.ini) 序列化读取/保存
└── TokenManager.cpp/h         # Token 统计管理器
```

---

### 🛠️ 编译与开发指南

- **环境要求**：支持 C++17 的编译器 (如 MSVC 2019+, MinGW 8.1+), Qt 6.2.0+, CMake 3.16+。
- **构建步骤**:
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_PREFIX_PATH=/your/qt/path
  cmake --build . --config Release
  ```

> **致二次开发者**：
> 为保证 UI 元素的完美对齐与最佳视觉体验，若您需要进行界面上的二次开发，请尽量将主窗口的像素大小硬性限制在 `500 x 832` 左右。

---

### 📦 部署与打包
- 编译生成 `.exe` 后，推荐使用 `windeployqt` 收集所需的 Qt 运行时依赖库。
- 若需发布单文件版本，可使用 **Enigma Virtual Box** 等虚拟化工具将所有依赖环境打包进单一可执行文件中。

---

### 📝 许可证
本项目基于 **MIT** 许可证开源。您可以自由地使用、修改及分发本项目代码，但需保留原作者版权声明。

---

> 📖 **Looking for the English version?** Check [README_US.md](README_US.md)
