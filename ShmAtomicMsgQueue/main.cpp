#include <thread>
// #include "shmIPC.h"
#include "shmIPC.h"
#include "MessageQueue.h"
#include "pevents.h"
#include <iostream>
#include <list>
#include <unistd.h>

#include "events.h"

#define N_THREADS 1

// Static variables
std::string path = "shm_queue";
long stats_server_rcv_count = 0;
long stats_client_rcv_count = 0;

double GetTickCount(void) 
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now))
    return 0;
  return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

// ShmClient thread
void test_client()
{
	// Create the IPC clientQueue_
//    ShmQueue<2048,2048> server_2(path);
    MessageQueue<2048,2048> server_2(path);
    server_2.connect();
//    server_2.setTimeout(5000);

	// Declare vars
	char testData[2048];

	// Continuously write data
	for (;;)
	{
		// Write some data
//        server_2.write(testData, 2048);
        int sts = server_2.send(testData, 2048, 0);
//        if (sts > 0) printf("Sent(%d)\n", sts);
		stats_client_rcv_count++;
	}
};

//// ShmQueue thread
//int main()
//{
//
//
//    typedef block<2048> Block;
//
//	//ShmQueue path
//
//	// Create the IPC server
//    ShmQueue<2048,2048>::removeOldQueues(std::string("shm_queue"));
////    ShmQueue<2048,2048> server(std::string("shm_queue"));
//    MessageQueue<2048,2048> server(path);
//    server.connect();
////    server.setTimeout(5000);
//
//
//    printf("Current count: %u\n", server.getMessageCount());
//	// Start lots of threads (for now 1)
//	std::list<std::thread*> threads;
//
//	for (int n = 0; n < N_THREADS; n++)
//	{
//		threads.push_back(new std::thread(test_client));
//	}
//
//	// Declare vars
//	uint32_t timerOutput = GetTickCount();
//	uint64_t N = 0;
//	uint64_t lN = 0;
//	uint64_t received = 0;
//
//	// Enter a loop reading data
//	for (;;N ++)
//	{
//		// Read the data
//        uint8_t buffer[4098];
////		Block *pBlock = server.get_block_read();
////
////		if(pBlock != NULL) {
////			if (pBlock->Amount > 0)
////			{
////				received += pBlock->Amount;
////				stats_server_rcv_count++;
////			}
////            server.return_block(pBlock);
////		}
//        stats_server_rcv_count++;
////        received += server.read(buffer, 2048);
//
//        int sts = server.receive((char*)buffer, 0);
//        if(sts > 0) received += sts;
//        else printf("Timeout: Msg size?: %d\n", server.getMessageCount());
////        printf("RECV(%d)\n", sts);
//
//		// Check the timer
//		uint32_t curTime = GetTickCount();
//		if (curTime - timerOutput > 1000) {
//			timerOutput = curTime;
//
//			// Print the data
//			printf("ShmQueue:%ld\tClient: %ld\tRate: %01u\tAmount: %01u\n", stats_server_rcv_count, stats_client_rcv_count, (uint32_t)(N - lN), (uint32_t)received);
//			printf("Current count: %d\n", server.getMessageCount());
//            lN = N;
//			received = 0;
//		}
//	}
//
//	// Success
//	return 0;
//};


event_t ev;

void wake() {
    sleep(5);

    printf("Signalling event...\n");
    SetEvent(&ev);
}

//int main() {
//
////    MessageQueue<32,32>::removeOldQueues(path);
////    MessageQueue<32,32> srv(path);
////    srv.connect();
////    srv.setTimeout(20000);
////
////    char sbuffer[] = "HelloWorld!";
////    char rBuffer[13];
////    memset(rBuffer, 0, 13);
////
////    int sts;
////    for(int i = 0; i < 30; i++) {
////        sts = srv.send(sbuffer, 12, 0);
////        printf("Sent(%d)\n", sts);
////    }
////
////    printf("Count: %d\n", srv.getMessageCount());
////
////    for(int i = 0; i < 38; i++) {
////        sts = (int) srv.receive(rBuffer, 0);
////        printf("Received(%d): %s\n", sts, rBuffer);
////        memset(rBuffer, 0, 13);
////
////    }
//
//    pevent_t events[2];
//
////    int sts = CreateEvent(&events[0], false, false);
////    sts = CreateEvent(&events[0], false, false);
////
////    printf("State before: %d\n", events[0].State);
////    SetEvent(&events[0]);
////    printf("St
/// ate after: %d\n", events[0].State);
////    int res;
////    if(WaitForMultipleEvents(events, 2, false, 1000, res) == ETIMEDOUT) printf("Timeout\n");
////    else printf("Returned from %d, State after2: %d\n", res, events[0].State);
//
//    CreateEvent(&ev, false, false);
//    SetEvent(&ev);
//
//    std::thread signaler(wake);
//    signaler.detach();
//
//    int sts = WaitForEvent(&ev, 10000);
//    printf("Woken up 1 (%d) \n",sts);
//
//
//    sts = WaitForEvent(&ev, 10000);
//    printf("Woken up 2 (%d) \n",sts);
//
//
//    return 0;
//}

int main() {
    CreateEvent(&ev);

    std::thread signaler(wake);
    signaler.detach();

    int sts = WaitForEvent(&ev, 10000);
    if(sts > 0) printf("Woken up 1 (%d) \n",sts);
    else printf("Timeout reached\n");


    return 0;
}