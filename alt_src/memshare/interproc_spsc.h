#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <string>
#include <cstdlib>
#include "shared_buffer.h"

namespace bip = boost::interprocess;
namespace shm
{
    template <typename T>
        using alloc = bip::allocator<T, bip::managed_shared_memory::segment_manager>;
    using char_alloc  =  alloc<char>;
    using ring_buffer = boost::lockfree::spsc_queue<
        char, 
        boost::lockfree::allocator<char_alloc>
    >;
}

// typedef allocator<char, managed_shared_memory::segment_manager> __IprocShmemAllocator;
// typedef boost::lockfree::spsc_queue<char, __IprocShmemAllocator> __iproc_spsc_q;


class Interproc_Spsc : SharedBuffer {
  public:
    // This is for creating a NEW buffer.
    // ONLY do this in the client.
    Interproc_Spsc(uint32_t capacity, std::string name);
    // This is for retrieving an ALREADY CREATED buffer.
    Interproc_Spsc(std::string name);
    // This doesn't really do anything.
    // In the DAEMON ONLY, call cleanup() once everything's done.
    ~Interproc_Spsc();    

    int put(char* data, uint size);
    int putAll(char* data, uint size);
    int get(char* data, uint size);
    int getAll(char* data, uint size);

    std::string name() { return shared_name; };

    uint32_t getUsed();
    uint32_t getRemaining();

    // ONLY CALL THIS IN THE DAEMON AFTER THE CLIENT SAYS TO.
    // After a client calls close() from a socket using this,
    // the daemon should receive a message that will be given
    // to the socket class. Once the daemon instance of the 
    // socket is done, it should call this function.
    void cleanup();

  private: 
    static std::string shared_name;
    static std::string mempool_name;
    shm::ring_buffer *data_queue;
    // const shm::char_alloc alloc_inst;
    // bip::managed_shared_memory segment;
};