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
set(CMAKE_CXX_STANDARD 20)
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

add_executable(make_indexes make_indexes.cpp tfidf_vectorizer.cpp)
add_executable(test test.cpp tfidf_vectorizer.cpp levenshtein.cpp)
add_executable(explore explore.cpp tfidf_vectorizer.cpp levenshtein.cpp)
add_executable(make_mapping make_mapping.cpp tfidf_vectorizer.cpp)
add_executable(server server.cpp tfidf_vectorizer.cpp levenshtein.cpp)

include_directories(deps/nmslib/similarity_search/include
                    deps/unidecode/include
                    deps/jpcre2/src
                    deps/cereal
                    deps/SQLiteCpp/include/SQLiteCpp
                    deps/Catch2/src
                    deps/Crow/include
                    deps/asio/asio/include
                    ${libpq_SOURCE_DIR}/libpq/interfaces/libpq
                    ${libpq_SOURCE_DIR}/libpq/include
                    deps/listenbrainz-matching-tools/cpp/include
                    ${CMAKE_CURRENT_BINARY_DIR}/deps/pcre2/interface
)


target_link_libraries(make_indexes NonMetricSpaceLib
                              unidecode
                              pcre2-8-static
                              armadillo
                              SQLiteCpp 
                              pthread
                              cereal
                              pq
                              bsd
)

target_link_libraries(test NonMetricSpaceLib
                           unidecode
                           pcre2-8-static
                           armadillo
                           SQLiteCpp
                           pthread
                           cereal
                           Catch2::Catch2
)

target_link_libraries(explore NonMetricSpaceLib
                                    unidecode
                                    pcre2-8-static
                                    armadillo
                                    SQLiteCpp
                                    pthread
                                    cereal
                                    Catch2::Catch2
                                    readline
)

target_link_libraries(make_mapping NonMetricSpaceLib
                             unidecode
                             pcre2-8-static
                             armadillo
                             SQLiteCpp
                             pthread
                             cereal
                             pq
                             bsd
)

target_link_libraries(server NonMetricSpaceLib
                             unidecode
                             pcre2-8-static
                             armadillo
                             SQLiteCpp
                             pthread
                             cereal
)