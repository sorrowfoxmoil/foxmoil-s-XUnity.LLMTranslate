# XUnity LLM Translator GUI

<div align="center">

<h2>
  <a href="README_US.md">English</a> | <a href="README.md">中文</a>
</h2>

</div>

<div align="center">

<img src="https://img.shields.io/badge/license-MIT-green" height="25">  
<img src="https://img.shields.io/badge/Qt-6.x-blue" height="25">  
<img src="https://img.shields.io/badge/C++-17-orange" height="25">
<img src="https://img.shields.io/badge/Platform-Windows-lightgrey" height="25">

</div>

---

## 🇬🇧 English Version

### 📖 Introduction
**XUnity LLM Translator GUI** is a local translation proxy tool designed for Unity games. It functions as a lightweight local HTTP relay server, primarily used to bridge **XUnity.AutoTranslator** requests to various Large Language Models (e.g., Grok, DeepSeek, OpenAI, Gemini, Ollama).

Built with **C++17 / Qt**, this project aims to provide a visual configuration interface while maintaining low response latency and high concurrent request handling capabilities.

---

### 🛠️ Core Features

#### 🎨 Dual UI Modes
- **Classic & Modern Modes**: Provides two UI styles to switch between freely.
  - **Classic Mode**: Simple design, removes complex transition animations and rendering. Recommended for older computers.
  - **Modern Mode**: Focuses on transition animations, glassmorphism, and lighting details. Recommended for newer computers. 
  *(Note: Thanks to Qt's underlying hardware acceleration, the actual performance impact difference between the two modes is minimal. Choose based on personal aesthetic preference.)*
- **Built-in Glossary Editor**: Slide-out panel to edit the glossary (`_Substitutions.txt`) directly within the interface, featuring basic original/translated text syntax highlighting.

#### 🧠 Translation Logic Handling
- **Batch Routing Mode**: When enabled, the program attempts to automatically modify the game's `Config.ini`, spoofing the Google endpoint and routing it to the local port to support concurrent multi-line batch translation. The config is automatically restored when the service is stopped.
- **Context Isolation (Anti-Bleed)**: Built-in local escape processing protects line breaks `[LF]` and game rich-text tags (e.g., `<T_0>`) before sending to the LLM. It forces the model to treat multi-line inputs as independent fragments, reducing hallucinations or logical contamination caused by the LLM trying to "auto-complete" the previous context.
- **Glossary Evolution (RAG)**: Automatically reads the specified glossary as context reference during translation and attempts to extract unrecorded nouns to append to the file.

#### 🚀 Concurrency & Service Control
- **Asynchronous Processing**: Underlying thread pool based on `httplib`, supporting 64~256 concurrent threads.
- **API Polling**: Supports entering multiple comma-separated API Keys, which the program will call in a round-robin manner during requests.
- **Dynamic Hot Reload**: Modify the model name, API Keys, or system prompts while the service is running. Configurations take effect immediately on the next request.

#### 🛡️ Monitoring & Fault Tolerance
- **HUD Mini-Window**: Switch to a minimalist HUD window featuring a simple 3-color status light (Green/Cyan/Red) to indicate the current working state and a Token consumption tracker.
- **Error Mapping & Timeouts**: Enforces a 10~40 second timeout mechanism for API requests to prevent game freezes caused by unresponsive long connections. Provides clear advice for common HTTP status codes (e.g., 401, 429).

---

### 🚀 Quick Start

#### Method 1: Standard Mode (Line-by-line processing)
1. Run this program, enter your API details, and click **Test Config**. Once passed, click **Start Service**.
2. Manually edit the `AutoTranslator/Config.ini` file in your game directory:
   ```ini
   [Service]
   Endpoint=CustomTranslate
   
   [Custom]
   Url=http://localhost:6800  # Ensure this port matches the one set in the GUI
   ```

#### Method 2: Batch Mode (Concurrent multi-line - Recommended for text-heavy UIs)
1. Check **📦 Batch Mode** on the main interface of this program.
2. Ensure you have selected the correct glossary (`.txt`) path in the interface, as the program relies on this path to reverse-locate the game's `Config.ini`.
3. Click **Start Service** (the program will automatically modify and hijack the game's configuration).
4. Run the game. When you exit this program, the modified game configuration will be automatically restored.

---

### 📂 Code Structure Overview

```text
src/
├── main.cpp                   # Application entry, UI switching & animation control
├── MainWindow.cpp/h           # Classic mode UI & business logic hub
├── ModernWindow.cpp/h         # Modern mode UI
├── TranslationServer.cpp/h    # HTTP server, API interaction & retry logic
├── XuaConfigHijacker.h        # Auto-modifier and restorer for game config
├── GlossaryManager.h          # Glossary file I/O & RAG injection logic
├── RegexManager.h             # Pre/Post regex processing
├── HudWindow.cpp/h            # HUD overlay & status light
├── ModernUI.h                 # Modern component library (built-in editor, delegates)
├── ConfigManager.cpp/h        # Configuration (config.ini) serialization
└── TokenManager.cpp/h         # Token consumption tracker
```

---

### 🛠️ Compilation & Development Guide

- **Requirements**: C++17 compatible compiler (e.g., MSVC 2019+, MinGW 8.1+), Qt 6.2.0+, CMake 3.16+.
- **Build Steps**:
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_PREFIX_PATH=/your/qt/path
  cmake --build . --config Release
  ```

> **Note to secondary developers**:
> To ensure perfect alignment of UI elements and the best visual experience, if you need to perform secondary development on the interface, please try to strictly limit the main window's pixel size to around `500 x 832`.

---

### 📦 Deployment & Packaging
- After generating the `.exe`, it is recommended to use `windeployqt` to collect the required Qt runtime dependency libraries.
- For a single-file release, virtualization tools like **Enigma Virtual Box** can be used to package all dependency environments into a single executable file.

---

### 📝 License
This project is open-sourced under the **MIT** License. You are free to use, modify, and distribute the code of this project, provided that the original author's copyright notice is retained.

---

> 📖 **中文版本请参阅**: [README.md](README.md)
