#include <string>
#include <cstdlib>
#include "crow.h"

using namespace std;

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --host <hostname>   Hostname/IP to bind to (default: 0.0.0.0)\n");
    printf("  -p, --port <port>       Port number to listen on (default: 5000)\n");
    printf("  --help                  Show this help message\n");
}

int main(int argc, char* argv[]) {
    string host = "0.0.0.0";
    int port = 5000;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", arg.c_str());
                return 1;
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: Invalid port number\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", arg.c_str());
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown argument: %s\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        return "";
    });

    printf("Starting server on %s:%d\n", host.c_str(), port);
    app.bindaddr(host).port(port).multithreaded().run();

    return 0;
}