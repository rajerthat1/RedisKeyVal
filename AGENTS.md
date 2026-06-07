# AGENTS.md — Project Guide for AI Coding Assistants

## Project Overview
A Redis-compatible key-value store built from scratch in C++20 to explore
systems programming, storage engines, and distributed consensus.

## Build & Run
```bash
# Build
cmake -B build && cmake --build build

# Run server
./build/kv-server

# Test with redis-cli
redis-cli PING
redis-cli SET name Alice
redis-cli GET name
```

## Project Structure
```
src/
  main.cpp                    — Entry point, wires everything together
  protocol/resp.h/cpp         — RESP parser/serializer (Command, Reply, RESPReader)
  server/server.h/cpp         — epoll TCP event loop (Server class)
  server/connection.h/cpp     — Per-client state (Connection struct)
  storage/storage.h           — IStorageEngine abstract interface
  storage/in_memory_storage.h/cpp — InMemoryEngine (unordered_map + shared_mutex)
  storage/wal_engine.h/cpp    — WalEngine (decorator: wraps IStorageEngine + disk log)
  commands/handler.h/cpp      — Command dispatch (PING, SET, GET, DEL, EXISTS)
tests/
  test_storage.cpp
  test_resp.cpp
CMakeLists.txt                — C++20, -O2 -Wall -Wextra -g, links pthread
```

## Architecture

### Event Loop Pattern
Single-threaded epoll-based server. One thread handles all clients using
non-blocking I/O. `epoll_wait` sleeps until any socket has data ready.

Flow per client event:
```
read(fd) → RESPReader::feed() → RESPReader::try_parse() → handle_command()
→ serialize(reply) → append to write_buf → write() on EPOLLOUT
```

### Storage Layer
`IStorageEngine` is the abstract interface. All engines implement it:
- `InMemoryEngine` — unordered_map + shared_mutex (concurrent readers)
- `WalEngine` — Decorator pattern; logs mutating ops to disk before applying

### Wire Protocol
RESP (Redis Serialization Protocol). Commands are arrays of bulk strings.
Replies are one of: Simple, Bulk, Nil, Integer, Error.

## Code Style
- C++20 standard
- 2-space indentation
- `#pragma once` in headers
- snake_case for functions/variables, PascalCase for classes
- `override` keyword on all overrides
- RAII for resource management (locks, file descriptors)
- Prefer `std::optional` over sentinel values
- Use `std::variant` + `std::visit` for tagged unions

## Key Conventions
- `I` prefix on interfaces (e.g. `IStorageEngine`)
- `namespace reply` for reply structs
- `try_*` prefix on parsing functions that can fail
- Static helper functions for file-local logic

## Testing
```bash
# Tests are not yet set up — current test files are placeholders.
```

## Common Tasks
- **Add a new command:** Add branch in `handler.cpp` → add any new reply type
  in `resp.h` → implement storage method in `IStorageEngine` + all engines
- **Add new storage engine:** Implement `IStorageEngine` → swap in `main.cpp`
- **Add new reply type:** Add struct in `resp.h` `namespace reply` → add branch
  in `serialize()` in `resp.cpp`

## Rules for AI Agents
1. Read the relevant header first before editing implementation files
2. Check both abstract interface (`storage.h`) and all implementations before
   adding/modifying methods
3. Run `cmake -B build && cmake --build build` after any changes
4. Test with `redis-cli` commands (not automated tests — yet)
5. Do not add comments unless asked — the code should be self-documenting
