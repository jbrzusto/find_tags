#ifndef TAG_FORAY_HPP
#define TAG_FORAY_HPP

#include "find_tags_common.hpp"

#include "Tag_Database.hpp"

#include "Tag_Finder.hpp"
#include "Rate_Limiting_Tag_Finder.hpp"
#include <sqlite3.h>


/*
  Tag_Foray - manager a collection of tag finders searching the same
  data stream.  The data stream has pulses from multiple ports, as
  well as frequency settings for those ports.
*/

class Tag_Foray {

public:
  
  Tag_Foray (Tag_Database &tags, std::istream * data, Frequency_MHz default_freq, bool force_default_freq, float min_dfreq, float max_dfreq,  float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing, bool unsigned_dfreq=false);

  ~Tag_Foray ();

  long long start(); // begin searching for tags; returns 0 if end of file; returns NN if receives command !NEWBN,NN
  void test(); // throws an exception if there are indistinguishable tags
  Tag_Database & tags; // registered tags on all known nominal frequencies

protected:

  void process_event(Event e); //!< process a tag event; force all tag
                               //! finders to process a tag event,
                               //! which typically adds or removes a
                               //! tag from the graph of active tags

  // settings

  std::istream * data; // stream from which data records are read
  Frequency_MHz default_freq; // default listening frequency on a port where no frequency setting has been seen
  bool force_default_freq; // ignore in-line frequency settings and always use default?
  float min_dfreq; // minimum allowed pulse offset frequency; pulses with smaller offset frequency are discarded
  float max_dfreq; // maximum allowed pulse offset frequency; pulses with larger offset frequency are discarded
  // rate-limiting parameters:

  float max_pulse_rate; // if non-zero, set a maximum per second pulse rate
  Gap pulse_rate_window; // number of consecutive seconds over which rate of incoming pulses must exceed max_pulse_rate in order to discard entire window
  Gap min_bogus_spacing; // when a window of pulses is discarded, we emit a bogus tag with ID 0.; this parameter sets the minimum number of seconds between consecutive emissions of this bogus tag ID

  bool unsigned_dfreq; // if true, ignore any sign on frequency offsets (use absolute value)

  // runtime storage

  // count lines of input seen
  unsigned long long line_no;
  
  // port number can be represented as a short

  typedef short Port_Num;
  
  // keep track of frequency settings on each port
  
  std::map < Port_Num, Freq_Setting > port_freq;

  // we need a Tag_Finder for each combination of port and nominal frequency
  // we'll use a map

  typedef std::pair < Port_Num, Nominal_Frequency_kHz > Tag_Finder_Key;
  typedef std::map < Tag_Finder_Key, Tag_Finder * > Tag_Finder_Map;

  Tag_Finder_Map tag_finders;
};


#endif // TAG_FORAY
