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

#define NUM_RUNS 1000000
#define DATA_SIZE 1
#define SLEEP_TIME_US 25

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

    assert(steady_clock::is_steady);

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

        // vector<int> sendbuf(DATA_SIZE);
        int *sendbuf = new int[DATA_SIZE];
        for (int i = 0; i < size-1; i++)
            sendbuf[i] = rand();

        vector<MPI_Request> r_handles(size-1);
        int cnt = 0;
        int done = 0;
        vector<int> senddone(size-1);
        for (int i = 0; i < size-1; i++)
            senddone[i] = 0;

        // "warm up" all the connections
        MPI_Barrier(MPI_COMM_WORLD);
        for (int i = 0; i < size-1; i++)
            MPI_Send(sendbuf, DATA_SIZE, MPI_INT, i+1, 0, MPI_COMM_WORLD);

        // -------------------------

        MPI_Barrier(MPI_COMM_WORLD);

        times[cnt++] = get_us(); // base time
        for (int k = 0; k < NUM_RUNS; k++){
            // Send everything
            // MPI_Barrier(MPI_COMM_WORLD);
            for (int i = 0; i < size-1; i++) {
                MPI_Isend(sendbuf, DATA_SIZE, MPI_INT, i+1, 0, MPI_COMM_WORLD, &r_handles[i]);
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
    } else {

        uint64_t *times = new uint64_t[NUM_RUNS];
        for (int i = 0; i < NUM_RUNS; i++)
            times[i] = 0;

        int *recvbuf = new int[DATA_SIZE];
        int cnt = 0;

        // "warm up" all the connections
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Recv(recvbuf, DATA_SIZE, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // --------------------------

        MPI_Barrier(MPI_COMM_WORLD);

        times[cnt++] = get_us(); // baseline time
        for (int j = 0; j < NUM_RUNS; j++) {
            // MPI_Barrier(MPI_COMM_WORLD);
            MPI_Recv(recvbuf, DATA_SIZE, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            times[cnt++] = get_us(); // send/recv time
        }

        // cout << "rank " << rank << " time deltas =";
        // for (int i = 1; i < NUM_RUNS; i++)
        //     cout << " " << times[i] - times[i-1];
        // cout << endl;

        delete times;
        delete recvbuf;
    }
}

