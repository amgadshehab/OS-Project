# OS Project 

This project is a simulation of an operating system's memory management, process scheduling, and mutex handling in C. It includes functionalities such as memory allocation and deallocation, process scheduling with multiple priority levels, and mutex operations for resource management.

## Features

- **Memory Management**: Allocation and deallocation of memory blocks for processes.
- **Process Scheduling**: Multi-level feedback queue scheduling with different time quantums.
- **Mutex Handling**: Mutex operations including wait and signal for resource synchronization.
- **Command Execution**: Ability to execute predefined commands including file operations and variable assignments.

## Prerequisites

- GCC (GNU Compiler Collection)
- pthreads library

## Compilation and Execution

To compile the project, use the following command:
```sh
gcc -o os_project M2.c -lpthread
```

To run the compiled program:
```sh
./os_project
```

