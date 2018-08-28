#include "interproc_spsc.h"


Interproc_Spsc::Interproc_Spsc(uint32_t capacity, std::string name) {
    this->capacity = capacity;
    this->shared_name = std::string(name);
    this->mempool_name = std::string("MEMPOOL_" + name);
    
    struct shm_remove { 
        shm_remove()  { bip::shared_memory_object::remove(mempool_name.c_str()); }
        ~shm_remove() { bip::shared_memory_object::remove(mempool_name.c_str());}
    } remover;
    bip::managed_shared_memory segment (bip::create_only, mempool_name.c_str(), capacity * sizeof(char));
    const shm::char_alloc alloc_inst (segment.get_segment_manager());
    this->data_queue = segment.construct<shm::ring_buffer>(name.c_str())(capacity, alloc_inst);
    
    if (this->data_queue == NULL) {
        throw std::runtime_error("Failed to allocate new data_queue with name " + name);
    }
}

Interproc_Spsc::Interproc_Spsc(std::string name) {
    this->shared_name = std::string(name);
    this->mempool_name = std::string("MEMPOOL_" + name);
    bip::managed_shared_memory segment (bip::open_only, mempool_name.c_str());
    const shm::char_alloc alloc_inst (segment.get_segment_manager());
    this->data_queue = segment.find<shm::ring_buffer>(name.c_str()).first;

    if (this->data_queue == NULL) {
        throw std::runtime_error("Failed to get data_queue with name " + name);
    }

    this->capacity = (uint32_t)segment.get_size();
}

Interproc_Spsc::~Interproc_Spsc() { }

void Interproc_Spsc::cleanup() {
    bip::managed_shared_memory segment (bip::open_only, mempool_name.c_str());
    segment.destroy<shm::ring_buffer>(this->shared_name.c_str());
}

int Interproc_Spsc::put(char* data, uint size) {
    return (int)(data_queue->push(data, size));
}

int Interproc_Spsc::putAll(char* data, uint size) {
    if (getRemaining() < size)
        return 0;

    return put(data, size);
}

int Interproc_Spsc::get(char* data, uint size) {
    return (int)(data_queue->pop(data, size));
}

int Interproc_Spsc::getAll(char* data, uint size) {
    if (getUsed() < size)
        return 0;

    return get(data, size);
}

uint32_t Interproc_Spsc::getUsed() {
    return (uint32_t)data_queue->read_available();
}

uint32_t Interproc_Spsc::getRemaining() {
    return (uint32_t)data_queue->write_available();
}
