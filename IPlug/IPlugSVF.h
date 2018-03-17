#pragma once

/**
 * @file
 * Multi-channel SVF Based on Andy Simper's code:
 * - http://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
 */

#define ISVFMODES_VALIST "LowPass", "HighPass", "BandPass", "Notch", "Peak", "Bell", "LowPassShelf", "HighPassShelf"

template<typename sampleType = double, int NC = 1>
class ISVF
{
public:

  enum EMode
  {
    kLowPass = 0,
    kHighPass,
    kBandPass,
    kNotch,
    kPeak,
    kBell,
    kLowPassShelf,
    kHighPassShelf,
    kNumModes
  };

  ISVF(EMode mode = kLowPass, double freqCPS = 1000.)
  {
    mNewState.mode = mState.mode = mode;
    mNewState.freq = mState.freq = freqCPS;
    UpdateCoefficients();
  }

  void SetFreqCPS(double freqCPS) { mNewState.freq = BOUNDED(freqCPS, 10, 20000.); }
  void SetQ(double Q) { mNewState.Q = BOUNDED(Q, 0.1, 100.); }
  void SetGain(double gainDB) { mNewState.gain = BOUNDED(gainDB, -36, 36.); }
  void SetMode(EMode mode) { mNewState.mode = mode; }
  void SetSampleRate(double sampleRate) { mNewState.sampleRate = sampleRate; }

  void ProcessBlock(sampleType** inputs, sampleType** outputs, int nChans, int nFrames)
  {
    assert(nChans <= NC);

    if(mState != mNewState)
      UpdateCoefficients();

    for (auto c = 0; c < nChans; c++)
    {
      for (auto s = 0; s < nFrames; s++)
      {
        const double v0 = (double) inputs[c][s];

        mV3[c] = v0 - mIc2eq[c];
        mV1[c] = m_a1 * mIc1eq[c] + m_a2*mV3[c];
        mV2[c] = mIc2eq[c] + m_a2 * mIc1eq[c] + m_a3 * mV3[c];
        mIc1eq[c] = 2. * mV1[c] - mIc1eq[c];
        mIc2eq[c] = 2. * mV2[c] - mIc2eq[c];

        outputs[c][s] = (sampleType) m_m0 * v0 + m_m1 * mV1[c] + m_m2 * mV2[c];
      }
    }
  }

  void Reset()
  {
    for (auto c = 0; c < NC; c++)
    {
      mV1[c] = 0.;
      mV2[c] = 0.;
      mV3[c] = 0.;
      mIc1eq[c] = 0.;
      mIc2eq[c] = 0.;
    }
  }

private:
  void UpdateCoefficients()
  {
    mState = mNewState;

    const double w = std::tan(PI * mState.freq/mState.sampleRate);

    switch(mState.mode)
    {
      case kLowPass:
      {
        const double g = w;
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 0;
        m_m1 = 0;
        m_m2 = 1.;
        break;
      }
      case kHighPass:
      {
        const double g = w;
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 1.;
        m_m1 = -k;
        m_m2 = -1.;
        break;
      }
      case kBandPass:
      {
        const double g = w;
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 0.;
        m_m1 = 1.;
        m_m2 = 0.;
        break;
      }
      case kNotch:
      {
        const double g = w;
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 1.;
        m_m1 = -k;
        m_m2 = 0.;
        break;
      }
      case kPeak:
      {
        const double g = w;
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 1.;
        m_m1 = -k;
        m_m2 = -2.;
        break;
      }
      case kBell:
      {
        const double A = std::pow(10., mState.gain/40.);
        const double g = w;
        const double k = 1 / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 1.;
        m_m1 = k * (A * A - 1.);
        m_m2 = 0.;
        break;
      }
      case kLowPassShelf:
      {
        const double A = std::pow(10., mState.gain/40.);
        const double g = w / std::sqrt(A);
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = 1.;
        m_m1 = k * (A - 1.);
        m_m2 = (A * A - 1.);
        break;
      }
      case kHighPassShelf:
      {
        const double A = std::pow(10., mState.gain/40.);
        const double g = w / std::sqrt(A);
        const double k = 1. / mState.Q;
        m_a1 = 1./(1. + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
        m_m0 = A*A;
        m_m1 = k*(1. - A)*A;
        m_m2 = (1. - A*A);
        break;
      }
      default:
        break;
    }
  }

private:
  double mV1[NC] = {};
  double mV2[NC] = {};
  double mV3[NC] = {};
  double mIc1eq[NC] = {};
  double mIc2eq[NC] = {};
  double m_a1 = 0.;
  double m_a2 = 0.;
  double m_a3 = 0.;
  double m_m0 = 0.;
  double m_m1 = 0.;
  double m_m2 = 0.;

  struct Settings
  {
    EMode mode;
    double freq = 1000.;
    double Q = 0.1;
    double gain = 1.;
    double sampleRate = 44100.;

    bool operator != (const Settings &other) const 
    {
      return !(mode == other.mode && freq == other.freq && Q == other.Q && gain == other.gain && sampleRate == other.sampleRate);
    }
  };

  Settings mState, mNewState;
};
