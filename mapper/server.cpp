#include <string>
#include <cstdlib>
#include <chrono>
#include "crow.h"
#include "fsm.hpp"

using namespace std;

// Shared resources (created once, shared across all threads)
static string g_index_dir;
static string g_templates_dir = "templates";
static int g_cache_size = 25;
static ArtistIndex* g_artist_index = nullptr;
static IndexCache* g_index_cache = nullptr;

MappingSearch* get_mapping_search() {
    thread_local MappingSearch* mapping_search = nullptr;
    if (mapping_search == nullptr) {
        mapping_search = new MappingSearch(g_index_dir, g_artist_index, g_index_cache);
    }
    return mapping_search;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --host <hostname>   Hostname/IP to bind to (default: 0.0.0.0)\n");
    printf("  -p, --port <port>       Port number to listen on (default: 5000)\n");
    printf("  -i, --index <dir>       Index directory (required)\n");
    printf("  -t, --templates <dir>   Templates directory (default: ./templates)\n");
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
        } else if (arg == "-t" || arg == "--templates") {
            if (i + 1 < argc) {
                g_templates_dir = argv[++i];
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

    // Create and load shared resources before starting server threads
    printf("Loading shared indexes...\n");
    g_artist_index = new ArtistIndex(g_index_dir);
    g_artist_index->load();
    
    g_index_cache = new IndexCache(g_cache_size);
    // g_index_cache->start();  // Enable cache cleaner if needed
    
    printf("Indexes loaded.\n");

    crow::SimpleApp app;
    crow::mustache::set_global_base(g_templates_dir);

    CROW_ROUTE(app, "/")
    ([](const crow::request& req) {
        auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx;
        
        auto artist_credit_name = req.url_params.get("artist_credit_name");
        auto release_name = req.url_params.get("release_name");
        auto recording_name = req.url_params.get("recording_name");
        
        // Preserve form values
        ctx["artist_credit_name"] = artist_credit_name ? artist_credit_name : "";
        ctx["release_name"] = release_name ? release_name : "";
        ctx["recording_name"] = recording_name ? recording_name : "";
        
        // Only search if we have required parameters
        if (artist_credit_name && recording_name) {
            ctx["searched"] = true;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            MappingSearch* mapping_search = get_mapping_search();
            SearchMatch* result = mapping_search->search(
                artist_credit_name,
                release_name ? release_name : "",
                recording_name
            );
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            ctx["search_time_ms"] = std::to_string(duration.count());
            
            if (result) {
                ctx["has_match"] = true;
                ctx["result_artist_credit_name"] = result->artist_credit_name;
                
                // Build artist MBIDs as list for template iteration
                std::vector<crow::mustache::context> mbid_list;
                for (size_t i = 0; i < result->artist_credit_mbids.size(); i++) {
                    crow::mustache::context mbid_ctx;
                    mbid_ctx["mbid"] = result->artist_credit_mbids[i];
                    if (i == result->artist_credit_mbids.size() - 1) {
                        mbid_ctx["last"] = true;
                    }
                    mbid_list.push_back(mbid_ctx);
                }
                ctx["artist_mbids"] = std::move(mbid_list);
                
                ctx["result_release_name"] = result->release_name;
                ctx["result_release_mbid"] = result->release_mbid;
                ctx["result_recording_name"] = result->recording_name;
                ctx["result_recording_mbid"] = result->recording_mbid;
                
                char conf_buf[32];
                snprintf(conf_buf, sizeof(conf_buf), "%.2f", result->confidence);
                ctx["result_confidence"] = conf_buf;
                
                delete result;
            } else {
                ctx["has_match"] = false;
            }
        }
        
        return page.render(ctx);
    });

    CROW_ROUTE(app, "/docs")
    ([]() {
        auto page = crow::mustache::load("docs.html");
        return page.render();
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