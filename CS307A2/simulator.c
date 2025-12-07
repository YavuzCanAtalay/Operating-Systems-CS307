#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include "sdd.h"
#include "constants.h"
#define Lower_mark 2
#define Upper_mark 3

extern int stop_threads;
extern SortedDispatcherDatabase** processor_queues;


// Thread function for each core simulator thread
void* processJobs(void* arg) { 
    // initialize local variables
    ThreadArguments* my_arg = (ThreadArguments*) arg;
    SortedDispatcherDatabase* my_queue = my_arg->q;
    int my_id = my_arg->id;

    while (!stop_threads) {
        Task* task = fetchTask(my_queue);
        //case1: no task in own queue
        if (task == NULL) {
            for (int i = 0; i < NUM_CORES; i++) {
                if (i == my_id) continue;// skip its own SSD

                Task* stolen = fetchTaskFromOthers(processor_queues[i]);
                if (stolen != NULL) {
                    task = stolen;
                    break;
                }
            }
            if (task == NULL) {
                usleep(100);  // avoid busy waiting
                continue;
            }
        }
        //case2: own queue has task but low on tasks
        if (my_queue->size < Lower_mark) {
            for (int i = 0; i < NUM_CORES; i++) {
                if (i == my_id) continue;
                SortedDispatcherDatabase* other_queue = processor_queues[i];
                if (other_queue->size > Upper_mark) {
                    Task* stolen = fetchTaskFromOthers(other_queue);
                    if (stolen != NULL) {   

                        submitTask(my_queue, task);
                        submitTask(my_queue, stolen);
    
                        task = fetchTask(my_queue);
                        break;
                    }
                }
            }
        }

        //execute task
        executeJob(task, my_queue, my_id);

        if (task->task_duration > 0) {
            submitTask(my_queue, task);
        } else {
            free(task->task_id);
            free(task);
        }
    }

    return NULL;
}


// Do any initialization of your shared variables here.
// For example initialization of your queues, any data structures 
// you will use for synchronization etc.
void initSharedVariables() {
     // Assuming NUM_CORES is defined somewhere globally
    for(int i = 0;i < NUM_CORES;i++) {
        processor_queues[i]->head = NULL;
        processor_queues[i]->tail = NULL;
        processor_queues[i]->size = 0;
        pthread_mutex_init(&processor_queues[i]->lock, NULL);
    }
}