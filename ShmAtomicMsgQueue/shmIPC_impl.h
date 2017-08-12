#ifndef SHM_IPC_IMPL__H
#define SHM_IPC_IMPL__H

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include "shmIPC.h"

template<int BLOCK_SIZE, int BLOCK_COUNT>
ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::ShmQueue(std::string path) {
    // Set default params TODO did I leave any out?
    m_pBuffer = NULL;

    // create the server
    create(path);
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::~ShmQueue(void) {
    // Close the server
    close();
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::create(std::string path) {
    // Determine the name of the memory
    m_addr = path;

    //DWB TODO: Add error checking

    // Create the file mapping
    std::string shmName = m_addr + "_shm";

    //Remove any memory that might have the same name (from previous runs, etc.)
    shared_memory_object::remove(shmName.c_str());

    //Create (or open if already there) shared memory object
    m_pShm = new shared_memory_object(
            open_or_create,
            shmName.c_str(),
            read_write
    );

    //Set the size of the shared memory region
    m_pShm->truncate(((unsigned long) (sizeof(MemBuff) + IPC_MEM_PAD)));

    // Map to the file
    m_pMapReg = new mapped_region(
            *m_pShm,
            read_write
    );

    //Assign the mapped_memory object
    m_pBuffer = new(m_pMapReg->get_address()) MemBuff();
    if (m_pBuffer == nullptr)
        return; //{ delete(m_hSignal); delete(m_hSignalMutex); delete(m_hAvail); delete(m_hAvailMutex); return; }

    // Create a circular linked list
    int i;
    m_pBuffer->m_Blocks[0].Next = 1;
    m_pBuffer->m_Blocks[0].Prev = (IPC_BLOCK_COUNT - 1);
    for (i = 1; i < IPC_BLOCK_COUNT - 1; i++) {
        // Add this block into the available list
        m_pBuffer->m_Blocks[i].Next = (i + 1);
        m_pBuffer->m_Blocks[i].Prev = (i - 1);
    }
    m_pBuffer->m_Blocks[i].Next = 0;
    m_pBuffer->m_Blocks[i].Prev = (IPC_BLOCK_COUNT - 2);

    // Initialize the 'pointers'
    m_pBuffer->m_msgCount = 0;
    m_pBuffer->m_ReadEnd = 0;
    m_pBuffer->m_ReadStart = 0;
    m_pBuffer->m_WriteEnd = 1;
    m_pBuffer->m_WriteStart = 1;
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::close(void) {
    // Close the file handle
    std::string shmName = m_addr + "_shm";
    shared_memory_object::remove(shmName.c_str());
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
typename ::block<BLOCK_SIZE> *ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::get_block_read(int32_t msTimeout) {
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
            if (m_pBuffer->signal_sem.timed_wait(timeout))
                continue;

            // Timeout
            return NULL;
        }

        // Make sure the operation is atomic
        if (__sync_val_compare_and_swap(&m_pBuffer->m_ReadStart, blockIndex, pBlock->Next) == blockIndex)
            return pBlock;

        // The operation was interrupted by another thread.
        // The other thread has already stolen this block, try again
        continue;
    }
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::return_block(Block *pBlock) {
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
        if (pBlock->Prev == m_pBuffer->m_WriteStart)
            m_pBuffer->avail_sem.post();

            //Decrement count TODO is this correct?
            decrementCount();

    }
};


template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::decrementCount() {
    __sync_fetch_and_sub(&m_pBuffer->m_msgCount, 1);
};


template<int BLOCK_SIZE, int BLOCK_COUNT>
uint32_t ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::getMsgCount() {
    return m_pBuffer->m_msgCount;
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
uint32_t ShmQueue<BLOCK_SIZE, BLOCK_COUNT>::read(void *pBuff, uint32_t buffSize, int32_t dwTimeout) {
    // Grab a block
    Block *pBlock = get_block_read(dwTimeout);
    if (!pBlock) return 0;

    // Copy the data //TODO: add more complicated stuff here to get a size (like sockets does it but coy instead of recv)
    uint32_t dwAmount = std::min(pBlock->Amount, buffSize);
    memcpy(pBuff, pBlock->Data, dwAmount);

    // Return the block
    return_block(pBlock);

    // Success
    return dwAmount;
};

//TODO: Do I need a default constructor
template<int BLOCK_SIZE, int BLOCK_COUNT>
ShmClient<BLOCK_SIZE, BLOCK_COUNT>::ShmClient(void) {
    // Set default params
    m_pBuffer = NULL;
};


template<int BLOCK_SIZE, int BLOCK_COUNT>
ShmClient<BLOCK_SIZE, BLOCK_COUNT>::ShmClient(std::string path) {
    // Set default params
    m_pBuffer = NULL;

    // Determine the name of the memory
    m_addr = path;

    // Open the shared file
    std::string shmName = m_addr + "_shm";
    m_pShm = new shared_memory_object(
            open_only,
            shmName.c_str(),
            read_write
    );

    // Map to the file
    m_pMapReg = new mapped_region(
            *m_pShm,
            read_write
    );

    //Get the pointer to the mapped region
    m_pBuffer = (MemBuff *) (m_pMapReg->get_address());
    if (m_pBuffer == nullptr)
        return;

};

template<int BLOCK_SIZE, int BLOCK_COUNT>
ShmClient<BLOCK_SIZE, BLOCK_COUNT>::~ShmClient(void) {

    // Unmap the memory
    // DWB TODO: Add un-mapping

};

template<int BLOCK_SIZE, int BLOCK_COUNT>
typename ::block<BLOCK_SIZE> *ShmClient<BLOCK_SIZE, BLOCK_COUNT>::get_block(int32_t msTimeout) {
    // Grab another block to write too
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
            if (m_pBuffer->avail_sem.timed_wait(timeout))
                continue;

            // Timeout
            return NULL;
        }

        // Make sure the operation is atomic
        if (__sync_val_compare_and_swap(&m_pBuffer->m_WriteStart, blockIndex, pBlock->Next) ==
            blockIndex) //dwb was - if (InterlockedCompareExchange (&m_pBuffer->m_WriteStart, pBlock->Next, blockIndex) == blockIndex)
            return pBlock;

        // The operation was interrupted by another thread.
        // The other thread has already stolen this block, try again
        continue;
    }
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmClient<BLOCK_SIZE, BLOCK_COUNT>::postBlock(Block *pBlock) {
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
        __sync_bool_compare_and_swap(&m_pBuffer->m_WriteEnd, blockIndex,
                                     pBlock->Next); // dwb was - InterlockedCompareExchange(&m_pBuffer->m_WriteEnd, pBlock->Next, blockIndex);


        // Signal availability of more data but only if threads are waiting
        if (pBlock->Prev == m_pBuffer->m_ReadStart)
            m_pBuffer->signal_sem.post();

        //Increment count
        incrementCount();
    }
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
void ShmClient<BLOCK_SIZE, BLOCK_COUNT>::incrementCount() {
    __sync_fetch_and_add(&m_pBuffer->m_msgCount, 1);
};




template<int BLOCK_SIZE, int BLOCK_COUNT>
uint32_t ShmClient<BLOCK_SIZE, BLOCK_COUNT>::write(void *pBuff, uint32_t amount, int32_t dwTimeout) {
    // Grab a block
    Block *pBlock = get_block(dwTimeout);
    if (!pBlock) return 0;

    // Copy the data
    uint32_t dwAmount = std::min(amount, (uint32_t) IPC_BLOCK_SIZE);
    memcpy(pBlock->Data, pBuff, dwAmount);
    pBlock->Amount = dwAmount;

    // Post the block
    postBlock(pBlock);

    // Fail : TODO? Would this not always fail then? fix this
    return 0;
};

template<int BLOCK_SIZE, int BLOCK_COUNT>
bool ShmClient<BLOCK_SIZE, BLOCK_COUNT>::waitAvailable(int32_t msTimeout) {
    //Setup timeout value
    boost::posix_time::ptime timeout = microsec_clock::local_time() + boost::posix_time::milliseconds(msTimeout);

    // Wait on the available event
    return m_pBuffer->avail_sem.timed_wait(timeout);

};

#endif // SHM_IPC_IMPL__H