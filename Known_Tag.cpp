#include "Known_Tag.hpp"

#include <sstream>
#include <math.h>
// Note: in the following, gaps points to an array of PULSES_PER_BURST gaps;
// the first PULSES_PER_BURST-1 gaps are intra burst gaps, but the last gap is the burst interval

Known_Tag::Known_Tag(Lotek_Tag_ID lid, string proj, Frequency_MHz freq, Frequency_MHz fcd_freq, Frequency_Offset_kHz dfreq, float *gaps):
  lid(lid),
  proj(proj),
  freq(freq),
  fcd_freq(fcd_freq),
  dfreq(dfreq)
{
  // gaps are stored in the database as milliseconds
  float burst_length = 0.0;
  for (unsigned i = 0; i < PULSES_PER_BURST - 1; ++i) {
    burst_length += this->gaps[i] = gaps[i];
  }
  // calculate gap to start of next pulse, 
  this->gaps[PULSES_PER_BURST - 1] = gaps[PULSES_PER_BURST - 1] - burst_length;

  // store the burst interval
  this->gaps[PULSES_PER_BURST] = gaps[PULSES_PER_BURST - 1];
  if (burst_length > max_burst_length)
    max_burst_length = burst_length;

  // generate a full ID string  Proj#Lid@NOMFREQ:BI
  std::ostringstream fid;
  fid << proj << '#' << lid << '@' << std::setprecision(6) << freq << ':' << round(10 * gaps[PULSES_PER_BURST - 1]) / 10;
  fullID = fid.str();
  if (all_fullIDs.count(fullID)) {
    std::cerr << "Warning - two very similar tags in project " << proj << ":\nLotek ID: " << lid << "; frequency: " << freq << " MHz; burst interval: " << round(10 * gaps[PULSES_PER_BURST - 1]) / 10 << " sec\nAppending '!' to fullID of second one.\n";
    while (all_fullIDs.count(fullID)) {
      fullID += '!';
    };
  }
  all_fullIDs.insert(fullID);
};

std::unordered_set < std::string > 
Known_Tag::all_fullIDs;       // will be populated as tags database is built

float 
Known_Tag::max_burst_length = 0.0;   // will be calculated as tags database is built
