#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <string>
#include <cstdlib>

using namespace std;

// Call this at the start of main() to ensure logs appear in Docker
inline void init_logging() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

inline void log(const char *format, ...) {
    va_list   args;
    char      buffer[255];

    va_start(args, format);
    time_t current_time = time(nullptr);
    tm *local_time = std::localtime(&current_time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
    printf("%s: ", buffer);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}

// Load environment variables from a .env file
// Only sets variables that are not already set in the environment
// (environment variables take precedence over .env file)
inline void load_env_file(const char* filename = ".env") {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Try common locations
        const char* locations[] = {
            ".env",
            "../.env",
            "../../.env",
            nullptr
        };
        
        for (int i = 0; locations[i] != nullptr; i++) {
            file.open(locations[i]);
            if (file.is_open()) break;
        }
        
        if (!file.is_open()) {
            // No .env file found - that's okay, just use environment variables
            return;
        }
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Find the = separator
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        // Extract key and value
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim whitespace from key
        size_t start = key.find_first_not_of(" \t");
        size_t end = key.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        key = key.substr(start, end - start + 1);
        
        // Trim whitespace and quotes from value
        start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            end = value.find_last_not_of(" \t");
            value = value.substr(start, end - start + 1);
        } else {
            value = "";
        }
        
        // Remove surrounding quotes if present
        if (value.size() >= 2) {
            if ((value.front() == '"' && value.back() == '"') ||
                (value.front() == '\'' && value.back() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
        }
        
        // Only set if not already in environment (env vars take precedence)
        if (std::getenv(key.c_str()) == nullptr) {
            setenv(key.c_str(), value.c_str(), 0);
        }
    }
    
    file.close();
}
