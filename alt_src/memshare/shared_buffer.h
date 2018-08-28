#include <cstdint>

/* An interface to define a buffer that can be used
 * concurrently between two separate processes.
 * 
 * All implementing classes must provide single
 * producer, single consumer semantics. Providing
 * stronger semantics is fine, e.g. via a mutex.
 *
 * Each implementing class is in charge of setting up
 * shared memory in its own way. 
 * Necessary information should be provided via a unique constructor.
 */

class SharedBuffer {
  public:
    // Put 'size' bytes from 'data' into the buffer.
    // Returns -1 on error, or else the # of bytes enqueued.
    virtual int put(char* data, uint size);
    // Same as above, but returns -1, 0, or size only.
    virtual int putAll(char* data, uint size);

    // Get 'size' bytes from this buffer, and put it into 'data'.
    // Returns -1 on error, or else the # of bytes dequeued.
    virtual int get(char* data, uint size);
    // Same as above, but returns -1, 0, or size only.
    virtual int getAll(char* data, uint size);

    virtual uint32_t getCapacity() { return capacity; }
    virtual uint32_t getUsed() { return used; }
    virtual uint32_t getRemaining() { return capacity - used; } 
  
  protected:
    uint32_t capacity;  // Capacity in bytes
    uint32_t used;      // Used bytes
};