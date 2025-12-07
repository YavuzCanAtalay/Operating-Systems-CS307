#ifndef STUDY_H
#define STUDY_H

#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>
#include <stdio.h>
#include <unistd.h>

class Study {
public:
    int sessionSize;
    int tutorPresent;
    int totalCapacity;     
    int current_students;  
    bool sessionstarted;
    
    pthread_t tutor_ID;
    
    sem_t Capacity;        // Room capacity
    sem_t TutorSpeaking;   // Students wait on this in leave() if tutor exists, works as a barrier
    pthread_mutex_t mtx;   // Protects shared state variables such as current_students and sessionstarted
    
    // Caller thread is tutor or not?mç
    
    bool isTutor() {
        if (tutorPresent == 0) return false;
        return pthread_equal(pthread_self(), tutor_ID); // Necessary???
    }

    Study(int sessionSize, int tutorPresent) : 
        sessionSize(sessionSize), tutorPresent(tutorPresent) {
        
        if (sessionSize <= 0) {
            throw std::invalid_argument("An error occurred.");
        }
        if (tutorPresent != 0 && tutorPresent != 1) {
            throw std::invalid_argument("An error occurred.");
        }
        else{
        this->totalCapacity = sessionSize + tutorPresent; // Include tutor as a student if present
        this->current_students = 0;
        this->sessionstarted = false;

        sem_init(&Capacity, 0, totalCapacity);
        sem_init(&TutorSpeaking, 0, 0); 
        pthread_mutex_init(&mtx, NULL);
        }
    }

    ~Study() {
        sem_destroy(&Capacity);
        sem_destroy(&TutorSpeaking);
        pthread_mutex_destroy(&mtx);
    }

    void arrive() {
        printf("Thread ID: %lu | Status: Arrived at the IC.\n", pthread_self());

        // If a session is running, this blocks the thread here until the session ends.
        // Check if all students should wait outside unlimitedly or not
        sem_wait(&Capacity);

        // 3. Enter Critical Section
        pthread_mutex_lock(&mtx);
        current_students++;

        // If session isn't full
        if (current_students != totalCapacity) {
            printf("Thread ID: %lu | Status: Only %i students inside, studying individually.\n", pthread_self(), current_students);
        } else {
            // Session can start if capacity is full
            sessionstarted = true;
            if (tutorPresent) {
                tutor_ID = pthread_self(); // The last arrival becomes the tutor, but a student
            }
            printf("Thread ID: %lu | Status: There are enough students, the study session is starting.\n", pthread_self());
        }
        pthread_mutex_unlock(&mtx);
    }
    void start(); 

    void leave() {
        pthread_mutex_lock(&mtx);

        // CASE 1: Session hasn't started while waiting

        if (!sessionstarted) {
            printf("Thread ID: %lu | Status: No group study formed while I was waiting, I am leaving.\n", pthread_self());
            current_students--;
            // Since they are leaving individually, we release one spot for the next person
            sem_post(&Capacity); 
            pthread_mutex_unlock(&mtx);
            return;
        }

        // CASE 2: Session Started
        if (tutorPresent) { // With Tutor
            if (isTutor() == true) {
                // Tutor speaks first
                printf("Thread ID: %lu | Status: Session tutor speaking, the session is over.\n", pthread_self());
                
                // Wake up all students waiting for the tutor
                // We post (totalCapacity - 1) times because that is how many students are waiting, exclude tutor with -1
                for(int i = 0; i < (totalCapacity - 1); i++) {
                    sem_post(&TutorSpeaking);
                }
            } else {
                // student block, wait for tutor to speak
                // unlock mutex while waiting to avoid deadlock
                pthread_mutex_unlock(&mtx);
                sem_wait(&TutorSpeaking); 
                pthread_mutex_lock(&mtx); // Re-acquire lock to print and decrement
                
                printf("Thread ID: %lu | Status: I am a student and I am leaving.\n", pthread_self());
            }
        } else {
            // No tutor, everyone just leaves naturally
            printf("Thread ID: %lu | Status: I am a student and I am leaving.\n", pthread_self());
        }
        current_students--;
        //last student to leave resets the session
        if (current_students == 0) {
            printf("Thread ID: %lu | Status: All students have left, the new students can come.\n", pthread_self());
            sessionstarted = false;
            
            // Restore capacity ONLY when the room is empty.
            // This allows waiting students to enter.
            for(int i = 0; i < totalCapacity; i++) {
                sem_post(&Capacity);
            }
        }

        pthread_mutex_unlock(&mtx);
    }
};

#endif