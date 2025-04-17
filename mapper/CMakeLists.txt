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
add_subdirectory(deps/SQLiteCpp)
add_executable(mapping_server server.cpp tfidf_vectorizer.cpp levenshtein.cpp)
add_executable(mapping_creator mapping.cpp tfidf_vectorizer.cpp)
include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/jpcre2/src
                    deps/cereal
                    deps/SQLiteCpp/include/SQLiteCpp
)
target_link_libraries(mapping_creator NonMetricSpaceLib
                                      unidecode
                                      libpcre2-8.a
                                      armadillo
                                      SQLiteCpp 
                                      pthread
                                      cereal
)
target_link_libraries(mapping_server NonMetricSpaceLib
                                     unidecode
                                     libpcre2-8.a
                                     armadillo
                                     SQLiteCpp 
                                     pthread
                                     cereal
)
