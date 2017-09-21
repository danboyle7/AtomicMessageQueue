#include <thread>
// #include "shmIPC.h"
#include "shmIPC.h"
#include "MessageQueue.h"
// #include "MessageQueue2.h"

// #include "/usr/local/include/MessageQueue.h"
#include "pevents.h"
#include <iostream>
#include <list>
#include <unistd.h>
#include <mqueue.h>
// #include "events.h" //boost cond
#include "events2.h"   //pthread cond

#include <time.h>
#include <sys/resource.h>
#include <sched.h>

#include <chrono>

#define N_THREADS 2

using namespace std::chrono;
using clock_type = typename std::conditional< std::chrono::high_resolution_clock::is_steady,
                                                  std::chrono::high_resolution_clock,
                                                  std::chrono::steady_clock >::type ;


// Static variables
std::string path = "/shm_queue";
long stats_server_rcv_count = 0;
long stats_client_rcv_count = 0;

double GetTickCount(void) 
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now))
    return 0; 
  // return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;   //ms
  // return now.tv_sec * 1000000.0 + now.tv_nsec / 1000.0; //us
  return now.tv_sec * 1000000000.0 + now.tv_nsec; //ns

}

// ShmClient thread
void test_client()
{
	// Create the IPC clientQueue_
    // ShmQueue<2048,8192> server_2(path);
    MessageQueue<4096,8192> server_2(path);

    // MessageQueue server_2(path,
    //                        4096,
    //                        8192,
    //                        0 //Flags
    //                       );

    // server_2.setTimeout(5000);

    int ret = server_2.connect();
    // server_2.setTimeout(0);

    // Declare vars
    char testData[4096];
    struct timespec ts;

    ts.tv_sec  = 0;
    ts.tv_nsec = 1;

    // uint32_t curTime = GetTickCount();
    // nanosleep(&ts, NULL);
    // uint32_t curTime2 = GetTickCount();

    // printf("%ld us\n", curTime2-curTime);

    // Continuously write data
		
    while(ret != -1)
    {
        // Write some data
//        server_2.write(testData, 2048);
        // nanosleep(&ts, NULL);
        // sched_yield();
        int sts = server_2.send(testData, 4096, 0);

       // if (sts > 0) printf("Sent(%d)\n", sts);
		stats_client_rcv_count++;
	}
};

// ShmQueue thread
int main() {
    struct rlimit rlim;
    memset(&rlim, 0, sizeof(rlim));
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;

    setrlimit(RLIMIT_MSGQUEUE, &rlim); 

	//ShmQueue path

	// Create the IPC server
    ShmQueue<4096,8192>::removeOldQueues(std::string("shm_queue"));
   // // ShmQueue<2048,8192> server(std::string("shm_queue"));
   // mq_unlink(path.c_str());
   // MessageQueue::removeOldQueues(path.c_str()); 
   MessageQueue<4096,8192> server(path);
   // MessageQueue server(path,
   //                     4096,
   //                     8192,
   //                     0 //Flags
   //                     );

   // server.setTimeout(0);

   int ret = server.connect();

   // printf("Current count: %u\n", server.getMessageCount());
	// Start lots of threads (for now 1)
	std::list<std::thread*> threads;

    for (int n = 0; n < N_THREADS; n++)
    {  
		threads.push_back(new std::thread(test_client));
	}

	// Declare vars
	uint32_t timerOutput = GetTickCount();
	uint64_t N = 0;
	uint64_t lN = 0;
	uint64_t received = 0;
    
    for (;;N ++)
	{
		// Read the data
       uint8_t buffer[4096];
//		Block *pBlock = server.get_block_read();
//
//		if(pBlock != NULL) {
//			if (pBlock->Amount > 0)
//			{
//				received += pBlock->Amount;
//				stats_server_rcv_count++;
//			}
//            server.return_block(pBlock);
//		}
       stats_server_rcv_count++;
//        int sts = server.read(buffer, 2048);
       int sts = server.receive((char*)buffer, 0);
       // server.setTimeout(0);

       if(sts > 0) received += sts;
       // else printf("Timeout: Ret: %d, Msg size?: %d\n", sts, server.getMessageCount());
//        printf("RECV(%d)\n", sts);

		// Check the timer
		uint32_t curTime = GetTickCount();
		if (curTime - timerOutput > 1000000000l) {
			timerOutput = curTime;

			// Print the data
			printf("ShmQueue:%ld\tClient: %ld\tRate: %01u\tAmount: %01u\n", stats_server_rcv_count, stats_client_rcv_count, (uint32_t)(N - lN), (uint32_t)received);
			printf("Current count: %d\n", server.getMessageCount());
            lN = N;
			received = 0;
		}
	}

	// Success
	return 0;
}


event_t ev;

// void wake() {
// //    sleep(5);
// //
// //    printf("Signalling event...\n");
// //    SetEvent(&ev);

//     MessageQueue<32,105> srv(path);
//     srv.connect();
//     srv.setTimeout(2000);

//     int sts = 0;
//     char rBuffer[16];
//     memset(rBuffer, 0, 16);
//     for(int i = 0; i < 100; i++) {
//         usleep(10);
//         sts = (int) srv.receive(rBuffer, 0);
//         printf("Received(%d): \"%s\"\n", sts, rBuffer);
//         memset(rBuffer, 0, 16);
//     }
// }

// int main() {

//     MessageQueue<32,105>::removeOldQueues(path);
//     MessageQueue<32,105> srv(path);
//     srv.connect();
//     srv.setTimeout(2000);

//     std::string temp = "HelloWorld: ";
//     char rBuffer[16];
//     memset(rBuffer, 0, 16);

//     int sts;

//    std::thread signaler(wake);
//    signaler.detach();

//     for(int i = 0; i < 100; i++) {
//         usleep(1000);
//         std::string sBuffer = temp + std::to_string(i);
//         sts = srv.send((char*)sBuffer.c_str(), 16, 0);
//         printf("Sent: \"%s\" (%d)\n", sBuffer.c_str(), sts);
//     }

//     sleep(10);
//     printf("Count: %d\n", srv.getMessageCount());

//     // for(int i = 0; i < 100; i++) {
//     //     sts = (int) srv.receive(rBuffer, 0);
//     //     printf("Received(%d): \"%s\"\n", sts, rBuffer);
//     //     memset(rBuffer, 0, 16);
//     // }

//     // std::string sBuffer = temp + std::to_string(5);
//     // sts = srv.send((char*)sBuffer.c_str(), 16, 0);
//     // printf("Sent: \"%s\" (%d)\n", sBuffer.c_str(), sts);

//     // sts = (int) srv.receive(rBuffer, 0);
//     // printf("Received(%d): \"%s\"\n", sts, rBuffer);

// //    event_t events[2];
// //
// //    CreateEvent(&ev);
// //    SetEvent(&ev);
// //
// //    std::thread signaler(wake);
// //    signaler.detach();
// //
// //    ssize_t sts = WaitForEvent(&ev, 10000);
// //    printf("Woken up 1 (%d) \n",sts);
// //
// //    sts = WaitForEvent(&ev, 10000);
// //    printf("Woken up 2 (%d) \n",sts);


//     return 0;
// }
