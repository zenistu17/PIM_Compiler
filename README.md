# PIM Compiler

A compiler that generates PIM (Processing-In-Memory) ISA code from matrix operation specifications.

## Features

- Converts matrix operations to PIM ISA instructions
- Recognizes C/C++ matrix-multiplication loops, including rectangular matrices
- Automatic memory allocation and management
- Emits validated PIM instruction streams

## Project Structure

```
PIM_Compiler/
├── include/               # Header files
│   ├── Lexer.h
│   ├── Parser.h
│   ├── CodeGen.h
│   └── TargetBackend.h
├── src/                   # Source files
│   ├── Lexer.cpp
│   ├── Parser.cpp
│   ├── CodeGen.cpp
│   ├── TargetBackend.cpp
│   └── main.cpp
├── tests/                 # Test cases
│   ├── test1.cpp
│   ├── test2.cpp
│   └── ...
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

## Prerequisites

- A C++17 compatible compiler (GCC, Clang, or MSVC)
- CMake 3.16 or newer

The compiler is self-contained and does not require LLVM.  A clean checkout
can therefore be configured on a machine that only has a standard C++ toolchain.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The executable is written to `build/PIM_Compiler` (or
`build/Debug/PIM_Compiler.exe` with a multi-configuration generator).

## Usage

```bash
./build/PIM_Compiler tests/test1.cpp -o output.isa
```

The command expects exactly one source path and an output path.  Diagnostics are
written to stderr and a non-zero exit status is returned when either file cannot
be opened or compilation fails.  Create the output directory before invoking
the command.

Matrix dimensions may be numeric literals or numeric `#define` values such as
`N`, `ROWS`, `COLS`, and `INNER`.  The frontend preserves each matrix's row and
column shape so `A[ROWS][INNER] * B[INNER][COLS]` is allocated correctly.

The accepted source subset is a C/C++ function with indexed assignments of the
form `C[i][j] = A[i][k] * B[k][j]` (or `+=`) inside loops.  This frontend does
not compile general C++ or scalar matrix addition; malformed expressions and
unknown dimensions are reported as compilation errors.

Example test.cpp:
```cpp
#include <iostream>
#define SIZE 4

void matmul(int X[SIZE][SIZE], int Y[SIZE][SIZE], int Z[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            Z[i][j] = 0;
            for (int k = 0; k < SIZE; k++) {
                Z[i][j] += X[i][k] * Y[k][j];
            }
        }
    }
}

int main() {
    int X[SIZE][SIZE] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    int Y[SIZE][SIZE] = {{2,2,2,2},{2,2,2,2},{2,2,2,2},{2,2,2,2}};
    int Z[SIZE][SIZE];
    
    matmul(X, Y, Z);
    return 0;
}

```

## Example Output

For the 4x4 example above, the generated stream contains the following
allocation and execution records (the complete stream also contains the MAC
and matrix-multiply program blocks):

```isa
# MATRIX ALLOCATIONS
# Matrix X allocated at 0x1000 (4x4)
# Matrix Y allocated at 0x1040 (4x4)
# Matrix Z allocated at 0x1080 (4x4)

# MATRIX OPERATIONS
# MATRIX MULTIPLICATION X * Y -> Z
EXE r2, 0x1000, 0x1040, 0x1080, 4, 4, 4

# MEMORY RELEASE
FREE 0x1080 64
FREE 0x1040 64
FREE 0x1000 64
END
```

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Contact

Sujith S - [LinkedIn](https://www.linkedin.com/in/sujith-s-62aa7527a/) - sujithsures@gmail.com
Project Link: [https://github.com/zenistu17/PIM_Compiler](https://github.com/zenistu17/PIM_Compiler)
