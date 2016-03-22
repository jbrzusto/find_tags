#include "Tag_Foray.hpp"

#include <string.h>
#include <sstream>
#include <time.h>

Tag_Foray::Tag_Foray () : hist(0) {}; // default ctor for deserializing into

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
  clock_fuzz(default_clock_fuzz),
  max_skipped_time(default_max_skipped_time),
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
Tag_Foray::set_default_clock_fuzz_ppm(float clock_fuzz) {
  default_clock_fuzz = clock_fuzz * 1E-6;	// ppm -> proportion
};

void
Tag_Foray::set_default_max_skipped_time(Gap skip) {
  default_max_skipped_time = skip;
};



void
Tag_Foray::start() {
  Tag_Candidate::ending_batch = false;

  char buf[MAX_LINE_SIZE + 1]; // input buffer

  for (;;) {
      // read and parse a line from a SensorGnome file

      if (! data->getline(buf, MAX_LINE_SIZE))
          break;

#ifdef DEBUG
      std::cerr << buf << std::endl;
#endif

      ++line_no;

      switch (buf[0]) {
      case 'G':
        {
          /* a GPS fix line like:
             G,1458001712,44.34021,-66.118733333,21.6
                 ts        lat        lon        alt
          */
          double ts, lat, lon, alt;
          if (4 == sscanf(buf+2, "%lf,%lf,%lf,%lf", &ts, &lat, &lon, &alt)) {
            // line is okay, so file it
            Tag_Candidate::filer-> add_GPS_fix( ts, lat, lon, alt );
          } // otherwise, quietly ignore malformed GPS fix line
        }
        break;
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
          // for future extension: in-band commands
        }
        break;
      default:
        break;
      }
  }
  Tag_Candidate::ending_batch = true;
};

void
Tag_Foray::process_event(Event e) {
  auto t = e.tag;
  auto fs = Freq_Setting::as_Nominal_Frequency_kHz(t->freq);
  Graph * g = graphs[fs];
  switch (e.code) {
  case Event::E_ACTIVATE:
    {
      auto rv = g->addTag(t, pulse_slop, clock_fuzz, max_skipped_time);
      for (auto i = tag_finders.begin(); i != tag_finders.end(); ++i)
        if (i->first.second == fs)
          i->second->rename_tag(rv);
    }
    break;
  case Event::E_DEACTIVATE:
    {
      auto rv = g->delTag(t, pulse_slop, clock_fuzz, max_skipped_time);
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

Gap Tag_Foray::default_pulse_slop = 0.0015; // 1.5 ms
float Tag_Foray::default_clock_fuzz = 50E-6; // 50 ppm
Gap Tag_Foray::default_max_skipped_time = 1000; // 1000 s; at 50ppm, this translate to fuzz of 50 ms
Tag_Foray::Run_Cand_Counter Tag_Foray::num_cands_with_run_id_ = Run_Cand_Counter();

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
    oa << make_nvp("default_clock_fuzz", Tag_Foray::default_clock_fuzz);
    oa << make_nvp("default_max_skipped_time", Tag_Foray::default_max_skipped_time);
    oa << make_nvp("num_cands_with_run_id_", Tag_Foray::num_cands_with_run_id_);

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
  ia >> make_nvp("default_clock_fuzz", Tag_Foray::default_clock_fuzz);
  ia >> make_nvp("default_max_skipped_time", Tag_Foray::default_max_skipped_time);
  ia >> make_nvp("num_cands_with_run_id_", Tag_Foray::num_cands_with_run_id_);

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

int
Tag_Foray::num_cands_with_run_id (DB_Filer::Run_ID rid, int delta) {
  if (rid == 0)
    return 0;
  auto i = num_cands_with_run_id_.find(rid);
  if (i == num_cands_with_run_id_.end()) {
    // rid not present
    if (delta == 0)
      return 0;
    if (delta < 0)
      throw std::runtime_error("Tried to reduce count of cands with run_id already at 0.");
    num_cands_with_run_id_.insert(std::make_pair(rid, delta));
    return delta;
  } else {
    if (delta == 0)
      return i->second;
    i->second += delta;
    if (i->second > 0)
      return i->second;
    num_cands_with_run_id_.erase(i);
    return(0);
  }
};
