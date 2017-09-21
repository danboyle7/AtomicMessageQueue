//
// Created by user on 8/1/17.
//

#ifndef SHMIPC_MESSAGEQUEUE2_H
#define SHMIPC_MESSAGEQUEUE2_H

//C system defined includes
#include <csignal>
#include <cstdint>
#include <climits>

//C++ system defined includes
#include <string>
#include <algorithm>

//User defined includes
#include <boost/interprocess/ipc/message_queue.hpp>
// #include "boostmq.hpp"

//use clauses
using namespace boost::interprocess;

//Defines and typedefs

// MessageQueue class
class MessageQueue {
public:

/***********************************************************************************
 *                  -- Constructor and destructor (MessageQueue) --                *
 ***********************************************************************************/

    /*
     * Function name: MessageQueue (MessageQueue)
     * ------------------------------
     * Description:  Default MessageQueue Constructor. It sets appropriate member fields.
     * Parameters:   None
     * Return value: Nothing.
     */
    MessageQueue() { } //Default Constructor


    /*
     * Function name: MessageQueue (MessageQueue)
     * ------------------------------
    * Description:  MessageQueue Constructor. It sets appropriate member fields.
    * Parameters:   TODO
    * Return value: Nothing.
    */
    MessageQueue(std::string path, int max_msgs, int msg_size, int flags) {
        //Set attributes for the queue
        max_queue_msgs_ = max_msgs;
        max_queue_msg_size_ = msg_size;

        //Set additional class variables
        shared_path_ = path;
        connected_ = false;

    }


    /*
     * Function name: ~MessageQueue (MessageQueue)
     * ------------------------------
     * Description:  Custom destructor that will close the underlying queue.
     * Parameters:   None
     * Return value: Nothing
     */
    ~MessageQueue() {
        closeQueue();
    }

/***********************************************************************************
 *                     -- Setup functions (MessageQueue) --                        *
 ***********************************************************************************/
    /*
     * Function name: connect (MessageQueue)
     * ------------------------------
     * Description:  Opens the message queue
     * Parameters:   None
     * Return value: Returns 0 if successful, otherwise -1.
     */
    int connect() {

        try {
            //Create the queue
            queue_ = new message_queue(open_or_create, shared_path_.c_str(), max_queue_msgs_, max_queue_msg_size_);

        } catch(std::exception &e) {
            std::cout << "Here? " << e.what() << std::endl; //TODO: remote later
            return -1;
        };

        //Set connected to true
        if(queue_ == nullptr) {
            return -1;
        }

        connected_ = true;
        return 0;
    }


    /*
     * Function name: registerSignal(MessageQueue)
     * ------------------------------
     * Description:  NOTE: This function does nothing for shared memory implementation. It is just
                           here as place holder and will be removed if this implementation is used.
     * Parameters:   signal - The signal the queue should raise when receiving messages
     * Return value: Returns 0 if it successfully registers the queue for signal notifications,
     *               otherwise -1.
     */
    int registerSignal(int signal, __sighandler_t handler) {
        //Return error if not connected
        if(!connected_) return -1;

        return 0;
    }

/***********************************************************************************
 *                    - Send and Rev Functions (MessageQueue) --                   *
 ***********************************************************************************/

    /*
     * Function name: send (MessageQueue)
     * ------------------------------
     * Description:  Sends a message to the current(this) process's message queue_id.
     * Parameters:   msg - Message to be sent
     *               priority - message priority (0 is lowest).
     * Return value: Returns 1 if successful, 0 if timeout reached, otherwise -1.
     */
    int send(char *buffer, size_t msg_length, unsigned int priority) {
        // Return error if not connected or if trying to send length larger than max_queue_msg_size_
        if(!connected_ || (msg_length > max_queue_msg_size_)) return -1;

        try {
            // Write (send) the buffer to the queue.
            boost::posix_time::ptime timeout = microsec_clock::universal_time() + boost::posix_time::milliseconds(timeout_);
            int sts = queue_->try_send(buffer, (uint32_t ) msg_length, priority);//, timeout);

            // Timeout was reached
            if(0 == sts) return 0;

                //The full message was not sent, return failure.
            else return 1;

        } catch(...) {
            std::cout << "Exception caught in MessageQueue::send()" << std::endl;
            return  -1;
        }

        return 1;
    }

    /*
     * Function name: receive (MessageQueue)
     * ------------------------------
     * Description:  Receives a message from the queue.
     * Parameters:   msg - Allocated message object to place the received message in.
     *               priority - Used to pass back priority of the message. If not needed, put NULL.
     * Return value: Returns the number of bytes received from the queue:
     *               > 0 - Received a message of that many bytes total
     *                 0 - Timeout reached. (No messages) (TODO: This might be a timeout now since it "blocks")
     *                -1 - Error receiving message from the queue
     */
    int receive(char *buffer, unsigned int *priority) { //TODO: remove the receive priority later
        std::string temp;
        int ret_val = 0;

        //Return error if not connected
        if(!connected_) return -1;

        try {
            message_queue::size_type recvd_size = 0;

            unsigned int prio = 0;

            // Get the next message in the queue.
            boost::posix_time::ptime timeout = microsec_clock::universal_time() + boost::posix_time::milliseconds(timeout_);
            ret_val = (int) queue_->timed_receive(buffer,max_queue_msg_size_, recvd_size, prio, timeout); //todo change return type

            // Timeout was reached
            if(!ret_val) return 0;
            else ret_val = (int) recvd_size;

        } catch(...) {
            std::cout << "Exception caught in MessageQueue::receive()" << std::endl;
            return  -1;
        }

        return ret_val;
    }


/***********************************************************************************
 *                   - Additional Queue Functions (MessageQueue) --                *
 ***********************************************************************************/

    /*
    * Function name: clearQueue (MessageQueue)
    * ------------------------------
    * Description:  Returns if the message queue has been connected.
    * Parameters:   None
    * Return value: Returns true if connected, and false otherwise.
    */
    bool isConnected() {
        return connected_;
    }


    /*
     * Function name: getMessageCount (MessageQueue)
     * ------------------------------
     * Description:  Gets the current number of messages in the message queue.
     * Parameters:   None
     * Return value: Current number of message in queue if connected. Otherwise returns -1.
     */
    int getMessageCount() { //TODO: implement in the underlying impl. Need new count var with atomic increment
        //Return error if not connected
        if(!connected_) return -1;

        //Get the size of the mapped queue
        try {
            return queue_->get_num_msg();
        } catch(...) {
            std::cout << "Exception caught in MessageQueue::getMessageCount()" << std::endl;
            return  -1;
        }
    }


    /*
     * Function name: clearQueue (MessageQueue)
     * ------------------------------
     * Description:  Removes any message that currently is in the message queue.
     * Parameters:   None
     * Return value: Nothing
     */
    void clearQueue() {
        // Check if themax_queue_msg_size_ queue is connected
        if(!connected_) {
            char *buffer = new char[max_queue_msg_size_]; //TODO: change when template is added

            // Loop through the queue til it is empty
            while(getMessageCount() > 0) {
                message_queue::size_type recvd_size = 0;
                unsigned int temp = 0;
                queue_->try_receive(buffer,max_queue_msg_size_, recvd_size, temp);
            }
            
            //Delete the buffer
            delete buffer;
        }


    }


    /*
     * Function name: closeQueue (MessageQueue)
     * ------------------------------
     * Description:  Removes signal notifications for the queue, removes it from the system
     *               and then closes the file descriptor corresponding to the queue.
     * Parameters:   None
     * Return value: Nothing
     */
    void closeQueue() {
        try {
            //Close the queue
            //queue_->close();

            //Remove dynamically allocaed memory
            if(queue_) {
                delete queue_;
                queue_ = nullptr;
            }

            //Remove the queue from the system
            message_queue::remove(shared_path_.c_str());
            //Catch any exceptions thrown
        } catch (...) { }
    }


/***********************************************************************************
 *                       -- Setter functions (MessageQueue) --                     *
 ***********************************************************************************/
    /*
     * Function name: setTimeout (MessageQueue)
     * ------------------------------
     * Description:  Set timeout value for send and receive operations
     * Parameters:   msec - timeout value in milliseconds
     * Return value: Nothing
     */
    void setTimeout(uint32_t msec) { timeout_ = msec; }


/***********************************************************************************
 *                       -- Static functions (MessageQueue) --                     *
 ***********************************************************************************/

    /*
     * Function name: removeOldQueues (MessageQueue)
     * ------------------------------
     * Description:  Removes old queues in shared memory that have been previously created
     * Parameters:   None
     * Return value: Nothing
     */
    static void removeOldQueues(std::string path) {
        message_queue::remove(path.c_str()); //todo: fill in with template param.
    }


private:

    //While one is uses will depend on whether this MessageQueue is a server / clientQueue_
    uint32_t timeout_ = INT32_MAX;
    int max_queue_msgs_;
    int max_queue_msg_size_;
    message_queue *queue_;
    std::string shared_path_ = "NULL";

    //Status
    bool connected_;


};

#endif //SHMIPC_MESSAGEQUEUE2_H
