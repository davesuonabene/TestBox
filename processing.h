#pragma once
#include "daisysp.h"
#include "daisy_seed.h"
#include <cmath>
#include <cstdlib>

// --- HELPER: One-Pole LowPass Filter (Replaces daisysp::Tone) ---
struct SimpleLPF {
    float val;
    float coeff;
    
    void Init(float sample_rate) {
        val = 0.0f;
        SetFreq(sample_rate, 5000.0f); // Default
    }
    
    void SetFreq(float sample_rate, float freq) {
        // Standard one-pole coefficient calculation
        // coeff = 1 - exp(-2 * PI * freq / sr)
        coeff = 1.0f - expf(-TWOPI_F * freq / sample_rate);
    }
    
    float Process(float in) {
        val += coeff * (in - val);
        return val;
    }
};

// --- CUSTOM FREEVERB (Modulated) ---
class NiceReverb {
public:
    void Init(float sample_rate) {
        combs_l[0].Init(); combs_l[1].Init(); combs_l[2].Init(); combs_l[3].Init();
        combs_l[4].Init(); combs_l[5].Init(); combs_l[6].Init(); combs_l[7].Init();
        ap_l[0].Init();    ap_l[1].Init();    ap_l[2].Init();    ap_l[3].Init();

        combs_r[0].Init(); combs_r[1].Init(); combs_r[2].Init(); combs_r[3].Init();
        combs_r[4].Init(); combs_r[5].Init(); combs_r[6].Init(); combs_r[7].Init();
        ap_r[0].Init();    ap_r[1].Init();    ap_r[2].Init();    ap_r[3].Init();

        for(int i=0; i<8; i++) { damp_l[i] = 0.0f; damp_r[i] = 0.0f; }
        
        mod_lfo.Init(sample_rate);
        mod_lfo.SetWaveform(daisysp::Oscillator::WAVE_SIN);
        mod_lfo.SetFreq(0.3f);
        mod_lfo.SetAmp(1.0f);
    }

    void Process(float in, float amt, float length, float tone, float& outL, float& outR) {
        if(amt < 0.01f) { outL = in; outR = in; return; }

        float feedback = 0.7f + (length * 0.28f);
        float damping  = 0.0f + ((1.0f - tone) * 0.4f);
        
        float mod = mod_lfo.Process(); 
        int mod_offset = (int)(mod * 15.0f * amt); 

        float wet_l = 0.0f; float wet_r = 0.0f;
        int tunes[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        
        for(int i=0; i<8; i++) {
            int t_l = tunes[i] + (i % 2 == 0 ? mod_offset : -mod_offset);
            int t_r = (tunes[i] + 23) + (i % 2 == 0 ? -mod_offset : mod_offset);
            wet_l += ProcessComb(combs_l[i], damp_l[i], in, feedback, damping, t_l);
            wet_r += ProcessComb(combs_r[i], damp_r[i], in, feedback, damping, t_r);
        }

        int ap_tunes[] = {225, 341, 441, 556};
        for(int i=0; i<4; i++) {
            wet_l = ProcessAllPass(ap_l[i], wet_l, ap_tunes[i]);
            wet_r = ProcessAllPass(ap_r[i], wet_r, ap_tunes[i] + 23);
        }

        outL = in * (1.0f - amt * 0.5f) + wet_l * amt * 0.015f;
        outR = in * (1.0f - amt * 0.5f) + wet_r * amt * 0.015f;
    }

private:
    float ProcessComb(daisysp::DelayLine<float, 1750>& dl, float& history, float in, float fb, float damp, int delay) {
        float output = dl.Read();
        history = output * (1.0f - damp) + history * damp;
        dl.Write(in + history * fb);
        
        // Safety clamp and indentation fix
        if(delay < 10) delay = 10; 
        if(delay > 1740) delay = 1740;
        
        dl.SetDelay((size_t)delay); 
        return output;
    }
    
    float ProcessAllPass(daisysp::DelayLine<float, 600>& dl, float in, int delay) {
        float read = dl.Read();
        float write = in + (read * 0.5f);
        dl.Write(write);
        dl.SetDelay((size_t)delay);
        return read - (write * 0.5f);
    }

    daisysp::DelayLine<float, 1750> combs_l[8];
    daisysp::DelayLine<float, 1750> combs_r[8];
    daisysp::DelayLine<float, 600>  ap_l[4];
    daisysp::DelayLine<float, 600>  ap_r[4];
    float damp_l[8]; float damp_r[8];
    daisysp::Oscillator mod_lfo;
};

enum SynthParam {
    PARAM_FREQ,
    PARAM_WAVEFORM,
    PARAM_AMP,
    PARAM_FILTER,
    PARAM_DIST,
    PARAM_PHASER,
    PARAM_DETUNE,
    PARAM_REV_AMT,
    PARAM_REV_LEN,
    PARAM_REV_TONE,
    PARAM_WOB_AMT,
    PARAM_WOB_SPD,
    PARAM_SWEEP_AMT, 
    PARAM_SWEEP_RATE, 
    PARAM_COUNT
};

class Processing {
public:
    void Init(float sample_rate);
    void Process(float &outL, float &outR);
    void UpdateControls(int32_t enc_inc, bool button_trig, float knob_val);
    void Randomize();
    void Reset();

    bool IsMuted() const { return is_muted; }
    int GetCurrentParamIndex() const { return current_param; }
    const char* GetParamName(int index);
    float GetParamValue(int index);
    bool IsParamLocked() const { return param_locked; }

private:
    daisysp::Oscillator osc_a_l, osc_b_l;
    daisysp::Oscillator osc_a_r, osc_b_r;
    daisysp::Oscillator lfo; // Wobble
    daisysp::Oscillator sweep_lfo; // Slow Sweep

    daisysp::Svf    filt_l, filt_r;
    daisysp::Phaser phaser_l, phaser_r;
    daisysp::Overdrive drive_l, drive_r;
    
    // Changed: Using local SimpleLPF instead of daisysp::Tone
    SimpleLPF fixed_lpf_l;
    SimpleLPF fixed_lpf_r;

    NiceReverb reverb; 

    bool is_muted;
    float sample_rate;
    int current_param;
    
    float p_freq;
    float p_waveform;
    float p_amp;
    float p_filter; 
    float p_dist;
    float p_phaser;
    float p_detune;
    float p_rev_amt;
    float p_rev_len;
    float p_rev_tone;
    float p_wob_amt;
    float p_wob_spd;
    float p_sweep_amt;
    float p_sweep_rate;

    bool param_locked;
    float lock_reference_val;
    const float LOCK_THRESHOLD = 0.15f; 
    
    inline float SoftLimit(float x) {
        if(x > 0.9f) return 0.9f + (x - 0.9f) * 0.1f;
        if(x < -0.9f) return -0.9f + (x + 0.9f) * 0.1f;
        return x;
    }
};