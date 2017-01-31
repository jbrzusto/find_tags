#ifndef DB_FILER_HPP
#define DB_FILER_HPP

#include "find_tags_common.hpp"

#include <sqlite3.h>
#include "Ambiguity.hpp"
#include "Tag_Database.hpp"

/*
  DB_Filer - manage output of detection data.
*/

class DB_Filer {

public:
  static const int MAX_ANT_NAME_CHARS = 11; //!< maximum number of chars in an antenna name; currently 11, for "A1+A2+A3+A4"

  typedef int Run_ID;
  typedef int Batch_ID;

  typedef struct _DTA_record {
    Timestamp ts;
    short id;
    char antName[MAX_ANT_NAME_CHARS + 1];
    short ant;
    short sig;
    Frequency_MHz freq;
    short gain;
    short codeSet;
    double lat;
    double lon;
  } DTA_Record;

  static const int MAX_TAGS_PER_AMBIGUITY_GROUP = 6;

  DB_Filer (const string &out, const string &prog_name, const string &prog_version, double prog_ts, int bootnum=1, double minGPSdt = 300, bool dump_line_numbers = false); // initialize a filer on an existing sqlite database file
  ~DB_Filer (); // write summary data

  Run_ID begin_run(Motus_Tag_ID mid, int ant ); // begin run of tag
  void end_run(Run_ID rid, int n, bool countOnly = false); // end run, noting number of hits; if countOnly is true, don't end run

  Seq_No add_hit(Run_ID rid, double ts, float sig, float sigSD, float noise, float freq, float freqSD, float slop, float burstSlop); // if dump_line_numbers is true, return the rowID of the added hit

  void add_hit_line(Seq_No hid, Seq_No line_no); // record a file line for a detection pulse

  void add_param(const string &name, double val); // record a program parameter value

  void add_GPS_fix(double ts, double lat, double lon, double alt); // record a GPS fix

  void add_time_fix(Timestamp tsLow, Timestamp tsHigh, Timestamp by, Timestamp error, char fixType); // record a time fix (i.e. correction)

  void add_pulse_count(double hourBin, int ant, int count); // record a count of pulses on from an antenna during an hour

  void begin_batch(int bootnum); // start new batch; uses 1 + ID of latest ended batch

  void end_batch(Timestamp tsBegin, Timestamp tsEnd); //!< end current batch

  void save_ambiguity(Motus_Tag_ID proxyID, const Ambiguity::AmbigTags & tags); // save one ambiguity group

  void load_ambiguity(Tag_Database & tdb); // restore all ambiguity groups

  void save_findtags_state(Timestamp tsData, Timestamp tsRun, std::string state);

  bool load_findtags_state(long long monoBN, Timestamp & tsData, Timestamp & tsRun, std::string & state);

  void start_blob_reader(int monoBN); //!< initialize reading of filecontents blobs for a given boot number

  void seek_blob (Timestamp tsseek); //!< skip to the first blob whose file timestamp >= ts.  This is used for resuming.

  bool get_blob (const char **bufout, int * lenout, Timestamp *ts); //!< get the next available blob; return true on success, false if none; set caller's pointer and length

  void rewind_blob_reader(Timestamp origin); //!< reset blob reader to start of stream; might be beginning of boot session, or part way into it

  void end_blob_reader(); //!< finalize blob reader

  void start_DTAtags_reader(Timestamp ts = 0); //!< initialize reading of DTAtags lines, starting at the specified timestamp

  bool get_DTAtags_record(DTA_Record &dta ); //!< get the next DTAtags record; return true on success, false if none left; set items in &dat.

  void end_DTAtags_reader(); //!< finalize DTAtags reader

  void rewind_DTAtags_reader(); //!< rewind DTAtags reader

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
  sqlite3_stmt * st_add_hit_line; //!< add a line to the hitLines table
  sqlite3_stmt * st_add_prog; //!< add batch program entry
  sqlite3_stmt * st_add_GPS_fix; //!< add a GPS fix
  sqlite3_stmt * st_add_time_fix; //!< add a time jump
  sqlite3_stmt * st_add_pulse_count; //!< add a count of pulses from an antenna during an hour bin
  sqlite3_stmt * st_check_param; //!< check whether parameter value has changed
  sqlite3_stmt * st_add_param; //!< add batch parameter entry
  sqlite3_stmt * st_load_ambig; //!< get ambiguity groups from database
  sqlite3_stmt * st_save_ambig; //!< save an ambiguity groups to database
  sqlite3_stmt * st_save_findtags_state; //!< save state of running findtags, for pause
  sqlite3_stmt * st_load_findtags_state; //!< load state of paused findtags, for resume
  sqlite3_stmt * st_get_blob; //!< grab and decompress file contents
  sqlite3_stmt * st_get_DTAtags; //!< grab DTA tag records

  string prog_name; //!< name of program, for recording in DB

  Batch_ID bid; //!< ID of batch
  Run_ID rid; //!< next ID to be used for a run
  Motus_Tag_ID next_proxyID; //!< next proxyID to be used for a tag ambiguity group; negative

  int num_runs; //!< accumulator: number of runs in this batch
  long long int num_hits; //!< accumulator: number of hits in this batch (all runs)

  static char qbuf[256]; //!< query buffer re-used in various places

  static const int steps_per_tx = 50000; //!< number of statement steps per transaction (typically inserts)
  int num_steps; //!< counter for steps since last BEGIN statement

  int bootnum; //!< boot number for current batch

  double minGPSdt; //!< minimum time step for GPS fixes

  double lastGPSts; //!< most recent GPS timestamp

  bool dump_line_numbers; //!< should line numbers of detection pulses be dumped?

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
  static const char * q_add_hit_line;
  static const char * q_add_GPS_fix;
  static const char * q_add_time_fix;
  static const char * q_add_pulse_count;
  static const char * q_load_ambig;
  static const char * q_save_ambig;
  static const char * q_load_findtags_state;
  static const char * q_save_findtags_state;
  static const char * q_get_blob;
  static const char * q_get_DTAtags;

};


#endif // DB_FILER
