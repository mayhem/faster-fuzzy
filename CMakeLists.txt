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
add_executable(ff_server test.cpp tfidf_vectorizer.cpp)
include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/pcre2/build
                    deps/jpcre2/src
                    deps/armadillo/include
)
target_link_libraries(ff_server /home/robert/metabrainz/faster-fuzzy/deps/nmslib/similarity_search/debug/libNonMetricSpaceLib.a
                                /home/robert/metabrainz/faster-fuzzy/deps/unidecode/build/libunidecode.a
                                /usr/lib/x86_64-linux-gnu/libboost_regex.a
                                /home/robert/metabrainz/faster-fuzzy/deps/pcre2/build/libpcre2-8.a
                                armadillo
				pthread
)
