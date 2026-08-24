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

**Zync** is a modern, expressive, compiled programming language designed to provide a simpler and more approachable developer experience while retaining the performance and ecosystem of C++.

Zync is transpiled to **C++17** for native compilation and can also target **WebAssembly** for portable web applications.

The primary goal of Zync is not to replace C++, but to make the **C++ ecosystem easier to learn and use**.

Instead of requiring developers to learn the complexity of modern C++ syntax, templates, build systems, memory management patterns, and framework-specific APIs, Zync provides a modern language layer with simpler syntax while still allowing developers to interact with existing C++ libraries.

```text
                    Zync Application
                           │
                           ▼
                     Zync Compiler
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
          C++17 Backend            WebAssembly
              │                         │
              ▼                         ▼
        clang++ / g++                 em++
              │                         │
              ▼                         ▼
        Native Binary              .wasm / .js
```

Zync follows a **zero-cost abstraction** philosophy wherever possible. High-level Zync constructs are lowered into efficient C++ structures and compiled using established native toolchains.

---

## Goals

Zync is built around several primary goals:

- **Make C++ easier to learn**
- **Provide modern and expressive syntax**
- **Reuse the existing C/C++ ecosystem**
- **Provide native-level performance**
- **Reduce boilerplate when working with C++ libraries**
- **Provide a simple and consistent toolchain**
- **Support native and WebAssembly targets**
- **Preserve interoperability with existing C++ code**

The core philosophy is:

> **Learn Zync once, use the C++ ecosystem everywhere.**

Zync therefore focuses on improving the programming experience instead of rebuilding an entirely new native library ecosystem from scratch.

---

## Features

### Modern Grammar & Ergonomics

Zync provides a modern syntax inspired by languages such as Go and Rust while retaining a familiar programming model for developers coming from C/C++.

Features include:

- Package hierarchy
- Grouped imports
- Type inference
- Explicit type annotations
- Lambdas
- Closures
- Pattern matching
- Traits and implementations
- Result-based error handling
- Compile-time execution

Example:

```zync
pkg main

fn main() -> void {
    var name = "Zync"
    println("Hello, " + name)
}
```

---

### C++ Ecosystem Integration

One of Zync's main goals is to make existing C++ libraries easier to use.

Instead of creating a completely new library ecosystem, Zync can expose C++ libraries through Zync-friendly wrappers.

For example, a web application using the **Crow** C++ web framework can be written as:

```zync
pkg main

import (
    std,
    "dependencies/wrapper/crow/crow.hpp"
)

fn main() -> void {
    var app = ZyncCrow::App()

    app.html("/user/<string>", (username: string) -> string => {
        return ZyncCrow::render("index.html", {
            "title": "Halaman Profil",
            "name": username,
            "role": "Fullstack Engineer"
        })
    })

    app.run(8080)
}
```

The example demonstrates the intended relationship between Zync and C++ libraries.

The developer writes Zync code:

```zync
var app = ZyncCrow::App()

app.html("/user/<string>", (username: string) -> string => {
    return ZyncCrow::render("index.html", {
        "title": "Halaman Profil",
        "name": username,
        "role": "Fullstack Engineer"
    })
})
```

while the underlying implementation can be provided by the existing C++ ecosystem.

Conceptually:

```text
Zync Code
    │
    ▼
Zync-Friendly Wrapper
    │
    ▼
Existing C++ Library
    │
    ▼
C++17
    │
    ▼
Native Binary
```

This means Zync does not need to recreate libraries such as Crow, Raylib, OpenCV, SQLite, or other C++ libraries from scratch.

Instead, Zync can provide a simpler interface on top of them.

---

## Example: Crow Web Application

A typical Crow application written directly in C++ may require developers to understand C++ templates, lambda syntax, framework-specific types, and C++ build configuration.

Zync aims to reduce that complexity.

A Zync application can look like:

```zync
pkg main

import (
    std,
    "dependencies/wrapper/crow/crow.hpp"
)

fn main() -> void {
    var app = ZyncCrow::App()

    app.html("/user/<string>", (username: string) -> string => {
        return ZyncCrow::render("index.html", {
            "title": "Halaman Profil",
            "name": username,
            "role": "Fullstack Engineer"
        })
    })

    app.run(8080)
}
```

The resulting application can still use the native performance and capabilities of the underlying C++ framework.

This approach gives developers a simpler development experience without abandoning the C++ ecosystem.

---

## Dual Target Compilation

Zync supports two primary compilation targets.

### Native

Zync source code is transpiled into C++17 and compiled using an existing C++ compiler.

```text
Zync (.zy)
    │
    ▼
Zync Compiler
    │
    ▼
C++17
    │
    ├── clang++
    │
    └── g++
    │
    ▼
Native Binary
```

Native compilation can take advantage of:

- Compiler optimizations
- SIMD vectorization
- CPU-specific optimizations
- Link-Time Optimization (LTO)
- Native threading
- Existing C/C++ libraries

---

### WebAssembly

Zync can also target WebAssembly through the Emscripten toolchain.

```text
Zync (.zy)
    │
    ▼
Zync Compiler
    │
    ▼
C++17
    │
    ▼
Emscripten (em++)
    │
    ├── .wasm
    ├── .js
    └── .html
```

This allows Zync applications to run in WebAssembly-compatible environments such as modern web browsers.

---

## Autonomous Toolchain

Zync provides an integrated compilation workflow instead of requiring developers to manually manage every C++ compilation step.

### Incremental Caching

The compiler can maintain a dependency and content cache to avoid recompiling unchanged source files.

Cache validation can consider:

- Source content
- Dependencies
- Compiler flags
- Target architecture
- Compilation configuration

This allows subsequent builds to skip work that has already been completed.

---

### Parallel Compilation

Zync supports parallel compilation through a multi-threaded work pool.

Example:

```bash
zync build . --jobs 8
```

or:

```bash
zync build . -j 8
```

Independent compilation tasks can be scheduled concurrently across available CPU cores.

---

### Recursive Header Resolution

Zync is designed to work with C++ headers and dependencies.

For example:

```zync
import (
    std,
    "vector",
    "list",
    "stack",
    "set",
    "unordered_map"
)
```

This allows Zync code to interact with common C++ data structures and libraries.

---

## Expressive Language Semantics

### Algebraic Pattern Matching

Zync provides `match` expressions for structured branching.

```zync
match value {
    0 => println("Zero"),
    1 => println("One"),
    _ => println("Other")
}
```

Pattern branches can support additional conditions through guards.

---

### Trait & Record Polymorphism

Zync provides `trait` and `impl` constructs for defining interfaces and implementations.

```zync
trait Drawable {
    fn draw() -> void
}

impl Drawable for Circle {
    fn draw() -> void {
        println("Drawing circle")
    }
}
```

This provides an abstraction mechanism for polymorphic behavior while remaining compatible with the generated C++ representation.

---

### Result Type

Zync provides a native `Result<T, E>` abstraction for representing successful and failed operations.

```zync
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 {
        return Err("division by zero")
    }

    return Ok(a / b)
}
```

Possible results can then be handled explicitly:

```zync
match divide(10, 2) {
    Ok(value) => println(value),
    Err(error) => println(error)
}
```

---

### Compile-Time Execution

Zync supports `comptime` constructs that can be lowered to C++ `constexpr` where applicable.

```zync
comptime fn square(x: int) -> int {
    return x * x
}

fn main() -> void {
    var result = square(10)
}
```

This allows certain computations to be performed during compilation instead of runtime.

---

## Integrated Unit Testing

Zync provides integrated testing without requiring an external testing framework.

Example:

```zync
test "addition works" {
    assert(1 + 1 == 2)
}

test "string comparison" {
    assert("zync" == "zync")
}
```

Tests can be executed directly through the Zync toolchain:

```bash
zync test
```

The test runner provides execution timing and test results without requiring an additional dependency.

---

## C++ Library Ecosystem

Because Zync targets C++17, applications can potentially leverage a large number of existing C/C++ libraries.

Examples include:

| Library | Purpose |
|---|---|
| Crow | HTTP / Web Framework |
| STL | Standard Data Structures & Algorithms |
| Raylib | Graphics / Game Development |
| OpenCV | Computer Vision |
| SQLite | Embedded Database |
| Boost | General-Purpose C++ Libraries |

The long-term objective is to make these libraries easier to consume through Zync wrappers and idiomatic Zync APIs.

For example:

```text
                 Zync
                   │
       ┌───────────┼───────────┐
       │           │           │
       ▼           ▼           ▼
   ZyncCrow    ZyncRaylib   ZyncOpenCV
       │           │           │
       ▼           ▼           ▼
     Crow        Raylib      OpenCV
       │           │           │
       └───────────┼───────────┘
                   ▼
                  C++
```

This approach gives Zync access to a mature native ecosystem while keeping the language itself relatively small.

---

## Compilation Pipeline

The general Zync compilation pipeline is:

```text
                  Zync Source
                    (.zy)
                      │
                      ▼
                 Lexer / Parser
                      │
                      ▼
                     AST
                      │
                      ▼
              Semantic Analysis
                      │
                      ▼
                  Code Generation
                      │
              ┌───────┴────────┐
              ▼                ▼
          C++17 Backend      WASM Backend
              │                │
              ▼                ▼
        clang++ / g++         em++
              │                │
              ▼                ▼
       Native Executable      .wasm
```

The C++ backend allows Zync to reuse mature compiler infrastructure instead of implementing a native machine-code backend from scratch.

---

## Requirements

- **C++ Compiler**: GCC >= 9.0 or Clang >= 11.0 with C++17 support
- **Build System**: GNU `make`
- **Operating System**: macOS or Linux
- **WebAssembly (Optional)**: [Emscripten SDK](https://emscripten.org/)
- **WebAssembly Runtime / Tooling (Optional)**: [Node.js](https://nodejs.org/)

---

## Installation

```bash
# Clone the repository
git clone https://github.com/your-username/zync.git

# Enter the project directory
cd zync

# Compile the compiler toolchain
make clean && make

# Optional: install globally
sudo make install
```

---

## Basic Usage

### Create a New Project

Create a new Zync project using the `zync create` command:

```bash
zync create my-project
```

This creates a new project directory with the default Zync project structure:

```text
my-project/
├── src/
│   └── main.zy
├── dependencies/
├── build/
└── zync.toml
```

Move into the project directory:

```bash
cd my-project
```

The generated `src/main.zy` contains a minimal Zync application:

```zync
pkg main

fn main() -> void {
    println("Hello, Zync!")
}
```

### Build the Project

Build the project using:

```bash
zync build
```

The Zync compiler parses the `.zy` source files, performs semantic analysis, generates C++17 code, and invokes the configured C++ compiler.

```text
Zync Source
    │
    ▼
   .zy
    │
    ▼
Zync Compiler
    │
    ▼
 C++17
    │
    ▼
clang++ / g++
    │
    ▼
Executable
```

### Run the Project

After building the project, run the generated executable:

```bash
zync run
```

This allows the development workflow to remain simple:

```bash
zync create my-project
cd my-project
zync build
zync run
```

### Create a Web Application

Zync projects can also use C++ web frameworks through Zync-compatible wrappers.

For example, a project using Crow can be created with:

```bash
zync create my-web-app
cd my-web-app
```

A Zync application using the Crow wrapper can then look like:

```zync
pkg main

import (
    std,
    "dependencies/wrapper/crow/crow.hpp"
)

fn main() -> void {
    var app = ZyncCrow::App()

    app.html("/user/<string>", (username: string) -> string => {
        return ZyncCrow::render("index.html", {
            "title": "Halaman Profil",
            "name": username,
            "role": "Fullstack Engineer"
        })
    })

    app.run(8080)
}
```

Build and run:

```bash
zync build
zync run
```

The application can then be accessed at:

```text
http://localhost:8080/user/Alegrarsio
```

The goal is to make the workflow feel similar to modern programming language toolchains while still compiling through the existing C++ ecosystem.

---

## Project Philosophy

Zync is built around a simple idea:

**C++ already has an enormous ecosystem. The problem is not the lack of libraries; the problem is the complexity of using them.**

Zync attempts to solve this by providing:

```text
Modern Syntax
      +
Simpler Tooling
      +
C++ Interoperability
      +
Native Performance
      =
Accessible Native Development
```

Rather than building another isolated ecosystem, Zync aims to become a language layer that makes the existing C++ ecosystem more approachable.

---

## Roadmap

Potential future development areas include:

- [ ] Improved C++ interoperability
- [ ] Automatic C++ header binding generation
- [ ] More C++ standard library wrappers
- [ ] Crow web framework integration
- [ ] Raylib integration
- [ ] OpenCV integration
- [ ] Improved package manager
- [ ] Dependency graph optimization
- [ ] Incremental compilation
- [ ] Improved WebAssembly support
- [ ] Language Server Protocol (LSP)
- [ ] VS Code extension
- [ ] Debugger integration
- [ ] More compile-time evaluation
- [ ] Native code generation improvements

---

## License

Zync is released under the **MIT License**.

See the `LICENSE` file for more information.