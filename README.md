# XeBoostLM ⚡

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![OpenVINO](https://img.shields.io/badge/Inference-Intel%20OpenVINO%20GenAI-0071C5.svg)](https://github.com/openvinotoolkit/openvino)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078D6.svg)](https://github.com/balaragavan2007/XeBoostLM/releases)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)

**XeBoostLM** is a high-performance, lightweight local LLM inference CLI engine written in modern C++. Built directly on Intel's **OpenVINO™ GenAI** runtime, it is designed to maximize inference speed and power efficiency across **Intel® Core™ Ultra NPUs (AI Boost)**, **Intel® Arc™ / Xe iGPUs**, and modern multi-threaded CPUs.

---

## Key Features

- 🚀 **Native C++ Performance:** Zero Python runtime overhead during text generation; compiled directly with MSVC for native execution.
- 🧠 **Multi-Hardware Routing:** Target Intel NPU, discrete/integrated Arc GPUs, CPUs, or hybrid pipelines with manual hardware assignment.
- 💬 **Interactive Terminal Chat:** Real-time token streaming and multi-turn conversational history directly in your CLI.
- 🌐 **OpenAI-Compatible REST Server:** Built-in multi-threaded HTTP server with **Server-Sent Events (SSE)** streaming for direct integration with **Open WebUI**, **LibreChat**, and third-party frontend clients.
- 📦 **Dynamic Catalog & Caching:** Instant model retrieval with a 24-hour TTL cached remote catalog to keep models verified and up-to-date.
- 📊 **Built-In Benchmarking:** Measure model loading latency, **Time-to-First-Token (TTFT)**, and throughput (tokens/sec).

---

## Hardware Acceleration Support

| Target | Architecture / Driver | Best For | Quantization Support |
| :--- | :--- | :--- | :--- |
| **NPU** | Intel® AI Boost (Meteor Lake / Lunar Lake / Panther Lake / Arrow Lake / Wildcat Lake) | Ultra-low-power background execution | INT8, INT4 |
| **GPU** | Intel® Arc™ Graphics & Iris® Xe iGPU | High-throughput interactive chat | INT4, INT8, FP16 |
| **CPU** | Intel® Core™ / Xeon® (AVX2, AVX-512, AMX) | Larger parameter models & fallback | INT4, INT8, FP16 |
| **HYBRID** | Auto heterogeneous pipelining | Balanced throughput and host responsiveness | INT8 |

---

## Quickstart

### 1. Prerequisites

- **OS:** Windows 10/11 (64-bit)
- **Drivers:** Intel Graphics & NPU drivers installed
- **Model Downloader:** Python 3.10+ with `huggingface_hub`

Install the downloader dependency:

```cmd
pip install huggingface_hub
```

### 2. Download the Pre-Built Release

Download the latest portable archive from the [Releases](https://github.com/balaragavan2007/XeBoostLM/releases) page, extract it, and open a terminal in the extracted folder.

Verify the installation:

```cmd
xeboost info
```

---

## Command Reference

### System & Diagnostics

Display detected compute devices (NPU, iGPU, CPU) and available system RAM:

```cmd
xeboost info
```

### Model Management

Browse the remote catalog of verified OpenVINO models:

```cmd
xeboost list
```

Pull an optimized model into your local environment:

```cmd
:: Pull by catalog shortcut
xeboost pull smollm2-360m-int8

:: Or pull directly from an OpenVINO Hugging Face repository
xeboost pull OpenVINO/Qwen2.5-Coder-1.5B-Instruct-int8-ov
```

Force-refresh the cached remote model catalog from GitHub:

```cmd
xeboost update
```

Delete a local model to free up disk space:

```cmd
xeboost remove smollm2-360m-int8
```

### Interactive Chat (REPL)

Start a multi-turn conversation in your terminal with real-time token streaming:

```cmd
:: Run on CPU
xeboost chat smollm2-360m-int8 cpu

:: Run on Intel Arc iGPU
xeboost chat qwen2.5-1.5b-int8 gpu

:: Run on Intel NPU (AI Boost)
xeboost chat deepseek-r1-1.5b-int8 npu
```

Type `/bye` or `exit` inside the session to end the chat.

### Hardware Benchmarking

Evaluate generation latency, memory overhead, and throughput (tokens/sec):

```cmd
xeboost benchmark qwen2.5-1.5b-int8
```

### OpenAI-Compatible API Server

Launch the HTTP endpoint for web frontends:

```cmd
xeboost serve
```

The server listens on:

```text
http://localhost:11434
```

It provides standard endpoints:

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/v1/models` | Lists available models |
| `POST` | `/v1/chat/completions` | Chat completions, including `stream: true` |

#### Connecting to Open WebUI

1. Open **Settings → Connections → OpenAI API** in Open WebUI.
2. Set the API URL to:
   `http://localhost:11434/v1`
3. Enter any string as the API key, for example:
   `xeboost`
4. Select your pulled model tag, such as `smollm2-360m-int8:gpu`.
5. Start chatting.

---

## Building from Source

### Requirements

- **Visual Studio 2022** with MSVC C++17
- **CMake** 3.20 or newer
- **Intel® OpenVINO™ Toolkit** 2024.4+ installed at `C:\openvino`

### Build Steps

Clone the repository:

```cmd
git clone https://github.com/balaragavan2007/XeBoostLM.git
cd XeBoostLM
```

Create a build directory:

```cmd
mkdir build
cd build
```

Configure and build with Visual Studio:

```cmd
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

The compiled binary will be located at:

```text
build/Release/xeboost.exe
```

---

## Contributing

Contributions are welcome! Whether you want to add support for new OpenVINO model architectures, optimize streamer callbacks, improve device routing, or expand hardware targets, feel free to open a Pull Request.

1. Fork the project.
2. Create your feature branch:
   ```bash
   git checkout -b feature/NewFeature
   ```
3. Commit your changes:
   ```bash
   git commit -m "feat: add support for new feature"
   ```
4. Push to the branch:
   ```bash
   git push origin feature/NewFeature
   ```
5. Open a Pull Request.

---

## License

Distributed under the **Apache License 2.0**.

See the [`LICENSE`](LICENSE) file for more information.
