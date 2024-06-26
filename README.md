# Project: Operating Systems 

## Overview

In this milestone, we created a custom scheduler and managed resource usage between different processes. The project involves reading and executing code from three provided program files, implementing memory management, and ensuring mutual exclusion over critical resources using mutexes. The goal is to implement a scheduler that manages the processes in our system effectively.

## Detailed Description

### Programs

We have three main programs:
1. **Program 1**: Given two numbers, the program prints the numbers between them (inclusive).
2. **Program 2**: Given a filename and data, the program writes the data to the file (assuming the file doesn't exist and should always be created).
3. **Program 3**: Given a filename, the program prints the contents of the file on the screen.

### Process Control Block (PCB)

The PCB is a data structure that stores information about a process. Each PCB contains:
- Process ID
- Process State
- Quantum
- Release Time
- Start Time
- End Time
- Program Counter
- Memory Boundaries
- Blocked Resource
- Variable Space

### Memory

The memory is of a fixed size (60 memory words) and is divided to store instructions, variables, and PCBs for the processes. Each process is allocated space for its instructions and three variables.

### Scheduler

The scheduler uses a Round Robin (RR) algorithm to schedule processes in the Ready Queue. The processes are scheduled based on their arrival times and quantum times.

### Mutual Exclusion

Mutexes ensure mutual exclusion over the critical resources:
- File access
- User input
- User output

Processes use `semWait` and `semSignal` instructions to acquire and release these resources, ensuring that only one process can access a resource at a time.

### Queues

We implemented several queues to manage the processes:
- Ready Queue: Processes waiting to be executed.
- Running Queue: Currently executing processes.
- Started Queue: Processes that have started but are not yet ready.
- Blocked Queues: Processes blocked due to unavailable resources.

## Implementation

### Program Loading

The programs are loaded from text files into the simulated memory. The PCB for each process is initialized with the necessary details and the processes are enqueued into the appropriate queues.

### Round Robin Scheduling

The RR scheduler runs in a loop, moving processes between queues based on their states and executing instructions. Each instruction is executed in one clock cycle. The scheduler prints the state of the memory and queues at each step.

### Mutex Handling

Mutexes are implemented using semaphores. Processes are blocked and enqueued in blocked queues if they cannot acquire the necessary mutex. When a resource is released, the highest priority blocked process is moved to the ready queue.

### Example Program Syntax

- `semWait resourceName`
- `semSignal resourceName`
- `assign variable value`
- `print variable`
- `writeFile filename data`
- `readFile filename variable`
- `printFromTo start end`

## Usage

### Compilation and Execution

1. **Compile the Code:**
   ```sh
   gcc -o scheduler scheduler.c -lpthread
   ```

2. **Run the Scheduler:**
   ```sh
   ./scheduler
   ```

### Input

The scheduler prompts the user to enter release times and quantum times for the processes. Example input:

```
Enter release time for program 1: 0
Enter release time for program 2: 5
Enter release time for program 3: 10
Enter quantum time for The OS : 2
```

### Output

The scheduler prints the state of the memory and queues at each clock cycle, including which process is currently executing, the current instruction, and the state of the queues.

