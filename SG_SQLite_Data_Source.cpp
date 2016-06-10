#include "SG_SQLite_Data_Source.hpp"

SG_SQLite_Data_Source::SG_SQLite_Data_Source(std::string source, unsigned int monoBN) : 
  monoBN(monoBN),
  bytesLeft(0)
{

  Check(sqlite3_open_v2(source.c_str(),
                        & db,
                        SQLITE_OPEN_READONLY,
                        0),
        "Input database file does not exist.");

  sqlite3_enable_load_extension(db, 1);

  Check(sqlite3_exec(db,
                     "select load_extension('/SG/code/Sqlite_Compression_Extension.so')",
                     0,
                     0,
                     0),
        "Unable to load required library Sqlite_Compression_Extension.so from /SG/code");

  Check( sqlite3_prepare_v2(db,
                            q_get_data,
                            -1,
                            &st_get_data,
                            0),
         "SQLite input database does not have valid 'files' or 'fileContents' table.");

  sqlite3_bind_int(st_get_data, 1, monoBN);

};

bool
SG_SQLite_Data_Source::getline(char * buf, int maxLen) {

  // bytesLeft will be -1 if we read an unterminated line on previous call

  while (bytesLeft <= 0) { 
    // Ensure we have some blob data to read.
    // It is guaranteed that lines are not split across blobs.
    // repeat until a non-empty blob is found, or none remain

      int res = sqlite3_step(st_get_data);
      if (res == SQLITE_DONE)
        return false; // indicate we're done

      if (res != SQLITE_ROW)
        throw std::runtime_error("Problem getting next blob.");

      bytesLeft = sqlite3_column_bytes(st_get_data, 0);
      blob = reinterpret_cast < const char * > (sqlite3_column_blob(st_get_data, 0));
    }
    const char * eol = reinterpret_cast < const char * > (memchr(blob, '\n', bytesLeft));
    // if no eol found, line goes to end of blob
    int lineLen = eol ? eol - blob : bytesLeft;
      
    int useLineLen = std::min(lineLen, maxLen);
    memcpy(buf, blob, useLineLen); // NB: we know these regions don't overlap; else we'd use memmove
    buf[useLineLen] = '\0';        // terminating 0
    bytesLeft -= lineLen + 1;      // NB: include the '\n' char in the calculation; will get -1 for unterminated line
    blob += lineLen + 1;
    return true;
};


int
SG_SQLite_Data_Source::Check(int code, int wants, int wants2, int wants3, const std::string & err) {
  if (code == wants
      || (wants2 != -1 && code == wants2)
      || (wants3 != -1 && code == wants3))
    return code;
  throw std::runtime_error(err + "\nSqlite error: " + sqlite3_errmsg(db));
};

SG_SQLite_Data_Source::~SG_SQLite_Data_Source() {
  sqlite3_finalize(st_get_data);
  sqlite3_close(db);
  db = 0;
};

const char *
SG_SQLite_Data_Source::q_get_data = "select bz2uncompress(t2.contents, t1.size) from files as t1 left join fileContents as t2 on t1.fileID=t2.fileID where t1.monoBN=? order by ts";
