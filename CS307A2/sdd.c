#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "sdd.h"

// Do your SortedDispatcherDatabase implementation here. 
// Implement the 3 given methods here. You can add 
// more methods as you see necessary.

void submitTask(SortedDispatcherDatabase* q, Task* _task) {
    //can be called by owner thread
    //q->lock.lock;
    pthread_mutex_lock(&q->lock);
    if(q->head == NULL) {
        TaskNode* new_node = malloc (sizeof(TaskNode));
        new_node -> task = _task;
        new_node -> next = NULL;
        q -> head = new_node;
        q -> tail = new_node;
        q->size++;
        pthread_mutex_unlock(&q->lock);
        return;
    } else {
        int duration = _task -> task_duration;
        TaskNode* current = q->head;
        TaskNode* previous = NULL;
        while((current != NULL) && (current->task->task_duration <= duration)) {
            previous = current;
            current = current->next;
        }
        if(current == NULL){ // Basically last element
            TaskNode* new_node = malloc (sizeof(TaskNode));
            new_node -> task = _task;
            new_node -> next = NULL;
            previous->next = new_node;
            q -> tail = new_node;
            q->size++;
            pthread_mutex_unlock(&q->lock);
            return;
        }else if(previous == NULL){ //First element
            TaskNode* new_node = malloc (sizeof(TaskNode));
            new_node -> task = _task;
            new_node -> next = q->head;
            q -> head = new_node;
            q->size++;
            pthread_mutex_unlock(&q->lock);
            return;
        }else{ //Middle element
        TaskNode* new_node = malloc (sizeof(TaskNode));
        new_node -> task = _task;
        new_node -> next = current;
        previous -> next = new_node;
        q->size++;
        pthread_mutex_unlock(&q->lock);
        return;
        }
    }
}


Task* fetchTask(SortedDispatcherDatabase* q) {
    //implement lock
    //only can be implemented by owner thread
    //if SDD empty return NULL
    pthread_mutex_lock(&q->lock);
	TaskNode* current = q->head;
    if (current == NULL){
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    q->size--;
    Task* fetched_task = current->task;
    q->head = current->next;
    free(current);
    pthread_mutex_unlock(&q->lock);
    return fetched_task;
}

Task* fetchTaskFromOthers(SortedDispatcherDatabase* q) {
	//remove highest duration task from q
    //can be called by other threads
    //if SSD empty reutnr NULL
    //implement lock
    pthread_mutex_lock(&q->lock);
    if(q->head == NULL){
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    else if((q->head == q->tail)){
        TaskNode* fetched_node = q->head;
        Task* fetched_task_data = fetched_node->task;
        q->head = NULL;
        q->tail = NULL;
        q->size--;
        free(fetched_node);
        pthread_mutex_unlock(&q->lock);
        return fetched_task_data;
    }
    else{
        TaskNode* current = q->head;
        while(current->next != q->tail) {
            current = current->next;
        }
        TaskNode* fetched_task = q->tail;
        Task* highest_element = fetched_task->task;
        current -> next = NULL;
        q->tail = current;
        q->size--;
        free(fetched_task);
        pthread_mutex_unlock(&q->lock);
        return highest_element;
    }
}

void print_queue(SortedDispatcherDatabase* q, int core_id) {
    pthread_mutex_lock(&q->lock);
    char buffer[10000];
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
    "Core %d queue [size=%d]: ", core_id, q->size);
    
    
    if (q->head == NULL) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, 
        "(empty)\n");
        pthread_mutex_unlock(&q->lock);
        printf("%s", buffer);
        return;
    }
    TaskNode* current = q->head;
    while(current != NULL) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "%s(%d)", current->task->task_id, current->task->task_duration);
        if(current->next != NULL) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, " -> ");
        }
        current = current->next;
    }
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
    pthread_mutex_unlock(&q->lock);
    printf("%s", buffer);
}