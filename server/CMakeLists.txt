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
add_subdirectory(deps/nmslib/similarity_search)
add_subdirectory(deps/pcre2)
add_subdirectory(deps/armadillo-code)
add_subdirectory(deps/unidecode)
add_subdirectory(deps/cereal)
add_executable(ff_server test.cpp tfidf_vectorizer.cpp)
include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/jpcre2/src
                    deps/cereal
)
target_link_libraries(ff_server NonMetricSpaceLib
                                unidecode
                                libpcre2-8.a
                                armadillo
                                pthread
                                cereal
)
