#!/usr/bin/env python3

import csv
from time import monotonic
from subprocess import Popen, PIPE
from struct import pack
import sys
import os

from alphabet_detector import AlphabetDetector
import psycopg2
from psycopg2.extras import DictCursor, execute_values

from fuzzy_index import FuzzyIndex
from database import Mapping, create_db, open_db, db

# TODO: Remove the combined field of canonical data dump. Done, but make PR

# For wolf

#DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz"

# For wolf/SSH
DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz"

# For wolf/docker
#DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=musicbrainz-docker_db_1 port=5432 password=musicbrainz"

ARTIST_CONFIDENCE_THRESHOLD = .45
NUM_ROWS_PER_COMMIT = 25000
MAX_THREADS = 8

class MappingLookupIndex:
    
    def save_index_data(self, ad_file, artist_data):
        with open(ad_file, "wb") as f:
            for entry in artist_data:
                text = bytes(entry["text"], "utf-8")
                f.write(pack("II", entry["id"], len(text)))
                f.write(text)

    def create(self, conn, index_dir):
        last_row = None
        current_part_id = None

        t0 = monotonic()
        with conn.cursor(cursor_factory=psycopg2.extras.DictCursor) as curs:
            artist_data = []
            stupid_artist_data = []  # reserved for stupid artists like !!!
            recording_data = []
            release_data = []
            relrec_offsets = []
            relrec_offset = 0

            db_file = os.path.join(index_dir, "mapping.db")
            create_db(db_file)
            db.close()

            print("execute query")
            curs.execute(""" SELECT artist_credit_id
                                  , artist_mbids::TEXT[]
                                  , artist_credit_name
                                  , COALESCE(array_agg(a.sort_name ORDER BY acn.position)) as artist_credit_sortname
                                  , rel.id AS release_id
                                  , rel.gid::TEXT AS release_mbid
                                  , release_name
                                  , rec.id AS recording_id
                                  , rec.gid::TEXT AS recording_mbid
                                  , recording_name
                                  , score
                               FROM mapping.canonical_musicbrainz_data_release_support
                               JOIN recording rec
                                 ON rec.gid = recording_mbid
                               JOIN release rel
                                 ON rel.gid = release_mbid
                               JOIN artist_credit_name acn
                                 ON artist_credit_id = acn.artist_credit
                               JOIN artist a
                                 ON acn.artist = a.id
                              WHERE artist_credit_id < 10000
                           GROUP BY artist_credit_id
                                  , artist_mbids
                                  , artist_credit_name
                                  , release_name
                                  , rel.id
                                  , recording_name
                                  , rec.id
                                  , score
                           ORDER BY artist_credit_id""")
#                              WHERE artist_credit_id > 1230420 and artist_credit_id < 1230800

            print("load data")
            mapping_data = []
            ad = AlphabetDetector()
            import_file = os.path.join(index_dir, "import.csv")
            with open(import_file, 'w', newline='') as csvfile:
                fieldnames = ["artist_credit_id", 
                              "artist_mbids", 
                              "artist_credit_name", 
                              "artist_credit_sortname", 
                              "release_id", 
                              "release_mbid", 
                              "release_name", 
                              "recording_id", 
                              "recording_mbid", 
                              "recording_name", 
                              "score"]
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames, dialect="unix")
                for i, row in enumerate(curs):
                    if i == 0:
                        continue

                    if i % 1000000 == 0:
                        print("Indexed %d rows" % i)

                    if last_row is not None and row["artist_credit_id"] != last_row["artist_credit_id"]:

                        # Save artist data for artist index
                        encoded = FuzzyIndex.encode_string(last_row["artist_credit_name"])
                        if encoded:
                            artist_data.append({ "text": encoded,
                                                 "id": last_row["artist_credit_id"] })
                            if not ad.only_alphabet_chars(last_row["artist_credit_name"], "LATIN"):
                                encoded = FuzzyIndex.encode_string(last_row["artist_credit_sortname"][0])
                                if encoded:
                                    # 幾何学模様 a                  Kikagaku Moyo c
                                    artist_data.append({ "text": encoded,
                                                         "id": last_row["artist_credit_id"] })

                        else:
                            encoded = FuzzyIndex.encode_string_for_stupid_artists(last_row["artist_credit_name"])
                            if not encoded:
                                last_row = row
                                continue
                            stupid_artist_data.append({ "text": encoded, 
                                                        "id": last_row["artist_credit_id"] })

                        recording_data = []
                        release_data = []
                
                    arow = dict(row)
                    arow["artist_mbids"] = ",".join(row["artist_mbids"])
                    arow["artist_credit_sortname"] = row["artist_credit_sortname"][0]
                    mapping_data.append(arow)

                    last_row = row

                    if len(mapping_data) > NUM_ROWS_PER_COMMIT:
                        for mrow in mapping_data:
                            writer.writerow(mrow)
                        mapping_data = []

                # dump out the last bits of data
                encoded = FuzzyIndex.encode_string(row["artist_credit_name"])
                if encoded:
                    artist_data.append({ "text": encoded,
                                         "id": row["artist_credit_id"] })
                else:
                    encoded = FuzzyIndex.encode_string_for_stupid_artists(last_row["artist_credit_name"])
                    stupid_artist_data.append({ "text": encoded,
                                               "id": row["artist_credit_id"] })

                if mapping_data:
                    for mrow in mapping_data:
                        writer.writerow(mrow)


        print("Import data into SQLite")
        try:
            with Popen(['sqlite3', db_file], stdin=PIPE, stdout=PIPE, universal_newlines=True, bufsize=1) as sql:
                print('.separator ","', file=sql.stdin, flush=True)
                print(".import '%s' mapping" % import_file, file=sql.stdin, flush=True)
                print("create index artist_credit_id_ndx on mapping(artist_credit_id);", file=sql.stdin, flush=True)
                print("create index release_id_ndx on mapping(release_id);", file=sql.stdin, flush=True)
                print("create index recording_id_ndx on mapping(recording_id);", file=sql.stdin, flush=True)
                print("create index release_id_recording_id_ndx on mapping(release_id, recording_id);", file=sql.stdin, flush=True)
        except subprocess.CalledProcessError as err:
            print("Failed to import data into SQLite: ", err)
            return

        # Get rid of the CSV files now that we've imported it
        os.unlink(import_file)

        print("save artist index data")
        ad_file = os.path.join(index_dir, "artist_data.txt")
        self.save_index_data(ad_file, artist_data)
        ad_file = os.path.join(index_dir, "stupid_artist_data.txt")
        self.save_index_data(ad_file, stupid_artist_data)
        
        t1 = monotonic()
        print("loaded data and saved artist index data in %.1f seconds." % (t1 - t0))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: mapping_index.py <index dir>")
        sys.exit(-1)

    index_dir = sys.argv[1]

    mi = MappingLookupIndex()
    with psycopg2.connect(DB_CONNECT) as conn:
        try:
            os.makedirs(index_dir)
        except OSError:
            pass

        mi.create(conn, index_dir)