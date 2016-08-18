#include "Lotek_Data_Source.hpp"
#include <sstream>
#include <cstdio>

Lotek_Data_Source::Lotek_Data_Source(std::istream * data, Tag_Database *tdb, Frequency_MHz defFreq) : 
  data(data),
  done(false),
  sgbuf(),
  latestInputTS(0)
{
  // generate the map from (codeSet, ID) -> Tag *
  auto kf = tdb->get_nominal_freqs();
  for (auto f = kf.begin(); f != kf.end(); ++ f) {
    auto tags = tdb->get_tags_at_freq(*f);
    for (auto t = tags->begin(); t != tags->end(); ++t) {
      Tag & tg = **t;
      tcode.insert(std::make_pair(std::make_pair(tg.codeSet, tg.mfgID), & tg.gaps));
    }
  }
  // generate the vector of antenna frequencies
  antFreq = std::vector < Frequency_MHz > ( MAX_ANTENNAS, defFreq);

  // create the scanf format string for input lines using MAX_ANT_NAME_CHARS as the ant name field width
  snprintf(&ltLineFormat[0], MAX_LINE_FORMAT_CHARS, "%%lf,%%hd,%%%d[^,],%%hd,%%lf,%%hd,Lotek%%hd,%%lf,%%lf", MAX_ANT_NAME_CHARS);
};

bool 
Lotek_Data_Source::getInputLine() {
  if (done)
    return false;
  for(;;) {
    if (! data->getline(ltbuf, MAX_LOTEK_LINE_SIZE)) {
      if (data->eof()) {
        done = true;
        return false;
      }
      data->clear();
      continue;
    } else {
      if (ltbuf[0])
        return true;
    }
  };
}
  
bool 
Lotek_Data_Source::getline(char * buf, int maxLen) {   
  
  for(;;) {
    // wait until either the true input is done, or
    // we have sufficiently old data in sgbuf (i.e. lines which
    // are old enough to be guaranteed (by the value of MAX_LEAD_SECONDS)
    // that no older data will be generated from subsequent input records
    auto i = sgbuf.begin();
    if (i != sgbuf.end() && i->first + MAX_LEAD_SECONDS <= latestInputTS) {
      // easy case - there's a sufficiently old line in the buffer
      strncpy(buf, i->second.c_str(), maxLen);
      sgbuf.erase(i);
      return true;
    }
    // no lines in sgbuf are sufficiently old.  If there are no true
    // input lines left, we're done (we save insufficiently old 
    // sgbuf lines until the next time the algorithm is run with 
    // subsequent data, because there might be pulses from there
    // which precede some of those in the current sgbuf.

    if (done)
      return false;

    // typical case; no sufficiently old lines left in sgbuf, but we haven't
    // reached EOF on input
        
    if (getInputLine())
      translateLine();

    // loop around until eof or a line is found
  }
};

bool
Lotek_Data_Source::translateLine()
{
  // add appropriate SG lines to the buffer for a given Lotek line
  //
  // Algorithm: if the current tag detection frequency does not match the
  // value for the current antenna, then issue a frequency-change command.
  // Then, issue a set of 4 pulse records, representing the coded ID.
  
  // return true on success, false otherwise

  Timestamp ts;
  short id; 
  char antName[MAX_ANT_NAME_CHARS + 1];
  short ant;
  short sig;
  Frequency_MHz freq;
  short gain;
  short codeSet;
  double lat;
  double lon;

  if (9 != sscanf(ltbuf, ltLineFormat, &ts, &id, antName, &sig, &freq, &gain, &codeSet, &lat, &lon)) {
    std::cerr << "bad Lotek input line: " << ltbuf << std::endl;
    return false;
  }
  // clean up weird lotek antenna naming: either a digit or AHdigit or "A1+A2+A3+A4" (the master antenna on SRX 800)
  ant = (strlen(antName) >= 3 && antName[2] == '+') ? -1 : antName[antName[0] == 'A' ? 2 : 0] - '0';

  // output a GPS fix
  if (lat != -999 && lon != -999)
    Tag_Candidate::filer->add_GPS_fix(ts, lat, lon, 0);

  latestInputTS = ts;
  if (freq != antFreq[ant]) {
    antFreq[ant] = freq;
    std::ostringstream freqRec;
    // make frequency setting record like: S,1366227448.192,5,-m,166.376,0,

    freqRec << "S," << std::setprecision(14) << ts << std::setprecision(3) << "," << ant << ",-m," << freq << ",0,";
    sgbuf.insert(std::make_pair(ts, freqRec.str()));
  }
  
  auto csid = std::make_pair(codeSet, id);
  auto tt = tcode.find(csid);
  if (tt == tcode.end()) {
    if (! warned.count(csid)) {
      std::cerr << "Ignoring record: no known tag with ID " << id << " in codeset " << codeSet << std::endl;
      warned.insert(csid);
    }
    return false;
  }
  auto gg = tt->second;
  // generate a record for each tag pulse 

  for(auto i = gg->begin(); i != gg->end(); ++i) {
    std::ostringstream pRec;
    // "%hd,%lf,%f,%f,%f", &port_num, &ts, &dfreq, &sig, &noise)) {
    // we use dfreq=4 to put it at the usual nominal SG frequency (i.e. funcube is tuned 4 kHz
    // below nominal, so dfreq=4 means a tag on nominal)
    pRec << "p" << ant << "," << std::setprecision(14) << ts << std::setprecision(3) << ",4," << sig << ",-96";
    sgbuf.insert(std::make_pair(ts, pRec.str()));
    ts += *i; // NB: the last gap takes us to the next burst, so is not actually used
  }
  return true;
}

// ugly macro because I couldn't figure out how to make this work
// properly with templates.

// Note: we only serialize what's needed to resume           
// processing buffered lines.  Tcode will be reconstructed   
// from the tag database provided, as we don't want          
// to be saving another copy of tag signatures (and the      
// copy saved might be saved with motus in the future).      

#define SERIALIZE_FUN_BODY \
   ar & BOOST_SERIALIZATION_NVP( sgbuf );                       \
   ar & BOOST_SERIALIZATION_NVP( latestInputTS );               \
   ar & BOOST_SERIALIZATION_NVP( antFreq );                     \
   ar & BOOST_SERIALIZATION_NVP( warned );
    
void
Lotek_Data_Source::serialize(boost::archive::binary_iarchive & ar, const unsigned int version) {
  
  SERIALIZE_FUN_BODY;

};

void
Lotek_Data_Source::serialize(boost::archive::binary_oarchive & ar, const unsigned int version) {

  SERIALIZE_FUN_BODY;

};

