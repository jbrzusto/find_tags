#ifndef BURST_PARAMS_H
#define BURST_PARAMS_H

struct Burst_Params {
  float freq;
  float freq_sd;
  float sig;
  float sig_sd;
  float noise;
  float slop;
  float burst_slop;
  int	num_pred;		// number of consecutive hits on same ID preceding this one
};

#endif // BURST_PARAMS_H
