//
// Created by user on 8/12/17.
//

#ifndef SHMATOMICMSGQUEUE_EVENTS_H
#define SHMATOMICMSGQUEUE_EVENTS_H

#include <unistd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>

#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace boost::interprocess;

struct event_t {
    int state_;
    bool kill_;
    interprocess_mutex mtx_;
    interprocess_condition cond_;
};

// Might not need
inline void CreateEvent(event_t *event) {
    // Set the state to 0
    event->state_ = 0;
}

inline void SetEvent(event_t *event) {
    // Lock mutex to eliminate posisble race conditions between setting and waiting on state
    scoped_lock<interprocess_mutex> lock(event->mtx_);

    // Set the event if it has not been already
    __sync_bool_compare_and_swap(&event->state_, 0, 1);

    // Notify anyone waiting
    event->cond_.notify_all();
}


inline ssize_t ClearEvent(event_t *event) {
    scoped_lock<interprocess_mutex> lock(event->mtx_);

    // Clear the event state if it has not been already
    int ret = __sync_val_compare_and_swap(&event->state_, 1, 0);
}

// Return 1 for normal, 0 for timeout, -1 for error.
inline ssize_t WaitForEvent(event_t *event, uint32_t msTimeout) {

    // If the state is not already been set.
    if(!event->state_) {
        bool ret = true;

        //Block for when to lock the mutex
        { 
            //Lock mutex to make sure no one touches the state 
            scoped_lock<interprocess_mutex> lock(event->mtx_);

            // Setup timeout value
            boost::posix_time::ptime timeout = microsec_clock::universal_time() + boost::posix_time::milliseconds(msTimeout);

            while(!event->state_ && ret && !event->kill_) {
                // Wait (timed)
                ret = event->cond_.timed_wait<scoped_lock<interprocess_mutex>>(lock, timeout);
            }
        }

       //Clear the event and return 
       ClearEvent(event);

       //If kill is set, return -1
       if(event->kill_) return -1;
       
       return ret ? 1 : 0;

    // If state is already set, no need to block. Just clear event and continue.
    } else {

        // Clear the event and then return true.
        ClearEvent(event);

        // Return success
        return 1;
    }
}

// Wakes up anything waiting on this event (to prevent process getting stuck when the object is destructed)
// The rest will be done by the event item constructor
inline void DestroyEvent(event_t *event) {
    //Set kill and Wake up the event
    event->kill_ = true;

    // Notify anyone waiting to wakeup
    pthread_cond_broadcast(&event->cond_); 

#endif //SHMATOMICMSGQUEUE_EVENTS_H
