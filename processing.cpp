#include "processing.h"
#include <cmath>

void Processing::Init(float sr)
{
    sample_rate = sr;
    
    // defaults
    p_freq = 440.0f;
    p_waveform = 0.0f; // Sine
    p_amp = 0.5f;
    is_muted = false;
    current_param = PARAM_FREQ;

    // Latching init
    param_locked = false;
    lock_reference_val = 0.0f;

    // Osc Setup
    osc.Init(sample_rate);
    osc.SetFreq(p_freq);
    osc.SetAmp(p_amp);
    osc.SetWaveform(daisysp::Oscillator::WAVE_SIN);
}

float Processing::Process()
{
    if (is_muted) return 0.0f;
    
    // Smooth parameters could be added here, 
    // strictly setting them in UpdateControls for simplicity.
    return osc.Process();
}

void Processing::UpdateControls(int32_t enc_inc, bool button_trig, float knob_val)
{
    // 1. Handle Mute Toggle
    if (button_trig) {
        is_muted = !is_muted;
    }

    // 2. Handle Encoder (Parameter Selection)
    if (enc_inc != 0) {
        // change param
        current_param += enc_inc;
        
        // Wrap logic
        if (current_param < 0) current_param = PARAM_COUNT - 1;
        else if (current_param >= PARAM_COUNT) current_param = 0;

        // ENGAGE LOCK: We just switched params, ignore knob until it moves 10%
        param_locked = true;
        lock_reference_val = knob_val; 
    }

    // 3. Handle Knob (Value Change with Latch)
    if (param_locked) {
        // Check if user moved knob enough to "catch" the value
        if (fabs(knob_val - lock_reference_val) > LOCK_THRESHOLD) {
            param_locked = false; // Unlock!
        }
    }

    // Only update actual DSP if unlocked
    if (!param_locked) {
        switch (current_param) {
            case PARAM_FREQ:
                // Map 0-1 to 50Hz - 2000Hz exponential
                p_freq = 50.0f * powf(10.0f, knob_val * 2.6f); // approx range
                osc.SetFreq(p_freq);
                break;
            case PARAM_WAVEFORM:
                p_waveform = knob_val;
                // Map 0.0-1.0 to 0-4 integer
                // SIN, TRI, SAW, RAMP, SQUARE
                osc.SetWaveform((uint8_t)(p_waveform * 4.9f)); 
                break;
            case PARAM_AMP:
                p_amp = knob_val;
                osc.SetAmp(p_amp);
                break;
        }
    }
}

const char* Processing::GetParamName(int index) {
    switch(index) {
        case PARAM_FREQ: return "Freq";
        case PARAM_WAVEFORM: return "Wave";
        case PARAM_AMP: return "Amp";
        default: return "Unknown";
    }
}

float Processing::GetParamValue(int index) {
    // Return normalized 0-1 for display bars
    switch(index) {
        case PARAM_FREQ: return (log10f(p_freq/50.0f) / 2.6f); // inverse mapping
        case PARAM_WAVEFORM: return p_waveform;
        case PARAM_AMP: return p_amp;
        default: return 0.0f;
    }
}