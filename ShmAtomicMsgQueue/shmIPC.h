#ifndef SHM_IPC__H
#define SHM_IPC__H

// C Definitions
#include <climits>
#include <cstdint>

// C++ Definitions 
#include <stdexcept>
#include <exception>

// #include "events.h" //boost cond
#include "events2.h"   //pthread cond

//Temp
#include <stdio.h>

// #define USING_BOOST

//If boost is defined, then include these headers
#if defined __gnu_linux__ && defined USING_BOOST
  
  #include <boost/interprocess/sync/interprocess_mutex.hpp>
  #include <boost/interprocess/sync/named_mutex.hpp>
  #include <boost/interprocess/sync/named_condition.hpp>
  #include <boost/interprocess/sync/scoped_lock.hpp>
  #include <boost/interprocess/shared_memory_object.hpp>
  #include <boost/interprocess/mapped_region.hpp>
  #include <boost/date_time/posix_time/posix_time.hpp>

  //use clauses
  using namespace boost::interprocess;

//Otherwise include headers for system shared memory
#elif defined __gnu_linux__ && !defined USING_BOOST
    
    #include <fcntl.h>
    #include <sys/shm.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    
#else //POSIX 
    
    #error "This package is not supported by your system" 

#endif


//#include "shmIPC_impl.h"
#include <unistd.h> //temp todo
#include "events.h"

#define IPC_BLOCK_COUNT_DEFAULT    512
#define IPC_BLOCK_SIZE_DEFAULT     8192
#define IPC_MEM_PAD                1024 * 8 //
//#define IPC_MAX_ADDR			256


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
    volatile uint32_t       doneRead;                   // Flag used to signal that this block has been read
    volatile uint32_t       doneWrite;                  // Flag used to signal that this block has been written

    // The size of data in the block
    uint32_t                Amount;                     // Amount of data help in this block
    uint32_t                Padding;                    // Padded used to ensure 64bit boundary

    // Byte array
    uint8_t                 Data[BLOCK_SIZE];           // Data contained in this block

};


/***********************************************************************************
 *                            -- Memory Buffer Class --                            *
 ***********************************************************************************/
// Shared memory buffer that contains everything required to transmit
// data between the clientQueue_ and server
template<int BLOCK_SIZE=IPC_BLOCK_SIZE_DEFAULT, int BLOCK_COUNT=IPC_BLOCK_COUNT_DEFAULT>
struct memBuff {

    // Queue attributes
    // volatile uint32_t      m_blocksize;
    // volatile uint32_t      m_blockCount;

    // Typedef block's template to make easier
    typedef block<BLOCK_SIZE> Block;

    // Block data, this is placed first to remove the offset (optimisation)
    Block                   m_blocks[BLOCK_COUNT];        // Array of buffers that are used in the communication

    // Message count
    volatile uint32_t       m_msgCount;

    // Read Cursors
    volatile uint32_t       m_readEnd;                    // End of the read cursor
    volatile uint32_t       m_readStart;                  // Start of read cursor

    // Write Cursors
    volatile uint32_t       m_writeEnd;                   // Pointer to the first write cursor, i.e. where we are currently writing to
    volatile uint32_t       m_writeStart;                 // Pointer in the list where we are currently writing

    // Inter-process semaphores (used as simple "signals") - not quite what is needed....
    // interprocess_semaphore  signal_sem;
    // interprocess_semaphore  avail_sem;

    // Default constructor
    // memBuff() : signal_sem(1), avail_sem(1) { };

    // Signals (events)
    event_t                 m_signal;
    event_t                 m_avail;
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
    // named_mutex             *m_initMtx;        // Mutex to use for initialization of shared memory

#ifdef USING_BOOST
    shared_memory_object    *m_pShm;           // Shared memory object
    mapped_region           *m_pMapReg;        // Handle to the mapped memory
#else //POSIX 
    int                     m_shmfd;
#endif

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
        int sts = create();

        if(-1 == sts) {
            throw std::runtime_error("Failed to initialize queue");
        }

        //Since creation is done, it is okay to remove mutex
        // std::string mtxName = m_addr + "_mtx";
        // named_mutex::remove(mtxName.c_str());
    };

    // Destructor
    ~ShmQueue() {        
        // Close the server
        disconnect();
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
        int amount = 0;
        Block *pBlock;

        //Loop through til there is data to read.
        do {
            pBlock = get_block_read(msTimeout);
            if (pBlock == nullptr) return 0;

            //get amount
            amount = pBlock->Amount;

            //If amount is 0
            if(0 == amount) {
                return_block(pBlock);
            }

        } while(0 == amount);

        // printf("\nReceiving from blk # %d\n", pBlock->Next-1);

        // Copy the data
        uint32_t dwAmount = std::min(pBlock->Amount, buffSize);
//        printf("pBlock->Amount: %d\n", dwAmount);
        memcpy(pBuff, pBlock->Data, dwAmount);

        // Return the block
        return_block(pBlock);

        //TODO: temp
//        printf("(End of read)R-S: %ld, R-E: %ld, W-S: %ld, W-E: %ld\n", (long)m_pBuffer->m_readStart, (long)m_pBuffer->m_readEnd, (long)m_pBuffer->m_writeStart, (long)m_pBuffer->m_writeEnd);

        //Decrement count
        decrementCount(); // todo: Might need to be somewhere else

        // Success
        return dwAmount;
    };

    uint32_t write(void *pBuff, uint32_t amount, int32_t msTimeout = INT32_MAX) {
        // Grab a block
        Block *pBlock = get_block_write(msTimeout);
        if (pBlock == nullptr) return 0;

        // Copy the data
        uint32_t dwAmount = std::min(amount, (uint32_t) BLOCK_SIZE);
        memcpy(pBlock->Data, pBuff, dwAmount);
        pBlock->Amount = dwAmount;

        // Post the block
        post_block(pBlock);

        //TODO: temp
        //printf("(End of write)R-S: %ld, R-E: %ld, W-S: %ld, W-E: %ld\n", (long)m_pBuffer->m_readStart, (long)m_pBuffer->m_readEnd, (long)m_pBuffer->m_writeStart, (long)m_pBuffer->m_writeEnd);

        //Increment count
        incrementCount(); // todo: Might need to be somewhere else

        // Success
        return dwAmount;
    };

    typename ::block<BLOCK_SIZE>* get_block_write(int32_t msTimeout = INT32_MAX) {
        // Grab a block to write too
        // Enter a continuous loop
        // (this is to make sure the operation is atomic)
        while(1) {
            // Check if there is room to expand the write start cursor
            long blockIndex = m_pBuffer->m_writeStart;
            Block *pBlock = &(m_pBuffer->m_blocks[blockIndex]);
            if (pBlock->Next == m_pBuffer->m_readEnd) { 

                // No room is available, wait for room to become available
                if(WaitForEvent(&m_pBuffer->m_avail, (uint32_t)msTimeout) > 0) {
                    continue;
                } else {
                    // Timeout
                    return nullptr;
                }
            }

            // Make sure the operation is atomic
            if (__sync_val_compare_and_swap(&m_pBuffer->m_writeStart, blockIndex, pBlock->Next) == blockIndex) {
                // printf("\nWriting to blk # %d\n", blockIndex);
                return pBlock;
            }


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
            uint32_t blockIndex = m_pBuffer->m_writeEnd;
            pBlock = &(m_pBuffer->m_blocks[blockIndex]);

            if (__sync_val_compare_and_swap(&pBlock->doneWrite, 1, 0) != 1) {
                // If we get here then another thread has already moved the pointer
                // for us or we have reached as far as we can possible move the pointer
                return;
            }

            // Move the pointer one forward (interlock protected)
            __sync_bool_compare_and_swap(&m_pBuffer->m_writeEnd, blockIndex, pBlock->Next);

            // Signal availability of more data but only if threads are waiting
            if (blockIndex == m_pBuffer->m_readStart) {
                SetEvent(&m_pBuffer->m_signal);
            }

        }
    };


    // Block functions
    typename ::block<BLOCK_SIZE>* get_block_read(int32_t msTimeout = INT32_MAX) {
        // Grab another block to read from
        // Enter a continuous loop (this is to make sure the operation is atomic)
        while(1) {

            // Check if there is room to expand the read start cursor
            uint32_t blockIndex = m_pBuffer->m_readStart;
            Block *pBlock = &(m_pBuffer->m_blocks[blockIndex]);

//            if (pBlock->Next == m_pBuffer->m_writeEnd) {
            if (blockIndex == m_pBuffer->m_writeEnd) {
                
                // No room is available, wait for room to become available
                if(msTimeout > 0 && WaitForEvent(&m_pBuffer->m_signal, (uint32_t)msTimeout) > 0) {
                    // printf("(WAITING_READ) R-S: %ld, R-E: %ld, W-S: %ld, W-E: %ld\n", (long)m_pBuffer->m_readStart, (long)m_pBuffer->m_readEnd, (long)m_pBuffer->m_writeStart, (long)m_pBuffer->m_writeEnd);
                    continue;
                } else {
                    // Timeout
                    return nullptr;
                }

            }

            // Make sure the operation is atomic
            if (__sync_val_compare_and_swap(&m_pBuffer->m_readStart, blockIndex, pBlock->Next) == blockIndex) {
               // printf("\n(GETBLKREAD)R-S: %ld, R-E: %ld, W-S: %ld, W-E: %ld\n", (long)m_pBuffer->m_readStart, (long)m_pBuffer->m_readEnd, (long)m_pBuffer->m_writeStart, (long)m_pBuffer->m_writeEnd);
                return pBlock;
            }

            // The operation was interrupted by another thread.
            // The other thread has already stolen this block, try again
            continue;
        }
    };


    void return_block(Block *pBlock) {
        // Set the done flag for this block and clear the "amount" field in it.
        pBlock->doneRead = 1;
        pBlock->Amount = 0; //TODO: check this later.

        // Move the read pointer as far forward as we can
        while(1) {

            // Try and get the right to move the pointer
            uint32_t blockIndex = m_pBuffer->m_readEnd;
            pBlock = &(m_pBuffer->m_blocks[blockIndex]);
            if (__sync_val_compare_and_swap(&pBlock->doneRead, 1, 0) != 1) {

                // If we get here then another thread has already moved the pointer
                // for us or we have reached as far as we can possible move the pointer
                return;
            }

            // Move the pointer one forward (interlock protected)
            __sync_bool_compare_and_swap(&m_pBuffer->m_readEnd, blockIndex, pBlock->Next);

            // Signal availability of more data but only if a thread is waiting
            if (pBlock->Prev == m_pBuffer->m_writeStart) {
                SetEvent(&m_pBuffer->m_avail);
            }
        }
    };

    // Create and destroy functions
    int create() {

        //DWB TODO: Add error checking!

        // Setup / open initialization mutex (TODO: Look into removal later)
        // std::string mtxName = m_addr + "_init_mtx";
        // named_mutex::remove(mtxName.c_str());
        // m_initMtx = new named_mutex (open_or_create, mtxName.c_str());

        // Lock the mutex ( To prevent two processes from creating / initializing shared mem at same time
        // scoped_lock<named_mutex> lock(*m_initMtx);

        
        
        //Get the size of memory to create
        size_t shm_size = sizeof(MemBuff) + IPC_MEM_PAD;
        

        // Setup name for shared memory
        std::string shmName = m_addr + "_shm";
        
#ifdef USING_BOOST
        offset_t current_size = 0; 

        //Create (or open if already there) shared memory object
        m_pShm = new shared_memory_object(
                open_or_create,
                shmName.c_str(),
                read_write
        );

        // Map to the file
        if(m_pShm == nullptr) {
            return -1;
        }

        m_pShm->get_size(current_size);

#else //POSIX  
        size_t current_size = 0; 

        //Create or open shared memory using shmget
        m_shmfd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);

        //Check if it was created properly
        if(-1 == m_shmfd) {
            perror("shm_open()");
            return -1;
        }

        //Get the size of the shared memory segment
        struct stat st;
        fstat(m_shmfd, &st);

        //Get current size from the stat struct.
        current_size = st.st_size;

#endif
        //Get the size of the region to determine if the buffer has been created  yet
        if(0 == current_size) {

#ifdef USING_BOOST
            //Set the size of the shared memory region
            m_pShm->truncate((unsigned long) shm_size);

            //Map the shared region
            m_pMapReg = new mapped_region(
                    *m_pShm,
                    read_write
            );

            //Allocate the buffer using the given address for shared memory
            m_pBuffer = new(m_pMapReg->get_address()) MemBuff();
            if (m_pBuffer == nullptr)
                return -1;

#else //POSIX 
            //Truncate the memory to specific size
            ftruncate(m_shmfd,shm_size);

            //Map the memory 
            void *shmaddr = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmfd, 0);;

            //Check if memory was mmaped correctly
            if (shmaddr == MAP_FAILED) {
                return -1;
            }
            
            //Allocate the buffer using the given address for shared memory
            m_pBuffer = new(shmaddr) MemBuff();
            if (m_pBuffer == nullptr)
                return -1;

#endif
            // Create a circular linked list
            int i;
            m_pBuffer->m_blocks[0].Next = 1;
            m_pBuffer->m_blocks[0].Prev = (BLOCK_COUNT - 1);
            for (i = 1; i < BLOCK_COUNT - 1; i++) {
                // Add this block into the available list
                m_pBuffer->m_blocks[i].Next = (i + 1);
                m_pBuffer->m_blocks[i].Prev = (i - 1);
            }
            m_pBuffer->m_blocks[i].Next = 0;
            m_pBuffer->m_blocks[i].Prev = (BLOCK_COUNT - 2);

            // Initialize the 'pointers'
            m_pBuffer->m_msgCount = 0;
            m_pBuffer->m_readEnd = 0;
            m_pBuffer->m_readStart = 0;
            m_pBuffer->m_writeEnd = 0;
            m_pBuffer->m_writeStart = 0;

            // Setup/Create event variables
            CreateEvent(&m_pBuffer->m_signal);
            CreateEvent(&m_pBuffer->m_avail);


            // If the process is not the creator of the shared memory segment then just map to the region
        // and obtain the pointer to the buffer.
        } else {

#ifdef USING_BOOST
            //Map the shared region
            m_pMapReg = new mapped_region(
                    *m_pShm,
                    read_write
            );

            //Get the pointer to the mapped region
            m_pBuffer = (MemBuff *) (m_pMapReg->get_address());
            if (m_pBuffer == nullptr)
                return -1;
#else //POSIX  
            //Map the memory 
            void *shmaddr = mmap(0,shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmfd, 0);

            //Check if memory was mmaped correctly
            if (shmaddr == MAP_FAILED) {
                return -1;
            }

            //Point the buffer to the newly mapped region
            m_pBuffer = (MemBuff *) (shmaddr);

#endif
            //Success
            return 0;
        }
    };


    //Disconnecting from the client queue ( Unmapping )
    void disconnect(void) {


#ifndef USING_BOOST //POSIX 

        //Unmap the memory
        if(m_pBuffer != nullptr) {
            munmap((void*)m_pBuffer,  sizeof(MemBuff) + IPC_MEM_PAD);
        }
#endif

    };


    //Closing the queue (Note this removes for good. No other process can access after this, otherwise it will cause issues)
    void close(void) {

        //Destroy the events, which signals everyone to wakeup
        DestroyEvent(&m_pBuffer->m_signal);
        DestroyEvent(&m_pBuffer->m_avail);


#ifdef USING_BOOST
        //Remove any memory that might have the same name (from previous runs, etc.)
        std::string shmName = m_addr + "_shm";
        // std::string mtxName = m_addr + "_mtx";

        shared_memory_object::remove(shmName.c_str());
        // named_mutex::remove(mtxName.c_str());
#else //POSIX 
        //Unmap the memory
        if(m_pBuffer != nullptr) {
            munmap((void*)m_pBuffer,  sizeof(MemBuff) + IPC_MEM_PAD);
        }

        //Remove shared memory segment
        if(m_shmfd > 0) {
            shmctl(m_shmfd, IPC_RMID, NULL);
        }
#endif

    };


/***********************************************************************************
 *                        -- Static functions (ShmQueue) --                        *
 ***********************************************************************************/
    static void removeOldQueues(std::string path) {
        // Setup name for shared memory
        std::string shmName = path + "_shm";
        // std::string mtxName = path + "_mtx";

#ifdef USING_BOOST
        //Remove any memory that might have the same name (from previous runs, etc.)
        // named_mutex::remove(mtxName.c_str());
        shared_memory_object::remove(shmName.c_str());
#else //POSIX 
        //Unlink shared memory object
        shm_unlink(shmName.c_str());
#endif 

    }
};

#endif //SHM_IPC__H
