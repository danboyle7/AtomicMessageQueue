//
// Created by user on 8/12/17.
//

#ifndef SHMATOMICMSGQUEUE_EVENTS_H
#define SHMATOMICMSGQUEUE_EVENTS_H

#include <sys/eventfd.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/poll.h>

//Types

struct event_t {
    int state_;
    int fd_;
    pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER;

};

    int CreateEvent(event_t *event) {
    event->fd_ = eventfd(0, 0);
    event->state_ = 0;
}

int SetEvent(event_t *event) {
    pthread_mutex_lock(&event->mtx_);

    //If the state is already 1, then it does not need to be set.
    if(!event->state_) {
        uint64_t set = 1;
        ssize_t  sts = write(event->fd_, &set, sizeof(uint64_t));
        event->state_ = 1;
    }

    pthread_mutex_lock(&event->mtx_);
}


int ClearEvent(event_t *event) {
    uint64_t clear = 0;

    //Lock mutex to write to fd.
    pthread_mutex_lock(&event->mtx_);
    ssize_t  sts = write(event->fd_, &clear, sizeof(uint64_t));
    event->state_ = 0;
    pthread_mutex_unlock(&event->mtx_);

    //Check if there was an error
    if(sts == -1)
        return -1;

    //Success
    return 0;
}

//Return 1 for normal, 0 for timeout, -1 for error.
int WaitForEvent(event_t *event, uint32_t msTimeout) {
    struct pollfd ufds[1];

    //If the state is not set, wait.
    if(!event->state_) {
        ufds[0].fd = event->fd_;
        ufds[0].events = POLLIN;
        int sts = poll(ufds, 1, msTimeout);

        //Check to see if a timeout occurred
        if(sts <= 0)
            return sts;

        //Wait successful
        ClearEvent(event);
        return 1;

    //If the state is already set, clear the event and return.
    } else {
        ClearEvent(event);
        return 1;
    }
}

int DestroyEvent() {
    //TODO
}

#endif //SHMATOMICMSGQUEUE_EVENTS_H
