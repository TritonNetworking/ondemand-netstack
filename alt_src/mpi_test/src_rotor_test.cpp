#include <iostream>
#include <chrono>
#include <cstdlib>
#include <mpi.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <sstream>
// #include <strstream>
#include <fstream>
#include <algorithm>
#include <netinet/in.h>

#define NUM_RUNS 1000
#define SYNC_DATA_SIZE 9
#define SLEEP_TIME_US 900
#define GUARD_TIME_US 150
#define LINK_SPEED_MBPS 10000
#define BULK_DATA_SIZE ((SLEEP_TIME_US - GUARD_TIME_US) * LINK_SPEED_MBPS) / 8
// #define BULK_DATA_SIZE 200

// this is to test synchronization in MPI

void rotor_test(int size, int rank);

using namespace std;
using namespace chrono;

// inline int64_t get_us()
// {
//     steady_clock::duration dur{steady_clock::now().time_since_epoch()};
//     return duration_cast<microseconds>(dur).count();
// }

inline uint64_t get_time_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (((uint64_t) ts.tv_sec) * 1000000000) + ts.tv_nsec;
}

inline uint64_t get_us() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (((uint64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

int main(int argc, char* argv[])
{
    int size, rank;

    // assert(steady_clock::is_steady);

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    rotor_test(size, rank);

    MPI_Finalize();

    return 0;
}

void rotor_test(int size, int rank) {

    if (rank == 0) {

        vector<int64_t> times_init(size-1);
        vector<int64_t> times_done(size-1);
        for (int i = 0; i < size-1; i++) {
            times_init[i] = -1;
            times_done[i] = -3;
        }

        // vector<int64_t> times(NUM_RUNS);
        uint64_t *times = new uint64_t[NUM_RUNS];
        for (int i = 0; i < NUM_RUNS; i++)
            times[i] = 0;

        // vector<int> sendbuf(SYNC_DATA_SIZE);
        char *sendbuf = new char[SYNC_DATA_SIZE];
        ((uint*)sendbuf)[0] = htonl(0xdeed);
        ((uint*)sendbuf)[1] = htonl(0xffffffff);
        sendbuf[8] = 0xab;
        // for (int i = 0; i < SYNC_DATA_SIZE; i++)
        //     sendbuf[i] = rand();

        // MPI_Request *r_handles = new MPI_Request[size-1];
        int cnt = 0;
        int done = 0;
        int *senddone = new int[size-1];
        for (int i = 0; i < size-1; i++)
            senddone[i] = 0;

        // "warm up" all the connections
        // MPI_Barrier(MPI_COMM_WORLD);
        for (int i = 0; i < size-1; i++)
            MPI_Send(sendbuf, SYNC_DATA_SIZE, MPI_CHAR, i+1, 0, MPI_COMM_WORLD);

        // -------------------------

        // MPI_Barrier(MPI_COMM_WORLD);

        times[cnt++] = get_us(); // base time
        for (int k = 0; k < NUM_RUNS; k++){
            MPI_Request r_handles[size-1];
            // Send everything
            // MPI_Barrier(MPI_COMM_WORLD);
            for (int i = 0; i < size-1; i++) {
                MPI_Isend(sendbuf, SYNC_DATA_SIZE, MPI_CHAR, i+1, 0, MPI_COMM_WORLD, &r_handles[i]);
                // times_init[i] = get_us(); // ! debug
            }

            while (done < size-1) {
                for (int i = 0; i < size-1; i++) {
                    if (senddone[i] == 0) {
                        // check if this one is done
                        MPI_Test(&r_handles[i], &senddone[i], MPI_STATUS_IGNORE);
                        if (senddone[i] == 1) {
                            done++;
                            // times_done[i] = get_us(); // ! debug
                        }
                    }
                }
            }
            times[cnt++] = get_us(); // send/recv time

            // Cleanup
            done = 0;
            for (int i = 0; i < size-1; i++)
                senddone[i] = 0;

            // Wait
            while(get_us() - times[cnt-1] < SLEEP_TIME_US);
        }

        cout << "rank " << rank << " time deltas =";
        for (int i = 1; i < NUM_RUNS; i++) {
            cout << " " << times[i] - times[i-1];
        }
        cout << endl;

        // cout << "debug, times init_send to rank 1, 2, ..." << endl;
        // cout << "   ";
        // for (int i = 0; i < size-1; i++)
        //     cout << " " << times_init[i] - times[cnt];
        // cout << endl;

        // cout << "debug, times done_send to rank 1, 2, ..." << endl;
        // cout << "   ";
        // for (int i = 0; i < size-1; i++)
        //     cout << " " << times_done[i] - times[cnt];
        // cout << endl;

        delete times;
        delete sendbuf;
        // delete r_handles;
        delete senddone;
    } else {

        uint64_t *times = new uint64_t[NUM_RUNS];
        uint64_t *bulk_times = new uint64_t[NUM_RUNS];
        for (int i = 0; i < NUM_RUNS; i++)
            times[i] = 0;

        char *recvbuf = new char[SYNC_DATA_SIZE];
        char *bulkbuf_snd = new char[BULK_DATA_SIZE];
        char *bulkbuf_rcv = new char[BULK_DATA_SIZE];
        int cnt = 0;

        for (int i = 0; i < BULK_DATA_SIZE; i++)
            bulkbuf_snd[i] = (char)(rand() % 256);

        // Get paired host
        // MPI_Request bulk_req;
        int paired;
        if (rank == 1)
            paired = 2;
        else if (rank == 2)
            paired = 1;
        else if (rank == 3)
            paired = 4;
        else if (rank == 4)
            paired = 3;

        // "warm up" all the connections
        // MPI_Barrier(MPI_COMM_WORLD);
        MPI_Recv(recvbuf, SYNC_DATA_SIZE, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // --------------------------

        // MPI_Barrier(MPI_COMM_WORLD);

        times[cnt] = get_us(); // baseline time
        bulk_times[cnt] = get_us();
        cnt++;
        for (int j = 0; j < NUM_RUNS; j++) {
            MPI_Request bulk_send_req, bulk_recv_req, sync_req;
            int bulk_send_done = 0;
            int bulk_recv_done = 0;
            int sync_done = 0;
            // MPI_Barrier(MPI_COMM_WORLD);
            MPI_Irecv(recvbuf, SYNC_DATA_SIZE, MPI_CHAR, 0,
                      MPI_ANY_TAG, MPI_COMM_WORLD, &sync_req);
            while(!sync_done)
                MPI_Test(&sync_req, &sync_done, MPI_STATUS_IGNORE);
            times[cnt] = get_us(); // sync time

            // Do the bulk send/recv with the dedicated pair
            MPI_Isend(bulkbuf_snd, BULK_DATA_SIZE, MPI_CHAR, paired,
                      0, MPI_COMM_WORLD, &bulk_send_req);
            MPI_Irecv(bulkbuf_rcv, BULK_DATA_SIZE, MPI_CHAR, paired, MPI_ANY_TAG,
                      MPI_COMM_WORLD, &bulk_recv_req);
            while(!bulk_send_done || !bulk_recv_done) {
                if(!bulk_send_done)
                    MPI_Test(&bulk_send_req, &bulk_send_done, MPI_STATUS_IGNORE);
                if(!bulk_recv_done)
                    MPI_Test(&bulk_recv_req, &bulk_recv_done, MPI_STATUS_IGNORE);
            }

            bulk_times[cnt] = get_us();
            cnt++;
        }

        cout << "rank " << rank << " bulk time deltas =";
        for (int i = 1; i < NUM_RUNS; i++)
            cout << " " << bulk_times[i] - times[i];
        cout << endl;

        delete times;
        delete recvbuf;
        delete bulkbuf_snd;
        delete bulkbuf_rcv;
    }
}

