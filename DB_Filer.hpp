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

  DB_Filer (const string &out, const string &prog_name, const string &prog_version, double prog_ts); // initialize a filer on an existing sqlite database file
  ~DB_Filer (); // write summary data

  Run_ID begin_run(Motus_Tag_ID mid); // begin run of tag
  void end_run(Run_ID rid, int n); // end run, noting number of hits

  void add_hit(Run_ID rid, char ant, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop);

  void add_param(const string &name, double val); // record a program parameter value

protected:
  // settings

  sqlite3 * outdb; //<! handle to sqlite connection

  // sqlite3 pre-compiled stateme nts
  sqlite3_stmt * st_begin_run; //!< start a run
  sqlite3_stmt * st_end_run; //!< end a run
  sqlite3_stmt * st_add_hit; //!< add a hit to a run
  sqlite3_stmt * st_add_prog; //!< add batch program entry
  sqlite3_stmt * st_add_param; //!< add batch parameter entry

  string prog_name; //!< name of program, for recording in DB

  Batch_ID bid; //!< ID of batch
  Run_ID rid; //!< next ID to be used for a run

  int num_runs; //!< accumulator: number of runs in this batch
  long long int num_hits; //!< accumulator: number of hits in this batch (all runs)

  static char qbuf[256]; //!< query buffer re-used in various places

  static const int steps_per_tx = 1000; //!< number of steps per transaction.
  int num_steps; //!< counter for steps since last BEGIN statement

  void step_commit(sqlite3_stmt *st); //!< step statement, and if number of steps has reached steps_per_tx, commit and start new tx
};


#endif // DB_FILER
