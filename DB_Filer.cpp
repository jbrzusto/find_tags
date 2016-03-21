#include "DB_Filer.hpp"
#include <stdio.h>
#include <time.h>

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
               "pragma journal_mode=wal; pragma cache_size=200000;",
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

  Check(sqlite3_prepare_v2(outdb, q_add_hit, -1, &st_add_hit, 0),
        "output DB does not have valid 'hits' table.");

  if (minGPSdt >= 0) {
    Check(sqlite3_prepare_v2(outdb, q_add_GPS_fix,-1, &st_add_GPS_fix, 0),
          "output DB does not have valid 'gps' table.");
  } else {
    lastGPSts = +1.0 / 0.0; // ensures new timestamp is never late enough to record a GPS fix
  }

  sqlite3_stmt * st_check_batchprog;
  msg = "output DB does not have valid 'batchProgs' table.";

  Check(sqlite3_prepare_v2(outdb,
                           "select progVersion from batchProgs where progName=? order by batchID desc limit 1",
                           -1, &st_check_batchprog, 0),
        SQLITE_OK,
        SQLITE_ROW,
        msg);
  sqlite3_bind_text(st_check_batchprog, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  int rv = Check(sqlite3_step(st_check_batchprog), SQLITE_DONE, SQLITE_ROW, msg);

  if (rv == SQLITE_DONE || 
      (rv == SQLITE_ROW && string((const char *) sqlite3_column_text(st_check_batchprog, 0)) != prog_version)) {
    // program version has changed since latest batchID for which it was recorded,
    // so record new value
    sprintf(qbuf, 
          "insert into batchProgs (batchID, progName, progVersion, progBuildTS, tsMotus) \
                           values (%d,      '%s',     '%s',        %f,          0)",
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

  msg = "output DB does not have valid 'batchParams' table.";
  Check( sqlite3_prepare_v2(outdb, 
                            "select paramVal from batchParams where progName = ? and paramName = ? order by batchID desc limit 1",
                            -1, &st_check_param, 0),
         msg);
  sqlite3_bind_text(st_check_param, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  Check( sqlite3_prepare_v2(outdb, 
                            "insert into batchParams (batchID, progName, paramName, paramVal, tsMotus) \
                                           values (?,       ?,        ?,         ?,          0)",
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

  Check( sqlite3_prepare_v2(outdb,
                            "insert into batchAmbig (ambigID, batchID, motusTagID) values (?, ?, ?);",
                            -1,
                            & st_add_ambig,
                            0),
         "SQLite output database does not have valid 'batchAmbig' table");

  const char * ftsm = "SQLite output database does not have valid 'batchState' table";

  Check ( sqlite3_prepare_v2(outdb, q_save_findtags_state, -1, & st_save_findtags_state, 0), ftsm);
  sqlite3_bind_text(st_save_findtags_state, 2, prog_name.c_str(), -1, SQLITE_TRANSIENT);
  
  Check ( sqlite3_prepare_v2(outdb, q_load_findtags_state, -1, & st_load_findtags_state, 0), ftsm);
  sqlite3_bind_text(st_load_findtags_state, 1, prog_name.c_str(), -1, SQLITE_TRANSIENT);

  begin_tx();
  begin_batch(bootnum);

};


DB_Filer::~DB_Filer() {

  end_batch();
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
  if (minGPSdt >= 0)
    sqlite3_finalize(st_add_GPS_fix);
  sqlite3_finalize(st_add_hit);
  sqlite3_close(outdb);
  outdb = 0;
};

const char *
DB_Filer::q_begin_run = 
 "insert into runs (runID, batchIDbegin, batchIDend, motusTagID, tsMotus) \
            values (?,     ?,            -1,         ?,          0)";

DB_Filer::Run_ID
DB_Filer::begin_run(Motus_Tag_ID mid) {
  sqlite3_bind_int(st_begin_run, 1, rid); // bind run ID
  sqlite3_bind_int(st_begin_run, 3, mid); // bind tag ID
  step_commit(st_begin_run);
  return rid++;
};

const char *
DB_Filer::q_end_run = "update runs set len=?, batchIDend=? where runID=?";

void
DB_Filer::end_run(Run_ID rid, int n, bool countOnly) {
  sqlite3_bind_int(st_end_run, 1, n); // bind number of hits in run
  sqlite3_bind_int(st_end_run, 2, countOnly ? -1 : bid); // bind ID of batch this run ends in
  sqlite3_bind_int(st_end_run, 3, rid);  // bind run number
  step_commit(st_end_run);
};

const char *
DB_Filer::q_add_hit = 
"insert into hits (batchID, runID, ant, ts, sig, sigSD, noise, freq, freqSD, slop, burstSlop) \
         values   (?,       ?,     ?,   ?,  ?,   ?,     ?,     ?,    ?,      ?,    ?)";
//               1       2     3    4  5    6     7     8    9      10    11

void
DB_Filer::add_hit(Run_ID rid, char ant, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop) {
  sqlite3_bind_int   (st_add_hit, 1, bid);
  sqlite3_bind_int   (st_add_hit, 2, rid);
  sqlite3_bind_int   (st_add_hit, 3, ant-'0');
  sqlite3_bind_double(st_add_hit, 4, ts);
  sqlite3_bind_double(st_add_hit, 5, sig);
  sqlite3_bind_double(st_add_hit, 6, sigSD);
  sqlite3_bind_double(st_add_hit, 7, noise);
  sqlite3_bind_double(st_add_hit, 8, freq);
  sqlite3_bind_double(st_add_hit, 9, freqSD);
  sqlite3_bind_double(st_add_hit, 10, slop);
  sqlite3_bind_double(st_add_hit, 11, burstSlop);
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

void
DB_Filer::step_commit(sqlite3_stmt * st) {
  Check(sqlite3_step(st), SQLITE_DONE, "unable to step statement");
  sqlite3_reset(st);
  if (++num_steps == steps_per_tx) {
    end_tx();
    begin_tx();
    num_steps = 0;
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
DB_Filer::q_drop_saved_state = "delete from batchState where batchID=?";

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

  // set batch ID for "insert run" query
  sqlite3_bind_int(st_begin_run, 2, bid);
};

const char *
DB_Filer::q_end_batch = "update batches set numHits=? where batchID=?";

void
DB_Filer::end_batch() {
  sqlite3_bind_int64(st_end_batch, 1, num_hits);
  sqlite3_bind_int(st_end_batch, 2, bid);
  step_commit(st_end_batch);
};

void
DB_Filer::begin_tx() {
  Check( sqlite3_exec(outdb, "begin", 0, 0, 0), "Failed to commit remaining inserts.");
};

void
DB_Filer::end_tx() {
  Check( sqlite3_exec(outdb, "commit", 0, 0, 0), "Failed to commit remaining inserts.");
};

void
DB_Filer::add_ambiguity(Motus_Tag_ID proxyID, Motus_Tag_ID mid) {
  // proxyID must be a negative integer
  // add mid to its ambiguity group.  
  if (proxyID >= 0)
    throw std::runtime_error("Called add_ambiguity with non-negative proxyID");
  sqlite3_reset(st_add_ambig);
  sqlite3_bind_int(st_add_ambig, 1, proxyID);
  sqlite3_bind_int(st_add_ambig, 2, bid);
  sqlite3_bind_int(st_add_ambig, 3, mid);
  step_commit(st_add_ambig);
};

const char *
DB_Filer::q_save_findtags_state = 
  "insert into batchState (batchID, progName, monoBN, tsData, tsRun, state, lastfileID, lastCharIndex)\
                   values (?,       ?,        ?,      ?,      ?,     ?,     -1,         -1);";
  //                     1       2        3      4      5      6     7          8

void
DB_Filer::save_findtags_state(Timestamp tsData, Timestamp tsRun, std::string state) {
  // drop any saved state for previous batch
  sqlite3_bind_int(st_drop_saved_state, 1, bid - 1);
  step_commit(st_drop_saved_state);

  sqlite3_reset(st_save_findtags_state);
  sqlite3_bind_int(st_save_findtags_state,    1, bid);
  sqlite3_bind_int(st_save_findtags_state,    3, bootnum);
  sqlite3_bind_double(st_save_findtags_state, 4, tsData);
  sqlite3_bind_double(st_save_findtags_state, 5, tsRun);
  sqlite3_bind_blob(st_save_findtags_state,   6, state.c_str(), state.length(), SQLITE_TRANSIENT);
  step_commit(st_save_findtags_state);
};

const char *
DB_Filer::q_load_findtags_state = "select batchID, monoBN, tsData, tsRun, state from batchState where progName=? order by tsRun desc limit 1;";
//                                    0       1      2      3     4 
          
bool
DB_Filer::load_findtags_state(Timestamp & tsData, Timestamp & tsRun, std::string & state) {
  sqlite3_reset(st_load_findtags_state);
  if (SQLITE_DONE == sqlite3_step(st_load_findtags_state))
    return false; // no saved state
  bid = 1 + sqlite3_column_int   (st_load_findtags_state, 0);
  bootnum = sqlite3_column_int   (st_load_findtags_state, 1);
  tsData = sqlite3_column_double (st_load_findtags_state, 2);
  tsRun = sqlite3_column_double  (st_load_findtags_state, 3);
  state = std::string(reinterpret_cast < const char * > (sqlite3_column_blob(st_load_findtags_state, 4)), sqlite3_column_bytes(st_load_findtags_state, 4));
  return true;
};
