#include "Tag_Foray.hpp"
#include "SG_Record.hpp"

#include <string.h>
#include <sstream>
#include <time.h>
#include <cmath>

Tag_Foray::Tag_Foray () :  // default ctor for deserializing into
  line_no(0),   // line numbers reset even when resuming
  pulse_count(MAX_PORT_NUM + 1),
  hist(0),      // we recreate history on resume
  tsBegin(0),
  prevHourBin(0)
{};

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
  pulse_count(MAX_PORT_NUM + 1),
  pulse_slop(default_pulse_slop),
  clock_fuzz(default_clock_fuzz),
  max_skipped_time(default_max_skipped_time),
  hist(tags->get_history()),
  cron(hist->getTicker()),
  tsBegin(0),
  prevHourBin(0)
{
  // create one empty graph for each nominal frequency
  auto fs = tags->get_nominal_freqs();
  for (auto i = fs.begin(); i != fs.end(); ++i)
    graphs.insert(std::make_pair(*i, new Graph()));
};


Tag_Foray::~Tag_Foray () {
  for (auto tfi = tag_finders.begin(); tfi != tag_finders.end(); ++tfi)
    delete (tfi->second);
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

  bool done = false;

  char buf[MAX_LINE_SIZE + 1] = {}; // input buffer

  cr = new Clock_Repair(Tag_Candidate::filer);
  SG_Record r;

  while (!done) {
    // read and parse a line from a SensorGnome file

    if (data->getline(buf, MAX_LINE_SIZE)) {
      ++line_no;
      r = SG_Record::from_buf(buf);
      if (r.type == SG_Record::BAD) {
        std::cerr << "Warning: malformed line in input\n  at line " << line_no << ":\n" << (string("") + buf) << std::endl;
        continue;
      }
      cr->put(r);
    } else {
      done = true;
      cr->done();
    }

    while(cr->get(r)) {
      if (! tsBegin || r.ts < tsBegin)
        tsBegin = r.ts;
      ts = r.ts;
      switch (r.type) {
      case SG_Record::GPS:
        // GPS is not stuck, or Clock_Repair would have dropped the record
        Tag_Candidate::filer-> add_GPS_fix( r.ts, r.v.lat, r.v.lon, r.v.alt );
        break;

      case SG_Record::PARAM:

        if (strcmp("-m", r.v.param_flag) || r.v.return_code) {
          // ignore non-frequency parameter setting, or failed frequency setting
          continue;
        }

        if (! force_default_freq)
          port_freq[r.port] = Freq_Setting(r.v.param_value);
        continue;
        break;

      case SG_Record::PULSE:
        {
          // bump up the pulse count for the current hour bin

          double hourBin = round(r.ts / 3600);
          if (hourBin != prevHourBin && prevHourBin > 0) {
            for (int i = 0; i <= MAX_PORT_NUM; ++i) {
              if (pulse_count[i] > 0) {
                Tag_Candidate::filer->add_pulse_count(prevHourBin, i, pulse_count[i]);
                pulse_count[i] = 0;
              }
            }
            prevHourBin = hourBin;
          }

          if (r.port >= 0 && r.port < MAX_PORT_NUM)
            ++pulse_count[r.port];

          // skip this record if its offset frequency is out of bounds
          if (r.v.dfreq > max_dfreq || r.v.dfreq < min_dfreq)
            continue;

          // if this pulse is from a port whose frequency hasn't been set,
          // use the default

          if (! port_freq.count(r.port))
            port_freq[r.port] = Freq_Setting(default_freq);

          // NB: cast r.port to work around optimization of passing reference
          // to packed struct

          // which tag finder should this pulse be passed to?  There is at
          // last a tag finder on each port, and possibly more than one if
          // the listening frequency is changing.

          Tag_Finder_Key key((short int) r.port, port_freq[r.port].f_kHz);

          // if there isn't already an appropriate Tag_Finder, create it
          if (! tag_finders.count(key)) {
            Tag_Finder *newtf;
            std::ostringstream prefix;
            prefix << r.port << ",";
            if (max_pulse_rate > 0)
              newtf = new Rate_Limiting_Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[key.second], pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix.str());
            else
              newtf = new Tag_Finder(this, key.second, tags->get_tags_at_freq(key.second), graphs[key.second], prefix.str());
            tag_finders[key] = newtf;
          };

          if (r.v.dfreq < 0 && unsigned_dfreq)
            r.v.dfreq = - r.v.dfreq;

          // create a pulse object from this record
          Pulse p = Pulse::make(r.ts, r.v.dfreq, r.v.sig, r.v.noise, port_freq[r.port].f_MHz);

          // process any tag events up to this point in time

          while (cron.ts() <= p.ts)
            process_event(cron.get());

          tag_finders[key]->process(p);
#ifdef DEBUG
          tag_finders[key]->dump(r.ts);
#endif
        }
        break;
      case SG_Record::EXTENSION:
        {
          // for future extension: in-band commands
        }
        break;
      default:
        break;
      }
    }
  }
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
  // serialize and save state of the tag finder
  // this is the top-level of the serializer,
  // so we dump class static members from here.


  // before doing so, reap candidates from all tag finders so
  // we finish runs which have expired. (needed e.g. when no
  // pulses have been received for an antenna in a long time)
  for (auto tfi = tag_finders.begin(); tfi != tag_finders.end(); ++tfi)
    tfi->second->reap(ts);

  // Now when destructors are called for remaining (non-expired)
  // tag candidates, we do *not* want to actually end the run,
  // as it might continue in the next batch of data.  Indicate
  // this.

  Tag_Candidate::ending_batch = true;
  std::ostringstream ofs;

  Tag_Candidate::filer->end_batch(tsBegin, ts);

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
#ifdef DEBUG
    oa << make_nvp("allSets", Set::allSets);
#endif

    // Tag_Candidate
    oa << make_nvp("freq_slop_kHz", Tag_Candidate::freq_slop_kHz);
    oa << make_nvp("sig_slop_dB", Tag_Candidate::sig_slop_dB);
    oa << make_nvp("pulses_to_confirm_id", Tag_Candidate::pulses_to_confirm_id);
    oa << make_nvp("num_cands", Tag_Candidate::num_cands);

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

  if (! Tag_Candidate::filer->
    load_findtags_state( paused,
                         lastLineTS,
                         blob                      // serialized state
                         ))
    return false;

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
#ifdef DEBUG
  ia >> make_nvp("allSets", Set::allSets);
#endif

  // Tag_Candidate
  ia >> make_nvp("freq_slop_kHz", Tag_Candidate::freq_slop_kHz);
  ia >> make_nvp("sig_slop_dB", Tag_Candidate::sig_slop_dB);
  ia >> make_nvp("pulses_to_confirm_id", Tag_Candidate::pulses_to_confirm_id);
  ia >> make_nvp("num_cands", Tag_Candidate::num_cands);

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
