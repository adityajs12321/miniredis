# miniredis

A Redis-compatible in-memory key-value server written in C++20, with support for the Redis
wire protocol (RESP).

## Features

- **RESP2 protocol**: incremental parser and encoder that handle partial reads and
  pipelined commands, with limits on bulk length, array length, line length and nesting
  depth to guard against hostile input.
- **Commands**: `PING`, `GET`, `SET` (with `EX` / `PX`), `DEL`, `EXPIRE`, `TTL`.
- **Networking**: `poll()`-based TCP server that hands requests to a fixed-size worker
  thread pool.
- **LRU eviction**: O(1) LRU cache that evicts the least recently used key once the
  configured capacity is reached.
- **Key expiration**: expired keys are dropped lazily when accessed and actively by a
  background reaper thread that walks a time-ordered expiry index.
- **Graceful shutdown**: `SIGINT` / `SIGTERM` stop the server cleanly.
- **Tested**: 113 GoogleTest cases covering the parser, store, reaper, thread pool,
  command dispatch and the TCP server.

## Install and build

Requirements:

- A C++20 compiler (GCC 11+, Clang 14+ or Apple Clang 14+)
- CMake 3.20+
- A POSIX system (Linux or macOS)
- Internet access on the first configure, to download GoogleTest

```sh
git clone https://github.com/adityajs12321/miniredis.git
cd miniredis
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run the tests:

```sh
ctest --test-dir build
```

Start the server (listens on `127.0.0.1:6379` by default; `--help` lists all options):

```sh
./build/miniredis --port 6379 --threads 8 --capacity 1000000
```
