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
  void rewind();

protected:
  DB_Filer * db;
  int bytesLeft; //!< bytes left to use in blob buffer
  const char * blob; //!< pointer to next char to use in blob buffer
  int offset; //!< offset from blob of next byte to use
  Timestamp blobTS; //!< timestamp of start of current blob; used in resume().
  Timestamp originTS; //!< timestamp of start of blob to which we rewind, after resume()
  int originOffset; //!< offset from first blob to which we rewind, after resume()
  int originBytesLeft; //!< bytes left in blob after rewind, after resume()

  void serialize(boost::archive::binary_iarchive & ar, const unsigned int version);
  void serialize(boost::archive::binary_oarchive & ar, const unsigned int version);

};

#endif // SG_SQLITE_DATA_SOURCE
