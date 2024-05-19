#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_SIZE 60
#define MAX_PROCESSES 10
#define BLOCK_SIZE 10
#define NUM_QUEUES 4
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
} PCB;

typedef struct {
    int start;
    int size;
    int processID;
} MemoryBlock;

typedef struct {
    char name[50];
    int data;
} MemoryWord;

typedef struct {
    char name[50];
    int flag;
} Mutex;

typedef struct Process {
    int processID;
    struct Process* next;
} Process;

typedef struct {
    Process* front;
    Process* rear;
} Queue;

typedef struct {
    Queue queues[NUM_QUEUES];
    int quantum[NUM_QUEUES];
} Scheduler;



Scheduler scheduler = { .quantum = {1, 2, 4, 8} }; // Quantum doubles for each lower priority level
MemoryBlock memoryBlocks[MEMORY_SIZE];
char memory[MEMORY_SIZE];
int nextFreeBlock = 0;
// Assuming you have an array to represent the mutexes
Mutex mutexes[3];
// Assuming you have an array to represent the memory
MemoryWord mem[100];
Queue readyQueue = {NULL, NULL};
Queue blockedQueue = {NULL, NULL};
pthread_mutex_t mutex;
pthread_mutex_t userInputMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t userOutputMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileAccessMutex = PTHREAD_MUTEX_INITIALIZER;
MemoryBlock freeBlocks[MAX_PROCESSES];
int nextFreeBlock = 0;

void initializeMemory() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        freeBlocks[i].start = i * BLOCK_SIZE;
        freeBlocks[i].size = BLOCK_SIZE;
    }
}

void allocateMemory(int processID, int size) {
    if (nextFreeBlock + size > MEMORY_SIZE) {
        printf("Not enough memory to allocate %d blocks for process %d\n", size, processID);
        return;
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

void initMutex() {
    pthread_mutex_init(&mutex, NULL);
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


void enqueue(Queue* queue, int processID) {
    Process* newProcess = malloc(sizeof(Process));
    newProcess->processID = processID;
    newProcess->next = NULL;

    if (queue->rear != NULL) {
        queue->rear->next = newProcess;
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
                        strcpy(mem[i].name, varName);
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
                    if (mutexes[i].flag == 0) {
                        mutexes[i].flag = 1;
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
                    if (mutexes[i].flag == 1) {
                        mutexes[i].flag = 0;
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

void initScheduler() {
    for (int i = 0; i < NUM_QUEUES; i++) {
        scheduler.queues[i] = (Queue){NULL, NULL};
    }
}

void schedule() {
    for (int i = 0; i < NUM_QUEUES; i++) {
        if (!isEmpty(&scheduler.queues[i])) {
            PCB* process = dequeue(&scheduler.queues[i]);
            executeProcess(process, scheduler.quantum[i]);
            
            if (process->state != TERMINATED) {
                enqueue(&scheduler.queues[i+1], process);  // Move to lower priority queue
            }
            break;
        }
    }
}

void addProcess(PCB* process) {
    enqueue(&scheduler.queues[0], process);  // New processes start in highest priority queue
}

void executeProcess(PCB* process, int quantum) {
    process->state = RUNNING;
    for (int i = 0; i < quantum; i++) {
        process->programCounter++;
        if (process->programCounter == process->memoryUpperBound) {
            process->state = TERMINATED;
            break;
        }
    }
}


int main() {
    // Initialize memory
    initializeMemory();

    // Allocate memory for processes
    allocateMemory(1, 10);
    allocateMemory(2, 20);
    allocateMemory(3, 30);

    // Create PCBs for processes
    PCB process1 = {1, "NEW", 0, 0, 0, 10};
    PCB process2 = {2, "NEW", 0, 0, 10, 30};
    PCB process3 = {3, "NEW", 0, 0, 30, 60};

    // Add processes to the scheduler
    addProcess(&process1);
    addProcess(&process2);
    addProcess(&process3);

    // Initialize the scheduler
    initScheduler();

    // Run the scheduler
    schedule();

    return 0;
}