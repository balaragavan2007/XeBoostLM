#include "memory_profiler.hpp"
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
#endif

// Function definitions (The Kitchen)
float get_bytes_per_param(const std::string& precision) {
    if (precision == "FP32") return 4.0f;
    if (precision == "FP16") return 2.0f;
    if (precision == "INT8") return 1.0f;
    if (precision == "INT4" || precision == "NF4") return 0.5f;
    return 2.0f; // Default fallback
}

float get_available_ram_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return statex.ullAvailPhys / (1024.0f * 1024.0f * 1024.0f);
#elif defined(__linux__)
    std::ifstream file("/proc/meminfo");
    std::string key;
    size_t value_kb = 0;
    while (file >> key >> value_kb) {
        if (key == "MemAvailable:") {
            return (value_kb * 1024.0f) / (1024.0f * 1024.0f * 1024.0f);
        }
        std::string discard;
        std::getline(file, discard);
    }
    return 0.0f;
#else
    return 0.0f;
#endif
}