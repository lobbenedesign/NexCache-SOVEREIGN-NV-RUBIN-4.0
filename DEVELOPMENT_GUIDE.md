# NexCache Development Guidelines

This document provides an overview of the design principles and coding standards for the NexCache engine.

## Core Philosophy
1. **Performance First**: Every microsecond counts. Avoid unnecessary locks, context switches, or garbage collection.
2. **Memory Density**: NexCache aims for the highest possible data density. Minimize per-entry overhead using structures like **NexDashTable**.
3. **Simplicity vs. Tuning**: Heuristics are preferred over complex configuration. The software should work optimally out-of-the-box.

## Coding Style
NexCache uses a modern C11/C++17 stack.
- **Comments**: Describe the *why*, not just the *what*. Use C-style `/* ... */` for blocks and `//` for brief notes.
- **Variables**: Use `snake_case` (e.g., `total_memory`).
- **Functions**: Use `camelCase` (e.g., `createEntryObject`).
- **Safety**: Prefer the **Arena Allocator** for memory management to prevent fragmentation.

## Licensing
All new source files must include the following header:
```c
/*
 * Copyright (c) Giuseppe Lobbene
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
```

## Testing
NexCache relies on a robust test suite:
- **Unit Tests**: Located in `src/unit/`. Test individual components (allocators, hash tables).
- **Integration Tests**: Located in `tests/`. Test the server as a whole (RESP compatibility, replication).

Use `./runtest` to execute the full test suite before submitting a Pull Request.

---
*NexCache — Rethink Memory, Accelerate AI.*
