#include <string>
#include <cstdlib>
#include "crow.h"
#include "fsm.hpp"

using namespace std;

// Thread-local MappingSearch instance
static string g_index_dir;
static int g_cache_size = 25;

MappingSearch* get_mapping_search() {
    thread_local MappingSearch* mapping_search = nullptr;
    if (mapping_search == nullptr) {
        mapping_search = new MappingSearch(g_index_dir, g_cache_size);
        mapping_search->load();
    }
    return mapping_search;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --host <hostname>   Hostname/IP to bind to (default: 0.0.0.0)\n");
    printf("  -p, --port <port>       Port number to listen on (default: 5000)\n");
    printf("  -i, --index <dir>       Index directory (required)\n");
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
        } else if (arg == "-i" || arg == "--index") {
            if (i + 1 < argc) {
                g_index_dir = argv[++i];
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

    if (g_index_dir.empty()) {
        fprintf(stderr, "Error: Index directory is required (-i or --index)\n");
        print_usage(argv[0]);
        return 1;
    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        return "";
    });

    CROW_ROUTE(app, "/mapping/lookup")
    ([](const crow::request& req) {
        auto artist_credit_name = req.url_params.get("artist_credit_name");
        auto release_name = req.url_params.get("release_name");
        auto recording_name = req.url_params.get("recording_name");

        if (!artist_credit_name || !recording_name) {
            crow::json::wvalue error;
            error["error"] = "Missing required parameters: artist_credit_name and recording_name are required";
            return crow::response(400, error);
        }

        MappingSearch* mapping_search = get_mapping_search();
        SearchMatch* result = mapping_search->search(
            artist_credit_name,
            release_name ? release_name : "",
            recording_name
        );

        crow::json::wvalue response;
        if (result) {
            response["artist_credit_id"] = result->artist_credit_id;
            response["artist_credit_name"] = result->artist_credit_name;
            
            crow::json::wvalue::list mbids;
            for (const auto& mbid : result->artist_credit_mbids) {
                mbids.push_back(mbid);
            }
            response["artist_credit_mbids"] = std::move(mbids);
            
            response["release_id"] = result->release_id;
            response["release_name"] = result->release_name;
            response["release_mbid"] = result->release_mbid;
            
            response["recording_id"] = result->recording_id;
            response["recording_name"] = result->recording_name;
            response["recording_mbid"] = result->recording_mbid;
            
            response["confidence"] = result->confidence;
            
            delete result;
            return crow::response(200, response);
        } else {
            response["error"] = "No match found";
            return crow::response(404, response);
        }
    });

    printf("Starting server on %s:%d\n", host.c_str(), port);
    printf("Index directory: %s\n", g_index_dir.c_str());
    app.bindaddr(host).port(port).multithreaded().run();

    return 0;
}