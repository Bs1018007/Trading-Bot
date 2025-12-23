#pragma once
#include <thread>

extern "C" {
    #include "aeronmd.h"
}

class GlobalMediaDriver {
public:
    static GlobalMediaDriver& get_instance();
    bool initialize();
    ~GlobalMediaDriver();
    
private:
    GlobalMediaDriver();
    static GlobalMediaDriver* singleton_instance;
    
    aeron_driver_t* driver;
    aeron_driver_context_t* driver_context;
    std::thread driver_thread;
    bool is_initialized;
};