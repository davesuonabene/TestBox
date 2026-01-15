#pragma once
#include "daisysp.h"
#include "daisy_seed.h"

// Define the parameters
enum SynthParam {
    PARAM_FREQ,
    PARAM_WAVEFORM,
    PARAM_AMP,
    PARAM_COUNT
};

class Processing {
public:
    void Init(float sample_rate);
    float Process();
    void UpdateControls(int32_t enc_inc, bool button_trig, float knob_val);

    // Getters
    bool IsMuted() const { return is_muted; }
    int GetCurrentParamIndex() const { return current_param; }
    const char* GetParamName(int index);
    float GetParamValue(int index);
    bool IsParamLocked() const { return param_locked; }

private:
    daisysp::Oscillator osc;
    
    // State
    bool is_muted;
    float sample_rate;
    int current_param;
    
    // Synth Values
    float p_freq;
    float p_waveform;
    float p_amp;

    // Latching State
    bool param_locked;
    float lock_reference_val;
    
    // MODIFIED: Threshold increased to 15%
    const float LOCK_THRESHOLD = 0.15f; 
};