#include "messaging/GlobalMediaDriver.h"
#include <iostream>
#include <chrono>
#include <thread>

GlobalMediaDriver* GlobalMediaDriver::singleton_instance = nullptr;

GlobalMediaDriver::GlobalMediaDriver() 
    : driver(nullptr), driver_context(nullptr), is_initialized(false) {}

GlobalMediaDriver& GlobalMediaDriver::get_instance() {
    if (!singleton_instance) {
        singleton_instance = new GlobalMediaDriver();
    }
    return *singleton_instance;
}

bool GlobalMediaDriver::initialize() {
    if (is_initialized) {
        return true;
    }

    // Initialize driver context
    if (aeron_driver_context_init(&driver_context) < 0) {
        std::cerr << "Failed to init driver context: " << aeron_errmsg() << "\n";
        return false;
    }

    // Initialize driver
    if (aeron_driver_init(&driver, driver_context) < 0) {
        std::cerr << "Failed to init driver: " << aeron_errmsg() << "\n";
        aeron_driver_context_close(driver_context);
        return false;
    }

    // Start driver thread
    driver_thread = std::thread([this]() {
        while (driver) {
            aeron_driver_main_do_work(driver);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    is_initialized = true;
    std::cout << "✓ Global Aeron MediaDriver initialized\n";
    return true;
}

GlobalMediaDriver::~GlobalMediaDriver() {
    if (driver) {
        driver = nullptr;
    }
    if (driver_context) {
        aeron_driver_context_close(driver_context);
    }
    if (driver_thread.joinable()) {
        driver_thread.join();
    }
}