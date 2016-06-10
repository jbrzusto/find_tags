#ifndef DATA_SOURCE_HPP
#define DATA_SOURCE_HPP

//!< Source for input data.  Data is either in sensorgnome format, from a file, stream,
//!< or motus .sqlite file, 
//!< or in Lotek format (generated by wrapper code in the motus R package)

#include "find_tags_common.hpp"
#include "Tag_Database.hpp"

using boost::serialization::make_nvp;


class Data_Source { 

public:
  Data_Source();
  ~Data_Source();

  virtual bool getline(char * buf, int maxLen) = 0;

  virtual void serialize(boost::archive::binary_iarchive & ar, const unsigned int version){};

  virtual void serialize(boost::archive::binary_oarchive & ar, const unsigned int version){};

  static Data_Source * make_SG_source(std::string infile, unsigned int monoBN=0);

  static Data_Source * make_Lotek_source(std::string infile, Tag_Database *tdb, Frequency_MHz defFreq);

};

#endif // DATA_SOURCE
