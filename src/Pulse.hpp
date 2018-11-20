#ifndef PULSE_HPP
#define PULSE_HPP

#include "find_tags_common.hpp"

#include <map>

struct Pulse {

  // the findpulsesfdbatch vamp plugin outputs data in text format
  // with all these fields except seq_no.  We use the latter here to
  // make it cheaper to refer to particular pulses (as when we kill
  // all DFAs using any pulses from a just-recognized burst).

  typedef long long Seq_No;

public:

  // data from the pulse detector

  double		ts;		// timestamp, in seconds past the Origin (to a precision of 0.0001 seconds)
  Frequency_Offset_kHz  dfreq;		// estimate of pulse frequency offset, in kHz
  Frequency_MHz         ant_freq;	// frequency to which receiving antenna was tuned
  float		        sig;		// estimate of pulse strength, in dB max
  float			noise;		// estimate of noise surrounding pulse, in dB max

  // additional parameters for algorithmic use

  Seq_No	seq_no;     

private:
  Pulse(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq);

public:
  Pulse(){};

  static Pulse make(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq);

  void dump();
};

typedef std::map < Pulse::Seq_No, Pulse > Pulse_Buffer;
typedef Pulse_Buffer :: iterator Pulses_Iter;

#endif // PULSE_HPP
