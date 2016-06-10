#ifndef SG_SQLITE_DATA_SOURCE_HPP
#define SG_SQLITE_DATA_SOURCE_HPP

//!< Source for SG-format input data from a motus-format .sqlite database

#include <sqlite3.h>
#include "find_tags_common.hpp"
#include "Data_Source.hpp"

class SG_SQLite_Data_Source : public Data_Source { 

public:
  SG_SQLite_Data_Source(std::string source, unsigned int monoBN);
  ~SG_SQLite_Data_Source();
  bool getline(char * buf, int maxLen);

protected:
  sqlite3 * db;
  unsigned int monoBN;
  int bytesLeft; //!< bytes left to use in blob buffer
  const char * blob; //!< pointer to next char to use in blob buffer

  static const char * q_get_data;
  sqlite3_stmt * st_get_data; //!< grab and decompress file contents

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
  
};

#endif // SG_SQLITE_DATA_SOURCE
