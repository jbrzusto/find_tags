#include "Tag_Foray.hpp"

#include <string.h>
#include <sstream>
#include <time.h>

Tag_Foray::Tag_Foray () {}; // default ctor for deserializing into

Tag_Foray::Tag_Foray (Tag_Database * tags, std::istream *data, Frequency_MHz default_freq, bool force_default_freq, float min_dfreq, float max_dfreq, float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing, bool unsigned_dfreq) :
  tags(tags),
  data(data),
  default_freq(default_freq),
  force_default_freq(force_default_freq),
  min_dfreq(min_dfreq),
  max_dfreq(max_dfreq),
  max_pulse_rate(max_pulse_rate),
  pulse_rate_window(pulse_rate_window),
  min_bogus_spacing(min_bogus_spacing),
  unsigned_dfreq(unsigned_dfreq),
  line_no(0),
  pulse_slop(default_pulse_slop),
  burst_slop(default_burst_slop),
  burst_slop_expansion(default_burst_slop_expansion),
  max_skipped_bursts(default_max_skipped_bursts),
  hist(tags->get_history()),
  cron(hist->getTicker())
{
  // create one empty graph for each nominal frequency
  auto fs = tags->get_nominal_freqs();
  for (auto i = fs.begin(); i != fs.end(); ++i)
    graphs.insert(std::make_pair(*i, new Graph()));
};


void
Tag_Foray::set_default_pulse_slop_ms(float pulse_slop_ms) {
  default_pulse_slop = pulse_slop_ms / 1000.0;	// stored as seconds
};

void
Tag_Foray::set_default_burst_slop_ms(float burst_slop_ms) {
  default_burst_slop = burst_slop_ms / 1000.0;	// stored as seconds
};

void
Tag_Foray::set_default_burst_slop_expansion_ms(float burst_slop_expansion_ms) {
  default_burst_slop_expansion = burst_slop_expansion_ms / 1000.0;   // stored as seconds
};

void
Tag_Foray::set_default_max_skipped_bursts(unsigned int skip) {
  default_max_skipped_bursts = skip;
};



long long
Tag_Foray::start() {
  long long bn = 0;
  char buf[MAX_LINE_SIZE + 1]; // input buffer

  while (! bn) {
      // read and parse a line from a SensorGnome file

      if (! data->getline(buf, MAX_LINE_SIZE)) {
        if (data->eof())
          break;
        data->clear();
        continue;
      }

      if (!buf[0])
        continue;
      //	break;

      ++line_no;
      lastLine = std::string(buf);

      switch (buf[0]) {
      case 'S':
        {
          /* a parameter-setting line like: 
             S,1366227448.192,5,-m,166.376,0,
             is S, timestamp, port_num, param flag, value, return code, other error
          */
          short port_num;
          char param_flag[16];
          double param_value;
          int return_code;
          char error[256];
          if (5 > sscanf(buf+2, "%lf,%hd,%[^,],%lf,%d,%[^\n]", &ts, &port_num, param_flag, &param_value, &return_code, error)) {
            // silently ignore malformed line (note:  %[^\n] field doesn't increase count if remainder of line is empty, so sscanf here can return 5 or 6
            continue;
          }
          if (strcmp("-m", param_flag) || return_code) {
            // ignore non-frequency parameter setting
            continue;
          }
        
          if (! force_default_freq)
            port_freq[port_num] = Freq_Setting(param_value);
          continue;
        }
        break;
      case 'p':
        {
          short port_num;
          float dfreq, sig, noise;
          if (5 != sscanf(buf+1, "%hd,%lf,%f,%f,%f", &port_num, &ts, &dfreq, &sig, &noise)) {
            std::cerr << "Warning: malformed line in input\n  at line " << line_no << ":\n" << (string("") + buf) << std::endl;
            continue;
          }

          if (dfreq > max_dfreq || dfreq < min_dfreq)
            continue;

          if (! port_freq.count(port_num))
            port_freq[port_num] = Freq_Setting(default_freq);

          Tag_Finder_Key key(port_num, port_freq[port_num].f_kHz);

          if (! tag_finders.count(key)) {
            Tag_Finder *newtf;
            std::ostringstream prefix;
            prefix << port_num << ",";
            if (max_pulse_rate > 0)
              newtf = new Rate_Limiting_Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[key.second], pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix.str());
            else
              newtf = new Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[key.second], prefix.str());
            tag_finders[key] = newtf;
#if 0
            //#ifdef FIND_TAGS_DEBUG
            std::cerr << "Interval Tree for " << prefix.str() << std::endl;
            newtf->graph.get_root()->dump(std::cerr);
            std::cerr << "Burst slop expansion is " << Tag_Finder::default_burst_slop_expansion << std::endl;
#endif
          };

          if (dfreq < 0 && unsigned_dfreq)
            dfreq = - dfreq;

          Pulse p = Pulse::make(ts, dfreq, sig, noise, port_freq[port_num].f_MHz);
          
          
          // process any tag events up to this point in time
          
          while (cron.ts() <= p.ts)
            process_event(cron.get());
							       
          tag_finders[key]->process(p);
        }
        break;
      case '!':
        {
          if (1 != sscanf(buf+1, "NEWBN,%lld", & bn)) {
            std::cerr << "Warning: malformed line in input\n  at line " << line_no << ":\n" << (string("") + buf) << std::endl;
            continue;
          }
        }
        break;
      default:
        break;
      }
  }
  return bn;
};

void
Tag_Foray::process_event(Event e) {
  auto t = e.tag;
  auto fs = Freq_Setting::as_Nominal_Frequency_kHz(t->freq);
  Graph * g = graphs[fs];
  switch (e.code) {
  case Event::E_ACTIVATE:
    {
      auto rv = g->addTag(t, pulse_slop, burst_slop / t->gaps[3], max_skipped_bursts * t->period);
      for (auto i = tag_finders.begin(); i != tag_finders.end(); ++i)
        if (i->first.second == fs)
          i->second->rename_tag(rv);
    }
    break;
  case Event::E_DEACTIVATE:
    {
      auto rv = g->delTag(t, pulse_slop, burst_slop / t->gaps[3], max_skipped_bursts * t->period);
      for (auto i = tag_finders.begin(); i != tag_finders.end(); ++i)
        if (i->first.second == fs)
          i->second->rename_tag(rv);
    }
    break;
  default:
    std::cerr << "Warning: Unknown event code " << e.code << " for tag " << t->motusID << std::endl;
  };
}

void
Tag_Foray::test() {
  // create a tagfinder for each nominal frequency, to verify that tags in the database
  // are distinguishable with the current parameters

  Freq_Set fs = tags->get_nominal_freqs();
  for (Freq_Set :: iterator it = fs.begin(); it != fs.end(); ++it) {
    Tag_Finder_Key key(0, *it);
    std::string prefix="p";
    Tag_Finder *newtf;
    port_freq[0] = Freq_Setting(*it / 1000.0);
    if (max_pulse_rate > 0)
      newtf = new Rate_Limiting_Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[*it], pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix);
    else
      newtf = new Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[*it], prefix);
    // FIXME: do something to check database validity
    delete newtf;
  }
}

Tag_Foray::~Tag_Foray () {
  for (auto tfi = tag_finders.begin(); tfi != tag_finders.end(); ++tfi)
    delete (tfi->second);
};

Gap Tag_Foray::default_pulse_slop = 0.0015; // 1.5 ms
Gap Tag_Foray::default_burst_slop = 0.010; // 10 ms
Gap Tag_Foray::default_burst_slop_expansion = 0.001; // 1ms = 1 part in 10000 for 10s BI
unsigned int Tag_Foray::default_max_skipped_bursts = 60;
  
void
Tag_Foray::pause() {
  // make an archive;
  // this is the top-level of the serializer,
  // so we dump class static members from here.
  std::ostringstream ofs;
  {
    // block to ensure oa dtor is called
    boost::archive::xml_oarchive oa(ofs);

    // Tag_Foray
    oa << Tag_Foray::default_pulse_slop;
    oa << Tag_Foray::default_burst_slop;
    oa << Tag_Foray::default_burst_slop_expansion;
    oa << Tag_Foray::default_max_skipped_bursts;

    // Freq_Setting
    oa << Freq_Setting::nominal_freqs;

    // Node 
    oa << Node::_numNodes;
    oa << Node::_numLinks;
    oa << Node::maxLabel;
    oa << Node::_empty;

    // Set
    oa << Set::_numSets;
    oa << Set::maxLabel;
    oa << Set::_empty;
    oa << Set::allSets;

    // Tag_Candidate
    oa << Tag_Candidate::freq_slop_kHz;
    oa << Tag_Candidate::sig_slop_dB;
    oa << Tag_Candidate::pulses_to_confirm_id;

    // Ambiguity (a singleton class)
    oa << Ambiguity::abm;
    oa << Ambiguity::nextID;
  
    // dynamic members of all classes
    serialize(oa, 1);
  }
  // record this state
  struct timespec tsp;
  clock_gettime(CLOCK_REALTIME, & tsp);

  Tag_Candidate::filer->
    save_findtags_state( tsp.tv_sec + 1e-9 * tsp.tv_nsec, // time now
                         ts,                            // last timestamp parsed from input
                         lastLine,              // last line read from input
                         ofs.str()                      // serialized state
                         );

};

bool
Tag_Foray::resume(Tag_Foray &tf) {
  Timestamp paused;
  Timestamp lastLineTS;
  std::string blob;
  
  Tag_Candidate::filer->
    load_findtags_state( paused,
                         lastLineTS,
                         tf.lastLine,
                         blob                      // serialized state
                         );

  std::istringstream ifs (blob);
  boost::archive::xml_iarchive ia(ifs);

  // Tag_Foray
  ia >> Tag_Foray::default_pulse_slop;
  ia >> Tag_Foray::default_burst_slop;
  ia >> Tag_Foray::default_burst_slop_expansion;
  ia >> Tag_Foray::default_max_skipped_bursts;

  // Freq_Setting
  ia >> Freq_Setting::nominal_freqs;

  // Node 
  ia >> Node::_numNodes;
  ia >> Node::_numLinks;
  ia >> Node::maxLabel;
  ia >> Node::_empty;

  // Set
  ia >> Set::_numSets;
  ia >> Set::maxLabel;
  ia >> Set::_empty;
  ia >> Set::allSets;

  // Tag_Candidate
  ia >> Tag_Candidate::freq_slop_kHz;
  ia >> Tag_Candidate::sig_slop_dB;
  ia >> Tag_Candidate::pulses_to_confirm_id;

  // Ambiguity (a singleton class)
  ia >> Ambiguity::abm;
  ia >> Ambiguity::nextID;
  
  // dynamic members of all classes
  tf.serialize(ia, 1);
  return true;
};

  
