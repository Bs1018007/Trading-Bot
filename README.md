 # Trading Bot 
 This project implements a high-performance C++ market data system that streams live Bybit USDT perpetual futures order books, parses them with SIMD-accelerated JSON, maintains in-memory order books, and publishes compact binary market data over Aeron IPC for ultra-low-latency downstream consumption. The system is designed to mimic real HFT market-data gateways, focusing on predictable latency, zero-copy parsing, and scalable symbol handling. 
 
 ## What This Project Does 
 
  ### At runtime, the system: 
    1.Fetches all available USDT perpetual trading symbols from Bybit (startup only) 
    2.Opens a WebSocket connection to Bybit’s public market data stream 
    3.Subscribes to order book updates for all symbols (batched) 
    4.Parses incoming JSON using simdjson 
    5.Updates an in-memory order book for each symbol 
    6.Extracts top-N bid/ask levels 
    7.Encodes the data using Simple Binary Encoding (SBE) 
    8.Publishes the binary messages via Aeron shared-memory IPC
    9.This allows trading strategies, analytics engines, or risk systems to consume market data with microsecond-level latency. 


 ## Installation & Build 
 1.Make the build script executable - chmod +x build.sh 
 2.Build the entire project - ./build.sh 
 3.Running the Application 
   - cd build
   - ./trading_bot