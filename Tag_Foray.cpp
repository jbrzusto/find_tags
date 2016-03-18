#include "Tag_Foray.hpp"

#include <string.h>
#include <sstream>
#include <time.h>

Tag_Foray::Tag_Foray () {}; // default ctor for deserializing into

Tag_Foray::Tag_Foray (Tag_Database * tags, Data_Source *data, Frequency_MHz default_freq, bool force_default_freq, float min_dfreq, float max_dfreq, float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing, bool unsigned_dfreq) :
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
  Tag_Candidate::ending_batch = false;

  long long bn = 0;
  char buf[MAX_LINE_SIZE + 1]; // input buffer

  while (! bn) {
      // read and parse a line from a SensorGnome file

      if (! data->getline(buf, MAX_LINE_SIZE))
          break;

#ifdef DEBUG
      std::cout << buf << std::endl;
#endif

      ++line_no;

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
  Tag_Candidate::ending_batch = true;
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
      auto rv = g->addTag(t, pulse_slop, burst_slop / t->gaps[3], (1 + max_skipped_bursts) * t->period);
      for (auto i = tag_finders.begin(); i != tag_finders.end(); ++i)
        if (i->first.second == fs)
          i->second->rename_tag(rv);
    }
    break;
  case Event::E_DEACTIVATE:
    {
      auto rv = g->delTag(t, pulse_slop, burst_slop / t->gaps[3], (1 + max_skipped_bursts) * t->period);
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
  // try build tag finders for each nominal frequency

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
    // FIXME: do something to test validity?
    delete newtf;
  }
}

void
Tag_Foray::graph() {
  // plot the DFA graphs for the given tag database, one per nominal frequency
  
  Timestamp t = now();
  while (cron.ts() < t) // process all events to this point in time
    process_event(cron.get());

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
    newtf->graph->viz();
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
    boost::archive::binary_oarchive oa(ofs);

    // Tag_Foray
    oa << make_nvp("default_pulse_slop", Tag_Foray::default_pulse_slop);
    oa << make_nvp("default_burst_slop", Tag_Foray::default_burst_slop);
    oa << make_nvp("default_burst_slop_expansion", Tag_Foray::default_burst_slop_expansion);
    oa << make_nvp("default_max_skipped_bursts", Tag_Foray::default_max_skipped_bursts);

    // Freq_Setting
    oa << make_nvp("nominal_freqs", Freq_Setting::nominal_freqs);

    // Pulse
    oa << make_nvp("count", Pulse::count);

    // Node 
    oa << make_nvp("_numNodes", Node::_numNodes);
    oa << make_nvp("_numLinks", Node::_numLinks);
    oa << make_nvp("maxLabel", Node::maxLabel);
    oa << make_nvp("_empty", Node::_empty);

    // Set
    oa << make_nvp("_numSets", Set::_numSets);
    oa << make_nvp("maxLabel", Set::maxLabel);
    oa << make_nvp("_empty", Set::_empty);
    oa << make_nvp("allSets", Set::allSets);

    // Tag_Candidate
    oa << make_nvp("freq_slop_kHz", Tag_Candidate::freq_slop_kHz);
    oa << make_nvp("sig_slop_dB", Tag_Candidate::sig_slop_dB);
    oa << make_nvp("pulses_to_confirm_id", Tag_Candidate::pulses_to_confirm_id);

    // Ambiguity (a singleton class)
    oa << make_nvp("abm", Ambiguity::abm);
    oa << make_nvp("nextID", Ambiguity::nextID);
  
    // dynamic members of all classes
    serialize(oa, 1);

    // data source
    data->serialize(oa, 1);

  }
  // record this state

  Tag_Candidate::filer->
    save_findtags_state( ts,                            // last timestamp parsed from input
                         now(), // time now
                         ofs.str()                      // serialized state
                         );

};

double 
Tag_Foray::now() {
  struct timespec tsp;
  clock_gettime(CLOCK_REALTIME, & tsp);
  return  tsp.tv_sec + 1e-9 * tsp.tv_nsec;
};

bool
Tag_Foray::resume(Tag_Foray &tf, Data_Source *data) {
  Timestamp paused;
  Timestamp lastLineTS;
  std::string blob;
  
  Tag_Candidate::filer->
    load_findtags_state( paused,
                         lastLineTS,
                         blob                      // serialized state
                         );

  std::istringstream ifs (blob);
  boost::archive::binary_iarchive ia(ifs);

  // Tag_Foray
  ia >> make_nvp("default_pulse_slop", Tag_Foray::default_pulse_slop);
  ia >> make_nvp("default_burst_slop", Tag_Foray::default_burst_slop);
  ia >> make_nvp("default_burst_slop_expansion", Tag_Foray::default_burst_slop_expansion);
  ia >> make_nvp("default_max_skipped_bursts", Tag_Foray::default_max_skipped_bursts);

  // Freq_Setting
  ia >> make_nvp("nominal_freqs", Freq_Setting::nominal_freqs);

  // Pulse
  ia >> make_nvp("count", Pulse::count);

  // Node 
  ia >> make_nvp("_numNodes", Node::_numNodes);
  ia >> make_nvp("_numLinks", Node::_numLinks);
  ia >> make_nvp("maxLabel", Node::maxLabel);
  ia >> make_nvp("_empty", Node::_empty);

  // Set
  ia >> make_nvp("_numSets", Set::_numSets);
  ia >> make_nvp("maxLabel", Set::maxLabel);
  ia >> make_nvp("_empty", Set::_empty);
  ia >> make_nvp("allSets", Set::allSets);

  // Tag_Candidate
  ia >> make_nvp("freq_slop_kHz", Tag_Candidate::freq_slop_kHz);
  ia >> make_nvp("sig_slop_dB", Tag_Candidate::sig_slop_dB);
  ia >> make_nvp("pulses_to_confirm_id", Tag_Candidate::pulses_to_confirm_id);

  // Ambiguity (a singleton class)
  ia >> make_nvp("abm", Ambiguity::abm);
  ia >> make_nvp("nextID", Ambiguity::nextID);
  
  // dynamic members of all classes
  tf.serialize(ia, 1);

  // data source deserialization happens into the
  // new data source
  tf.data = data;

  data->serialize(ia, 1);
  return true;
};

  
