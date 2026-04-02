# Raspberry Pi 4 V4L2 Camera 3A Pipeline (C++)

This project implements a high-performance, multi-threaded camera pipeline on Raspberry Pi 4 using raw V4L2 (Video4Linux2) ioctl commands. It features Zero-Copy DMA memory transfer and a custom hardware-aligned 3A (Auto Exposure / Auto White Balance) closed-loop control algorithm.

## 🚀 Key Technical Highlights (技術亮點)

* **Direct V4L2 Driver Interfacing**: Bypassed high-level libraries (like OpenCV) to directly manipulate `/dev/v4l-subdev*` and `/dev/video*` nodes using C++ OOP encapsulation.
* **Zero-Copy Memory Architecture**: Implemented DMA-BUF to stream frames directly from the Camera Sensor to the ISP without CPU intervention, achieving zero-copy memory flow.
* **Modern C++ Asynchronous Multithreading**:
  * Decoupled the main frame-fetching loop, Thermal monitoring, and 3A algorithms into independent threads.
  * Utilized `std::atomic` for cache-coherent flag management, avoiding heavy mutex locks.
* **Frame-Dropping Ring Buffer**: Designed a custom Producer-Consumer ring buffer with `std::condition_variable` that automatically drops outdated frames (Meta Payload), guaranteeing the 3A algorithm always processes the freshest data without stalling the main 30-FPS loop.
* **Hardware-Aligned 3A Control System**:
  * Parsed Broadcom's raw `bcm2835_isp_stats` struct directly from kernel headers.
  * Implemented a dual-stage PID controller for Auto Exposure (adjusting Exposure Time first, then Analog Gain) and Manual/Auto White Balance based on the Gray World Assumption.

## 🛤️ Project Evolution (專案演進史)

This project was built iteratively to ensure stability and understand the lowest-level bottlenecks. The repository contains the entire evolution of the architecture:

1. **`main.c` (Stage 1)**: Basic V4L2 raw capture using standard memory mapping (MMAP).
2. **`main_isp.c` (Stage 2)**: Integrated Broadcom ISP (`/dev/video13`, `/dev/video14`) for format conversion (Bayer to RGB).
3. **`main_isp_dma.c` (Stage 3)**: Implemented Zero-Copy DMA-BUF to eliminate CPU memory copying between the Sensor node and ISP node.
4. **`main_isp_dma_multi.cpp` (Final Stage)**: Completely refactored into a Modern C++ (C++14) Object-Oriented architecture, introducing multi-threading, Ring Buffers, and the custom 3A closed-loop control system.

## ⚙️ Software Architecture

1. **Main Thread (Producer)**: High-speed V4L2 DQBUF/QBUF loop handling DMA passing.
2. **Thermal Thread (Background)**: Monitors RPi CPU temperature via `/sys/class/thermal/` and dynamically throttles camera FPS if overheating occurs.
3. **3A Worker Thread (Consumer)**: Parses ISP metadata, calculates luminance/color errors, and writes new Exposure/Gain registers back to the sensor.


## 🛠️ Environment
* Hardware: Raspberry Pi 4 Model B
* OS: Raspberry Pi OS (Linux Kernel 6.x)
* Language: C++11 / C++14
