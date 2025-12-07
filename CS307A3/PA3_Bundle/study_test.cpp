#include <semaphore.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "Study.h"
using namespace std;

Study* study = nullptr;

void Study::start() {
    sleep(2);
}

void dummy_thread() {
    study->arrive();
    study->start();
    study->leave();

}


int main(int argc, char *argv[]){
    int studentNum = atoi(argv[1]);
    int sessionSize = atoi(argv[2]);
    int tutorPresent = atoi(argv[3]);
    vector<pthread_t> allThreads;
    try {
        study = new Study(sessionSize, tutorPresent);   
    } catch (const std::exception& e) {
        // Catch all exceptions derived from std::exception
        printf("Exception caught:  %s\n", e.what());
        return 0;
    }

    for(int i=0;i<studentNum;i++){
        pthread_t thread;
        pthread_create(&thread,NULL,(void *(*)(void *))dummy_thread,NULL);
        allThreads.push_back(thread);
    }
    for(int i=0;i<allThreads.size();i++)
        pthread_join(allThreads[i],NULL);
    printf("The Main terminates.\n");
    return 0;
}