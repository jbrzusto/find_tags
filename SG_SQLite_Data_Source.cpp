#include "SG_SQLite_Data_Source.hpp"

SG_SQLite_Data_Source::SG_SQLite_Data_Source(DB_Filer * db, unsigned int monoBN) :
  db(db),
  bytesLeft(0)
{
  db->start_blob_reader(monoBN);
};

bool
SG_SQLite_Data_Source::getline(char * buf, int maxLen) {

  // bytesLeft will be -1 if we read an unterminated line on previous call

  while (bytesLeft <= 0) { 
    // Ensure we have some blob data to read.
    // It is guaranteed that lines are not split across blobs.
    // repeat until a non-empty blob is found, or none remain

    if (! db->get_blob(& blob, & bytesLeft))
      return false;
  }
  const char * eol = reinterpret_cast < const char * > (memchr(blob, '\n', bytesLeft));

  // if no eol found, line goes to end of blob
  int lineLen = eol ? eol - blob : bytesLeft;

  // truncate to max permitted line size
  int useLineLen = std::min(lineLen, maxLen);

  // copy to output
  memcpy(buf, blob, useLineLen); // NB: we know these regions don't overlap; else we'd use memmove
  buf[useLineLen] = '\0';        // terminating 0
  bytesLeft -= lineLen + 1;      // NB: include the '\n' char in the calculation; will get -1 for unterminated line
  blob += lineLen + 1;
  return true;
};

SG_SQLite_Data_Source::~SG_SQLite_Data_Source() {
  db->end_blob_reader();
};
