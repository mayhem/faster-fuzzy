#include <string>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>
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

void print_usage() {
    log("Usage: server");
    log("");
    log("Required environment variables:");
    log("  INDEX_DIR        Directory containing mapping.db and index files");
    log("");
    log("Optional environment variables:");
    log("  HOST             Hostname/IP to bind to (default: 0.0.0.0)");
    log("  PORT             Port number to listen on (default: 5000)");
    log("  TEMPLATE_DIR     Templates directory (default: /mapper/templates)");
    log("  NUM_THREADS      Number of worker threads (0 = auto, default: 0)");
    log("  MAX_CACHE_SIZE   Max index cache size in MB (default: 100)");
}

int main(int argc, char* argv[]) {
    init_logging();
    load_env_file();  // Load .env file, env vars take precedence
    
    // Parse arguments (options only)
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            log("Error: Unknown option: %s", arg.c_str());
            print_usage();
            return 1;
        }
    }
    
    // Get required INDEX_DIR from environment
    const char* env_index_dir = std::getenv("INDEX_DIR");
    if (!env_index_dir || strlen(env_index_dir) == 0) {
        log("Error: INDEX_DIR environment variable not set");
        print_usage();
        return 1;
    }
    g_index_dir = env_index_dir;
    
    // Get optional config from environment variables
    string host = "0.0.0.0";
    int port = 5000;
    
    const char* env_host = std::getenv("HOST");
    if (env_host && strlen(env_host) > 0) {
        host = env_host;
    }
    
    const char* env_port = std::getenv("PORT");
    if (env_port && strlen(env_port) > 0) {
        int p = atoi(env_port);
        if (p > 0 && p <= 65535) port = p;
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

    // Create index cache immediately (lightweight)
    g_index_cache = new IndexCache(g_cache_size);

    // Load shared indexes BEFORE starting the server
    log("Loading shared indexes...");
    g_artist_index = new ArtistIndex(g_index_dir);
    g_artist_index->load();
    log("Indexes loaded. Server ready.");
    g_ready = true;

    crow::SimpleApp app;
    crow::mustache::set_global_base(g_templates_dir);

    CROW_ROUTE(app, "/")
    ([](const crow::request& req) {
        // Show loading page if not ready
        if (!g_ready) {
            auto page_text = crow::mustache::load_text("loading.html");
            if (page_text.empty()) {
                return crow::response(500, "Template \"loading.html\" not found.");
            }
            auto page = crow::mustache::compile(page_text);
            return crow::response(200, page.render());
        }

        auto page_text = crow::mustache::load_text("index.html");
        if (page_text.empty()) {
            return crow::response(500, "Template \"index.html\" not found.");
        }
        auto page = crow::mustache::compile(page_text);
        crow::mustache::context ctx;
        
        auto artist_credit_name = req.url_params.get("artist_credit_name");
        auto release_name = req.url_params.get("release_name");
        auto recording_name = req.url_params.get("recording_name");
        
        // Preserve form values
        ctx["artist_credit_name"] = artist_credit_name ? artist_credit_name : "";
        ctx["release_name"] = release_name ? release_name : "";
        ctx["recording_name"] = recording_name ? recording_name : "";
        ctx["searched"] = false;
        
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

    CROW_ROUTE(app, "/supported")
    ([]() {
        if (!g_ready) {
            auto page = crow::mustache::load("loading.html");
            return crow::response(200, page.render());
        }
        
        auto page_text = crow::mustache::load_text("supported.html");
        if (page_text.empty()) {
            return crow::response(500, "Template \"supported.html\" not found.");
        }
        auto page = crow::mustache::compile(page_text);
        crow::mustache::context ctx;
        
        // Test cases - these match the ones in test.cpp
        struct TestCase {
            std::string artist;
            std::string release;
            std::string recording;
        };
        
        std::vector<TestCase> test_cases = {
            { "portishead", "portishead", "western eyes" },
            { "portished", "portishad", "western ey" },
            { "morcheeba", "parts of the process", "trigger hippie" },
            { "Billie Eilish", "", "COPYCAT" },
            { "Angelo Badalamenti", "Music From Twin Peaks", "Laura Palmer's Theme" },
            { "Alicia Keys", "Keys", "Skydive (originals)" },
            { "Godspeed You! Black Emperor", "Lift Your Skinny Fists Like Antennas to Heaven", "Like Antennas To Heaven\u2026" },
            { "Charli xcx", "BRAT", "365" },
            { "Milana", "MALLORCA STONER VOL.1", "Forest Tale" },
            { "x#!*\u2566\x39\x37", "1 LP", "untitled 1" },
            { "Ishay Ribo", "\u05e1\u05d5\u05e3 \u05d7\u05de\u05d4 \u05dc\u05d1\u05d5\u05d0", "\u05e8\u05d1\u05d9 \u05e9\u05de\u05e2\u05d5\u05df" },
            { "\u5e7e\u4f55\u5b66\u6a21\u69d8", "masana temples", "nana" },
            { "Kikagaku Moyo", "masana temples", "nana" },
            { "Harry Nilsson", "Nilsson Schmilsson", "Without You" },
            { "xhashsymbolexclamationpointasteriskrightdoublevertical97", "1 LP", "untitled 1" },
            { "Enya Patricia Brennan", "watermark", "watermark" },
            { "Hanan Ben Ari", "\u05dc\u05d0 \u05dc\u05d1\u05d3", "\u05e8\u05d2\u05e2" },
            { "!!!", "As If", "Ooo" },
            { "Yaakov Shwekey", "\u05d2\u05d5\u05e3 \u05d5\u05e0\u05e9\u05de\u05d4 1/4", "\u05d2\u05dc\u05d2\u05dc\u05d9\u05dd" },
            { "mynoise", "primeval forest", "springtime birds" },
            { "queen & david bowie", "Hot Space", "under pressure" },
            { "queen", "Hot Space", "under pressure" },
            { "darkseed", "", "entre dos tierras" },
            { "guns n' roses", "Appetite for Destruction", "welcome to the jungle" },
            { "guns n' roses", "Appetite for Destruction", "mr. brownstone" },
            { "pink floyd", "Animals", "pigs on the wing part 1" },
            { "nine inch nails", "The Downward Spiral", "hurt" },
            { "portishead", "Dummy", "glory box" },
            { "thievery corporation", "The Richest Man in Babylon", "heaven's gonna burn your eyes" },
            { "telepopmusik", "Genetic World", "trishika" },
            { "charli xcx", "SUCKER", "break the rules" },
            { "charli xcx", "SUCKER", "break the rules (femme remix)" },
            { "daft punk", "Random Access Memories", "horizon" },
            { "daft punk", "", "horizon" },
            { "the xx", "Coexist", "reconsider" },
            { "florence + the machine", "Lungs", "bird song" },
            { "the libertines", "The Libertines", "cyclops" },
            { "foo fighters", "Echoes, Silence, Patience & Grace", "seda" },
            { "lorde", "Pure Heroine", "bravado (fffrrannno remix)" },
            { "queen", "A Day at the Races", "teo torriatte (let us cling together)" },
            { "lana del rey", "Ultraviolence", "flipside" },
            { "david bowie", "The Next Day", "god bless the girl" },
            { "arctic monkeys", "AM", "2013" },
            { "void", "", "verdict for worst dictator" },
            { "\u9b31P feat. \u521d\u97f3\u30df\u30af", "HAPPYPILLS", "\u30ac" },
            { "void", "Remind a Locus", "verdict for worst dictator" },
            { "\u9b31P", "HAPPYPILLS", "\u30ac" },
            { "Eve", "pray - Single", "pray" },
        };
        
        // URL encode helper
        auto url_encode = [](const std::string& value) -> std::string {
            std::ostringstream escaped;
            escaped.fill('0');
            escaped << std::hex;
            for (char c : value) {
                if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << c;
                } else {
                    escaped << std::uppercase;
                    escaped << '%' << std::setw(2) << int((unsigned char)c);
                    escaped << std::nouppercase;
                }
            }
            return escaped.str();
        };
        
        std::vector<crow::mustache::context> cases_list;
        for (const auto& tc : test_cases) {
            crow::mustache::context tc_ctx;
            tc_ctx["artist_credit_name"] = tc.artist;
            tc_ctx["release_name"] = tc.release;
            tc_ctx["recording_name"] = tc.recording;
            tc_ctx["artist_credit_name_encoded"] = url_encode(tc.artist);
            tc_ctx["release_name_encoded"] = url_encode(tc.release);
            tc_ctx["recording_name_encoded"] = url_encode(tc.recording);
            cases_list.push_back(tc_ctx);
        }
        ctx["test_cases"] = std::move(cases_list);
        
        return crow::response(200, page.render(ctx));
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