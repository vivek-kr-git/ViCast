# 🎥 ViCast-NIP

[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-FFDD00?style=flat-square&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/vivek.kr)

[![Build Status](https://img.shields.io/github/actions/workflow/status/yourrepo/vicast/build.yml?branch=main&style=flat-square)](https://github.com/yourrepo/vicast/actions) 
[![License](https://img.shields.io/github/license/yourrepo/vicast?style=flat-square)](LICENSE) 
[![NDI SDK](https://img.shields.io/badge/NDI-SDK-blue?style=flat-square)](https://www.ndi.tv/sdk/) 
[![DeckLink SDK](https://img.shields.io/badge/DeckLink-SDK-black?style=flat-square)](https://www.blackmagicdesign.com/support/family/capture-and-playback) 
[![C++](https://img.shields.io/badge/C++-17-blue?style=flat-square)](https://isocpp.org/)

ViCast is a high-performance C++ broadcast gateway for real-time video routing between **NDI® network video**, **GPU desktop capture**, and **SDI broadcast hardware**.  
Designed for 24/7 production environments, ViCast delivers ultra-low latency video processing using SIMD-accelerated pipelines, lock-free frame pools, and multithreaded architecture.  
It automatically manages **signal routing, failover, synchronization, and broadcast output**, making it ideal for studio automation, live production, and IP video infrastructure.

---

## 📌 Table of Contents

- [🧠 Architecture Overview](#-architecture-overview)  
- [⚡ Core Features](#-core-features)  
- [🔄 Input Failover Logic](#-input-failover-logic)  
- [🔄 Video Routing Pipeline](#-video-routing-pipeline)  
- [📊 Output Capability Matrix](#-output-capability-matrix)  
- [⚙️ Supported Pixel Formats](#-supported-pixel-formats)  
- [⏱ Timing & Synchronization](#-timing--synchronization)  
- [🚀 Performance Design](#-performance-design)  
- [🧩 Internal Processing Pipeline](#-internal-processing-pipeline)  
- [📡 Broadcast Integration](#-broadcast-integration)  
- [💻 System Requirements](#-system-requirements)  
- [⚙️ Configuration](#-configuration)  
- [Key Parameters](#key-parameters)  
- [📺 Supported DeckLink Video Modes](#-supported-decklink-video-modes)  
- [🛠️ Building the Project](#-building-the-project)  
- [📄 License](#-license)  
- [🙏 Acknowledgements](#-acknowledgements)  
- [🎬 Master Signal & Failover Diagram](#-master-signal--failover-diagram)

---

## 🧠 Architecture Overview

### INPUT
- GPU Desktop Capture (DXGI duplication)  
- NDI Primary / Backup

### PROCESS
- libyuv scaling  
- Fill / Key generation  
- Frame pool (zero allocation)

### OUTPUT
- NDI Fill  
- NDI Key  
- NDI Embedded  
- DeckLink SDI (V210 10-bit)

### TIMING
- DeckLink Genlock (if present)  
- Software frame pacing fallback

---

## ⚡ Core Features

- **Intelligent Input Routing**  
  Prioritizes local GPU Desktop Duplication. If unavailable, switches to NDI sources automatically.

- **Automatic NDI Failover**  
  Monitors Primary NDI source. If signal drops beyond a configurable threshold, switches to Backup seamlessly.

- **Zero-Overhead Desktop Capture**  
  Uses DXGI API for 60fps capture with minimal CPU overhead and direct GPU memory access.

- **Multi-Channel NDI Output**  
  Simultaneously broadcasts Fill, Key (Alpha), and Embedded RGBA streams.

- **DeckLink SDI Integration**  
  Professional 10-bit V210 output with optional hardware Genlock / Reference lock.

- **Thread-Safe Telemetry**  
  Real-time console dashboard for routing, FPS, dropped frames, clock drift, and system health.

---

## 🔄 Input Failover Logic

Priority Order:

1. GPU Desktop Capture  
2. NDI Primary  
3. NDI Backup

![ViCast Failover State Machine](docs/vicast-failover.svg)

---

## 🔄 Video Routing Pipeline

The engine dynamically switches between Desktop Capture, NDI Primary, and NDI Backup sources.  
Failover engages automatically when frames are dropped beyond `flapThreshold`.

Priority Table:

| Priority | Source               |
|----------|--------------------|
| 1        | GPU Desktop Capture |
| 2        | NDI Primary         |
| 3        | NDI Backup          |

---

## 📊 Output Capability Matrix

| Feature                     | NDI Fill | NDI Key | NDI Embedded | SDI (DeckLink) |
|------------------------------|:--------:|:-------:|:------------:|:--------------:|
| Color                        | ✓        | ✗       | ✓            | ✓              |
| Alpha                        | ✗        | ✓       | ✓            | Optional       |
| 10-bit                       | ✓        | ✓       | ✓            | ✓              |
| Hardware Sync                | Network  | Network | Network      | ✓              |
| Broadcast Switcher Compatible | ✓        | ✓       | ✓            | ✓              |

---

## ⚙️ Supported Pixel Formats

| Format       | Bit Depth | Usage             |
|--------------|-----------|-----------------|
| RGBA         | 8-bit     | GPU capture      |
| UYVY         | 8-bit     | NDI pipeline     |
| V210         | 10-bit    | SDI output       |
| YUV 4:2:2    | 10-bit    | DeckLink hardware|

---

## ⏱ Timing & Synchronization

### Hardware Genlock (Preferred)
- Locks output to external reference if present (Blackburst, Tri-Level, or House Ref).  
- Ensures frame-accurate sync across devices.

### Internal Clock (Fallback)
- High-precision software timer if no Genlock detected.  
- Maintains stable frame pacing for NDI and single-device SDI pipelines.

---

## 🚀 Performance Design

- SIMD acceleration via libyuv  
- Lock-free frame pools  
- Zero dynamic allocation in hot paths  
- Multithreaded pipeline  
- GPU-assisted capture  

**Typical Latency:**

| Pipeline        | Latency      |
|-----------------|-------------|
| Desktop → NDI   | ~1 frame    |
| NDI → SDI       | ~1–2 frames |
| Desktop → SDI   | ~1 frame    |

---

## 🧩 Internal Processing Pipeline

**INPUT**  
- GPU Desktop Capture (DXGI)  
- NDI Primary / Backup  

**PROCESS**  
- Frame pool (zero allocation)  
- libyuv scaling & color conversion  
- Fill / Key generation  

**OUTPUT**  
- NDI Fill  
- NDI Key  
- NDI Embedded  
- DeckLink SDI (V210)

---

## 📡 Broadcast Integration

| System                       | Compatibility |
|-------------------------------|:------------:|
| NDI Production workflows       | ✓            |
| SDI Switchers                  | ✓            |
| Graphics Engines               | ✓            |
| IP Video Infrastructure        | ✓            |
| Studio Automation              | ✓            |

---

## 💻 System Requirements

**Operating System**  
- Windows 10 / 11 (64-bit)

**Hardware (Optional)**  
- Blackmagic DeckLink card for SDI output

**Dependencies**  

| Dependency                 | Purpose               |
|-----------------------------|--------------------|
| NDI® SDK                    | Network video transport |
| Blackmagic Desktop Video    | DeckLink drivers      |
| Visual Studio 2022 (MSVC v143)| Build toolchain     |
| libyuv                      | SIMD video processing |

---

## ⚙️ Configuration

- Controlled via `config.json` in `AppData/Local/ViCast` or executable folder.  
- **Blank = Disabled** design, e.g., `"ndiOutputEmbedded": ""` disables the sender.

**Example config.json**

```json
{
  "enableDesktopCapture": true,
  "enableNdiInput": true,
  
  "ndiInputPrimary": "STUDIO-PC (Mix 1)",
  "ndiInputBackup": "STUDIO-PC (Mix 2)",
  
  "flapThreshold": 5,
  
  "ndiOutputFill": "ViCast Fill",
  "ndiOutputKey": "ViCast Key",
  "ndiOutputEmbedded": "ViCast NDI",
  
  "outputWidth": 1920,
  "outputHeight": 1080,
  "outputFpsN": 25,
  "outputFpsD": 1,
  
  "forceProgressive": true,
  "forceInterlaced": false,
  
  "ndiHighestQuality": true,
  "clockVideo": true,
  "clockAudio": true,
  "pixelFormat": "v210",
  
  "enableDeckLink": false,
  "decklinkDevices": [
    {
      "name": "DeckLink 8K Pro",
      "outputMode": "1080p25",
      "keyOutput": true
    }
  ],
  "verboseLogging": true
}```

## Key Parameters

- **outputFpsN / outputFpsD**: frame rate fraction (e.g., 60000/1000 = 60fps)  
- **enableDesktopCapture**: overrides NDI inputs if true  
- **flapThreshold**: consecutive dropped frames before failover  
- **enableDeckLink**: enables SDI output (V210 10-bit)  

---

## 🛠️ Building the Project

## 1⃣ Clone the repository 
```bash
git clone https://github.com/vivek-kr-git/ViCast

## 2️⃣ Install Dependencies

Install the following:

- NDI SDK  
- DeckLink SDK  
- libyuv  

Add them to Visual Studio:

- **Project Properties** → **Include Directories**  
- **Project Properties** → **Linker Dependencies**

## 3️⃣ Build

- **Configuration:** Release  
- **Platform:** x64  

⚠️ Debug builds may not sustain real-time 60fps performance due to disabled compiler optimizations.

---

## 📊 Performance Characteristics

ViCast is optimized for real-time broadcast workloads.  
Key techniques include:

- Lock-free frame pools  
- SIMD processing  
- Zero dynamic allocation in hot paths  
- Multithreaded pipeline  
- Hardware synchronization  

**Typical latency:** ~1–2 frames (depending on hardware and pipeline configuration)

---

## 📄 License

ViCast is licensed under the Apache License 2.0. See LICENSE for details.

---

## 🙏 Acknowledgements

- NDI® by Vizrt  
- DeckLink SDK by Blackmagic Design  
- libyuv by Google  

See THIRDPARTY.md for full copyright and licensing information.

---

## ☕ Support

If you like **ViCast-NIP** and want to support development, you can buy me a coffee:

[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-FFDD00?style=flat-square&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/vivek.kr)

---

## 📺 Supported DeckLink Video Modes

### Standard Definition (SD)
| Resolution | Frame Rate | Scan Type   | DeckLink Mode   |
| ---------- | ---------- | ----------- | --------------- |
| 720×486    | 29.97      | Interlaced  | bmdModeNTSC     |
| 720×486    | 23.98      | Progressive | bmdModeNTSC2398 |
| 720×486    | 29.97      | Progressive | bmdModeNTSCp    |
| 720×576    | 25         | Interlaced  | bmdModePAL      |
| 720×576    | 25         | Progressive | bmdModePALp     |

### HD 720 (Progressive Only)
| Resolution | Frame Rate | Scan Type   | DeckLink Mode     |
| ---------- | ---------- | ----------- | ----------------- |
| 1280×720   | 50         | Progressive | bmdModeHD720p50   |
| 1280×720   | 59.94      | Progressive | bmdModeHD720p5994 |
| 1280×720   | 60         | Progressive | bmdModeHD720p60   |

### HD 1080 Interlaced
| Resolution | Frame Rate | Scan Type  | DeckLink Mode      |
| ---------- | ---------- | ---------- | ------------------ |
| 1920×1080  | 25         | Interlaced | bmdModeHD1080i50   |
| 1920×1080  | 59.94      | Interlaced | bmdModeHD1080i5994 |
| 1920×1080  | 60         | Interlaced | bmdModeHD1080i6000 |

### HD 1080 Progressive
| Resolution | Frame Rate | Scan Type   | DeckLink Mode      |
| ---------- | ---------- | ----------- | ------------------ |
| 1920×1080  | 23.98      | Progressive | bmdModeHD1080p2398 |
| 1920×1080  | 24         | Progressive | bmdModeHD1080p24   |
| 1920×1080  | 25         | Progressive | bmdModeHD1080p25   |
| 1920×1080  | 29.97      | Progressive | bmdModeHD1080p2997 |
| 1920×1080  | 30         | Progressive | bmdModeHD1080p30   |
| 1920×1080  | 50         | Progressive | bmdModeHD1080p50   |
| 1920×1080  | 59.94      | Progressive | bmdModeHD1080p5994 |
| 1920×1080  | 60         | Progressive | bmdModeHD1080p6000 |

### Legacy 2K Film (1556 lines)
| Resolution | Frame Rate | Scan Type   | DeckLink Mode |
| ---------- | ---------- | ----------- | ------------- |
| 2048×1556  | 23.98      | Progressive | bmdMode2k2398 |
| 2048×1556  | 24         | Progressive | bmdMode2k24   |
| 2048×1556  | 25         | Progressive | bmdMode2k25   |

### 2K DCI
| Resolution | Frame Rate | Scan Type   | DeckLink Mode    |
| ---------- | ---------- | ----------- | ---------------- |
| 2048×1080  | 23.98      | Progressive | bmdMode2kDCI2398 |
| 2048×1080  | 24         | Progressive | bmdMode2kDCI24   |
| 2048×1080  | 25         | Progressive | bmdMode2kDCI25   |
| 2048×1080  | 29.97      | Progressive | bmdMode2kDCI2997 |
| 2048×1080  | 30         | Progressive | bmdMode2kDCI30   |
| 2048×1080  | 50         | Progressive | bmdMode2kDCI50   |
| 2048×1080  | 59.94      | Progressive | bmdMode2kDCI5994 |
| 2048×1080  | 60         | Progressive | bmdMode2kDCI60   |

### 4K UHD
| Resolution | Frame Rate | Scan Type   | DeckLink Mode      |
| ---------- | ---------- | ----------- | ------------------ |
| 3840×2160  | 23.98      | Progressive | bmdMode4K2160p2398 |
| 3840×2160  | 24         | Progressive | bmdMode4K2160p24   |
| 3840×2160  | 25         | Progressive | bmdMode4K2160p25   |
| 3840×2160  | 29.97      | Progressive | bmdMode4K2160p2997 |
| 3840×2160  | 30         | Progressive | bmdMode4K2160p30   |
| 3840×2160  | 50         | Progressive | bmdMode4K2160p50   |
| 3840×2160  | 59.94      | Progressive | bmdMode4K2160p5994 |
| 3840×2160  | 60         | Progressive | bmdMode4K2160p60   |

### 4K DCI
| Resolution | Frame Rate | Scan Type   | DeckLink Mode    |
| ---------- | ---------- | ----------- | ---------------- |
| 4096×2160  | 23.98      | Progressive | bmdMode4kDCI2398 |
| 4096×2160  | 24         | Progressive | bmdMode4kDCI24   |
| 4096×2160  | 25         | Progressive | bmdMode4kDCI25   |
| 4096×2160  | 29.97      | Progressive | bmdMode4kDCI2997 |
| 4096×2160  | 30         | Progressive | bmdMode4kDCI30   |
| 4096×2160  | 50         | Progressive | bmdMode4kDCI50   |
| 4096×2160  | 59.94      | Progressive | bmdMode4kDCI5994 |
| 4096×2160  | 60         | Progressive | bmdMode4kDCI60   |

### 8K UHD
| Resolution | Frame Rate | Scan Type   | DeckLink Mode      |
| ---------- | ---------- | ----------- | ------------------ |
| 7680×4320  | 23.98      | Progressive | bmdMode8K4320p2398 |
| 7680×4320  | 24         | Progressive | bmdMode8K4320p24   |
| 7680×4320  | 25         | Progressive | bmdMode8K4320p25   |
| 7680×4320  | 29.97      | Progressive | bmdMode8K4320p2997 |
| 7680×4320  | 30         | Progressive | bmdMode8K4320p30   |
| 7680×4320  | 50         | Progressive | bmdMode8K4320p50   |
| 7680×4320  | 59.94      | Progressive | bmdMode8K4320p5994 |
| 7680×4320  | 60         | Progressive | bmdMode8K4320p60   |

### 8K DCI
| Resolution | Frame Rate | Scan Type   | DeckLink Mode    |
| ---------- | ---------- | ----------- | ---------------- |
| 8192×4320  | 23.98      | Progressive | bmdMode8kDCI2398 |
| 8192×4320  | 24         | Progressive | bmdMode8kDCI24   |
| 8192×4320  | 25         | Progressive | bmdMode8kDCI25   |
| 8192×4320  | 29.97      | Progressive | bmdMode8kDCI2997 |
| 8192×4320  | 30         | Progressive | bmdMode8kDCI30   |
| 8192×4320  | 50         | Progressive | bmdMode8kDCI50   |
| 8192×4320  | 59.94      | Progressive | bmdMode8kDCI5994 |
| 8192×4320  | 60         | Progressive | bmdMode8kDCI60   |

---
