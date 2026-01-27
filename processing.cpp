#include "processing.h"
#include <cmath>
#include <cstdio>

void Processing::Init(float sr)
{
    sample_rate = sr;
    
    osc_a_l.Init(sample_rate); osc_b_l.Init(sample_rate);
    osc_a_r.Init(sample_rate); osc_b_r.Init(sample_rate);
    for(auto* o : {&osc_a_l, &osc_b_l, &osc_a_r, &osc_b_r}) o->SetAmp(1.0f);

    lfo.Init(sample_rate);
    lfo.SetWaveform(daisysp::Oscillator::WAVE_SIN);

    sweep_lfo.Init(sample_rate);
    sweep_lfo.SetWaveform(daisysp::Oscillator::WAVE_TRI);
    sweep_lfo.SetAmp(1.0f);

    filt_l.Init(sample_rate);   filt_r.Init(sample_rate);
    phaser_l.Init(sample_rate); phaser_r.Init(sample_rate);
    drive_l.Init();             drive_r.Init();
    
    // Fixed Dampening (7kHz) - Custom LPF Init
    fixed_lpf_l.Init(sample_rate); 
    fixed_lpf_r.Init(sample_rate);
    fixed_lpf_l.SetFreq(sample_rate, 7000.0f);
    fixed_lpf_r.SetFreq(sample_rate, 7000.0f);

    reverb.Init(sample_rate);

    Reset();
}

void Processing::Reset()
{
    p_freq = 110.0f;
    p_waveform = 0.0f; 
    p_amp = 0.5f;
    p_filter = 0.5f;
    p_dist = 0.0f;
    p_phaser = 0.0f;
    p_detune = 0.0f;
    p_rev_amt = 0.0f;
    p_rev_len = 0.5f;
    p_rev_tone = 0.8f;
    p_wob_amt = 0.0f;
    p_wob_spd = 0.5f;
    p_sweep_amt = 0.0f;
    p_sweep_rate = 0.2f;

    is_muted = false;
    current_param = PARAM_FREQ;
    param_locked = false;
}

void Processing::Randomize()
{
    auto rnd = []() { return rand() / (float)RAND_MAX; };

    p_freq      = 55.0f + (rnd() * 2945.0f);
    p_waveform  = rnd();
    p_amp       = 0.3f + (rnd() * 0.4f);
    p_filter    = rnd();
    p_dist      = rnd() * 0.4f;
    p_phaser    = rnd() * 0.5f;
    p_detune    = rnd() * 0.3f;
    p_rev_amt   = rnd() * 0.6f;
    p_rev_len   = rnd();
    p_rev_tone  = rnd();
    p_wob_amt   = rnd() * 0.3f;
    p_wob_spd   = rnd();
    p_sweep_amt = rnd() * 0.5f;
    p_sweep_rate = rnd() * 0.4f;
}

void Processing::Process(float &outL, float &outR)
{
    if (is_muted) { outL = 0.0f; outR = 0.0f; return; }

    // 1. Sweep & Wobble
    sweep_lfo.SetFreq(0.02f + (p_sweep_rate * 0.48f));
    float sweep_val = sweep_lfo.Process(); 
    float sweep_factor = powf(2.0f, sweep_val * p_sweep_amt); 

    lfo.SetFreq(0.1f + (p_wob_spd * 14.9f));
    float wobble = lfo.Process() * (p_freq * 0.2f * p_wob_amt); 

    float base_freq = (p_freq + wobble) * sweep_factor;
    
    float detune_hz = base_freq * 0.05f * p_detune; 
    float freq_l = base_freq - detune_hz;
    float freq_r = base_freq + detune_hz;
    
    if(freq_l < 20.f) freq_l = 20.f; if(freq_r < 20.f) freq_r = 20.f;
    if(freq_l > 12000.f) freq_l = 12000.f; if(freq_r > 12000.f) freq_r = 12000.f;

    osc_a_l.SetFreq(freq_l); osc_b_l.SetFreq(freq_l);
    osc_a_r.SetFreq(freq_r); osc_b_r.SetFreq(freq_r);

    float morph = p_waveform * 3.0f;
    int idx_a = (int)morph;
    int idx_b = idx_a + 1;
    float frac = morph - (float)idx_a;
    if (idx_a >= 3) { idx_a = 3; idx_b = 3; frac = 0.0f; }

    uint8_t waves[] = { daisysp::Oscillator::WAVE_SIN, daisysp::Oscillator::WAVE_TRI, daisysp::Oscillator::WAVE_SAW, daisysp::Oscillator::WAVE_SQUARE };

    osc_a_l.SetWaveform(waves[idx_a]); osc_b_l.SetWaveform(waves[idx_b]);
    osc_a_r.SetWaveform(waves[idx_a]); osc_b_r.SetWaveform(waves[idx_b]);

    float raw_l = osc_a_l.Process() * (1.0f - frac) + osc_b_l.Process() * frac;
    float raw_r = osc_a_r.Process() * (1.0f - frac) + osc_b_r.Process() * frac;

    // 2. FX
    drive_l.SetDrive(0.1f + (p_dist * 0.8f));
    drive_r.SetDrive(0.1f + (p_dist * 0.8f));
    if(p_dist > 0.01f) {
        float dl = drive_l.Process(raw_l);
        float dr = drive_r.Process(raw_r);
        raw_l = raw_l * (1.0f - p_dist) + dl * p_dist;
        raw_r = raw_r * (1.0f - p_dist) + dr * p_dist;
    }

    if(p_phaser > 0.01f) {
        phaser_l.SetLfoDepth(p_phaser); phaser_r.SetLfoDepth(p_phaser);
        phaser_l.SetFreq(0.5f + (p_phaser * 2.0f)); 
        phaser_r.SetFreq(0.4f + (p_phaser * 2.1f));
        raw_l = phaser_l.Process(raw_l); raw_r = phaser_r.Process(raw_r);
    }

    filt_l.SetRes(0.1f); filt_r.SetRes(0.1f);
    if (p_filter < 0.45f) {
        float cutoff = 100.0f + (p_filter / 0.45f) * 10000.0f;
        filt_l.SetFreq(cutoff); filt_r.SetFreq(cutoff);
        filt_l.Process(raw_l); filt_r.Process(raw_r);
        raw_l = filt_l.Low(); raw_r = filt_r.Low();
    }
    else if (p_filter > 0.55f) {
        float norm = (p_filter - 0.55f) / 0.45f;
        float cutoff = 50.0f + (norm * norm) * 8000.0f;
        filt_l.SetFreq(cutoff); filt_r.SetFreq(cutoff);
        filt_l.Process(raw_l); filt_r.Process(raw_r);
        raw_l = filt_l.High(); raw_r = filt_r.High();
    }

    // 3. Fixed High Dampening (7kHz)
    raw_l = fixed_lpf_l.Process(raw_l);
    raw_r = fixed_lpf_r.Process(raw_r);

    // 4. Reverb
    reverb.Process(raw_l, p_rev_amt, p_rev_len, p_rev_tone, raw_l, raw_r);

    // 5. Final Output
    raw_l *= p_amp;
    raw_r *= p_amp;

    outL = SoftLimit(raw_l);
    outR = SoftLimit(raw_r);
}

void Processing::UpdateControls(int32_t enc_inc, bool button_trig, float knob_val)
{
    if (button_trig) is_muted = !is_muted;

    if (enc_inc != 0) {
        current_param += enc_inc;
        if (current_param < 0) current_param = PARAM_COUNT - 1;
        else if (current_param >= PARAM_COUNT) current_param = 0;
        
        param_locked = true;
        lock_reference_val = knob_val; 
    }

    if (param_locked) {
        if (fabs(knob_val - lock_reference_val) > LOCK_THRESHOLD) {
            param_locked = false;
        }
    }

    if (!param_locked) {
        switch (current_param) {
            case PARAM_FREQ:      p_freq = 55.0f * powf(109.0f, knob_val); break;
            case PARAM_WAVEFORM:  p_waveform = knob_val; break;
            case PARAM_AMP:       p_amp = knob_val; break;
            case PARAM_FILTER:    p_filter = knob_val; break;
            case PARAM_DIST:      p_dist = knob_val; break;
            case PARAM_PHASER:    p_phaser = knob_val; break;
            case PARAM_DETUNE:    p_detune = knob_val; break;
            case PARAM_REV_AMT:   p_rev_amt = knob_val; break;
            case PARAM_REV_LEN:   p_rev_len = knob_val; break;
            case PARAM_REV_TONE:  p_rev_tone = knob_val; break;
            case PARAM_WOB_AMT:   p_wob_amt = knob_val; break;
            case PARAM_WOB_SPD:   p_wob_spd = knob_val; break;
            case PARAM_SWEEP_AMT: p_sweep_amt = knob_val; break;
            case PARAM_SWEEP_RATE:p_sweep_rate = knob_val; break;
        }
    }
}

const char* Processing::GetParamName(int index) {
    switch(index) {
        case PARAM_FREQ: return "FREQ";
        case PARAM_WAVEFORM: return "WAVE";
        case PARAM_AMP: return "AMP";
        case PARAM_FILTER: return "FILTER";
        case PARAM_DIST: return "DIST";
        case PARAM_PHASER: return "PHASER";
        case PARAM_DETUNE: return "DETUNE";
        case PARAM_REV_AMT: return "REV AMT";
        case PARAM_REV_LEN: return "REV LEN";
        case PARAM_REV_TONE: return "REV TONE";
        case PARAM_WOB_AMT: return "WOB AMT";
        case PARAM_WOB_SPD: return "WOB SPD";
        case PARAM_SWEEP_AMT: return "SWEEP AMT";
        case PARAM_SWEEP_RATE:return "SWEEP RT";
        default: return "UNKNOWN";
    }
}

float Processing::GetParamValue(int index) {
    switch(index) {
        case PARAM_FREQ: return (log10f(p_freq/55.0f) / log10f(109.0f));
        case PARAM_WAVEFORM: return p_waveform;
        case PARAM_AMP: return p_amp;
        case PARAM_FILTER: return p_filter;
        case PARAM_DIST: return p_dist;
        case PARAM_PHASER: return p_phaser;
        case PARAM_DETUNE: return p_detune;
        case PARAM_REV_AMT: return p_rev_amt;
        case PARAM_REV_LEN: return p_rev_len;
        case PARAM_REV_TONE: return p_rev_tone;
        case PARAM_WOB_AMT: return p_wob_amt;
        case PARAM_WOB_SPD: return p_wob_spd;
        case PARAM_SWEEP_AMT: return p_sweep_amt;
        case PARAM_SWEEP_RATE: return p_sweep_rate;
        default: return 0.0f;
    }
}