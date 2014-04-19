#ifndef KNOWN_TAG_HPP
#define KNOWN_TAG_HPP

#include "find_tags_common.hpp"

#include <unordered_map>
#include <unordered_set>

struct Known_Tag {

  // represents a database of tags we have "registered" (i.e. estimated
  // gaps, burst interval, and frequency offset from raw receiver audio)

public:

  Mfr_Tag_ID		lid;				// Manufacturer ID
  string		proj;				// project name
  Frequency_MHz		freq;				// nominal transmit frequency (MHz)
  Frequency_MHz		fcd_freq;			// frequency of funcubedongle at which tag was registered (MHz)
  Frequency_Offset_kHz	dfreq;				// offset from nominal frequency observed by funcube at registration (kHz)
  float			gaps[PULSES_PER_BURST+1];	// gaps between pulses; if p0..p3 are times of pulses in one burst, and p4 is the time of the first
							// pulse in the next burst,
							// then gaps[0] = p1-p0; gaps[1] = p2-p1; gaps[2] = p3-p2; gaps[3] = p4 - p3
							// Also, gaps[PULSES_PER_BURST] is the burst interval (sec) = gaps[0] + gaps[1] + gaps[2] + gaps[3] = p4-p0.

  static float	        max_burst_length;		// maximum across known tags of gap between first and last pulses in a burst

  std::string           fullID;                         // full ID, used when printing tags

private:
  static std::unordered_set < std::string > all_fullIDs;       // full ID list

public:
  Known_Tag(){};

  // Note: in the following, gaps points to an array of PULSES_PER_BURST gaps;
  // the first PULSES_PER_BURST-1 gaps are intra burst gaps, but the last gap is the burst interval

  Known_Tag(Mfr_Tag_ID lid, string proj, Frequency_MHz freq, Frequency_MHz fcd_freq, Frequency_Offset_kHz dfreq, float *gaps);

};

typedef std::unordered_set < Known_Tag * > Tag_Set; 

#endif // KNOWN_TAG_HPP
