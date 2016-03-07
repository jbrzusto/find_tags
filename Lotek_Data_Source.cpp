#include "Lotek_Data_Source.hpp"
#include <sstream>

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
      tcode.insert(std::make_pair(std::make_pair(tg.codeSet, tg.mfgID), *t));
    }
  }
  // generate the vector of antenna frequencies
  antFreq = std::vector < Frequency_MHz > ( MAX_ANTENNAS, defFreq);
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
    if (done) {
      // easy case: no input left on true source, so just return
      // lines from sgbuf, if there are any.
      if (i == sgbuf.end())
        return false;
      strncpy(buf, i->second.c_str(), maxLen);
      sgbuf.erase(i);
      return true;
    }
    if (i->first + MAX_LEAD_SECONDS <= latestInputTS) {
      // easy case - there'a a sufficiently old line in the buffer
      strncpy(buf, i->second.c_str(), maxLen);
      return true;
    }
    // typical case; no sufficiently old lines left in sgbuf, but lines left on input
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
  short ant;
  short sig;
  Frequency_MHz freq;
  short gain;
  short codeSet;

  if (7 != sscanf(ltbuf, "%lf,%hd,%hd,%hd,%lf,%hd,%hd", &ts, &id, &ant, &sig, &freq, &gain, &codeSet)) {
    std::cerr << "bad Lotek input line: " << ltbuf << std::endl;
    return false;
  }

  if (freq != antFreq[ant]) {
    antFreq[ant] = freq;
    std::ostringstream freqRec;
    // make frequency setting record like: S,1366227448.192,5,-m,166.376,0,

    freqRec << "S," << std::setprecision(14) << ts << std::setprecision(3) << "," << ant << ",-m," << freq << ",0,";
    sgbuf.insert(std::make_pair(ts, freqRec.str()));
  }
  
  auto tt = tcode.find(std::make_pair(codeSet, id));
  if (tt == tcode.end()) {
    std::cerr << "Ignoring record: no known tag with ID " << id << " in codeset " << codeSet << std::endl;
    return false;
  }
  auto gg = tt->second;
  // generate a record for each tag pulse 

  for(auto i = gg->begin(); i != gg->end(); ++i) {
    std::ostringstream pRec;
    // "%hd,%lf,%f,%f,%f", &port_num, &ts, &dfreq, &sig, &noise)) {

    pRec << "p" << ant << "," << std::setprecision(14) << ts << std::setprecision(3) << ",0," << sig << ",-96";
    sgbuf.insert(std::make_pair(ts, pRec));
    ts += *i; // NB: the last gap takes us to the next burst, so is not actually used
  }
  return true;
}
    


