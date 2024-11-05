cmake_minimum_required(VERSION 3.28)
project(CQ_backend_improved C)

set(CMAKE_C_STANDARD 23)

add_executable(CQ_backend_improved main.c
        QPU.h
        src/Integer.c
        src/QPU.c
        src/gate.c
        src/IntegerAddition.c
        src/IntegerMultiplication.c
        src/AssemblyOperations.c
        src/LogicOperations.c)

add_executable(bench benchmark/benchmark.c
        QPU.h
        src/Integer.c
        src/QPU.c
        src/gate.c
        src/IntegerAddition.c
        src/IntegerMultiplication.c
        src/AssemblyOperations.c
        include/gate.h
        include/definition.h
        include/AssemblyOperations.h
        include/QPU.h
        include/Integer.h
        src/LogicOperations.c
        include/LogicOperations.h)

