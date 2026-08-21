# Zync Programming Language & Toolchain

<p align="center">
  <strong>A modern, expressive, compiled systems programming language transpiled to high-performance C++17 and WebAssembly.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-Zync-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/Backend-C%2B%2B17%20%7C%20WASM-orange.svg" alt="Backend">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
  <img src="https://img.shields.io/badge/Platform-macOS%20%7C%20Linux-lightgrey.svg" alt="Platform">
</p>

---

## Overview

**Zync** combines high-level syntax ergonomics and modern developer experience with the bare-metal execution performance of ISO C++17 and the portability of WebAssembly. 

Designed around a **zero-cost abstraction** model, Zync lowers directly to efficient C++ AST structures and native binaries or WebAssembly modules (`.wasm`). It ships with a built-in multi-threaded parallel compiler, incremental dependency-graph cache, interactive REPL, and micro-benchmark test runner.

---

## Features

- **Modern Grammar & Ergonomics**: Go/Rust-inspired package hierarchy, grouped imports, type inference, lambdas, and closures.
- **Dual Target Compilation**:
  - **Native**: Optimized via `g++` / `clang++` with SIMD vectorization, CPU-tailored optimization, and Link-Time Optimization (LTO).
  - **WebAssembly**: Transpiled and linked to `.wasm`, `.js`, and `.html` using the Emscripten SDK (`em++`).
- **Autonomous Toolchain**:
  - **Incremental Caching**: Content and flag hash verification to prevent redundant compilation.
  - **Parallel Work Pool**: Multi-threaded job scheduling across physical CPU cores (`-j` / `--jobs`).
  - **Recursive Header Resolution**: Seamless interop with C++ standard libraries (`vector`, `list`, `stack`, `set`, `unordered_map`, etc.).
- **Expressive Language Semantics**:
  - **Algebraic Pattern Matching**: Exhaustive `match` expressions with multiple pattern branches, range tests, and conditional `when`/`if` guards.
  - **Trait & Record Polymorphism**: Dynamic dispatch and abstract interface contracts (`trait`) with concrete implementations (`impl`).
  - **Monadic Result Type**: Native `Result<T, E>` with `Ok(T)` and `Err(E)`.
  - **Compile-Time Execution**: `comptime` functions and expression blocks mapped to `constexpr`.
- **Integrated Unit Testing**: Zero-dependency test blocks with sub-millisecond execution reporting.

---

## Requirements

- **C++ Compiler**: GCC (>= 9.0) or Clang (>= 11.0) with C++17 support.
- **Build System**: GNU `make`.
- **WebAssembly (Optional)**: [Emscripten SDK](https://emscripten.org/) and [Node.js](https://nodejs.org/).

---

## Installation

```bash
# Clone the repository
git clone [https://github.com/your-username/zync.git](https://github.com/your-username/zync.git)
cd zync

# Compile the compiler toolchain
make clean && make

# (Optional) Install globally
sudo make install