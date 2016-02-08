#ifndef DB_FILER_HPP
#define DB_FILER_HPP

#include "find_tags_common.hpp"

#include <sqlite3.h>


/*
  DB_Filer - manage output of detection data.
*/

class DB_Filer {

public:
  typedef long long int Run_ID;
  typedef long long int Batch_ID;

  DB_Filer (const string &out, const string &prog_name, const string &prog_version, double prog_ts, long long bootnum=1); // initialize a filer on an existing sqlite database file
  ~DB_Filer (); // write summary data

  Run_ID begin_run(Motus_Tag_ID mid); // begin run of tag
  void end_run(Run_ID rid, int n); // end run, noting number of hits

  void add_hit(Run_ID rid, char ant, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop);

  void add_param(const string &name, double val); // record a program parameter value

  void begin_batch(long long bootnum); // start new batch; uses 1 + ID of latest ended batch

  void end_batch(); //!< end current batch

  void add_ambiguity(Motus_Tag_ID mid1, Motus_Tag_ID mid2); // indicate that two tags are indistinguishable

protected:
  // settings

  sqlite3 * outdb; //<! handle to sqlite connection

  // sqlite3 pre-compiled statements
  sqlite3_stmt * st_begin_batch; //!< create a batch record
  sqlite3_stmt * st_end_batch; //!< update a batch record, when finished
  sqlite3_stmt * st_begin_run; //!< start a run
  sqlite3_stmt * st_end_run; //!< end a run
  sqlite3_stmt * st_add_hit; //!< add a hit to a run
  sqlite3_stmt * st_add_prog; //!< add batch program entry
  sqlite3_stmt * st_check_param; //!< check whether parameter value has changed
  sqlite3_stmt * st_add_param; //!< add batch parameter entry
  sqlite3_stmt * st_find_ambig; //!< add ambiguity entry
  sqlite3_stmt * st_new_ambig; //!< add ambiguity entry
  sqlite3_stmt * st_add_ambig; //!< add ambiguity entry

  string prog_name; //!< name of program, for recording in DB

  Batch_ID bid; //!< ID of batch
  Run_ID rid; //!< next ID to be used for a run

  int num_runs; //!< accumulator: number of runs in this batch
  long long int num_hits; //!< accumulator: number of hits in this batch (all runs)

  static char qbuf[256]; //!< query buffer re-used in various places

  static const int steps_per_tx = 5000; //!< number of statement steps per transaction (typically inserts)
  int num_steps; //!< counter for steps since last BEGIN statement

  long long bootnum; //!< boot number for current batch

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
};


#endif // DB_FILER
