import os
from peewee import *

PRAGMAS = (
    ('foreign_keys', 1),
#    ('journal_mode', 'WAL'),
)

db = SqliteDatabase(None, pragmas=PRAGMAS)

class Mapping(Model):

    class Meta:
        database = db
        table_name = "mapping"
        primary_key = False

    artist_credit_id = IntegerField(null=False)
    artist_mbids = TextField(null=False)
    artist_credit_name = TextField(null=False)
    artist_credit_sortname = TextField()

    release_id = IntegerField(null=False)
    release_mbid = TextField(null=False)
    release_name = TextField()

    recording_id = IntegerField(null=False)
    recording_mbid = TextField(null=False)
    recording_name = TextField()

    score = IntegerField(null=False)
    shard_ch = FixedCharField(null=False)

class IndexCache(Model):
    class Meta:
        database = db
        table_name = "index_cache"
        primary_key = False

    artist_credit_id = IntegerField(null=False, index=True)
    artist_data = BlobField(null=False)

def create_db(db_file):
    try:
        os.unlink(db_file)
    except OSError:
        pass

    db.init(db_file)
    db.connect()
    db.create_tables((Mapping,IndexCache))

def open_db(db_file):
    db.init(db_file)
    db.connect()
