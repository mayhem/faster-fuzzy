cmake_minimum_required(VERSION 3.16)

# cmake -DCMAKE_BUILD_TYPE=Debug

project(faster_fuzzy
  VERSION
    0.1
  DESCRIPTION
    "Fast MBID mapper"
  LANGUAGES
    CXX
)

add_compile_options("-std=c++17")
add_subdirectory(deps/pcre2)
add_subdirectory(deps/cista)
add_subdirectory(deps/armadillo-code)
add_subdirectory(deps/unidecode)
add_executable(ff_server test.cpp tfidf_vectorizer.cpp)
include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/jpcre2/src
)
target_link_libraries(ff_server /home/robert/metabrainz/faster-fuzzy/deps/nmslib/similarity_search/debug/libNonMetricSpaceLib.a
                                libunidecode
                                libpcre2-8
                                armadillo
                 				pthread
                                cista
)
#                                /usr/lib/x86_64-linux-gnu/libboost_regex.a
