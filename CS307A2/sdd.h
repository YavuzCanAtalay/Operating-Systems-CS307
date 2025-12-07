#ifndef SDD_H
#define SDD_H

// Structs and methods for SortedDispatcherDatabase, you can use additional structs 
// and data structures ON TOP OF the ones provided here.

// **********************************************************
typedef struct SortedDispatcherDatabase SortedDispatcherDatabase;

typedef struct ThreadArguments {
    SortedDispatcherDatabase* q;
    int id;
} ThreadArguments;

typedef struct Task {
    char* task_id;
    int task_duration;
	double cache_warmed_up;
	SortedDispatcherDatabase* owner;
} Task;

typedef struct TaskNode {
    Task* task;
    struct TaskNode* next;
} TaskNode;

// TODO: You can modify this struct and add any 
// fields you may needfNode
struct SortedDispatcherDatabase {
    TaskNode* head;
    TaskNode* tail;
    pthread_mutex_t lock;
    int size;
};

// **********************************************************

// SortedDispatcherDatabase API **********************************************************
void submitTask(SortedDispatcherDatabase* q, Task* _task);
Task* fetchTask(SortedDispatcherDatabase* q);
Task* fetchTaskFromOthers(SortedDispatcherDatabase* q);
void print_queue(SortedDispatcherDatabase* q, int core_id);
// You can add more methods to Queue API
// .
// . 
// **********************************************************


// Your simulator threads should call this function to simulate execution. 
// Don't change the function signature, you can use the provided implementation of 
// this function. We will use potentially different implementations while testing.
void executeJob(Task* task, SortedDispatcherDatabase* my_queue, int my_id );
void* processJobs(void* arg);
void initSharedVariables();
#endif