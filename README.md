<h1 align="center">🚀 Ultra-Low Latency Trading Bot</h1>

<p align="center">
  <b>C++ · WebSocket · simdjson · SBE · Aeron IPC</b>
</p>

---

## 📌 Overview

This project implements a **high-performance C++ market data system** that streams live **Bybit USDT perpetual futures order books**, parses them using **SIMD-accelerated JSON**, maintains **in-memory order books**, and publishes **compact binary market data** over **Aeron IPC** for ultra-low-latency downstream consumption.

The system is designed to **mimic real HFT market-data gateways**, focusing on predictable latency, zero-copy parsing, and scalable symbol handling.

---

## ⚙️ What This Project Does

At runtime, the system:

1. Fetches all available **USDT perpetual trading symbols** from Bybit *(startup only)*
2. Opens a **WebSocket connection** to Bybit’s public market data stream
3. Subscribes to **order book updates for all symbols** (batched)
4. Parses incoming JSON using **simdjson**
5. Updates an **in-memory order book** for each symbol
6. Extracts **top-N bid/ask levels**
7. Encodes the data using **Simple Binary Encoding (SBE)**
8. Publishes binary messages via **Aeron shared-memory IPC**
9. Enables trading strategies, analytics engines, or risk systems to consume market data with **microsecond-level latency**

---

## 🛠 Installation & Build

### 1️⃣ Make the build script executable
```bash
chmod +x build.sh

### 2️⃣ Build the entire project
```bash
./build.sh

- The build script automatically installs all dependencies, builds simdjson, Aeron, and SBE, and compiles the trading bot.

### ▶️ Running the Application
```bash
 - cd build
 -./trading_bot


 ⚡ Key Features

1. Live Bybit order book streaming (all USDT perpetual pairs)

2. SIMD-accelerated JSON parsing

3. Lock-free in-memory order books

4. Binary serialization using SBE

5. Ultra-low-latency IPC via Aeron

6. Designed like real HFT market-data gateways

7. Scalable, deterministic, and low-jitter processing



