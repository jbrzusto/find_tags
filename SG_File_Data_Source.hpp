#ifndef SG_FILE_DATA_SOURCE_HPP
#define SG_FILE_DATA_SOURCE_HPP

//!< Source for SG-format input data from a file/stream

#include "find_tags_common.hpp"
#include "Data_Source.hpp"

class SG_File_Data_Source : public Data_Source { 

public:
  SG_File_Data_Source(std::istream * data);
  bool getline(char * buf, int maxLen);
  
protected:
  std::istream *data;
};

#endif // SG_FILE_DATA_SOURCE
