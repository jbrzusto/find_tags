#ifndef TAG_FORAY_HPP
#define TAG_FORAY_HPP

#include "find_tags_common.hpp"

#include "Tag_Database.hpp"

#include "Tag_Finder.hpp"
#include "Rate_Limiting_Tag_Finder.hpp"
#include <sqlite3.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/config.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>

//#include <boost/serialization/access.hpp>

/*
  Tag_Foray - manager a collection of tag finders searching the same
  data stream.  The data stream has pulses from multiple ports, as
  well as frequency settings for those ports.
*/

class Tag_Foray {

public:
  
  Tag_Foray (Tag_Database * tags, std::istream * data, Frequency_MHz default_freq, bool force_default_freq, float min_dfreq, float max_dfreq,  float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing, bool unsigned_dfreq=false);

  ~Tag_Foray ();

  long long start();                 // begin searching for tags; returns 0 if end of file; returns NN if receives command
                                     // !NEWBN,NN
  void test();                       // throws an exception if there are indistinguishable tags
  Tag_Database * tags;               // registered tags on all known nominal frequencies

  void pause(const char * filename); //!< serialize foray to file

protected:
                                     // settings

  std::istream * data;               // stream from which data records are read
  Frequency_MHz default_freq;        // default listening frequency on a port where no frequency setting has been seen
  bool force_default_freq;           // ignore in-line frequency settings and always use default?
  float min_dfreq;                   // minimum allowed pulse offset frequency; pulses with smaller offset frequency are
                                     // discarded
  float max_dfreq;                   // maximum allowed pulse offset frequency; pulses with larger offset frequency are
                                     // discarded rate-limiting parameters:

  float max_pulse_rate;              // if non-zero, set a maximum per second pulse rate
  Gap pulse_rate_window;             // number of consecutive seconds over which rate of incoming pulses must exceed
                                     // max_pulse_rate in order to discard entire window
  Gap min_bogus_spacing;             // when a window of pulses is discarded, we emit a bogus tag with ID 0.; this parameter
                                     // sets the minimum number of seconds between consecutive emissions of this bogus tag ID

  bool unsigned_dfreq;               // if true, ignore any sign on frequency offsets (use absolute value)

  // runtime storage


  unsigned long long line_no;                    // count lines of input seen
  


  typedef short Port_Num;                        // port number can be represented as a short
  
  
  std::map < Port_Num, Freq_Setting > port_freq; // keep track of frequency settings on each port


  // we need a Tag_Finder for each combination of port and nominal frequency
  // we'll use a map

  typedef std::pair < Port_Num, Nominal_Frequency_kHz > Tag_Finder_Key;
  typedef std::map < Tag_Finder_Key, Tag_Finder * > Tag_Finder_Map;

  Tag_Finder_Map tag_finders;

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & tags;
    ar & default_freq;
    ar & force_default_freq;
    ar & min_dfreq;
    ar & max_dfreq;
    ar & max_pulse_rate;
    ar & pulse_rate_window;
    ar & min_bogus_spacing;
    ar & unsigned_dfreq;
    ar & line_no;
    ar & port_freq;
    ar & tag_finders;
  };  
};


// serialize std::unordered_set

template< class Member >
void serialize(
               boost::archive::text_oarchive &ar,
               std::unordered_set < Member > &s,
               const unsigned int file_version
               ){
  save(ar, s, file_version);
};

template< class Member >
void serialize(
               boost::archive::text_iarchive &ar,
               std::unordered_set < Member > &s,
               const unsigned int file_version
               ){
  load(ar, s, file_version);
};

template< class Member >
void save(boost::archive::text_oarchive & ar, std::unordered_set < Member > & s, const unsigned int version) {
  
  size_t n = s.size();
  ar & n;
  for (auto i = s.begin(); i != s.end(); ++i)
    ar & *i;
};

template<class Archive, class Member>
void load(Archive & ar, std::unordered_set < Member > & s, const unsigned int version) {

  size_t n;
  ar & n;
  for (size_t i = 0; i < n; ++i) {
    Member m;
    ar & m;
    s.insert(m);
  }
};

// serialize std::unordered_multiset

template< class Member >
void serialize(
               boost::archive::text_oarchive &ar,
               std::unordered_multiset < Member > &s,
               const unsigned int file_version
               ){
  save(ar, s, file_version);
};

template< class Member >
void serialize(
               boost::archive::text_iarchive &ar,
               std::unordered_multiset < Member > &s,
               const unsigned int file_version
               ){
  load(ar, s, file_version);
};


template< class Member >
void save(boost::archive::text_oarchive & ar, std::unordered_multiset < Member > & s, const unsigned int version) {
  
  size_t n = s.size();
  ar & n;
  for (auto i = s.begin(); i != s.end(); ++i)
    ar & *i;
};

template<class Archive, class Member>
void load(Archive & ar, std::unordered_multiset < Member > & s, const unsigned int version) {

  size_t n;
  ar & n;
  for (size_t i = 0; i < n; ++i) {
    Member m;
    ar & m;
    s.insert(m);
  }
};

// serialize std::unordered_multimap

template< class Key, class Value >
void serialize(
               boost::archive::text_oarchive &ar,
               std::unordered_multimap < Key, Value > &s,
               const unsigned int file_version
               ){
  save(ar, s, file_version);
};

template< class Key, class Value >
void serialize(
               boost::archive::text_iarchive &ar,
               std::unordered_multimap < Key, Value > &s,
               const unsigned int file_version
               ){
  load(ar, s, file_version);
};


template< class Key, class Value >
void save(boost::archive::text_oarchive & ar, std::unordered_multimap < Key, Value > & s, const unsigned int version) {
  
  size_t n = s.size();
  ar & n;
  for (auto i = s.begin(); i != s.end(); ++i) {
    ar & i->first;
    ar & i->second;
  };

};

template<class Archive, class Key, class Value>
void load(Archive & ar, std::unordered_multimap < Key, Value > & s, const unsigned int version) {

  size_t n;
  ar & n;
  for (size_t i = 0; i < n; ++i) {
    Key k;
    Value v;
    ar & k;
    ar & v;
    s.insert(std::make_pair(k, v));
  }
};

#endif // TAG_FORAY
