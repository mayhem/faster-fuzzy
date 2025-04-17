Dependencies
============

Clone and build the following depedencies here:

  nmslib

  git@github.com:metabrainz/nmslib.git

  install deps: 

  sudo apt-get install libboost-all-dev libgsl0-dev libeigen3-dev
  
  then:

  cd nmslib/similarity_index/build
  cmake -DCMAKE_BUILD_TYPE=Release .
  make

  Unidecode for C++

  https://github.com/marekru/unidecode.git

  edit unidecode/CMakeLists.txt and comment out:

  # add_subdirectory(tests)


  SQLite Cpp:

  https://github.com/SRombauts/SQLiteCpp.git
  No setup needed


  Crow:
  https://github.com/CrowCpp/Crow.git


  PCRE2

  https://github.com/PCRE2Project/pcre2.git
  cd pcre2
  mkdir build
  cmake ..
  make


  JPCRE2

  https://github.com/jpcre2/jpcre2.git

  ./configure
  make
  make install
  
  Armadillo

  https://gitlab.com/conradsnicta/armadillo-code
  No setup needed

  Cereal

  https://github.com/USCiLab/cereal.git
  No setup needed
