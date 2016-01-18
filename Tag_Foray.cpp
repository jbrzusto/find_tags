#include "Tag_Foray.hpp"

#include <string.h>

Tag_Foray::Tag_Foray (Tag_Database &tags, std::istream *data, Frequency_MHz default_freq, bool force_default_freq, float min_dfreq, float max_dfreq, float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing, bool unsigned_dfreq) :
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
  line_no(0)
{
};


long long
Tag_Foray::start() {
  long long bn = 0;
  double ts;
  while (! bn) {
      // read and parse a line from a SensorGnome file

      char buf[MAX_LINE_SIZE + 1];
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

      switch (buf[0]) {
      case 'S':
        {
          /* a parameter-setting line like: 
             S,1366227448.192,5,-m,166.376,0,
             is S, timestamp, port_num, param flag, value, return code, other error
          */
          double ts;
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
              newtf = new Rate_Limiting_Tag_Finder(this, key.second, tags.get_tags_at_freq(key.second), pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix.str());
            else
              newtf = new Tag_Finder(this, key.second, tags.get_tags_at_freq(key.second), prefix.str());
            newtf->init();
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
							       
          tag_finders[key]->process(p);
        }
        break;
      case '!':
        {
          std::cerr << string(buf) << std::endl;
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
Tag_Foray::test() {
  // create a tagfinder for each nominal frequency, to verify that tags in the database
  // are distinguishable with the current parameters

  Freq_Set fs = tags.get_nominal_freqs();
  for (Freq_Set :: iterator it = fs.begin(); it != fs.end(); ++it) {
    Tag_Finder_Key key(0, *it);
    std::string prefix="p";
    Tag_Finder *newtf;
    port_freq[0] = Freq_Setting(*it / 1000.0);
    if (max_pulse_rate > 0)
      newtf = new Rate_Limiting_Tag_Finder(this, key.second, tags.get_tags_at_freq(key.second), pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix);
    else
      newtf = new Tag_Finder(this, key.second, tags.get_tags_at_freq(key.second), prefix);
    newtf->init();
  }
}
    
Tag_Foray::~Tag_Foray () {
  for (auto tfi = tag_finders.begin(); tfi != tag_finders.end(); ++tfi)
    delete (tfi->second);
}
  



