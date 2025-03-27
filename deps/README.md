Dependencies
============

Clone and build the following depedencies here:

  nmslib

  https://github.com/nmslib/nmslib.git

  install deps: 

  sudo apt-get install libboost-all-dev libgsl0-dev libeigen3-dev
  
  then:

  cd nmslib/similarity_index/build
  cmake -DCMAKE_BUILD_TYPE=Release .
  make

  Unidecode for C++

  https://github.com/marekru/unidecode.git


  SQLite Cpp:

  https://github.com/SRombauts/SQLiteCpp.git


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

  Cista

  https://github.com/felixguendling/cista.git

  No setup needed
