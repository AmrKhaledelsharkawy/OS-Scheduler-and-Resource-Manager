#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

// Define constants
#define MEMORY_SIZE 60
#define MAX_PROGRAMS 3
#define INPUT_SPACE_PER_PROCESS 3
#define MAX_LINES 100
#define MAX_LINE_LENGTH 256
// Mutexes for resource access control
sem_t file_mutex, input_mutex, output_mutex;

// Process states
enum ProcessState {
    READY, RUNNING, WAITING, BLOCKED, TERMINATED
};

// Process Control Block (PCB)
typedef struct PCB {
    int process_id;
    enum ProcessState state;
    unsigned int quantum;
    unsigned int release_time;
    unsigned int start_time;
    unsigned int end_time;
    unsigned int program_counter;
    int lower_memory_bound;
    int upper_memory_bound;
    char blocked_resource[20];
    int var;
} PCB;

// Memory block structure
typedef struct MemoryBlock {
    char name[20];
    char value[256];
} MemoryBlock;

MemoryBlock memory[MEMORY_SIZE]; // Simulated memory

// Queue node structure
typedef struct QueueNode {
    PCB *process;
    struct QueueNode *next;
} QueueNode;

// Queue structure
typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
} Queue;

Queue ready_queue;
Queue running_queue;
Queue started_queue;
Queue blocked_queues[3]; // 0: file, 1: user input, 2: user output


// Initialize a queue
void initQueue(Queue *queue) {
    queue->front = queue->rear = NULL;
}

// Enqueue process
void enqueue(Queue *queue, PCB *process) {
    QueueNode *newNode = (QueueNode *)malloc(sizeof(QueueNode));
    newNode->process = process;
    newNode->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }
}

// Dequeue process
PCB *dequeue(Queue *queue) {
    if (queue->front == NULL)
        return NULL;
    QueueNode *temp = queue->front;
    PCB *process = temp->process;
    queue->front = queue->front->next;
    if (queue->front == NULL)
        queue->rear = NULL;
    free(temp);
    return process;
}

// Remove PCB from queue
void removePCB(Queue *queue, int pcb_id) {
    if (queue->front == NULL) {
        return;
    }

    if (queue->front->process->process_id == pcb_id) {
        QueueNode *temp = queue->front;
        queue->front = queue->front->next;
        free(temp);
        if (queue->front == NULL) {
            queue->rear = NULL;
        }
        return;
    }

    QueueNode *prev = queue->front;
    QueueNode *curr = queue->front->next;
    while (curr != NULL && curr->process->process_id != pcb_id) {
        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL) {
        prev->next = curr->next;
        if (curr == queue->rear) {
            queue->rear = prev;
        }
        free(curr);
    }
}

// Function to load a program into memory and initialize its PCB
PCB *loadProgram(int pid, char *program[], int program_size, int start_index, int release_time, unsigned int quantum) {
    int mem_index = start_index;

    for (int i = 0; i < program_size; i++) {
        strcpy(memory[start_index + i].name, "Instruction");
        strcpy(memory[start_index + i].value, program[i]);
        mem_index++;
    }
    // Reserve space for inputs
    for (int i = 0; i < INPUT_SPACE_PER_PROCESS; i++) {
        strcpy(memory[mem_index].name, "Free");
        strcpy(memory[mem_index].value, "");
        mem_index++;
    }

    PCB *pcb = (PCB *)malloc(sizeof(PCB));
    pcb->process_id = pid;
    pcb->state = READY;
    pcb->quantum = quantum;
    pcb->release_time = release_time;
    pcb->start_time = -1;  // Initialize start_time to -1 to indicate it hasn't started yet
    pcb->end_time = -1;    // Initialize end_time to -1 to indicate it hasn't ended yet
    pcb->program_counter = start_index;
    pcb->lower_memory_bound = start_index;
    pcb->upper_memory_bound = mem_index - 1;
    pcb->var = 0;
    strcpy(pcb->blocked_resource, "");
    return pcb;
}

// Print memory state
void printMemory(int time) {
    printf("\n-------------------------- Memory State at Time: %d--------------------------\n", time);
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (strlen(memory[i].name) > 0) {
            printf("| %-3d | %-11s | %-20s |\n", i, memory[i].name, memory[i].value);
        }
    }
    printf("-----------------------------------------------------------------------\n");
}

// Print queue state
void printQueue(const char *name, Queue *queue) {
    printf("------ %s ------\n", name);
    QueueNode *currentNode = queue->front;
    while (currentNode != NULL) {
        printf("| Process ID: %-3d |\n", currentNode->process->process_id);
        currentNode = currentNode->next;
    }
    printf("--------------------\n");
}

// Print all queues state
void printQueues(int time , int Q) {
    printf("\n-------------------------- Queue States at Time: %d--------------------------\n", time);
    switch (Q)
    {
    case 0:
         printQueue("Running Queue", &running_queue);
                  printQueue("Ready Queue", &ready_queue);
                           printQueue("Started Queue", &started_queue);



        break;
    case 1:
        break;
    case 2:
        break;
    default:
        break;
    }
    
    

    for (int i = 0; i < 3; i++) {
        char blocked_name[20];

        sprintf(blocked_name, "Blocked Queue %d", i);
        printQueue(blocked_name, &blocked_queues[i]);
    }
}


// Function to handle semWait
void semWait(sem_t *sem, PCB *pcb, Queue *blocked_queue, const char *resource_name) {
    if (sem_trywait(sem) == 0) {
        return; // Successfully acquired the semaphore
    } else {
        pcb->state = BLOCKED;
        strcpy(pcb->blocked_resource, resource_name);
        enqueue(blocked_queue, pcb);
    }
}

// Function to handle semSignal
void semSignal(sem_t *sem, Queue *blocked_queue) {
    sem_post(sem);
    PCB *unblocked_process = dequeue(blocked_queue);
    if (unblocked_process != NULL) {
        unblocked_process->state = READY;
        enqueue(&ready_queue, unblocked_process);
    }
}

// Function to simulate the Round Robin scheduling
void rr_scheduling() {
    int currentTime = 0;
    int terminatedCount = 0;

    PCB *currentProcess = NULL;

    while (1) {
        // Move processes from started to ready based on release time
        QueueNode *prevNode = NULL;
        QueueNode *currentNode = started_queue.front;

        while (currentNode != NULL) {
            if (currentNode->process->release_time <= currentTime) {
                PCB *readyProcess = currentNode->process;
                readyProcess->state = READY;

                if (prevNode == NULL) {
                    started_queue.front = currentNode->next;
                    if (started_queue.front == NULL) {
                        started_queue.rear = NULL;
                    }
                } else {
                    prevNode->next = currentNode->next;
                    if (prevNode->next == NULL) {
                        started_queue.rear = prevNode;
                    }
                }

                enqueue(&ready_queue, readyProcess);

                QueueNode *temp = currentNode;
                currentNode = currentNode->next;
                free(temp);
            } else {
                prevNode = currentNode;
                currentNode = currentNode->next;
            }
        }

        int executed = 0;

        // Execute the next process in the ready queue
        if (currentProcess == NULL && ready_queue.front != NULL) {
            printf("------------------------------------------------------------------Executing Process ID: %d at Time: %d------------------------------------------------------------------\n", ready_queue.front->process->process_id, currentTime);
         printMemory(currentTime); // Print memory state every clock cycle
               

            currentProcess = dequeue(&ready_queue);
             enqueue(&running_queue,currentProcess);
                 
                                 printQueues(currentTime,0); // Print queues after every scheduling event

            currentProcess->state = RUNNING;
            if (currentProcess->start_time == -1) {
                currentProcess->start_time = currentTime;
            }
                
        }

        if (currentProcess != NULL) {
            unsigned int time_slice = currentProcess->quantum;

            for (unsigned int t = 0; t < time_slice&&strcmp(memory[currentProcess->program_counter].name, "Instruction") == 0; t++) {
                printf("Current Time: %d\n", currentTime);
                printf("Currently Executing Process ID: %d\n", currentProcess->process_id);
                printf("Instruction: %s\n", memory[currentProcess->program_counter].value);

                // Check if current memory block is an instruction
                if (strcmp(memory[currentProcess->program_counter].name, "Instruction") != 0) {
                    currentProcess->state = TERMINATED;
                    currentProcess->end_time = currentTime;
                    printf("Process ID: %d terminated.\n", currentProcess->process_id);
                    free(currentProcess);
                    currentProcess = NULL;
                    terminatedCount++;
                    break;
                }

                char instruction[256];
                strcpy(instruction, memory[currentProcess->program_counter].value);

                char *token = strtok(instruction, " ");
                if (token == NULL) {
                    printf("Malformed instruction encountered. Skipping...\n");
                    if (currentProcess->program_counter + 1 <= currentProcess->upper_memory_bound) {
                        currentProcess->program_counter++;
                    }
                    currentTime++;
                    continue;
                }

                if (strcmp(token, "semWait") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        if (strcmp(token, "file") == 0) {
                            semWait(&file_mutex, currentProcess, &blocked_queues[0], "file");
                        } else if (strcmp(token, "userInput") == 0) {
                            semWait(&input_mutex, currentProcess, &blocked_queues[1], "userInput");
                        } else if (strcmp(token, "userOutput") == 0) {
                            semWait(&output_mutex, currentProcess, &blocked_queues[2], "userOutput");
                        }
                    }
                } else if (strcmp(token, "semSignal") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        if (strcmp(token, "file") == 0) {
                            semSignal(&file_mutex, &blocked_queues[0]);
                        } else if (strcmp(token, "userInput") == 0) {
                            semSignal(&input_mutex, &blocked_queues[1]);
                        } else if (strcmp(token, "userOutput") == 0) {
                            semSignal(&output_mutex, &blocked_queues[2]);
                        }
                    }
                } else if (strcmp(token, "assign") == 0) {
    char var[20], value[256], data[256];
    sscanf(memory[currentProcess->program_counter].value, "assign %s %[^\n]", var, value);

    if (strcmp(value, "input") == 0) {
        printf("Please enter a value for %s: ", var);
        scanf("%s", data);
        switch (currentProcess->var) {
            case 0:
                strcpy(memory[currentProcess->upper_memory_bound - 2].name, var);
                strcpy(memory[currentProcess->upper_memory_bound - 2].value, data);
                currentProcess->var++;
                break;
            case 1:
                strcpy(memory[currentProcess->upper_memory_bound - 1].name, var);
                strcpy(memory[currentProcess->upper_memory_bound - 1].value, data);
                currentProcess->var++;
                break;
            case 2:
                strcpy(memory[currentProcess->upper_memory_bound].name, var);
                strcpy(memory[currentProcess->upper_memory_bound].value, data);
                currentProcess->var++;
                break;
            case 3:
                printf("No space left for the variable '%s'.\n", var);
                break;
            default:
                printf("Error: Invalid input space index.\n");
                break;
        }
    } else if (strncmp(value, "readFile", 8) == 0) {
        // Extract the file name
        char fileName[256];
        sscanf(value, "readFile %s", fileName);
        printf("File name is %s\n", fileName);  // Debug print statement

        // Open the file and read its content
        FILE *file = fopen(fileName, "r");
        if (file == NULL) {
            printf("Error: Could not open file '%s'.\n", fileName);
        } else {
            char fileContent[256];
            if (fgets(fileContent, sizeof(fileContent), file) != NULL) {
                // Remove newline character if present
                fileContent[strcspn(fileContent, "\n")] = '\0';

                // Store file content in memory
                switch (currentProcess->var) {
                    case 0:
                        strcpy(memory[currentProcess->upper_memory_bound - 2].name, var);
                        strcpy(memory[currentProcess->upper_memory_bound - 2].value, fileContent);
                        currentProcess->var++;
                        break;
                    case 1:
                        strcpy(memory[currentProcess->upper_memory_bound - 1].name, var);
                        strcpy(memory[currentProcess->upper_memory_bound - 1].value, fileContent);
                        currentProcess->var++;
                        break;
                    case 2:
                        strcpy(memory[currentProcess->upper_memory_bound].name, var);
                        strcpy(memory[currentProcess->upper_memory_bound].value, fileContent);
                        currentProcess->var++;
                        break;
                    case 3:
                        printf("No space left for the variable '%s'.\n", var);
                        break;
                    default:
                        printf("Error: Invalid input space index.\n");
                        break;
                }
            } else {
                printf("Error: Could not read from file '%s'.\n", fileName);
            }
            fclose(file);
        }
    } else {
        // Handle assigning a value directly
        switch (currentProcess->var) {
            case 0:
                strcpy(memory[currentProcess->upper_memory_bound - 2].name, var);
                strcpy(memory[currentProcess->upper_memory_bound - 2].value, value);
                currentProcess->var++;
                break;
            case 1:
                strcpy(memory[currentProcess->upper_memory_bound - 1].name, var);
                strcpy(memory[currentProcess->upper_memory_bound - 1].value, value);
                currentProcess->var++;
                break;
            case 2:
                strcpy(memory[currentProcess->upper_memory_bound].name, var);
                strcpy(memory[currentProcess->upper_memory_bound].value, value);
                currentProcess->var++;
                break;
            case 3:
                printf("No space left for the variable '%s'.\n", var);
                break;
            default:
                printf("Error: Invalid input space index.\n");
                break;
        }
    }
}
else if (strcmp(token, "print") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        for (int i = 0; i < MEMORY_SIZE; i++) {
                            if (strcmp(memory[i].name, token) == 0) {
                                printf("%s\n", memory[i].value);
                                break;
                            }
                        }
                    }
                } else if (strcmp(token, "printFromTo") == 0) {
                    char var1[20], var2[20];
                    sscanf(memory[currentProcess->program_counter].value, "printFromTo %s %s", var1, var2);
                    int start, end;
                    for (int i = 0; i < MEMORY_SIZE; i++) {
                        if (strcmp(memory[i].name, var1) == 0) {
                            start = atoi(memory[i].value);
                        }
                        if (strcmp(memory[i].name, var2) == 0) {
                            end = atoi(memory[i].value);
                        }
                    }
                    for (int i = start; i <= end; i++) {
                        printf("%d\n", i);
                    }
                } else if (strcmp(token, "writeFile") == 0) {
                    char filename[20], data[256];
                    sscanf(memory[currentProcess->program_counter].value, "writeFile %s %s", filename, data);
                    for(int i = currentProcess->upper_memory_bound-3;i<currentProcess->upper_memory_bound;i++){
                        if(strcmp(memory[i].name,data)==0){
                            strcpy(data,memory[i].value);
                        }
                    }
                    FILE *file = fopen(filename, "w");
                    if (file != NULL) {
                        fprintf(file, "%s", data);
                        fclose(file);
                    }
                } else if (strcmp(token, "readFile") == 0) {
                    char filename[20], var[20];
                    sscanf(memory[currentProcess->program_counter].value, "readFile %s %s", filename, var);
                    FILE *file = fopen(filename, "r");
                    if (file != NULL) {
                        char data[256];
                        fscanf(file, "%s", data);
                        fclose(file);
                        for (int i = 0; i < MEMORY_SIZE; i++) {
                            if (strcmp(memory[i].name, var) == 0) {
                                strcpy(memory[i].value, data);
                                break;
                            }
                        }
                    }
                }

                if (currentProcess->program_counter + 1 <= currentProcess->upper_memory_bound) {
                    currentProcess->program_counter++;
                }
                currentTime++;
               

                if (currentProcess->state == BLOCKED) {
                    currentProcess = NULL;
                     PCB *donePros = dequeue(&running_queue);

                    break;
                }
                 printf("------------------------------------------------------------------Executing Process ID: %d at Time: %d------------------------------------------------------------------\n", currentProcess->process_id, currentTime);
                     printMemory(currentTime); // Print memory state every clock cycle
                 
                    printQueues(currentTime,0); 

                sleep(1);
            }

            if (currentProcess != NULL) {
                if (currentProcess->program_counter > currentProcess->upper_memory_bound || strcmp(memory[currentProcess->program_counter].name, "Instruction") != 0) {
                    currentProcess->state = TERMINATED;
                    currentProcess->end_time = currentTime;
                    printf("Process ID: %d terminated.\n", currentProcess->process_id);
                    PCB *donePros = dequeue(&running_queue);

                    free(currentProcess);
                    currentProcess = NULL;
                    terminatedCount++;
                } else {
                    currentProcess->state = READY;
                    PCB *donePros = dequeue(&running_queue);
                    enqueue(&ready_queue, currentProcess);
                    currentProcess = NULL;
                }
            }
            executed = 1;
        }

        if (!executed) {
            currentTime++;
        }

        if (terminatedCount == MAX_PROGRAMS) {
            break;
        }
    }
}

void printStartedQueue(Queue *started) {
    QueueNode *currentNode = started->front;
    printf("Processes in started queue:\n");
    while (currentNode != NULL) {
        printf("| Process ID: %-3d |\n", currentNode->process->process_id);
        printf("|   State: %-2d    |\n", currentNode->process->state);
        printf("|   Quantum: %-3d  |\n", currentNode->process->quantum);
        printf("|   Release Time: %-3d |\n", currentNode->process->release_time);
        printf("|   Program Counter: %-3d |\n", currentNode->process->program_counter);
        printf("|   Lower Memory Bound: %-3d |\n", currentNode->process->lower_memory_bound);
        printf("|   Upper Memory Bound: %-3d |\n", currentNode->process->upper_memory_bound);
        printf("|   Start Time: %-3d |\n", currentNode->process->start_time);
        printf("|   End Time: %-3d |\n", currentNode->process->end_time);
        currentNode = currentNode->next;
    }
}

void cleanup() {
    // Free all remaining PCBs in the queues
    while (ready_queue.front != NULL) {
        PCB *pcb = dequeue(&ready_queue);
        free(pcb);
    }

    while (blocked_queues[0].front != NULL) {
        PCB *pcb = dequeue(&blocked_queues[0]);
        free(pcb);
    }
    while (blocked_queues[1].front != NULL) {
        PCB *pcb = dequeue(&blocked_queues[1]);
        free(pcb);
    }
    while (blocked_queues[2].front != NULL) {
        PCB *pcb = dequeue(&blocked_queues[2]);
        free(pcb);
    }

    // Destroy the semaphores
    sem_destroy(&file_mutex);
    sem_destroy(&input_mutex);
    sem_destroy(&output_mutex);
}


char **read_program(const char *filename, int *line_count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    char **program = malloc(MAX_LINES * sizeof(char *));
    if (!program) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    char line[MAX_LINE_LENGTH];
    *line_count = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        program[*line_count] = malloc(strlen(line) + 1);
        if (!program[*line_count]) {
            perror("Failed to allocate memory");
            fclose(file);
            return NULL;
        }
        strcpy(program[*line_count], line);
        (*line_count)++;
    }

    fclose(file);
    return program;
}
void readFileIntoArray(const char *filename, char *program[], int *lineCount) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_LINE_LENGTH];
    *lineCount = 0;

    while (fgets(buffer, MAX_LINE_LENGTH, file) != NULL) {
        if (*lineCount >= MAX_LINES) {
            fprintf(stderr, "Error: file has too many lines\n");
            exit(EXIT_FAILURE);
        }

        program[*lineCount] = malloc(strlen(buffer) + 1);
        if (program[*lineCount] == NULL) {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }
        strcpy(program[*lineCount], buffer);
        (*lineCount)++;
    }

    fclose(file);
}

void free_program(char **program, int line_count) {
    for (int i = 0; i < line_count; i++) {
        free(program[i]);
    }
    free(program);
}







#define MaxNumberOfStrings 100
#define MaxStringSize 100

void rtrim(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && (str[i] == '\n' || str[i] == '\r')) {
        str[i] = '\0';
        i--;
    }
}

void removeCarriageReturn(char *a[], int rows) {
    for (int i = 0; i < rows; i++) {
        rtrim(a[i]);
    }
}

void insertIntoArray(char* a[], int row, char* str) {
    a[row] = strdup(str);
}



// Function to read a file and return a 2D array of strings
int ReadFile(char* array []  ,char *filename) {
    int row = 0;

    FILE *fh = fopen(filename, "r");
    if (fh != NULL) {
        char buffer[MaxStringSize];
        while (fgets(buffer, MaxStringSize, fh) != NULL) {
            insertIntoArray(array, row, buffer);
            row++;
            if (row >= MaxNumberOfStrings)
                break;
        }
        removeCarriageReturn(array, row);
        fclose(fh);
    }
    return row;
}

void printArray(char* array[], int size) {
    for (int i = 0; i < size; i++) {
        if (array[i] != NULL) {
            printf("%s\n", array[i]);
        }
    }
}

int  interpret(char* array[] , int  a ){
    int size;
    switch (a)
    {
    case 1:
          size = ReadFile(array,"Program_1.txt");
        break;

    case 2:
          size = ReadFile(array,"Program_2.txt");
        break;
    case 3:
          size = ReadFile(array,"Program_3.txt");
        break;
    
    default:
        break;
    }

                                                // actual used size
    return size;
}








int main() {
       

    sem_init(&file_mutex, 0, 1);
    sem_init(&input_mutex, 0, 1);
    sem_init(&output_mutex, 0, 1);

    initQueue(&running_queue);

    initQueue(&ready_queue);
    initQueue(&started_queue);
    initQueue(&blocked_queues[0]);
    initQueue(&blocked_queues[1]);
    initQueue(&blocked_queues[2]);


    char* program1[MaxNumberOfStrings];
    char* program2[MaxNumberOfStrings];
    char* program3[MaxNumberOfStrings];
    char*p1 = "program_1.txt";
    char*p2 = "program_2.txt";
    char*p3 = "program_3.txt";
    int program1size = interpret( program1, 1);
    int program2size = interpret( program2, 2);
    int program3size = interpret( program3, 3);


   



    // User inputs for release times and quantum times
    int release_time1, release_time2, release_time3;
    unsigned int quantum1, quantum2, quantum3;
    printf("Enter release time for program 1: ");
    scanf("%d", &release_time1);
   
    printf("Enter release time for program 2: ");
    scanf("%d", &release_time2);

    printf("Enter release time for program 3: ");
    scanf("%d", &release_time3);
    printf("Enter quantum time for The OS : ");
    scanf("%u", &quantum3);






    // Load programs into memory and queues
    PCB *pcb1 = loadProgram(1, program1, program1size, 0, release_time1, quantum3);
    PCB *pcb2 = loadProgram(2, program2, program2size, pcb1->upper_memory_bound + 1, release_time2, quantum3);
    PCB *pcb3 = loadProgram(3, program3, program3size, pcb2->upper_memory_bound + 1, release_time3, quantum3);



    enqueue(&started_queue, pcb1);
    enqueue(&started_queue, pcb2);
    enqueue(&started_queue, pcb3);





    // Run the RR scheduling
    if(quantum1<=0 || quantum2 <=0 || quantum3 <=0){
           printf("The process can't have zero or less quantum Please Change it ");
            cleanup();

        return 0;
    }
    rr_scheduling();

    // Cleanup resources
    cleanup();

    return 0;
}
