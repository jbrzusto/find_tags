#include "Tag_Foray.hpp"

#include <string.h>

Tag_Foray::Tag_Foray (Tag_Database &tags, std::istream *data, std::ostream *out, Frequency_MHz default_freq, float max_dfreq,  float max_pulse_rate, Gap pulse_rate_window, Gap min_bogus_spacing) :
  tags(tags),
  data(data),
  out(out),
  default_freq(default_freq),
  max_dfreq(max_dfreq),
  max_pulse_rate(max_pulse_rate),
  pulse_rate_window(pulse_rate_window),
  min_bogus_spacing(min_bogus_spacing),
  line_no(0)
{
  
};


void
Tag_Foray::start() {
  double ts;
  for (;;) {
      // read and parse a line from a SensorGnome file

      char buf[MAX_LINE_SIZE + 1];
      if (! data->getline(buf, MAX_LINE_SIZE)) {
        if (data->eof())
          break;
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

          if (max_dfreq > 0.0 && fabs(dfreq) > max_dfreq)
            continue;

          if (! port_freq.count(port_num))
            port_freq[port_num] = Freq_Setting(default_freq);

          Tag_Finder_Key key(port_num, port_freq[port_num].f_kHz);

          if (! tag_finders.count(key)) {
            Tag_Finder *newtf;
            std::ostringstream prefix;
            prefix << port_num << ",";
            if (max_pulse_rate > 0)
              newtf = new Rate_Limiting_Tag_Finder(key.second, tags.get_tags_at_freq(key.second), pulse_rate_window, max_pulse_rate, min_bogus_spacing, prefix.str());
            else
              newtf = new Tag_Finder(key.second, tags.get_tags_at_freq(key.second), prefix.str());
            newtf->set_out_stream(out);
            newtf->init();
            tag_finders[key] = newtf;
          };

          Pulse p = Pulse::make(ts, dfreq, sig, noise, port_freq[port_num].f_MHz);
							       
          tag_finders[key]->process(p);
        }
        break;
      default:
        break;
      }
    }

    // dump any remaining candidates (FIXME: option this once we have resume capability)
    for (auto tfi = tag_finders.begin(); tfi != tag_finders.end(); ++tfi)
      (tfi->second)->end_processing();
};




