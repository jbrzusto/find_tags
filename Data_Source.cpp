#include "Data_Source.hpp"
#include "Lotek_Data_Source.hpp"
#include "SG_File_Data_Source.hpp"
#include "SG_SQLite_Data_Source.hpp"

#include <iostream>

Data_Source::Data_Source() {};

Data_Source::~Data_Source(){};

Data_Source *
Data_Source::make_SG_source(std::string infile) {
  if (infile.length() == 0)
    return new SG_File_Data_Source(& std::cin);
  return new SG_File_Data_Source(new std::ifstream(infile));
};

Data_Source *
Data_Source::make_SQLite_source(DB_Filer * db, unsigned int monoBN) {
  return new SG_SQLite_Data_Source(db, monoBN);
};

Data_Source *
Data_Source::make_Lotek_source(DB_Filer * db, Tag_Database *tdb, Frequency_MHz defFreq) {
  return new Lotek_Data_Source(db, tdb, defFreq);

};
