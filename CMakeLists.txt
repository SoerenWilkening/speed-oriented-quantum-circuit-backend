cmake_minimum_required(VERSION 3.28)
project(CQ_backend_improved C)

set(CMAKE_C_STANDARD 23)

add_executable(CQ_backend_improved main.c
        QPU.h
        src/stack.c
        src/Integer.c
        src/circuit.c
        src/gate.c)

add_executable(bench benchmark/benchmark.c
        QPU.h
        src/stack.c
        src/Integer.c
        src/circuit.c
        src/gate.c)

