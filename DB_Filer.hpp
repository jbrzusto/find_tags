#ifndef DB_FILER_HPP
#define DB_FILER_HPP

#include "find_tags_common.hpp"

#include <sqlite3.h>


/*
  DB_Filer - manage output of detection data.
*/

class DB_Filer {

public:
  typedef int Run_ID;
  typedef int Batch_ID;

  DB_Filer (const string &out, const string &prog_name, const string &prog_version, double prog_ts, int bootnum=1, double minGPSdt = 300); // initialize a filer on an existing sqlite database file
  ~DB_Filer (); // write summary data

  Run_ID begin_run(Motus_Tag_ID mid, int ant ); // begin run of tag
  void end_run(Run_ID rid, int n, bool countOnly = false); // end run, noting number of hits; if countOnly is true, don't end run

  void add_hit(Run_ID rid, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop);

  void add_param(const string &name, double val); // record a program parameter value

  void add_GPS_fix(double ts, double lat, double lon, double alt); // record a GPS fix

  void add_time_jump(double tsBefore, double tsAfter, char jumpType); // record a time jump

  void add_pulse_count(double hourBin, int ant, int count); // record a count of pulses on from an antenna during an hour

  void begin_batch(int bootnum); // start new batch; uses 1 + ID of latest ended batch

  void end_batch(Timestamp tsBegin, Timestamp tsEnd); //!< end current batch

  void add_ambiguity(Motus_Tag_ID proxyID, Motus_Tag_ID mid); // add a motus tag ID to a proxy group, given by proxyID, which is negative

  void save_findtags_state(Timestamp tsData, Timestamp tsRun, std::string state);

  bool load_findtags_state(Timestamp & tsData, Timestamp & tsRun, std::string & state);

  void start_blob_reader(int monoBN); //!< initialize reading of filecontents blobs for a given boot number

  bool get_blob (const char **bufout, int * lenout); //!< get the next available blob; return true on success, false if none; set caller's pointer and length 

  void end_blob_reader(); //!< finalize blob reader

protected:
  // settings

  sqlite3 * outdb; //<! handle to sqlite connection

  // sqlite3 pre-compiled statements
  sqlite3_stmt * st_begin_batch; //!< create a batch record
  sqlite3_stmt * st_drop_saved_state; //!< drop saved state for previous batch
  sqlite3_stmt * st_end_batch; //!< update a batch record, when finished
  sqlite3_stmt * st_begin_run; //!< start a run
  sqlite3_stmt * st_end_run; //!< end a run
  sqlite3_stmt * st_add_hit; //!< add a hit to a run
  sqlite3_stmt * st_add_prog; //!< add batch program entry
  sqlite3_stmt * st_add_GPS_fix; //!< add a GPS fix
  sqlite3_stmt * st_add_time_jump; //!< add a time jump
  sqlite3_stmt * st_add_pulse_count; //!< add a count of pulses from an antenna during an hour bin
  sqlite3_stmt * st_check_param; //!< check whether parameter value has changed
  sqlite3_stmt * st_add_param; //!< add batch parameter entry
  sqlite3_stmt * st_add_ambig; //!< add ambiguity entry
  sqlite3_stmt * st_save_findtags_state; //!< save state of running findtags, for pause
  sqlite3_stmt * st_load_findtags_state; //!< load state of paused findtags, for resume
  sqlite3_stmt * st_get_blob; //!< grab and decompress file contents

  string prog_name; //!< name of program, for recording in DB

  Batch_ID bid; //!< ID of batch
  Run_ID rid; //!< next ID to be used for a run

  int num_runs; //!< accumulator: number of runs in this batch
  long long int num_hits; //!< accumulator: number of hits in this batch (all runs)

  static char qbuf[256]; //!< query buffer re-used in various places

  static const int steps_per_tx = 50000; //!< number of statement steps per transaction (typically inserts)
  int num_steps; //!< counter for steps since last BEGIN statement

  int bootnum; //!< boot number for current batch

  double minGPSdt; //!< minimum time step for GPS fixes

  double lastGPSts; //!< most recent GPS timestamp

  void step_commit(sqlite3_stmt *st); //!< step statement, and if number of steps has reached steps_per_tx, commit and start new tx

  int Check(int code, int wants, int wants2, int wants3, const std::string & err); //!< check that sqlite3 result is one of specified values, otherwise throuw runtime error with given text; -1 is not a valid SQLITE return code
  int Check(int code, int wants, int wants2, const std::string & err) {
    return Check(code, wants, wants2, -1, err);
  };

  int Check(int code, int wants, const std::string & err) {
    return Check(code, wants, -1, -1, err);
  };

  int Check(int code, const std::string & err) {
    return Check(code, SQLITE_OK, -1, -1, err);
  };

  void begin_tx(); //!< begin transaction, which is just a bunch of insert queries
  void end_tx(); //!< end transaction

  static const char * q_begin_batch;
  static const char * q_drop_saved_state;
  static const char * q_end_batch;
  static const char * q_begin_run;
  static const char * q_end_run;
  static const char * q_add_hit;
  static const char * q_add_GPS_fix;
  static const char * q_add_time_jump;
  static const char * q_add_pulse_count;
  static const char * q_load_findtags_state;
  static const char * q_save_findtags_state;
  static const char * q_get_blob;

};


#endif // DB_FILER
