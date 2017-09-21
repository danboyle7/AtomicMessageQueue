//
// Created by user on 8/12/17.
//

#ifndef SHMATOMICMSGQUEUE_EVENTS_H
#define SHMATOMICMSGQUEUE_EVENTS_H

#include <unistd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>


struct event_t {
    int state_;
    bool kill_;
    pthread_mutex_t mtx_;
    pthread_cond_t cond_;
};

// Might not need ( Fold into constructor )
inline void CreateEvent(event_t *event) {
    // Set the state to 0
    event->state_ = 0;
    event->kill_ = false;

    //Set up condition variable and mutex
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&(event->cond_), &cond_attr);
    pthread_condattr_destroy(&cond_attr);

    pthread_mutexattr_t m_attr;
    pthread_mutexattr_init(&m_attr);
    pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(event->mtx_), &m_attr);
    pthread_mutexattr_destroy(&m_attr);
}

inline void SetEvent(event_t *event) {
    // Lock mutex to eliminate posisble race conditions between setting and waiting on state
    // pthread_mutex_lock(&event->mtx_);

    // Set the event if it has not been already
    __sync_bool_compare_and_swap(&event->state_, 0, 1);

    // Notify anyone waiting
    pthread_cond_broadcast(&event->cond_); 

    //Unlock the mutex
    // pthread_mutex_unlock(&event->mtx_);
}


inline ssize_t ClearEvent(event_t *event) {
    // Lock mutex to eliminate posisble race conditions between setting and waiting on state
    // pthread_mutex_lock(&event->mtx_);

    // Clear the event state if it has not been already
    __sync_bool_compare_and_swap(&event->state_, 1, 0);

    //Unlock the mutex
    // pthread_mutex_unlock(&event->mtx_);
}

// Return 1 for normal, 0 for timeout, -1 for error.
inline ssize_t WaitForEvent(event_t *event, uint32_t msTimeout) {

    // If the state is not already been set.
    if(!event->state_) {
        int ret = 0;

        // Lock mutex to eliminate posisble race conditions between setting and waiting on state
        pthread_mutex_lock(&event->mtx_);

        // Setup timeout value
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec = msTimeout * 1000000;

        //Wait til condition has been signalled
        while(!event->state_ && ret == 0 && !event->kill_) {
            // Wait (timed)
            ret = pthread_cond_timedwait(&event->cond_, &event->mtx_, &ts);
        }

        
        //Unlock the mutex
        pthread_mutex_unlock(&event->mtx_);

       //Clear the event and return 
       ClearEvent(event);
       
       //Check if there is a kill event
       if(event->kill_) return -1;

       //Check return status and return correct value
       if(ret == 0) return 1;
       else if(ret == ETIMEDOUT) return 0;
       else return -1;

    // If state is already set, no need to block. Just clear event and continue.
    } else {

        // Clear the event and then return true.
        ClearEvent(event);

        // Return success
        return 1;
    }
}

// Wakes up anything waiting on this event (to prevent process getting stuck when the object is destructed)
inline void DestroyEvent(event_t *event) {

    //Set kill and Wake up the event
    event->kill_ = true;

    // Notify anyone waiting to wakeup
    pthread_cond_broadcast(&event->cond_); 

}

#endif //SHMATOMICMSGQUEUE_EVENTS_H
