#include "SG_File_Data_Source.hpp"

SG_File_Data_Source::SG_File_Data_Source(std::istream * data) : 
  data(data)
{
};

bool
SG_File_Data_Source::getline(char * buf, int maxLen) {
  
  for(;;) {
    if (! data->getline(buf, maxLen)) {
      if (data->eof())
        return false;
      data->clear();
      continue;
    }
    if (buf[0])
      return true;
  }
};
