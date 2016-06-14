#ifndef SG_SQLITE_DATA_SOURCE_HPP
#define SG_SQLITE_DATA_SOURCE_HPP

//!< Source for SG-format input data from a motus-format .sqlite database

#include <sqlite3.h>
#include "find_tags_common.hpp"
#include "Data_Source.hpp"
#include "DB_Filer.hpp"

class SG_SQLite_Data_Source : public Data_Source { 

public:
  SG_SQLite_Data_Source(DB_Filer * db, unsigned int monoBN);
  ~SG_SQLite_Data_Source();
  bool getline(char * buf, int maxLen);

protected:
  DB_Filer * db;
  int bytesLeft; //!< bytes left to use in blob buffer
  const char * blob; //!< pointer to next char to use in blob buffer
};

#endif // SG_SQLITE_DATA_SOURCE
