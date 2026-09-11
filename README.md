<div align="center">
  <h1> ⚡XeBoostLM</h1>
  <p><b>A high-performance, zero-bloat local AI inference engine built in pure C++ for Intel hardware.</b></p>

  [![Platform: Windows](https://img.shields.io/badge/Platform-Windows-blue.svg)](#)
  [![C++17](https://img.shields.io/badge/Language-C++17-00599C.svg)](#)
  [![Framework: OpenVINO](https://img.shields.io/badge/Powered_by-OpenVINO-purple.svg)](#)
  [![API: OpenAI](https://img.shields.io/badge/API-OpenAI_Compliant-green.svg)](#)
</div>

---

> **What is XeBoostLM?**  
> It is a standalone C++ executable that lets you run powerful AI models completely offline on your own hardware. No Python environments, no Docker containers, and no complex setups. Just download, pull a model, and chat.

## ✨ The Engineering Journey & Features

XeBoostLM wasn't just pieced together—every subsystem was custom-engineered to solve the biggest pain points in local AI:

- **Bare-Metal C++ Core:** We stripped away the heavy Python wrappers to build a raw C++ pipeline using OpenVINO GenAI. The result? Maximum memory efficiency and fast execution.
- **Dynamic Hardware Routing (NPU/GPU/CPU):** We engineered a custom parser that reads model tags (like `:npu` or `:gpu`) to route neural network computations to your Intel Core Ultra NPU or Arc Graphics—bypassing the CPU when requested.
- **Persistent Disk Caching:** OpenVINO models can suffer from graph compilation delays on startup. We engineered a persistent caching system that writes compilation blobs to disk, resulting in significantly faster warm starts on future loads.
- **Custom OpenAI-Compliant Server:** Instead of relying on external API frameworks, we built a multi-threaded REST API server using `httplib`. It features native Server-Sent Events (SSE) streaming and real-time telemetry (tokens/second) that plugs directly into UIs like Open WebUI.
- **Integrated CLI Downloader:** Users shouldn't have to navigate Hugging Face to hunt down IR files. We built `xeboost pull`, an orchestrator that securely fetches, validates, and sets up OpenVINO models with a single command.

---

## 💻 Hardware Support

XeBoostLM is heavily optimized for modern Intel architectures:

- **NPU (Neural Processing Unit):** Intel Core Ultra (Series 1 & 2)
- **iGPU / dGPU:** Intel Arc Graphics, Intel Iris Xe
- **CPU:** Intel Core 11th Gen and newer (AVX2 / AVX-512)

---

## 🚀 Getting Started (For Beginners)

Never run a local AI model before? Follow these 3 easy steps.

### Step 1: Download the Engine

1. Go to the [Releases page](../../releases) on this repository.
2. Download `xeboost-v0.1.0-windows-x64.zip`.
3. Extract the folder anywhere on your PC.
4. Open Command Prompt or PowerShell in that folder.

### Step 2: Pull a Model

You need an AI model to chat with. We have pre-configured, optimized models. Run this command to download a recommended fast model:

```bash
xeboost pull qwen2.5-0.5b-int8
```

> **Note:** This requires Python and `huggingface_hub` installed on your system to handle the download.
>
> ```bash
> pip install huggingface_hub
> ```

**Other built-in models you can pull:**

- `qwen2.5-1.5b-int8` — Better reasoning, slightly larger.
- `phi-3-mini-int4` — Microsoft's highly capable compact model.

### Step 3: Chat in Your Terminal

Test the model directly in your command line:

```bash
xeboost run qwen2.5-0.5b-int8 --device NPU
```

> If you don't have an NPU, change `--device NPU` to `--device CPU` or `--device GPU`.

---

## 🌍 Connecting to a Web UI (Like ChatGPT)

Want a beautiful web interface instead of a terminal? XeBoostLM acts as a backend for [Open WebUI](https://docs.openwebui.com/).

### 1. Start the XeBoostLM server

```bash
xeboost serve
```

### 2. Open Open WebUI

Open Open WebUI in your browser.

### 3. Configure the OpenAI connection

Go to:

**Settings → Connections → OpenAI API**

Set the following:

- **API Base URL:** `http://localhost:11434/v1`
- **API Key:** `xeboost`

Click **Save**.

You can now select your models (for example, `qwen2.5-0.5b-int8:npu`) directly from the web interface dropdown.

---

## 📊 Built-in Hardware Benchmarking

Curious how much faster your GPU or NPU is compared to your CPU? XeBoostLM includes an automated benchmarking tool to test:

- Time to First Token (TTFT)
- Decode speed
- Hardware-specific inference performance

Run:

```bash
xeboost benchmark qwen2.5-0.5b-int8
```

### Example Output

```text
=========================================================
 XeBoostLM Hardware Benchmark: local_models/qwen2.5-0.5b-int8
=========================================================
Device    Load Time      TTFT (ms)      Speed (tok/s)
---------------------------------------------------------
CPU       0.42s          38.50          41.20
GPU       0.85s          24.10          68.40
NPU       0.51s          19.80          52.10
=========================================================
```

---

## 🛠️ Building from Source (For Developers)

Want to compile XeBoostLM yourself?

### Requirements

- Windows 11
- MSVC (Visual Studio 2022)
- CMake 3.20+
- OpenVINO C++ 2024.4+ SDK

### Build

```bash
git clone https://github.com/YOUR_USERNAME/XeBoostLM.git
cd XeBoostLM

mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Your compiled binary will be available at:

```text
build/Release/xeboost.exe
```

---

## 📝 License

This project is open-source and licensed under the **Apache 2.0 License**.

See the [`LICENSE`](LICENSE) file for details.
