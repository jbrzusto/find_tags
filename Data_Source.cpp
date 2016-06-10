#include "Data_Source.hpp"
#include "Lotek_Data_Source.hpp"
#include "SG_File_Data_Source.hpp"
#include "SG_SQLite_Data_Source.hpp"

#include <iostream>

Data_Source::Data_Source() {};

Data_Source::~Data_Source(){};

Data_Source *
Data_Source::make_SG_source(std::string infile, unsigned int monoBN) {
  if (infile.length() == 0)
    return new SG_File_Data_Source(& std::cin);
  if (infile.find(".sqlite") != infile.length() - 7)
    return new SG_File_Data_Source(new std::ifstream(infile));
  return new SG_SQLite_Data_Source(infile, monoBN);
};

Data_Source *
Data_Source::make_Lotek_source(std::string infile, Tag_Database *tdb, Frequency_MHz defFreq) {
  if (infile.length() == 0)
    return new Lotek_Data_Source(& std::cin, tdb, defFreq);
  return new Lotek_Data_Source(new std::ifstream(infile), tdb, defFreq);

};
