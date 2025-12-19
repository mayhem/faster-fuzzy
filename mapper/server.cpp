#include <string>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <atomic>
#include "crow.h"
#include "fsm.hpp"

using namespace std;

// Shared resources (created once, shared across all threads)
static string g_index_dir = "/data";
static string g_templates_dir = "/mapper/templates";
static int g_cache_size = 100;  // in MB
static int g_num_threads = 0;  // 0 = use all available cores
static ArtistIndex* g_artist_index = nullptr;
static IndexCache* g_index_cache = nullptr;
static std::atomic<bool> g_ready{false};

MappingSearch* get_mapping_search() {
    thread_local MappingSearch* mapping_search = nullptr;
    if (mapping_search == nullptr) {
        mapping_search = new MappingSearch(g_index_dir, g_artist_index, g_index_cache);
    }
    return mapping_search;
}

void print_usage(const char* program_name) {
    log("Usage: %s [options]", program_name);
    log("Options:");
    log("  -h, --host <hostname>   Hostname/IP to bind to (default: 0.0.0.0)");
    log("  -p, --port <port>       Port number to listen on (default: 5000)");
    log("  -i, --index <dir>       Index directory (required)");
    log("  -t, --templates <dir>   Templates directory (default: /mapper/templates)");
    log("  -c, --cache-memory <MB> Max index cache memory usage");
    log("  --help                  Show this help message");
}

int main(int argc, char* argv[]) {
    init_logging();
    
    // Default values
    string host = "0.0.0.0";
    int port = 5000;
    
    // Read defaults from environment variables first
    const char* env_host = std::getenv("HOST");
    if (env_host && strlen(env_host) > 0) {
        host = env_host;
    }
    
    const char* env_port = std::getenv("PORT");
    if (env_port && strlen(env_port) > 0) {
        int p = atoi(env_port);
        if (p > 0 && p <= 65535) port = p;
    }
    
    const char* env_index_dir = std::getenv("INDEX_DIR");
    if (env_index_dir && strlen(env_index_dir) > 0) {
        g_index_dir = env_index_dir;
    }
    
    const char* env_template_dir = std::getenv("TEMPLATE_DIR");
    if (env_template_dir && strlen(env_template_dir) > 0) {
        g_templates_dir = env_template_dir;
    }
    
    const char* env_num_threads = std::getenv("NUM_THREADS");
    if (env_num_threads && strlen(env_num_threads) > 0) {
        g_num_threads = atoi(env_num_threads);
        if (g_num_threads < 0) g_num_threads = 0;
    }
    
    const char* env_cache_size = std::getenv("MAX_CACHE_SIZE");
    if (env_cache_size && strlen(env_cache_size) > 0) {
        g_cache_size = atoi(env_cache_size);
        if (g_cache_size < 1) g_cache_size = 100;
    }

    // Parse command line arguments (override env vars)
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                log("Error: %s requires an argument", arg.c_str());
                return 1;
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    log("Error: Invalid port number");
                    return 1;
                }
            } else {
                log("Error: %s requires an argument", arg.c_str());
                return 1;
            }
        } else if (arg == "-i" || arg == "--index") {
            if (i + 1 < argc) {
                g_index_dir = argv[++i];
            } else {
                log("Error: %s requires an argument", arg.c_str());
                return 1;
            }
        } else if (arg == "-t" || arg == "--templates") {
            if (i + 1 < argc) {
                g_templates_dir = argv[++i];
            } else {
                log("Error: %s requires an argument", arg.c_str());
                return 1;
            }
        } else {
            log("Error: Unknown argument: %s", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required environment variables
    if (g_index_dir.empty()) {
        log("Error: INDEX_DIR environment variable not set and no index directory provided");
        print_usage(argv[0]);
        return 1;
    }

    // Create index cache immediately (lightweight)
    g_index_cache = new IndexCache(g_cache_size);

    crow::SimpleApp app;
    crow::mustache::set_global_base(g_templates_dir);

    // Start background thread to load indexes
    std::thread loader_thread([&]() {
        log("Loading shared indexes...");
        g_artist_index = new ArtistIndex(g_index_dir);
        g_artist_index->load();
        log("Indexes loaded. Server ready.");
        g_ready = true;
    });
    loader_thread.detach();

    CROW_ROUTE(app, "/")
    ([](const crow::request& req) {
        // Show loading page if not ready
        if (!g_ready) {
            auto page = crow::mustache::load("loading.html");
            if (page.body().empty()) {
                return crow::response(500, "Template \"loading.html\" not found.");
            }
            return crow::response(200, page.render());
        }

        auto page = crow::mustache::load("index.html");
        if (page.body().empty()) {
            return crow::response(500, "Template \"index.html\" not found.");
        }
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
        
        return crow::response(200, page.render(ctx));
    });

    CROW_ROUTE(app, "/docs")
    ([]() {
        if (!g_ready) {
            auto page = crow::mustache::load("loading.html");
            return crow::response(200, page.render());
        }
        auto page = crow::mustache::load("docs.html");
        return crow::response(200, page.render());
    });

    CROW_ROUTE(app, "/mapping/lookup")
    ([](const crow::request& req) {
        // Return 503 Service Unavailable if not ready
        if (!g_ready) {
            crow::json::wvalue error;
            error["error"] = "Server is starting up, indexes are still loading";
            return crow::response(503, error);
        }

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

    log("Starting server on %s:%d", host.c_str(), port);
    log("Index directory: %s", g_index_dir.c_str());
    if (g_num_threads > 0) {
        log("Using %d threads", g_num_threads);
        app.bindaddr(host).port(port).concurrency(g_num_threads).run();
    } else {
        log("Using all available CPU cores");
        app.bindaddr(host).port(port).multithreaded().run();
    }

    return 0;
}