#!/usr/bin/env python3

import csv
from time import monotonic
from subprocess import Popen, PIPE
from struct import pack
import sys
import os

import psycopg2
from psycopg2.extras import DictCursor, execute_values

from database import Mapping, create_db, open_db, db

# TODO: Remove the combined field of canonical data dump. Done, but make PR
DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz"

# For wolf
#DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz"

# For wolf/SSH
#DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=localhost port=5432 password=musicbrainz"

# For wolf/docker
#DB_CONNECT = "dbname=musicbrainz_db user=musicbrainz host=musicbrainz-docker_db_1 port=5432 password=musicbrainz"

ARTIST_CONFIDENCE_THRESHOLD = .45
NUM_ROWS_PER_COMMIT = 25000
MAX_THREADS = 8


class MappingLookupIndex:

    def create(self, conn, index_dir):
        t0 = monotonic()
        with conn.cursor(name="big_ass_cursor", cursor_factory=psycopg2.extras.DictCursor) as curs:
            curs.itersize = NUM_ROWS_PER_COMMIT
            db_file = os.path.join(index_dir, "mapping.db")
            create_db(db_file)
            db.close()

            print("execute query")
            curs.execute("""         SELECT rec.artist_credit AS artist_credit_id
                                          , artist_mbids::TEXT[]
                                          , artist_credit_name
                                          , COALESCE(array_agg(a.sort_name ORDER BY acn.position)) as artist_credit_sortname
                                          , rel.id AS release_id
                                          , rel.gid::TEXT AS release_mbid
                                          , rel.artist_credit AS release_artist_credit_id
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
                                      WHERE artist_credit_id > 1
                                   GROUP BY artist_credit_id
                                          , artist_mbids
                                          , artist_credit_name
                                          , release_name
                                          , rel.id
                                          , rel.artist_credit
                                          , recording_name
                                          , rec.id
                                          , score
                              UNION
                                   SELECT r.artist_credit as artist_credit_id
                                        , array_agg(a.gid::TEXT) as artist_mbids
                                        , acn."name" as artist_credit_name
                                        , COALESCE(array_agg(a.sort_name ORDER BY acn.position)) as artist_credit_sortname
                                        , 0 AS release_id
                                        , '' AS release_mbid
                                        , 0 AS release_artist_credit_id
                                        , '' AS release_name
                                        , r.id AS recording_id
                                        , r.gid::TEXT AS recording_mbid
                                        , r.name
                                        , 0
                                     FROM recording r
                                LEFT JOIN track t
                                       ON t.recording = r.id
                                     JOIN artist_credit_name acn
                                       ON r.artist_credit = acn.artist_credit
                                     JOIN artist a
                                       ON acn.artist = a.id
                                    WHERE t.id IS null
                                 GROUP BY r.artist_credit
                                        , artist_credit_name
                                        , release_name
                                        , r.name
                                        , r.id""")

            print("load data")
            mapping_data = []
            import_file = os.path.join(index_dir, "import.csv")
            with open(import_file, 'w', newline='') as csvfile:
                fieldnames = [
                    "artist_credit_id", "artist_mbids", "artist_credit_name", "artist_credit_sortname", "release_id", "release_mbid",
                    "release_artist_credit_id", "release_name", "recording_id", "recording_mbid", "recording_name", "score"
                ]
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames, dialect="unix")
                for i, row in enumerate(curs):
                    if i == 0:
                        continue

                    if i % 1000000 == 0:
                        print("Indexed %d rows" % i)

                    arow = dict(row)
                    arow["artist_mbids"] = ",".join(row["artist_mbids"])
                    arow["artist_credit_sortname"] = row["artist_credit_sortname"][0]
                    mapping_data.append(arow)

                    if len(mapping_data) > NUM_ROWS_PER_COMMIT:
                        for mrow in mapping_data:
                            writer.writerow(mrow)
                        mapping_data = []

                # dump out the last bits of data
                if mapping_data:
                    for mrow in mapping_data:
                        writer.writerow(mrow)

        print("Import data into SQLite")
        try:
            with Popen(['sqlite3', db_file], stdin=PIPE, stdout=PIPE, universal_newlines=True, bufsize=1) as sql:
                print('.separator ","', file=sql.stdin, flush=True)
                print(".import '%s' mapping" % import_file, file=sql.stdin, flush=True)
                print("create index artist_credit_id_ndx on mapping(artist_credit_id);", file=sql.stdin, flush=True)
                print("create index release_artist_credit_id_ndx on mapping(release_artist_credit_id);", file=sql.stdin, flush=True)
                print("create index release_id_ndx on mapping(release_id);", file=sql.stdin, flush=True)
                print("create index recording_id_ndx on mapping(recording_id);", file=sql.stdin, flush=True)
                print("create index release_id_recording_id_ndx on mapping(release_id, recording_id);", file=sql.stdin, flush=True)
        except subprocess.CalledProcessError as err:
            print("Failed to import data into SQLite: ", err)
            return

        # Get rid of the CSV files now that we've imported it
        os.unlink(import_file)

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
