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

# TODO: Pin all depedencies
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(deps/nmslib/similarity_search)
add_subdirectory(deps/pcre2)
add_subdirectory(deps/armadillo-code)
add_subdirectory(deps/unidecode)
add_subdirectory(deps/cereal)
add_subdirectory(deps/SQLiteCpp)
add_subdirectory(deps/Catch2)

include(FetchContent)
FetchContent_Declare(libpq
    GIT_REPOSITORY https://gitlab.com/sabelka/libpq-standalone
    GIT_TAG        REL_16_1
    SOURCE_SUBDIR libpq)
FetchContent_MakeAvailable(libpq)

add_executable(mapping_server server.cpp tfidf_vectorizer.cpp levenshtein.cpp)
add_executable(mapping_creator mapping.cpp tfidf_vectorizer.cpp)
add_executable(mapping_test test.cpp tfidf_vectorizer.cpp levenshtein.cpp)

include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/jpcre2/src
                    deps/cereal
                    deps/SQLiteCpp/include/SQLiteCpp
                    deps/Catch2/src
                    ${libpq_SOURCE_DIR}/libpq/interfaces/libpq
                    ${libpq_SOURCE_DIR}/libpq/include
)

target_link_libraries(mapping_creator NonMetricSpaceLib
                                      unidecode
                                      libpcre2-8.a
                                      armadillo
                                      SQLiteCpp 
                                      pthread
                                      cereal
                                      pq
                                      bsd
)
target_link_libraries(mapping_server NonMetricSpaceLib
                                     unidecode
                                     libpcre2-8.a
                                     armadillo
                                     SQLiteCpp 
                                     pthread
                                     cereal
)

target_link_libraries(mapping_test NonMetricSpaceLib
                                   unidecode
                                   libpcre2-8.a
                                   armadillo
                                   SQLiteCpp
                                   pthread
                                   cereal
                                   Catch2::Catch2
)