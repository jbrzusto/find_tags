#include "DB_Filer.hpp"
#include <stdio.h>
#include <time.h>
#include <math.h>

DB_Filer::DB_Filer (const string &out, const string &prog_name, const string &prog_version, double prog_ts, int  bootnum, double minGPSdt):
  prog_name(prog_name),
  num_hits(0),
  num_steps(0),
  bootnum(bootnum),
  minGPSdt(minGPSdt),
  lastGPSts(0)
{
  Check(sqlite3_open_v2(out.c_str(),
                        & outdb,
                        SQLITE_OPEN_READWRITE,
                        0),
        "Output database file does not exist.");

  sqlite3_exec(outdb,
               "pragma cache_size=4000;",
               0,
               0,
               0);

  string msg = "SQLite output database does not have valid 'batches' table.";

  Check( sqlite3_prepare_v2(outdb,
                            q_begin_batch,
                            -1,
                            &st_begin_batch,
                            0),
         msg);

  Check( sqlite3_prepare_v2(outdb,
                            q_end_batch,
                            -1,
                            &st_end_batch,
                            0),
         msg);

  Check( sqlite3_prepare_v2(outdb,
                            q_drop_saved_state,
                            -1,
                            &st_drop_saved_state,
                            0),
         "SQLite output database does not have valid 'batchState' table.");

  msg = "output DB table 'runs' is invalid";
  Check( sqlite3_prepare_v2(outdb, q_begin_run,
                            -1, &st_begin_run, 0), msg);

  Check( sqlite3_prepare_v2(outdb, q_end_run, -1, &st_end_run, 0),
         msg);

  Check( sqlite3_prepare_v2(outdb, q_end_run2, -1, &st_end_run2, 0),
         msg);

  Check(sqlite3_prepare_v2(outdb, q_add_hit, -1, &st_add_hit, 0),
        "output DB does not have valid 'hits' table.");

  if (minGPSdt >= 0) {
    Check(sqlite3_prepare_v2(outdb, q_add_GPS_fix,-1, &st_add_GPS_fix, 0),
          "output DB does not have valid 'gps' table.");
  } else {
    lastGPSts = +1.0 / 0.0; // ensures new timestamp is never late enough to record a GPS fix
  }

  Check(sqlite3_prepare_v2(outdb, q_add_time_fix, -1, &st_add_time_fix, 0),
        "output DB does not have valid 'timeFixes' table.");

  Check(sqlite3_prepare_v2(outdb, q_add_pulse_count, -1, &st_add_pulse_count, 0),
        "output DB does not have valid 'pulseCounts' table.");

  Check(sqlite3_prepare_v2(outdb, q_add_recv_param, -1, &st_add_recv_param, 0),
        "output DB does not have valid 'params' table.");

  sqlite3_stmt * st_check_batchprog;
  msg = "output DB does not have valid 'batchProgs' table.";

  Check(sqlite3_prepare_v2(outdb,
                           "select progVersion from batchProgs where progName=? order by batchID desc limit 1",
                           -1, &st_check_batchprog, 0),
        SQLITE_OK,
        SQLITE_ROW,
        msg);
  sqlite3_bind_text(st_check_batchprog, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  msg = "output DB does not have valid 'batchParams' table.";
  Check( sqlite3_prepare_v2(outdb,
                            "select paramVal from batchParams where progName = ? and paramName = ? order by batchID desc limit 1",
                            -1, &st_check_param, 0),
         msg);
  sqlite3_bind_text(st_check_param, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  Check( sqlite3_prepare_v2(outdb,
                            "insert into batchParams (batchID, progName, paramName, paramVal) \
                                           values (?,       ?,        ?,         ?)",
                            -1, &st_add_param, 0),
         "output DB does not have valid 'batchParams' table.");

  sqlite3_stmt * st_get_rid;
  Check( sqlite3_prepare_v2(outdb,
                            "select max(runID) from runs",
                            -1,
                            & st_get_rid,
                            0),
         "SQLite output database does not have valid 'runs' table - missing runID?");

  sqlite3_step(st_get_rid);
  rid = 1 + sqlite3_column_int(st_get_rid, 0);
  sqlite3_finalize(st_get_rid);
  st_get_rid = 0;

  // get the next ambigID to use; this is the most negative existing value minus 1.
  // if there are no entries in batchAmbig so far, this is -1.

  msg = "SQLite output database does not have valid 'tagAmbig' table";

  sqlite3_stmt * st_get_last_ambigID;
  Check( sqlite3_prepare_v2(outdb,
                            "select coalesce(min(ambigID) - 1, -1) from tagAmbig",
                            -1,
                            & st_get_last_ambigID,
                            0),
         msg);

  sqlite3_step(st_get_last_ambigID);
  next_proxyID = sqlite3_column_int(st_get_last_ambigID, 0);
  sqlite3_finalize(st_get_last_ambigID);
  st_get_last_ambigID = 0;

  // prepare queries for loading/saving ambiguity groups

  Check( sqlite3_prepare_v2(outdb,
                            q_load_ambig,
                            -1,
                            & st_load_ambig,
                            0),
         msg);

  Check( sqlite3_prepare_v2(outdb,
                            q_save_ambig,
                            -1,
                            & st_save_ambig,
                            0),
         msg);


  const char * ftsm = "SQLite output database does not have valid 'batchState' table";

  sqlite3_enable_load_extension(outdb, 1);

  const static int MAX_PATH_SIZE = 2048;
  char extension_lib_path_buffer[2*MAX_PATH_SIZE + 1];
  int n = readlink("/proc/self/exe", extension_lib_path_buffer, MAX_PATH_SIZE);
  for (;;) { // not a loop
    if (n > 0) {
      extension_lib_path_buffer[n] = '\0';
      char * dir_slash = (char *) memrchr(extension_lib_path_buffer, '/', n);
      if (dir_slash) {
        strcpy( dir_slash + 1, "Sqlite_Compression_Extension.so");
        Check( sqlite3_prepare_v2(outdb,
                                  q_load_extension,
                                  -1,
                                  &st_load_extension,
                                  0),
               "Can't prepare statement to load extension library!");

        sqlite3_bind_text(st_load_extension, 1, extension_lib_path_buffer, -1, SQLITE_STATIC);
        int res = sqlite3_step(st_load_extension);
        if (res == SQLITE_DONE || res == SQLITE_ROW) {
          sqlite3_finalize(st_load_extension);
          st_load_extension = 0;
          break;
        }
      }
    }
    // all errors end up here
    throw std::runtime_error("Unable to load Sqlite_Compression_Extension.so from folder with `find_tags_motus`");
  };

  Check ( sqlite3_prepare_v2(outdb, q_save_findtags_state, -1, & st_save_findtags_state, 0), ftsm);
  sqlite3_bind_text(st_save_findtags_state, 2, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  Check ( sqlite3_prepare_v2(outdb, q_load_findtags_state, -1, & st_load_findtags_state, 0), ftsm);
  sqlite3_bind_text(st_load_findtags_state, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  begin_tx();
  begin_batch(bootnum);

  int rv = Check(sqlite3_step(st_check_batchprog), SQLITE_DONE, SQLITE_ROW, msg);

  if (rv == SQLITE_DONE ||
      (rv == SQLITE_ROW && string((const char *) sqlite3_column_text(st_check_batchprog, 0)) != prog_version)) {
    // program version has changed since latest batchID for which it was recorded,
    // so record new value
    sprintf(qbuf,
          "insert into batchProgs (batchID, progName, progVersion, progBuildTS) \
                           values (%d,      '%s',     '%s',        %f)",
          bid,
          prog_name.c_str(),
          prog_version.c_str(),
          prog_ts);

    Check( sqlite3_exec(outdb,
                        qbuf,
                        0,
                        0,
                        0),
           msg);
  }

  Check( sqlite3_exec(outdb,        "\
create table if not exists pulses (  \
   batchID integer,                  \
   ts      float(53),                \
   ant     integer,                  \
   antFreq float(53),                \
   dfreq    float(53),               \
   sig     float,                    \
   noise   float                     \
);                                   \
create index if not exists pulses_ts on pulses(ts);\
create index if not exists pulses_batchID on pulses(batchID);",
                      0,
                      0,
                      0),
         "unable to create pulses table in output database");

  msg = "unable to prepare query for add_pulse";
  Check( sqlite3_prepare_v2(outdb, q_add_pulse,
                            -1, &st_add_pulse, 0), msg);
};


DB_Filer::~DB_Filer() {

  end_tx();
  sqlite3_exec(outdb,
               "pragma journal_mode=delete;",
               0,
               0,
               0);
  sqlite3_finalize(st_save_findtags_state);
  sqlite3_finalize(st_load_findtags_state);
  sqlite3_finalize(st_begin_batch);
  sqlite3_finalize(st_drop_saved_state);
  sqlite3_finalize(st_end_batch);
  sqlite3_finalize(st_begin_run);
  sqlite3_finalize(st_end_run);
  sqlite3_finalize(st_end_run2);
  if (minGPSdt >= 0)
    sqlite3_finalize(st_add_GPS_fix);
  sqlite3_finalize(st_add_hit);
  sqlite3_finalize(st_add_pulse);
  sqlite3_close(outdb);
  outdb = 0;
};

const char *
DB_Filer::q_begin_run =
 "insert into runs (runID, batchIDbegin, motusTagID, ant, tsBegin) values (?, ?, ?, ?, ?)";
//                  1      2             3           4    5

DB_Filer::Run_ID
DB_Filer::begin_run(Motus_Tag_ID mid, int ant, Timestamp ts) {
  sqlite3_bind_int(st_begin_run, 1, rid); // bind run ID
  // batchIDbegin bound at start of batch
  sqlite3_bind_int(st_begin_run, 3, mid); // bind tag ID
  sqlite3_bind_int(st_begin_run, 4, ant); // bind antenna
  sqlite3_bind_double(st_begin_run, 5, ts); // bind tsBegin
  step_commit(st_begin_run);
  return rid++;
};

const char *
DB_Filer::q_end_run  = "update runs set len=?,tsEnd=?,done=? where runID=?";
//                                      1     2       3            4
const char *
DB_Filer::q_end_run2 = "insert into batchRuns (batchID, runID) values (?,    ?)";
//                                             1        2
void
DB_Filer::end_run(Run_ID rid, int n, Timestamp ts, bool countOnly) {
  sqlite3_bind_int(st_end_run, 1, n); // bind number of hits in run
  sqlite3_bind_double(st_end_run, 2, ts); // bind tsEnd
  sqlite3_bind_int(st_end_run, 3, countOnly ? 0: 1); // is this run really finished?
  sqlite3_bind_int(st_end_run, 4, rid);  // bind run number
  step_commit(st_end_run);

  // add record indicating this run overlaps this batch
  // (this doesn't necessarily mean the run had hits in this batch; it might
  // simply have ended due to no more hits, or the run might still be active
  // because a short batch didn't span enough time to expire the candidate)
  sqlite3_bind_int(st_end_run2, 2, rid); // bind run ID
  step_commit(st_end_run2);
};

const char *
DB_Filer::q_add_hit =
"insert into hits (batchID, runID, ts, sig, sigSD, noise, freq, freqSD, slop, burstSlop) \
         values   (?,       ?,     ?,  ?,   ?,     ?,     ?,    ?,      ?,    ?)";
//               1       2     3  4    5     6     7    8      9     10

void
DB_Filer::add_hit(Run_ID rid, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop) {
  sqlite3_bind_int   (st_add_hit, 1, bid);
  sqlite3_bind_int   (st_add_hit, 2, rid);
  sqlite3_bind_double(st_add_hit, 3, ts);
  sqlite3_bind_double(st_add_hit, 4, sig);
  sqlite3_bind_double(st_add_hit, 5, sigSD);
  sqlite3_bind_double(st_add_hit, 6, noise);
  sqlite3_bind_double(st_add_hit, 7, freq);
  sqlite3_bind_double(st_add_hit, 8, freqSD);
  sqlite3_bind_double(st_add_hit, 9, slop);
  sqlite3_bind_double(st_add_hit, 10, burstSlop);
  step_commit(st_add_hit);
  ++ num_hits;
};

const char *
DB_Filer::q_add_GPS_fix =
"insert or ignore into gps (ts, batchID, gpsts, lat, lon, alt) \
                     values(?,  ?,       null,  ?,   ?,   ?)";
//                       1   2             3   4    5

void
DB_Filer::add_GPS_fix(double ts, double lat, double lon, double alt) {
  if (ts - lastGPSts < minGPSdt)
    return;
  lastGPSts = ts;
  sqlite3_bind_double   (st_add_GPS_fix, 1, ts);
  sqlite3_bind_int      (st_add_GPS_fix, 2, bid);
  // for now, there is no gpsts, so we use null; eventually:  sqlite3_bind_double   (st_add_GPS_fix, 3, gpsts);
  sqlite3_bind_double   (st_add_GPS_fix, 3, lat);
  sqlite3_bind_double   (st_add_GPS_fix, 4, lon);
  sqlite3_bind_double   (st_add_GPS_fix, 5, alt);
  step_commit(st_add_GPS_fix);
};


const char *
DB_Filer::q_add_time_fix =
"insert into timeFixes (monoBN, tsLow, tsHigh, fixedBy, error, comment) \
                 values(?,       ?,     ?,    ?,       ?,     ?)";
//                      1        2      3     4        5      6

void
DB_Filer::add_time_fix(Timestamp tsLow, Timestamp tsHigh, Timestamp by, Timestamp error, char fixType) {
  sqlite3_bind_int    (st_add_time_fix, 1, bootnum);
  sqlite3_bind_double (st_add_time_fix, 2, tsLow);
  sqlite3_bind_double (st_add_time_fix, 3, tsHigh);
  sqlite3_bind_double (st_add_time_fix, 4, by);
  sqlite3_bind_double (st_add_time_fix, 5, error);
  sqlite3_bind_text   (st_add_time_fix, 6, & fixType, 1, SQLITE_TRANSIENT);
  step_commit(st_add_time_fix);
};

const char *
DB_Filer::q_add_pulse_count =
"insert or replace into pulseCounts (batchID, ant, hourBin, count) \
                           values(?,       ?,        ?,       ?)";
//                            1       2        3       4

void
DB_Filer::add_pulse_count(double hourBin, int ant, int count) {
  sqlite3_bind_int    (st_add_pulse_count, 1, bid);
  sqlite3_bind_int    (st_add_pulse_count, 2, ant);
  sqlite3_bind_double (st_add_pulse_count, 3, hourBin);
  sqlite3_bind_int    (st_add_pulse_count, 4, count);
  step_commit(st_add_pulse_count);
};


void
DB_Filer::step_commit(sqlite3_stmt * st) {
  Check(sqlite3_step(st), SQLITE_DONE, "unable to step statement");
  sqlite3_reset(st);
  if (++num_steps == steps_per_tx) {
    end_tx();
    begin_tx();
  }
}

char
DB_Filer::qbuf[256];

void
DB_Filer::add_param(const string &name, double value) {
  sqlite3_reset(st_check_param);
  sqlite3_bind_text(st_check_param, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  int rv = sqlite3_step(st_check_param);
  if (rv == SQLITE_DONE || (rv == SQLITE_ROW && sqlite3_column_double(st_check_param, 0) != value)) {
    // parameter value has changed since last batchID where it was set,
    // so record new value
    sqlite3_bind_int(st_add_param, 1, bid);
    // we use SQLITE_TRANSIENT in the following to make a copy, otherwise
    // the caller's copy might be destroyed before this transaction is committed
    sqlite3_bind_text(st_add_param, 2, prog_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st_add_param, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st_add_param, 4, value);
    step_commit(st_add_param);
  }
};

void
DB_Filer::add_param(const string &name, const string &value) {
  sqlite3_reset(st_check_param);
  sqlite3_bind_text(st_check_param, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  int rv = sqlite3_step(st_check_param);
  if (rv == SQLITE_DONE || (rv == SQLITE_ROW && std::string((const char *)sqlite3_column_text(st_check_param, 0)) != value)) {
    // parameter value has changed since last batchID where it was set,
    // so record new value
    sqlite3_bind_int(st_add_param, 1, bid);
    // we use SQLITE_TRANSIENT in the following to make a copy, otherwise
    // the caller's copy might be destroyed before this transaction is committed
    sqlite3_bind_text(st_add_param, 2, prog_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st_add_param, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st_add_param, 4, value.c_str(), -1, SQLITE_TRANSIENT);
    step_commit(st_add_param);
  }
};

int
DB_Filer::Check(int code, int wants, int wants2, int wants3, const std::string & err) {
  if (code == wants
      || (wants2 != -1 && code == wants2)
      || (wants3 != -1 && code == wants3))
    return code;
  throw std::runtime_error(err + "\nSqlite error: " + sqlite3_errmsg(outdb));
};


// start new batch; uses 1 + ID of latest ended batch

const char *
DB_Filer::q_begin_batch = "insert into batches (monoBN, ts) values (?, ?)";

const char *
DB_Filer::q_drop_saved_state = "delete from batchState where monoBN=?";

void
DB_Filer::begin_batch(int bootnum) {
  num_hits = 0;

  sqlite3_bind_int(st_begin_batch, 1, bootnum);
  // get current time, in GMT
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  sqlite3_bind_double(st_begin_batch, 2, tp.tv_sec + 1.0e-9 * tp.tv_nsec);
  step_commit(st_begin_batch);
  bid = sqlite3_last_insert_rowid(outdb);

  // set batch ID for "insert into runs" query
  sqlite3_bind_int(st_begin_run, 2, bid);

  // set batch ID for "insert into batchRuns" query
  sqlite3_bind_int(st_end_run2, 1, bid);

  // set bootnum (monotonic boot session) for this batch
  this->bootnum = bootnum;
};

const char *
DB_Filer::q_end_batch = "update batches set tsStart=?, tsEnd=?, numHits=? where batchID=?";
//                                     1         2       3             4
void
DB_Filer::end_batch(Timestamp tsStart, Timestamp tsEnd) {
  sqlite3_bind_double(st_end_batch, 1, tsStart);
  sqlite3_bind_double(st_end_batch, 2, tsEnd);
  sqlite3_bind_int64(st_end_batch, 3, num_hits);
  sqlite3_bind_int(st_end_batch, 4, bid);
  step_commit(st_end_batch);
};

void
DB_Filer::begin_tx() {
  num_steps = 0;
  Check( sqlite3_exec(outdb, "begin", 0, 0, 0), "Failed to begin transaction.");
};

void
DB_Filer::end_tx() {
  if (num_steps > 0) {
    Check( sqlite3_exec(outdb, "commit", 0, 0, 0), "Failed to commit remaining inserts.");
    num_steps = 0;
  }
};

const char *
DB_Filer::q_load_ambig =
  "select ambigID, motusTagID1, motusTagID2, motusTagID3, motusTagID4, motusTagID5, motusTagID6 from tagAmbig order by ambigID desc;";
  //        0           1            2            3            4            5            6


void
DB_Filer::load_ambiguity() {
  // recreate the persistent tag ID ambiguity map from the database
  // For each record in tagAmbig, we create an ambiguity group

  // the next ID to be used if a new ambiguity group is created
  Ambiguity::setNextProxyID(next_proxyID);

  sqlite3_reset(st_load_ambig);
  for (;;) {
    if (SQLITE_DONE == sqlite3_step(st_load_ambig))
      break;
    // store this ambiguity
    Ambiguity::AmbigIDs ids;
    Motus_Tag_ID proxyID = sqlite3_column_int(st_load_ambig, 0);
    for (int i=1; i <= 6; ++i) {
      if (SQLITE_NULL == sqlite3_column_type(st_load_ambig, i))
        break;
      ids.insert(sqlite3_column_int(st_load_ambig, i));
    }
    Ambiguity::addIDs (proxyID, ids);
  }
};

const char *
DB_Filer::q_save_ambig =
  "insert or ignore into tagAmbig (ambigID, motusTagID1, motusTagID2, motusTagID3, motusTagID4, motusTagID5, motusTagID6) values (?, ?, ?, ?, ?, ?, ?);";
  //                       1           2            3            4            5            6            7

void
DB_Filer::save_ambiguity(Motus_Tag_ID proxyID, const Ambiguity::AmbigIDs & tags) {
  // proxyID must be a negative integer
  // add mid to its ambiguity group.
  if (proxyID >= 0)
    throw std::runtime_error("Called add_ambiguity with non-negative proxyID");
  sqlite3_reset(st_save_ambig);
  sqlite3_bind_int(st_save_ambig, 1, proxyID);
  auto t = tags.begin();
  for (int i = 1; i <= MAX_TAGS_PER_AMBIGUITY_GROUP; ++i) {
    if (t != tags.end())
      sqlite3_bind_int( st_save_ambig, 1 + i, (*t++));
    else
      sqlite3_bind_null(st_save_ambig, 1 + i);
  }
  step_commit(st_save_ambig);
};

const char *
DB_Filer::q_save_findtags_state =
  "insert or replace into batchState \
             (batchID, progName, monoBN, tsData, tsRun, state, version)\
      values (?,       ?,        ?,      ?,      ?,     gzcompress(?),     ? );";
  //          1        2         3       4       5                 6       7

void
DB_Filer::save_findtags_state(Timestamp tsData, Timestamp tsRun, std::string state, int version) {
  // drop any saved state for previous batch
  // FIXME: we need a reasonable way to decide when we can drop saved state from
  // boot session; i.e. when do we have all of its files?  This argues for a file counter...
  // The primary key on the batchState table will permit only one saved state per boot session,
  // and this will be the latest, given the use of "insert or replace" in q_save_findtags_state

  sqlite3_reset(st_save_findtags_state);
  sqlite3_bind_int(st_save_findtags_state,    1, bid);
  sqlite3_bind_int(st_save_findtags_state,    3, bootnum);
  sqlite3_bind_double(st_save_findtags_state, 4, tsData);
  sqlite3_bind_double(st_save_findtags_state, 5, tsRun);
  sqlite3_bind_blob(st_save_findtags_state,   6, state.c_str(), state.length(), SQLITE_STATIC);
  sqlite3_bind_int(st_save_findtags_state,    7, version);
  step_commit(st_save_findtags_state);
  end_tx(); // force a commit, because the bind_blob above is to a local variable
  begin_tx(); // open last transaction; see https://github.com/jbrzusto/find_tags/issues/64
};

// the query for fetching saved state compares only the major portion of the version
// number (the upper 16 bits)

const char *
DB_Filer::q_load_findtags_state = "select (select max(batchID) from batchState) as batchID, tsData, tsRun, gzuncompress(state), version from batchState where progName=? and monoBN=? and cast(version/65536 as integer)=cast(?/65536 as integer)";
//                                                                                 0        1       2      3                    4

bool
DB_Filer::load_findtags_state(long long monoBN, Timestamp & tsData, Timestamp & tsRun, std::string & state, int version, int &blob_version) {
  sqlite3_reset(st_load_findtags_state);
  sqlite3_bind_int64(st_load_findtags_state, 2, monoBN);
  sqlite3_bind_int(st_load_findtags_state, 3, version);
  if (SQLITE_DONE == sqlite3_step(st_load_findtags_state))
    return false; // no saved state, or at least not of the correct version
  bid = 1 + sqlite3_column_int   (st_load_findtags_state, 0);
  tsData = sqlite3_column_double (st_load_findtags_state, 1);
  tsRun = sqlite3_column_double  (st_load_findtags_state, 2);
  blob_version = sqlite3_column_int (st_load_findtags_state, 4);
  state = std::string(reinterpret_cast < const char * > (sqlite3_column_blob(st_load_findtags_state, 3)), sqlite3_column_bytes(st_load_findtags_state, 3));
  return true;
};

const char *
DB_Filer::q_get_blob = R"(select ts,
case compressed when 0 then readfile(filename) else gzreadfile(filename) end,
fileID from
(select
   (printf('%s/%s/%s%s',
           (select val from meta where key='fileRepo'),
           strftime('%Y-%m-%d', datetime(t1.ts, 'unixepoch')),
           t1.name,
           case isDone when 0 then '' else '.gz' end)
   ) as filename,
   t1.ts as ts,
   t1.isDone as compressed,
   t1.fileID as fileID
from
   files as t1
where
   t1.monoBN=? and t1.ts >= ?
order by ts)
)";

const char *
DB_Filer::q_add_batch_file = "insert or ignore into batchFiles values (?, ?)";
//                                                                     1  2

const char *
DB_Filer::q_load_extension = "select load_extension(?)";

void
DB_Filer::start_blob_reader(int monoBN) {


  Check( sqlite3_prepare_v2(outdb,
                            q_get_blob,
                            -1,
                            &st_get_blob,
                            0),
         "SQLite input database does not have valid 'files' or 'fileContents' table.");

  Check( sqlite3_prepare_v2(outdb,
                            q_add_batch_file,
                            -1,
                            &st_add_batch_file,
                            0),
         "SQLite input database does not have valid 'batchFiles' table.");

  sqlite3_bind_int(st_get_blob, 1, monoBN);

  // initially, assume we're starting at the first file in that boot session, by specifying fileTS=0.
  // this might be changed by the resume() code.
  sqlite3_bind_int(st_get_blob, 2, 0);
};

void
DB_Filer::seek_blob (Timestamp tsseek) {
  sqlite3_bind_int(st_get_blob, 2, tsseek);
};

bool
DB_Filer::get_blob (const char **bufout, int * lenout, Timestamp *ts) {
  int res = sqlite3_step(st_get_blob);
  if (res == SQLITE_DONE)
    return false; // indicate we're done

  if (res != SQLITE_ROW)
    throw std::runtime_error("Problem getting next blob.");

  // if row's second field was null (e.g. if decompression failed),
  // the following will set lenout = 0 and bufout = (void*) 0
  // The caller should then try read the next blob.

  * lenout = sqlite3_column_bytes(st_get_blob, 1);
  * bufout = reinterpret_cast < const char * > (sqlite3_column_blob(st_get_blob, 1));
  * ts = sqlite3_column_double(st_get_blob, 0);

  // record which file we're reading
  sqlite3_bind_int(st_add_batch_file, 1, bid);
  sqlite3_bind_int(st_add_batch_file, 2, sqlite3_column_int(st_get_blob, 2));
  step_commit(st_add_batch_file);

  return true;
};

void
DB_Filer::rewind_blob_reader(Timestamp origin) {
  sqlite3_reset (st_get_blob);
  seek_blob(origin);
};

void
DB_Filer::end_blob_reader () {
  sqlite3_finalize (st_get_blob);
};

const char *
DB_Filer::q_get_DTAtags = "select ts, id, ant, sig, antFreq, gain, 0+substr(codeSet, 6, 1), lat, lon "
  //                               0   1   2    3      4        5        6                    7    8
  "from DTAtags where ts between (select ts from DTAboot where relboot=?) and ifnull((select ts from DTAboot where relboot=?),1e20) order by ts"
  //                                                                   1                                                   2
  ;

void
DB_Filer::start_DTAtags_reader(Timestamp ts, int bootnum) {
  Check(sqlite3_prepare_v2(outdb, q_get_DTAtags, -1, &st_get_DTAtags, 0),
        "output DB does not have valid 'DTAtags' table.");
  sqlite3_bind_int(st_get_DTAtags, 1, bootnum);
  sqlite3_bind_int(st_get_DTAtags, 2, bootnum + 1);
};

bool
DB_Filer::get_DTAtags_record(DTA_Record &dta) {
  if (! st_get_DTAtags)
    throw std::runtime_error("Attempt to use uninitialized st_get_DTAtags in get_DTAtags_record");

  // loop until we have a record with a valid antenna

  const char * aname=0;
  while (! aname) {
    int rv = sqlite3_step(st_get_DTAtags);
    if (rv == SQLITE_DONE)
      return false; // indicate we're done

    if (rv != SQLITE_ROW)
      throw std::runtime_error("Problem getting next DTAtags record.");

    aname = reinterpret_cast<const char *> (sqlite3_column_text(st_get_DTAtags, 2));
  };

  strncpy(dta.antName, aname, MAX_ANT_NAME_CHARS);
  dta.ts      = sqlite3_column_double (st_get_DTAtags, 0);
  dta.id      = sqlite3_column_int    (st_get_DTAtags, 1);
  dta.antName[sqlite3_column_bytes(st_get_DTAtags, 2)] = 0;
  dta.sig     = sqlite3_column_int    (st_get_DTAtags, 3);
  dta.freq    = sqlite3_column_double (st_get_DTAtags, 4);
  dta.gain    = sqlite3_column_int    (st_get_DTAtags, 5);
  dta.codeSet = sqlite3_column_int    (st_get_DTAtags, 6);
  if (sqlite3_column_type (st_get_DTAtags, 7) == SQLITE_NULL) {
    dta.lat = dta.lon = nan("0");
  } else {
    dta.lat     = sqlite3_column_double (st_get_DTAtags, 7);
    dta.lon     = sqlite3_column_double (st_get_DTAtags, 8);
  }
  return true;
};

void
DB_Filer::end_DTAtags_reader() {
  if (st_get_DTAtags)
    sqlite3_finalize(st_get_DTAtags);
  st_get_DTAtags = 0;
};

void
DB_Filer::rewind_DTAtags_reader() {
  end_DTAtags_reader();
  start_DTAtags_reader(0, bootnum);
};

const char *
DB_Filer::q_add_pulse =
"insert into pulses (batchID, ts, ant, antFreq, dfreq, sig, noise) \
           values   (?,       ?,  ?,   ?,       ?,     ?,   ?)";
//                   1        2   3    4        5      6    7

void
DB_Filer::add_pulse(int ant, Pulse &p) {
  sqlite3_bind_int   (st_add_pulse, 1, bid);
  sqlite3_bind_double(st_add_pulse, 2, p.ts);
  sqlite3_bind_int   (st_add_pulse, 3, ant);
  sqlite3_bind_double(st_add_pulse, 4, p.ant_freq);
  sqlite3_bind_double(st_add_pulse, 5, p.dfreq);
  sqlite3_bind_double(st_add_pulse, 6, p.sig);
  sqlite3_bind_double(st_add_pulse, 7, p.noise);
  step_commit(st_add_pulse);
};

const char *
DB_Filer::q_add_recv_param =
"insert into params (batchID, ts, ant, param, val, error, errinfo) \
           values   (?,       ?,  ?,   ?,     ?,   ?,   ?)";
//                   1        2   3    4      5    6    7

void
DB_Filer::add_recv_param(Timestamp ts, int ant, char *param, double val, int error, char *extra) {
  sqlite3_bind_int   (st_add_recv_param, 1, bid);
  sqlite3_bind_double(st_add_recv_param, 2, ts);
  sqlite3_bind_int   (st_add_recv_param, 3, ant);
  sqlite3_bind_text  (st_add_recv_param, 4, param, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(st_add_recv_param, 5, val);
  sqlite3_bind_int   (st_add_recv_param, 6, error);
  sqlite3_bind_text  (st_add_recv_param, 7, extra, -1, SQLITE_TRANSIENT);
  step_commit(st_add_recv_param);
};
