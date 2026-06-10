# MPMC with Memory Management

Multi-producer multi-consumer queue with OS-backed memory management for fixed-size exchange protocol messages (NASDAQ ITCH, OUCH, NSE MTBT, NNF). Bypasses the C++ default allocator via `mmap`/`VirtualAlloc`, with x86_64 and ARM64 selected at compile time.

## How to Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
./mpmc_memory_management
```
