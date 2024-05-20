#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_SIZE 60
#define FREE 0
#define USED 1
#define MAX_PROCESSES 10
#define BLOCK_SIZE 10
#define NUM_QUEUES 4
#define MAX_PROGRAM_SIZE 50
#define TIME_QUANTUM 2
#define TERMINATED 0 // Define the TERMINATED identifier
#define READY 1 // Define the READY identifier
#define BLOCKED 2 // Define the BLOCKED/WAITING identifier
#define NEW 3 // Define the NEW identifier
#define RUNNING 4 // Define the RUNNING identifier
#define FINISHED 5 // Define the FINISHED identifier

typedef struct {
    int processID;
    char state[50];
    int currentPriority;
    int programCounter;
    int memoryLowerBound;
    int memoryUpperBound;
     int time_slice; // To keep track of the remaining time slice
} PCB;

typedef struct {
    int start;
    int size;
    int processID;
} MemoryBlock;

typedef struct {
    int size;
    char memory[MEMORY_SIZE][50];
    char name[50];
    int data;
} Memory;

typedef struct {
    char name[50];
    int locked;
    PCB* blocked_queue[MAX_PROCESSES];
    int blocked_count;
} Mutex;

typedef struct Process {
    int processID;
    struct Process* next;
} Process;

typedef struct {
    Process* front;
    Process* rear;
    int count;
    PCB* queue[MAX_PROCESSES];
} Queue;

typedef struct {
    PCB* ready_queues[4][MAX_PROCESSES];
    int ready_counts[4];
    PCB* general_blocked_queue[MAX_PROCESSES];
    int blocked_count;
    PCB* current_process;
    int quantum[NUM_QUEUES];
} Scheduler;




Queue ready_queue;
Scheduler scheduler = { .quantum = {1, 2, 4, 8} }; // Quantum doubles for each lower priority level
MemoryBlock memoryBlocks[MEMORY_SIZE];
int memory[MEMORY_SIZE];
Mutex mutexes[3];
Memory mem[100];
Queue readyQueue = {NULL, NULL};
Queue blockedQueue = {NULL, NULL};
pthread_mutex_t mutex;
pthread_mutex_t userInputMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t userOutputMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileAccessMutex = PTHREAD_MUTEX_INITIALIZER;
MemoryBlock freeBlocks[MAX_PROCESSES];
int nextFreeBlock = 0;
char programs[MAX_PROCESSES][MAX_PROGRAM_SIZE][50];



void executeCommand(char* line);
void initScheduler(void);
//void initializeMemory();
void initialize_scheduler();

void initializeMemory() {
    mem->size = MEMORY_SIZE;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        for (int j = 0; j < MEMORY_SIZE; j++) {
            strcpy(mem[i].memory[j], "");
        }    
    }
}

int allocateMemory(int processID, int size) {
    if (nextFreeBlock + size > MEMORY_SIZE) {
        printf("Not enough memory to allocate %d blocks for process %d\n", size, processID);
        return 0;
    }
    memoryBlocks[processID].start = nextFreeBlock;
    memoryBlocks[processID].size = size;
    memoryBlocks[processID].processID = processID;

    nextFreeBlock += size;
    
}

void deallocateMemory(int processID) {
    // Add the block used by the process back to the free list
    freeBlocks[nextFreeBlock] = memoryBlocks[processID];
    nextFreeBlock++;

    // Clear the memory block entry for the process
    memoryBlocks[processID].start = 0;
    memoryBlocks[processID].size = 0;
    memoryBlocks[processID].processID = 0;
}

void parseFile(char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open file %s\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        executeCommand(line);
    }

    fclose(file);
}


void initialize_mutexes() {
    for (int i = 0; i < 3; i++) {
        mutexes[i].locked = 0;
        mutexes[i].blocked_count = 0;
    }
}

void sem_wait(Mutex* mutex, PCB* pcb) {
    if (!mutex->locked) {
        mutex->locked = 1;
    } else {
        strcpy(pcb->state, "Blocked");
        mutex->blocked_queue[mutex->blocked_count++] = pcb;
    }
}

PCB* sem_signal(Mutex* mutex) {
    if (mutex->blocked_count > 0) {
        PCB* unblocked_process = mutex->blocked_queue[0];
        for (int i = 1; i < mutex->blocked_count; i++) {
            mutex->blocked_queue[i-1] = mutex->blocked_queue[i];
        }
        mutex->blocked_count--;
        strcpy(unblocked_process->state, "Ready");
        mutex->locked = 0;
        return unblocked_process;
    }
    mutex->locked = 0;
    return NULL;
}

void criticalSection(int processID) {
    pthread_mutex_lock(&mutex);

    // Critical section of code goes here.
    // This is where you would access shared resources.
    printf("Process %d is in the critical section\n", processID);

    pthread_mutex_unlock(&mutex);
}

void destroyMutex() {
    pthread_mutex_destroy(&mutex);
}


void enqueue(Queue* queue, PCB *pcb) {
    Process* newProcess = malloc(sizeof(Process));
    newProcess->processID = pcb->processID;
    newProcess->next = NULL;

    if (queue->rear != NULL) {
        queue->rear = newProcess;
    }
    queue->rear = newProcess;

    if (queue->front == NULL) {
        queue->front = newProcess;
    }
}

int dequeue(Queue* queue) {
    if (queue->front == NULL) {
        printf("Queue is empty\n");
        return -1;
    }

    int processID = queue->front->processID;
    Process* nextProcess = queue->front->next;
    free(queue->front);
    queue->front = nextProcess;

    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    return processID;
}

void executeCommand(char* command) {
    char* token = strtok(command, " ");
    if (strcmp(token, "readFile") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            FILE* file = fopen(token, "r");
            if (file != NULL) {
                char line[256];
                while (fgets(line, sizeof(line), file)) {
                    printf("%s", line);
                }
                fclose(file);
            } else {
                printf("Could not open file %s\n", token);
            }
        } else {
            printf("No filename specified\n");
        }
    }

    if (strcmp(token, "writeFile") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            char* filename = token;
            token = strtok(NULL, " ");
            if (token != NULL) {
                char* data = token;
                FILE* file = fopen(filename, "w");
                if (file != NULL) {
                    fprintf(file, "%s", data);
                    fclose(file);
                } else {
                    printf("Could not open file %s\n", filename);
                }
            } else {
                printf("No data specified\n");
            }
        } else {
            printf("No filename specified\n");
        }
    }

    if (strcmp(token, "printFromTo") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            int start = atoi(token);
            token = strtok(NULL, " ");
            if (token != NULL) {
                int end = atoi(token);
                for (int i = start; i <= end; i++) {
                    printf("%d\n", i);
                }
            } else {
                printf("No end number specified\n");
            }
        } else {
            printf("No start number specified\n");
        }
    }

    if (strcmp(token, "assign") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            char* varName = token;
            token = strtok(NULL, " ");
            if (token != NULL) {
                int value = atoi(token);
                // Find an empty spot in the memory
                for (int i = 0; i < 100; i++) {
                    if (mem[i].name[0] == '\0') {
                        mem[i].data = value;
                        break;
                    }
                }
            } else {
                printf("No value specified\n");
            }
        } else {
            printf("No variable name specified\n");
        }
    }

    if (strcmp(token, "semWait") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            char* resourceName = token;
            // Find the mutex for the resource
            for (int i = 0; i < 3; i++) {
                if (strcmp(mutexes[i].name, resourceName) == 0) {
                    if (mutexes[i].locked == 0) {
                        mutexes[i].locked = 1;
                    } else {
                        printf("Resource %s is currently in use\n", resourceName);
                    }
                    break;
                }
            }
        } else {
            printf("No resource name specified\n");
        }
    }

    if (strcmp(token, "print") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            char* varName = token;
            // Find the variable in the memory
            for (int i = 0; i < 100; i++) {
                if (strcmp(mem[i].name, varName) == 0) {
                    printf("%s = %d\n", varName, mem[i].data);
                    break;
                }
            }
        } else {
            printf("No variable name specified\n");
        }
    }

    if (strcmp(token, "semSignal") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            char* resourceName = token;
            // Find the mutex for the resource
            for (int i = 0; i < 3; i++) {
                if (strcmp(mutexes[i].name, resourceName) == 0) {
                    if (mutexes[i].locked == 1) {
                        mutexes[i].locked = 0;
                    } else {
                        printf("Resource %s is not currently in use\n", resourceName);
                    }
                    break;
                }
            }
        } else {
            printf("No resource name specified\n");
        }
    }
}


void interpret(char program[MAX_PROGRAM_SIZE][50], PCB* pcb) {
    while (pcb->programCounter < MAX_PROGRAM_SIZE && strlen(program[pcb->programCounter]) != 0) {
        executeCommand(program[pcb->programCounter]);
        pcb->programCounter++;
    }
}


void initialize_ready_queue() {
    ready_queue.front = 0;
    ready_queue.rear = NULL;
    ready_queue.count = 0;
}

void add_process_to_ready_queue(PCB* pcb) {
    int priority = pcb->currentPriority - 1;
    scheduler.ready_queues[priority][scheduler.ready_counts[priority]++] = pcb;
}

void initialize_scheduler() {
    for (int i = 0; i < 4; i++) {
        scheduler.ready_counts[i] = 0;
    }
    scheduler.blocked_count = 0;
    scheduler.current_process = NULL;
}

void schedule() {
    initialize_ready_queue();

    // Create and initialize PCBs
    PCB pcbs[MAX_PROCESSES];
    PCB* current_process = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcbs[i].processID = i + 1;
        strcpy(pcbs[i].state, "Ready");
        pcbs[i].currentPriority = 1;
        pcbs[i].programCounter = 0;
        pcbs[i].time_slice = TIME_QUANTUM;
        int mem_start = allocateMemory(10, sizeof(mem));
        pcbs[i].memoryLowerBound = mem_start;
        pcbs[i].memoryUpperBound = mem_start + 9;
        enqueue(&ready_queue, &pcbs[i]);
    }

     while (ready_queue.count > 0) {
        int pid = dequeue(&ready_queue);
        current_process = &pcbs[pid - 1];
       
        while (ready_queue.count > 0) {
            int pid = dequeue(&ready_queue);
            current_process = &pcbs[pid - 1];
            // Rest of the code...
        }

        printf("Executing process %d\n", current_process->processID);

        // Execute instructions within the time slice
        while (current_process->time_slice > 0 && current_process->programCounter < MAX_PROGRAM_SIZE && strlen(programs[current_process->processID - 1][current_process->programCounter]) != 0) {
            executeCommand(programs[current_process->processID - 1][current_process->programCounter]);
            current_process->programCounter++;
            current_process->time_slice--;
        }

        // If the process is not finished, reset the time slice and re-enqueue
        if (current_process->programCounter < MAX_PROGRAM_SIZE && strlen(programs[current_process->processID - 1][current_process->programCounter]) != 0) {
            current_process->time_slice = TIME_QUANTUM;
            enqueue(&ready_queue, current_process);
            } else {
            // Process is finished
            deallocateMemory(current_process->memoryLowerBound);
            printf("Process %d finished\n", current_process->processID);
        }
    }

}

int main() {
    // Initialize memory, mutexes, and scheduler
    initializeMemory();
  
    

    // Sample programs
    char programs[MAX_PROCESSES][MAX_PROGRAM_SIZE][50] = {
        {"assign 0 10", "assign 1 20", "printFromTo 0 1", ""},
        {"assign 0 file.txt", "assign 1 Hello, World!", "writeFile 0 1", ""},
        {"assign 0 file.txt", "readFile 0", ""}
    };

    // Create PCBs and allocate memory for each process
    PCB pcbs[MAX_PROCESSES];
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcbs[i].processID = i + 1;
        strcpy(pcbs[i].state, "Ready");
        pcbs[i].currentPriority = 1;
        pcbs[i].programCounter = 0;
        int mem_start = allocateMemory(10, sizeof(mem));
        pcbs[i].memoryLowerBound = mem_start;
        pcbs[i].memoryUpperBound = mem_start + 9;
        add_process_to_ready_queue(&pcbs[i]);
    }

    // Run the scheduler
    //run_scheduler(programs);
    schedule(); 

    return 0;
}

