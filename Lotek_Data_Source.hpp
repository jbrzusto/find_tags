#ifndef LOTEK_DATA_SOURCE_HPP
#define LOTEK_DATA_SOURCE_HPP

//!< Source for Lotek-format input data.

#include "find_tags_common.hpp"
#include "Data_Source.hpp"
#include "Tag_Database.hpp"
#include <map>

class Lotek_Data_Source : public Data_Source { 

public:
  Lotek_Data_Source(std::istream * data, Tag_Database *tdb, Frequency_MHz defFreq);
  bool getline(char * buf, int maxLen);
  static const int MAX_LOTEK_LINE_SIZE = 100;
  static const int MAX_LEAD_SECONDS = 10;  //!< maximum number of
                                         //! seconds before we dump
                                         //! pulses from a tag; allows
                                         //! for small time reversals
                                         //! and interleaving of
                                         //! output pulses
  static const int MAX_ANTENNAS=10;      //!< maximum number of antennas


protected:
  std::istream *data; //!< pointer to true input stream
  std::map < std::pair < short , short > , std::vector < Gap > * > tcode; //!< map (codeset, ID) to pulse gaps from the tag DB
  char ltbuf[MAX_LOTEK_LINE_SIZE]; //!< buffer for reading true input stream
  bool done; //!< true input stream is finished
  std::map < double, std::string > sgbuf; //!< buffer of SG-format lines
  Timestamp latestInputTS; //!< timestmap of most recent input line
  std::vector < Frequency_MHz > antFreq; //!< frequency on each antenna, in MHz
  
  // methods

  bool getInputLine(); //!< get line from true input stream; return
                       //! false on EOS

  bool translateLine(); //!< parse
};

#endif // LOTEK_DATA_SOURCE
