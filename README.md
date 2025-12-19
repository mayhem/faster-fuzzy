# faster-fuzzy

Fast fuzzy matching service for mapping music metadata to MusicBrainz IDs.

## Quick Start

### 1. Clone

```bash
git clone --recurse-submodules https://github.com/metabrainz/faster-fuzzy.git
cd faster-fuzzy
```

### 2. Build

```bash
cd mapper
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. Create Base Index

```bash
./create <postgres_connection_string> <output_dir>
```

Example:
```bash
./create "host=localhost dbname=musicbrainz_db user=musicbrainz" ../index
```

### 4. Build Search Indexes

```bash
./indexer <index_dir>
```

Example:
```bash
./indexer ../index
```

### 5. Run Server

```bash
./server -i <index_dir> -t <templates_dir>
```

Example:
```bash
./server -i ../index -t ../templates -p 5000
```

Then open http://localhost:5000 in your browser.
