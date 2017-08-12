#ifndef SHM_IPC__H
#define SHM_IPC__H

// Definitions
#include <climits>
#include <cstdint>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
//#include "shmIPC_impl.h"
#include <unistd.h> //temp todo
#include "pevents.h"

#define IPC_BLOCK_COUNT_DEFAULT    512
#define IPC_BLOCK_SIZE_DEFAULT     8192
#define IPC_MEM_PAD                1024 * 8 //
//#define IPC_MAX_ADDR			256

//use clauses
using namespace boost::interprocess;

/***********************************************************************************
 *                      -- Inter-Process Communication class --                    *
 ***********************************************************************************/



/***********************************************************************************
 *                                 -- Block Class --                               *
 ***********************************************************************************/
// Block that represents a piece of data to transmit between the
// clientQueue_ and server
template<int BLOCK_SIZE=IPC_BLOCK_SIZE_DEFAULT>
struct block {
    // Variables
    int32_t                 Next;                        // Next block in the circular linked list //TODO: unsigned instead?
    int32_t                 Prev;                        // Previous block in the circular linked list

    // Done reading and writing flags
    volatile uint32_t       doneRead;          // Flag used to signal that this block has been read
    volatile uint32_t       doneWrite;         // Flag used to signal that this block has been written

    // The size of data in the block
    uint32_t                Amount;                     // Amount of data help in this block
    uint32_t                Padding;                   // Padded used to ensure 64bit boundary

    // Byte array
    uint8_t                 Data[BLOCK_SIZE];            // Data contained in this block

};


/***********************************************************************************
 *                            -- Memory Buffer Class --                            *
 ***********************************************************************************/
// Shared memory buffer that contains everything required to transmit
// data between the clientQueue_ and server
template<int BLOCK_SIZE=IPC_BLOCK_SIZE_DEFAULT, int BLOCK_COUNT=IPC_BLOCK_COUNT_DEFAULT>
struct memBuff {

    // Typedef block's template to make easier
    typedef block<BLOCK_SIZE> Block;

    // Block data, this is placed first to remove the offset (optimisation)
    Block                   m_Blocks[BLOCK_COUNT];    // Array of buffers that are used in the communication

    // Message count
    volatile uint32_t       m_msgCount;

    // Read Cursors
    volatile uint32_t       m_ReadEnd;                    // End of the read cursor
    volatile uint32_t       m_ReadStart;                  // Start of read cursor

    // Write Cursors
    volatile uint32_t       m_WriteEnd;                   // Pointer to the first write cursor, i.e. where we are currently writing to
    volatile uint32_t       m_WriteStart;                 // Pointer in the list where we are currently writing

    // Inter-process semaphores (used as simple "signals")
    // interprocess_semaphore  signal_sem;
    // interprocess_semaphore  avail_sem;

    // Default constructor
    // memBuff() : signal_sem(1), avail_sem(1) { };

    // Signals (events)
    pevent_t                 m_signal;
    pevent_t                 m_avail;
};


/***********************************************************************************
 *                              -- ShmQueue Class --                               *
 ***********************************************************************************/
template<int BLOCK_SIZE=IPC_BLOCK_SIZE_DEFAULT, int BLOCK_COUNT=IPC_BLOCK_COUNT_DEFAULT>
class ShmQueue { // was ShmServer ( to go with client )

    // Typedef'ing Block and MemBuff
    typedef  block<BLOCK_SIZE> Block;
    typedef  memBuff<BLOCK_SIZE,BLOCK_COUNT> MemBuff;


/***********************************************************************************
 *              -- Private Member Functions / Variables (ShmQueue) --              *
 ***********************************************************************************/
private:

    // Internal variables
    std::string             m_addr;            // Address of this server
    uint32_t                m_timeOut;         // Timeout to use for waiting
    named_mutex             *m_initMtx;        // Mutex to use for initialization of shared memory
    shared_memory_object    *m_pShm;           // Shared memory object
    mapped_region           *m_pMapReg;        // Handle to the mapped memory
    MemBuff                 *m_pBuffer;        // Buffer that points to the shared memory buffer for the server

    // Internal functions
    void incrementCount() {
        __sync_fetch_and_add(&m_pBuffer->m_msgCount, 1);
    };


    void decrementCount() {
        __sync_fetch_and_sub(&m_pBuffer->m_msgCount, 1);
    };


/***********************************************************************************
 *               -- Public Member Functions / Variables (ShmQueue) --              *
 ***********************************************************************************/
/***********************************************************************************
 *                   -- Constructor and Destructor (ShmQueue) --                   *
 ***********************************************************************************/
public:
    // Constructor
    ShmQueue() {
        // Set default params
        m_pBuffer = nullptr;

    };

    // Constructor
    ShmQueue(std::string path) {
        // Set default params
        m_pBuffer = nullptr;

        // Set the path
        m_addr = path;

        // create the server
        create();

        //Since creation is done, it is okay to remove mutex
        std::string mtxName = m_addr + "_mtx";
        named_mutex::remove(mtxName.c_str());
    };

    // Destructor
    ~ShmQueue(void) {
        // Close the server
        close();
    };


/***********************************************************************************
 *                    -- IPC Functions and Helpers (ShmQueue) --                   *
 ***********************************************************************************/
    uint32_t getMsgCount() {
        return m_pBuffer->m_msgCount;
    };


    const char *getServerName(void) { return m_addr.c_str(); }

    void setTimeout(uint32_t time) { m_timeOut = time; }

    uint32_t read(void *pBuff, uint32_t buffSize, int32_t msTimeout = INT32_MAX) {
        // Grab a block
        Block *pBlock = get_block_read(msTimeout);
        if (!pBlock) return 0;

        // Copy the data //TODO: add more complicated stuff here to get a size (like sockets does it but coy instead of recv)
        uint32_t dwAmount = std::min(pBlock->Amount, buffSize);
        memcpy(pBuff, pBlock->Data, dwAmount);

        // Return the block
        return_block(pBlock);

        // Success
        return dwAmount;
    };

    uint32_t write(void *pBuff, uint32_t amount, int32_t msTimeout = INT32_MAX) {
        // Grab a block
        Block *pBlock = get_block_write(msTimeout);
        if (!pBlock) return 0;

        // Copy the data
        uint32_t dwAmount = std::min(amount, (uint32_t) BLOCK_SIZE);
        memcpy(pBlock->Data, pBuff, dwAmount);
        pBlock->Amount = dwAmount;

        // Post the block
        post_block(pBlock);

        // Success : TODO Changed to be success?
        return dwAmount;
    };

    typename ::block<BLOCK_SIZE>* get_block_write(int32_t msTimeout = INT32_MAX) {
        // Grab a block to write too
        // Enter a continuous loop
        // (this is to make sure the operation is atomic)
        while(1) {
            // Check if there is room to expand the write start cursor
            long blockIndex = m_pBuffer->m_WriteStart;
            Block *pBlock = &(m_pBuffer->m_Blocks[blockIndex]);
            if (pBlock->Next == m_pBuffer->m_ReadEnd) {
                //Setup timeout value
                boost::posix_time::ptime timeout =
                        microsec_clock::local_time() + boost::posix_time::milliseconds(msTimeout);

                // No room is available, wait for room to become available
//                if (m_pBuffer->avail_sem.timed_wait(timeout))
                if(WaitForEvent(&m_pBuffer->m_avail, msTimeout) != ETIMEDOUT)
                    continue;

                // Timeout
                return nullptr;
            }

            // Make sure the operation is atomic
            if (__sync_val_compare_and_swap(&m_pBuffer->m_WriteStart, blockIndex, pBlock->Next) ==
                blockIndex)
                return pBlock;

            // The operation was interrupted by another thread.
            // The other thread has already stolen this block, try again
            continue;
        }
    };


    void post_block(Block *pBlock) {
        // Set the done flag for this block
        pBlock->doneWrite = 1;

        // Move the write pointer as far forward as we can
        while(1) {
            // Try and get the right to move the pointer
            uint32_t blockIndex = m_pBuffer->m_WriteEnd;
            pBlock = &(m_pBuffer->m_Blocks[blockIndex]);;

            if (__sync_val_compare_and_swap(&pBlock->doneWrite, 1, 0) != 1) {
                // If we get here then another thread has already moved the pointer
                // for us or we have reached as far as we can possible move the pointer
                return;
            }

            // Move the pointer one forward (interlock protected)
            __sync_bool_compare_and_swap(&m_pBuffer->m_WriteEnd, blockIndex, pBlock->Next);

            // Signal availability of more data but only if threads are waiting
//            if (pBlock->Prev == m_pBuffer->m_ReadStart) // dwb temp
//                m_pBuffer->signal_sem.post();
                SetEvent(&m_pBuffer->m_signal);

            //Increment count
            incrementCount();
        }
    };


    // Block functions
    typename ::block<BLOCK_SIZE>* get_block_read(int32_t msTimeout = INT32_MAX) {
        // Grab another block to read from
        // Enter a continuous loop (this is to make sure the operation is atomic)
        while(1) {

            // Check if there is room to expand the read start cursor
            uint32_t blockIndex = m_pBuffer->m_ReadStart;
            Block *pBlock = &(m_pBuffer->m_Blocks[blockIndex]);
            if (pBlock->Next == m_pBuffer->m_WriteEnd) {

                //Setup timeout value
                boost::posix_time::ptime timeout = microsec_clock::local_time() + boost::posix_time::milliseconds(msTimeout);

                // No room is available, wait for room to become available
//                if (m_pBuffer->signal_sem.timed_wait(timeout))
                if(WaitForEvent(&m_pBuffer->m_signal, msTimeout) != ETIMEDOUT)
                    continue;

                // Timeout
                return nullptr;
            }

            // Make sure the operation is atomic
            if (__sync_val_compare_and_swap(&m_pBuffer->m_ReadStart, blockIndex, pBlock->Next) == blockIndex)
                return pBlock;

            // The operation was interrupted by another thread.
            // The other thread has already stolen this block, try again
            continue;
        }
    };


    void return_block(Block *pBlock) {
        // Set the done flag for this block
        pBlock->doneRead = 1;

        // Move the read pointer as far forward as we can
        while(1) {

            // Try and get the right to move the pointer
            uint32_t blockIndex = m_pBuffer->m_ReadEnd;
            pBlock = &(m_pBuffer->m_Blocks[blockIndex]);
            if (__sync_val_compare_and_swap(&pBlock->doneRead, 1, 0) != 1) {

                // If we get here then another thread has already moved the pointer
                // for us or we have reached as far as we can possible move the pointer
                return;
            }

            // Move the pointer one forward (interlock protected)
            __sync_bool_compare_and_swap(&m_pBuffer->m_ReadEnd, blockIndex, pBlock->Next);

            // Signal availability of more data but only if a thread is waiting
//            if (pBlock->Prev == m_pBuffer->m_WriteStart) //dwb temp
//            if (pBlock->Prev == m_pBuffer->m_WriteStart) //dwb temp
//                m_pBuffer->avail_sem.post();
                SetEvent(&m_pBuffer->m_avail);

            //Decrement count
            decrementCount();

        }
    };


    // Create and destroy functions
    void create() {

        //DWB TODO: Add error checking!!

        // Setup / open initialization mutex (TODO: Look into removal later)
        std::string mtxName = m_addr + "_init_mtx";
        named_mutex::remove(mtxName.c_str());
        m_initMtx = new named_mutex (open_or_create, mtxName.c_str());

        // Lock the mutex ( To prevent two processes from creating / initializing shared mem at same time
        scoped_lock<named_mutex> lock(*m_initMtx);

        // Setup name for shared memory
        std::string shmName = m_addr + "_shm";

        //Create (or open if already there) shared memory object
        m_pShm = new shared_memory_object(
                open_or_create,
                shmName.c_str(),
                read_write
        );

        // Map to the file
        if(m_pShm == nullptr) {
            return;
        }

        offset_t size = 0;
        m_pShm->get_size(size);

        //Get the size of the region to determine if the buffer has been created  yet
        if(0 == size) {

            //Set the size of the shared memory region
            m_pShm->truncate(((unsigned long) (sizeof(MemBuff) + IPC_MEM_PAD)));

            //Map the shared region
            m_pMapReg = new mapped_region(
                    *m_pShm,
                    read_write
            );

            // Create the buffer
            m_pBuffer = new(m_pMapReg->get_address()) MemBuff();
            if (m_pBuffer == nullptr)
                return;

            // Create a circular linked list
            int i;
            m_pBuffer->m_Blocks[0].Next = 1;
            m_pBuffer->m_Blocks[0].Prev = (BLOCK_COUNT - 1);
            for (i = 1; i < BLOCK_COUNT - 1; i++) {
                // Add this block into the available list
                m_pBuffer->m_Blocks[i].Next = (i + 1);
                m_pBuffer->m_Blocks[i].Prev = (i - 1);
            }
            m_pBuffer->m_Blocks[i].Next = 0;
            m_pBuffer->m_Blocks[i].Prev = (BLOCK_COUNT - 2);

            // Initialize the 'pointers'
            m_pBuffer->m_msgCount = 0;
            m_pBuffer->m_ReadEnd = 0;
            m_pBuffer->m_ReadStart = 0;
            m_pBuffer->m_WriteEnd = 1;
            m_pBuffer->m_WriteStart = 1;

            // Setup/Create event variables
            int sts = CreateEvent(&m_pBuffer->m_signal, false, false);
            sts = CreateEvent(&m_pBuffer->m_avail, false, false);


            // If the process is not the creator of the shared memory segment then just map to the region
        // and obtain the pointer to the buffer.
        } else {

            //Map the shared region
            m_pMapReg = new mapped_region(
                    *m_pShm,
                    read_write
            );

            //Get the pointer to the mapped region
            m_pBuffer = (MemBuff *) (m_pMapReg->get_address());
            if (m_pBuffer == nullptr)
                return;

        }
    };


    void close(void) {

        // Delete dynamically created memory
        // Delete mutex
        if(m_initMtx) {
            delete m_initMtx;
            m_initMtx = nullptr;
        }

        // Delete shared mem
        if(m_pShm) {
            delete m_pShm;
            m_pShm = nullptr;
        }

        // Delete mapped region
        if(m_pMapReg) {
            delete m_pMapReg;
            m_pMapReg = nullptr;
        }

        // Remove the shared memory
        std::string shmName = m_addr + "_shm";
        shared_memory_object::remove(shmName.c_str());
    };


/***********************************************************************************
 *                        -- Static functions (ShmQueue) --                        *
 ***********************************************************************************/
    static void removeOldQueues(std::string path) {
        // Setup name for shared memory
        std::string shmName = path + "_shm";
        std::string mtxName = path + "_mtx";


        //Remove any memory that might have the same name (from previous runs, etc.)
        named_mutex::remove(mtxName.c_str());
        shared_memory_object::remove(shmName.c_str());
    }
};

//
///***********************************************************************************
// *                            -- ShmClient Class --                                *
// ***********************************************************************************/
//template<int BLOCK_SIZE=IPC_BLOCK_SIZE_DEFAULT, int BLOCK_COUNT=IPC_BLOCK_COUNT_DEFAULT>
//class ShmClient {
//
//    // Typedef'ing a template for block
//    typedef  block<BLOCK_SIZE> Block;
//    typedef  memBuff<BLOCK_SIZE,BLOCK_COUNT> MemBuff;
//
//
///***********************************************************************************
// *            -- Private Member Functions / Variables (ShmClient) --               *
// ***********************************************************************************/
//private:
//
//    // Internal variables
//    std::string                 m_addr;       // Address of this server
//    uint32_t                    m_timeOut;     // Timeout to use for waiting //TODO
//    shared_memory_object        *m_pShm;        // Shared memory object
//    mapped_region               *m_pMapReg;     // Handle to the mapped memory
//    MemBuff                     *m_pBuffer;     // Buffer that points to the shared memory buffer for the server
//
//
//    // Internal functions
//    void incrementCount() {
//        __sync_fetch_and_add(&m_pBuffer->m_msgCount, 1);
//    };
//
//
//
///***********************************************************************************
// *             -- Public Member Functions / Variables (ShmClient) --               *
// ***********************************************************************************/
///***********************************************************************************
// *                  -- Constructor and Destructor (ShmClient) --                   *
// ***********************************************************************************/
//public:
//
//    // Constructor(1) (Default)
//    ShmClient(void) {
//        // Set default params
//        m_pBuffer = NULL;
//    };
//
//    // Constructor(2)
//    ShmClient(std::string path) {
//        sleep(1);
//        // Set default params
//        m_pBuffer = NULL;
//
//        // Determine the name of the memory
//        m_addr = path;
//
//        // Open the shared file
//        std::string shmName = m_addr + "_shm";
//        m_pShm = new shared_memory_object(
//                open_or_create,
//                shmName.c_str(),
//                read_write
//        );
//
//        offset_t size = 0;
//        m_pShm->get_size(size);
//
//        // Map to the file
//        m_pMapReg = new mapped_region(
//                *m_pShm,
//                read_write
//        );
//
//        //Get the pointer to the mapped region
//        m_pBuffer = (MemBuff *) (m_pMapReg->get_address());
//        if (m_pBuffer == nullptr)
//            return;
//
//    };
//
//    // Destructor
//    ~ShmClient(void) {
//
//        // Unmap the memory
//        // DWB TODO: Add un-mapping
//
//    };
//
///***********************************************************************************
// *                    -- IPC Functions and Helpers (ShmClient) --                  *
// ***********************************************************************************/
//    // Exposed functions
//    void setTimeout(uint32_t time) { m_timeOut = time; }
//
//
//    bool isConnected() { return m_pBuffer != nullptr; };
//
//
//    bool waitAvailable(int32_t msTimeout) {
//        //Setup timeout value
//        boost::posix_time::ptime timeout = microsec_clock::local_time() + boost::posix_time::milliseconds(msTimeout);
//
//        // Wait on the available event
//        return m_pBuffer->avail_sem.timed_wait(timeout);
//
//    };
//
//    uint32_t write(void *pBuff, uint32_t amount, int32_t msTimeout = INT32_MAX) {
//        // Grab a block
//        Block *pBlock = get_block(msTimeout);
//        if (!pBlock) return 0;
//
//        // Copy the data
//        uint32_t dwAmount = std::min(amount, (uint32_t) BLOCK_SIZE);
//        memcpy(pBlock->Data, pBuff, dwAmount);
//        pBlock->Amount = dwAmount;
//
//        // Post the block
//        postBlock(pBlock);
//
//        // Fail : TODO? Would this not always fail then? fix this
//        return 0;
//    };
//
//
//    typename ::block<BLOCK_SIZE>* get_block(int32_t msTimeout = INT32_MAX) {
//        // Grab another block to write too
//        // Enter a continuous loop
//        // (this is to make sure the operation is atomic)
//        while(1) {
//
//            // Check if there is room to expand the write start cursor
//            long blockIndex = m_pBuffer->m_WriteStart;
//            Block *pBlock = &(m_pBuffer->m_Blocks[blockIndex]);
//            if (pBlock->Next == m_pBuffer->m_ReadEnd) {
//                //Setup timeout value
//                boost::posix_time::ptime timeout =
//                        microsec_clock::local_time() + boost::posix_time::milliseconds(msTimeout);
//
//                // No room is available, wait for room to become available
//                if (m_pBuffer->avail_sem.timed_wait(timeout))
//                    continue;
//
//                // Timeout
//                return NULL;
//            }
//
//            // Make sure the operation is atomic
//            if (__sync_val_compare_and_swap(&m_pBuffer->m_WriteStart, blockIndex, pBlock->Next) ==
//                blockIndex)
//                return pBlock;
//
//            // The operation was interrupted by another thread.
//            // The other thread has already stolen this block, try again
//            continue;
//        }
//    };
//
//
//    void postBlock(Block *pBlock) {
//        // Set the done flag for this block
//        pBlock->doneWrite = 1;
//
//        // Move the write pointer as far forward as we can
//        while(1) {
//            // Try and get the right to move the pointer
//            uint32_t blockIndex = m_pBuffer->m_WriteEnd;
//            pBlock = &(m_pBuffer->m_Blocks[blockIndex]);;
//
//            if (__sync_val_compare_and_swap(&pBlock->doneWrite, 1, 0) != 1) {
//                // If we get here then another thread has already moved the pointer
//                // for us or we have reached as far as we can possible move the pointer
//                return;
//            }
//
//            // Move the pointer one forward (interlock protected)
//            __sync_bool_compare_and_swap(&m_pBuffer->m_WriteEnd, blockIndex,
//                                         pBlock->Next); // dwb was - InterlockedCompareExchange(&m_pBuffer->m_WriteEnd, pBlock->Next, blockIndex);
//
//
//            // Signal availability of more data but only if threads are waiting
//            if (pBlock->Prev == m_pBuffer->m_ReadStart)
//                m_pBuffer->signal_sem.post();
//
//            //Increment count
//            incrementCount();
//        }
//    };
//
//
//};

#endif