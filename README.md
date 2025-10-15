## Redis Clone in C++ — Interview-Ready README

This project implements a minimal Redis-compatible server in modern C++. It supports core commands (e.g., `PING`, `SET`, `GET`) and the Redis Serialization Protocol (RESP). The codebase is structured for clarity and interview discussion, with explicit concurrency control via threads, mutexes, and condition variables.

### Tech Stack
- **Language**: C++17 (STL threads, `std::mutex`, `fstd::unique_lock`, `std::condition_variable`, `std::atomic`)
- **Build**: CMake
- **Networking**: POSIX sockets
- **OS**: Linux/macOS compatible (tested on macOS `darwin 24.6.0`)

### Repo Layout (key files)
- `src/Server.cpp`: process initialization, listener socket, connection lifecycle
- `src/connection.{h,cpp}`: per-connection I/O helpers
- `src/handlers.{h,cpp}`: command dispatch for `PING`/`SET`/`GET` (and related)
- `src/parser.{h,cpp}`: RESP decoder (handles partial reads/incremental parse)
- `src/resp_create.{h,cpp}` / `src/resp_send.{h,cpp}`: RESP encoders and send helpers
- `src/state.{h,cpp}`: shared key-value store and related state
- `src/background.{h,cpp}`: background tasks (e.g., expiry/housekeeping)
- `src/StreamHandler.{h,cpp}`: stream-oriented helpers
- `src/utils.{h,cpp}`: misc utilities

### High-Level Architecture
- **Accept Loop**: A main thread accepts incoming TCP connections and hands them off to connection handlers.
- **Command Handling**: Each connection reads bytes, feeds the RESP parser, then dispatches to command handlers that operate on shared state.
- **Shared State**: Central key-value store guarded by mutexes for correctness under concurrency.
- **Background Work**: Optional background thread(s) for periodic tasks (e.g., key expiry) coordinated via condition variables.

### Thorough Project Overview
This section walks through how data flows through the system, how threads are coordinated, and where key responsibilities live in the codebase.

1) Server startup and connection lifecycle
- The process starts in `src/Server.cpp`. It creates a TCP socket, sets `SO_REUSEADDR`, binds to port 6379, and starts listening.
- An expiry/background cleaner is started (`start_expiry_cleaner()`), then the server loops on `accept()`.
- For every client, a detached thread is spawned to handle that connection via `handleResponse(client_fd, StreamHandler_ptr)`.

2) Per-connection request handling
- In `src/connection.cpp`, `handleResponse` performs a blocking `recv()` on the client socket, feeds the buffer to the RESP parser, and dispatches commands.
- The parser (`src/parser.cpp`) implements RESP array decoding, carefully handling lengths and offsets to produce a `deque<string>` of tokens.
- Supported commands include: `PING`, `ECHO`, `SET [EX|PX]`, `GET`, `RPUSH`, `LPUSH`, `LRANGE`, `LLEN`, `LPOP` (count), `BLPOP` (blocking), `TYPE`, and stream commands `XADD`, `XRANGE`, `XREAD`/`XREAD BLOCK`.

3) Shared state and synchronization
- Global state lives in `src/state.cpp` and is declared in `src/state.h`:
  - `kv` (unordered_map<string,string>): string values
  - `expiry_map` (unordered_map<string,time_point>): expirations per key
  - `lists` (unordered_map<string,deque<string>>): list values
  - `clients_cvs` (unordered_map<int,condition_variable>): per-client condition variables for blocking ops
  - `blocked_clients` (unordered_map<string,deque<int>>): list of clients blocked per key (e.g., for `BLPOP`)
  - stream-related state and mutexes for stream commands (see `StreamHandler`)
- Corresponding mutexes exist for each shared structure (e.g., `kv_mutex`, `lists_mutex`, `blocked_clients_mutex`, etc.). All access to shared maps/deques is protected by the appropriate mutex.

4) Condition variables and blocking operations
- `BLPOP` uses `unique_lock<mutex>` on `lists_mutex`. If the list is empty:
  - The client fd is registered in `blocked_clients[key]` under `blocked_clients_mutex`.
  - The thread then waits on `clients_cvs[client_fd]` with either `wait()` or `wait_until()` (for timeouts) while holding `lists_mutex`.
  - When data becomes available (e.g., `LPUSH`/`RPUSH`), push handlers notify waiting clients via `clients_cvs[client_fd].notify_one()` outside the main list lock to avoid unnecessary contention.
- Timeouts are handled by removing the client from the blocked queue and returning a RESP null array.

5) Deadlock avoidance patterns
- Short critical sections: locks are held only around map/deque mutations and quick lookups.
- No blocking I/O while holding locks: for example, values are selected and locks released before performing `send()` operations.
- Consistent lock ordering: where multiple locks are taken (e.g., `kv_mutex` then `expiry_map_mutex`), the order is consistent across code paths.
- Condition-variable handoffs: producers (`LPUSH`/`RPUSH`) notify consumers (`BLPOP`) rather than spinning.

6) Expiry and key lifecycle
- `SET` supports optional expiry (`EX` seconds or `PX` milliseconds). Expiry timestamps are stored in `expiry_map` guarded by `expiry_map_mutex`.
- Reads (`GET`) check expiration: if expired, a cleanup path removes the key (under both `kv_mutex` and `expiry_map_mutex`). A background cleaner thread also assists with periodic eviction (see `src/background.*`).

7) RESP encoding/decoding and I/O helpers
- Parsing is centralized in `parser.cpp` and returns tokens for the dispatcher.
- Response creators live in `resp_create.*` (`create_resp_array`, etc.), while send helpers live in `resp_send.*` (`send_simple_string`, `send_bulk_string`, `send_array`, `send_null_array`, etc.). These centralize RESP-conformant replies and reduce duplication.

8) Streams support
- `StreamHandler` manages Redis-like streams. It supports `XADD` (append), `XRANGE` (read range), and `XREAD` including blocking reads with `BLOCK`.
- Stream state is guarded by `m_stream_mutex` with additional structures to track blocked stream readers (`blocked_streams`, etc.). Blocking semantics are similar to lists but scoped to streams.

9) Error handling and robustness
- Defensive checks around parsing (empty buffers, argument counts) avoid invalid memory access.
- Where timeouts apply, the code ensures blocked clients are cleaned up before returning.
- All network errors on `recv()` lead to connection close and thread exit to prevent leaks.

10) Performance considerations
- Per-connection threads keep the implementation simple and interview-friendly; it’s straightforward to reason about.
- Critical sections are kept short to reduce contention; notifications are precise (per-client CVs) to avoid thundering herds.
- RESP writes are kept simple and can be optimized further (buffered writes, writev) if needed.

11) Extending the server
- Add a new command by updating the dispatcher in `handleResponse` and implementing the handler in `handlers.*` (or a new module).
- Any new shared data structure should define a dedicated mutex and follow existing lock-ordering rules.
- For blocking commands, prefer the established pattern: register client, wait on a per-client CV, notify on producer paths.

### Concurrency Model & Deadlock Handling
- **Threads**:
  - Main acceptor thread for `accept()` and connection setup
  - Worker(s) for connection read/parse/execute, plus optional background maintenance thread
- **Shared Resources**: The key-value map and metadata (e.g., expirations) are shared across threads.
- **Synchronization Primitives**:
  - `std::mutex` / `std::unique_lock` to protect shared structures
  - `std::condition_variable` to notify between threads (e.g., new work, shutdown, or time-based events)
  - `std::atomic<bool>` flags to coordinate lifecycle (e.g., shutdown) without holding locks
- **Deadlock Avoidance**:
  - Single-writer lock style on the main store; keep critical sections short
  - Never perform blocking I/O while holding a mutex
  - Establish a consistent lock order for any multi-lock operations
  - Prefer notifying via `condition_variable` instead of busy waiting
  - Release locks before signaling long-running work and reacquire only when needed

### Key Interview Talking Points (Challenges & Trade-offs)
- **Protocol Parsing**: Handling partial reads and framing in RESP; validating types; avoiding buffer overreads.
- **Backpressure & Framing**: TCP coalescing means commands may arrive split or batched; parser must be incremental.
- **Concurrency Correctness**: Ensuring `SET`/`GET` mutual exclusion while maximizing parallelism across connections.
- **Deadlocks**: Avoid holding locks during I/O or callbacks; use lock ordering and condition variables for hand-offs.
- **Condition Variables**: Used for background tasks (e.g., expirations) and for graceful shutdown notifications.
- **Graceful Shutdown**: Atomics to signal stop, `condition_variable` to wake waiting threads, join worker threads cleanly.
- **Performance**: Minimizing lock contention with short critical sections; batching RESP writes; avoiding unnecessary copies.

### Build & Run
Prereqs: CMake and a C++17 compiler.

```bash
cmake -S . -B build
cmake --build build --config Release
./your_program.sh
```

`./your_program.sh` starts the server (see `src/Server.cpp`).

### Quick Test
Using `redis-cli`:

```bash
redis-cli -p 6379 PING
redis-cli -p 6379 SET foo bar
redis-cli -p 6379 GET foo
```

Using `nc` (manual RESP):

```bash
printf "*1\r\n$4\r\nPING\r\n" | nc localhost 6379
```

### How to Discuss This in an Interview
- **Design**: Main acceptor + per-connection workers; RESP parser; shared store with mutex; background thread for expirations.
- **Concurrency**: Mutex guards around store; `condition_variable` to notify between threads; atomics for lifecycle; strict lock ordering to prevent deadlocks.
- **Robustness**: Incremental parsing, input validation, and clear error paths; no blocking while holding locks.
- **Extensibility**: New commands plug into `handlers.*`; state evolution stays centralized in `state.*`.

### Original Challenge Reference
This codebase was bootstrapped from the CodeCrafters "Build Your Own Redis" challenge. See the challenge page at [codecrafters.io](https://codecrafters.io/challenges/redis) for staged requirements.
