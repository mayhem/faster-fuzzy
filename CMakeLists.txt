cmake_minimum_required(VERSION 3.22)

project(faster_fuzzy
  VERSION
    0.1
  DESCRIPTION
    "Fast MBID mapper"
  LANGUAGES
    CXX
)

add_executable(ff_server test.cpp)
include_directories(deps/nmslib/similarity_search/include deps/unidecode/include)
target_link_libraries(ff_server /home/robert/metabrainz/faster-fuzzy/deps/nmslib/similarity_search/release/libNonMetricSpaceLib.a
                                /home/robert/metabrainz/faster-fuzzy/deps/unidecode/build/libunidecode.a
                                /usr/lib/x86_64-linux-gnu/libboost_regex.a)
