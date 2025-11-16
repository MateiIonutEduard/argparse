# argparse

**A type-safe argument parser for C.**  Minimal, dependency-free, and inspired by Python’s <br/>`argparse` — optimized for C command-line argument processing.

---

## 🧩 Overview

`argparse` is a lightweight library for handling command-line arguments in C.  
It mimics Python’s `argparse` interface while focusing on performance, simplicity, and type safety.

**Features:**
- 🪶 Header + source file (`argparse.h` + `argparse.c`) — simple integration into your project.  
- 🔒 Type-safe — argument types enforced at compile-time.  
- ⚡ Fast & minimal — suitable for embedded or low-overhead applications.  
- 🧠 Python-like interface — expressive and readable API.  
- ✅ Supports integers, doubles, strings, boolean flags, and lists.  
- 🎯 Supports default values, required flags, and help text.

## ⚙️ Setup

Follow these steps to get started with **argparse** in your C projects.

### 1. Clone the repository
```bash
git clone https://github.com/MateiIonutEduard/argparse.git
cd argparse
```

### 2. Build the static library
Linux/macOS (g++ or clang++):

```bash
make
```

Windows (Visual Studio Developer Command Prompt):

```bash
nmake /f Makefile
```

The static library will be created as:
* `libargparse.a` on Linux/Mac OSX
* `argparse.lib` on Windows

### 3. Include in your project
Copy the library and header to your project directory:

```css
average/
├── src/
│   ├── main.c
│   ├── argparse.h
│   └── libargparse.a  # or argparse.lib on Windows
```

### 4. Compile an example program
* Linux/macOS:<br/>
```bash
gcc main.c -o average -I. -L. -largparse -lm
```

* Windows (Visual C++):
```bash
cl main.c argparse.lib
```

### 5. Run the example
* Linux/Mac OSX:
  
```bash
./average -a -n 8 9 9 10 7 --verbose
```

* Windows:

```bat
average -a -n 8 9 9 10 7 --verbose
```
