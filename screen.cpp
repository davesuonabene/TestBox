#include "screen.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>

using namespace daisy;

static OledDisplay<OledDriver> display;

// --- HELPERS ---
static void DrawPixelRot180(OledDisplay<OledDriver> &disp, int x, int y, bool on)
{
    int rx = disp.Width() - 1 - x;
    int ry = disp.Height() - 1 - y;
    if(rx >= 0 && ry >= 0 && rx < (int)disp.Width() && ry < (int)disp.Height())
        disp.DrawPixel(rx, ry, on);
}

static void DrawStringRot180(OledDisplay<OledDriver> &disp, int x, int y, const char *str, const FontDef &font, bool on)
{
    int cx = x;
    while(*str)
    {
        if(*str >= 32 && *str <= 126)
        {
            for(int i = 0; i < (int)font.FontHeight; i++)
            {
                uint32_t row = font.data[(*str - 32) * font.FontHeight + i];
                for(int j = 0; j < (int)font.FontWidth; j++)
                {
                    if((row << j) & 0x8000) DrawPixelRot180(disp, cx + j, y + i, on);
                }
            }
        }
        cx += font.FontWidth;
        ++str;
    }
}

// --- WAVEFORM LOGIC ---

static float GetBaseSample(float phase, int wave_type)
{
    switch(wave_type) {
        case 0: return sinf(phase * TWOPI_F);
        case 1: return 1.0f - fabsf(fmodf(phase * 4.0f, 4.0f) - 2.0f);
        case 2: return 2.0f * (phase - floorf(phase + 0.5f));
        case 3: return (phase < 0.5f) ? 0.8f : -0.8f;
        default: return 0.0f;
    }
}

static float GetMorphSample(float phase, float morph_0_3)
{
    int idx_a = (int)morph_0_3;
    int idx_b = idx_a + 1;
    if (idx_a >= 3) { idx_a = 3; idx_b = 3; }
    float frac = morph_0_3 - (float)idx_a;
    return GetBaseSample(phase, idx_a) * (1.0f - frac) + GetBaseSample(phase, idx_b) * frac;
}

// --- UNIFIED VISUALIZER ---

static void DrawUnifiedWaveform(OledDisplay<OledDriver> &disp, int x, int y, int w, int h, 
                                float freq, float wave, float amp, 
                                float dist, float detune, float phaser, 
                                float reverb, float wobble)
{
    int mid_y = y + h / 2;
    
    // 1. WOBBLE VISUALS
    // Modulate density to make the wave "breathe" (expand/contract)
    float time_sec = System::GetNow() / 1000.0f;
    // Wobble rate ~3Hz visual, depth scaled by parameter
    float wob_mod = sinf(time_sec * 3.0f * TWOPI_F) * (wobble * 0.3f); 
    
    float density = 1.0f + (freq * 3.5f);
    density *= (1.0f + wob_mod); // Apply wobble breathing

    float morph_val = wave * 3.0f;

    // 2. DETUNE LOOP
    int passes = (detune > 0.01f) ? 2 : 1;
    for(int p = 0; p < passes; p++)
    {
        float phase_offset = (p == 1) ? (detune * 0.2f) : 0.0f;
        int last_py = mid_y;

        for(int i = 0; i < w; i++)
        {
            float t = (float)i / (float)w;
            float phase = t * density + phase_offset;
            
            // Phaser Visual (Warping)
            if(phaser > 0.01f) phase += sinf(t * TWOPI_F * 2.0f) * (phaser * 0.2f);
            phase = phase - floorf(phase);

            // Morph Oscillator
            float val = GetMorphSample(phase, morph_val);

            // Distortion Visual (Clipping)
            if(dist > 0.01f) {
                float limit = 1.0f - (dist * 0.6f);
                if (val > limit) val = limit;
                if (val < -limit) val = -limit;
                val /= limit; 
            }

            // Amplitude Scale
            float effective_h = (amp < 0.01f) ? 0.0f : (amp * (h / 2.0f - 2.0f));
            int py = mid_y - (int)(val * effective_h);
            
            // Clamp
            if(py < y) py = y; 
            if(py >= y + h) py = y + h - 1;

            // --- REVERB VISUALS (Halo around the wave) ---
            if(reverb > 0.05f) {
                // Determine scatter amount
                int scatter = (int)(reverb * 8.0f); 
                // Draw random points near the main line
                if((rand() % 10) < (reverb * 10.0f)) { // Density probability
                    int rx = i + (rand() % (scatter*2 + 1)) - scatter;
                    int ry = py + (rand() % (scatter*2 + 1)) - scatter;
                    if(rx >= 0 && rx < w && ry >= y && ry < y+h) {
                         DrawPixelRot180(disp, x + rx, ry, true);
                    }
                }
            }

            // Draw Main Line
            DrawPixelRot180(disp, x + i, py, true);
            
            // Connect lines
            if(abs(py - last_py) > 1) {
                int dir = (py > last_py) ? 1 : -1;
                for(int k = last_py; k != py; k += dir) {
                    DrawPixelRot180(disp, x + i, k, true);
                    
                    // Add Reverb Halo to vertical segments too
                    if(reverb > 0.05f && (rand() % 10) < (reverb * 5.0f)) {
                         int rx = i + (rand() % 5) - 2;
                         int ry = k + (rand() % 5) - 2;
                         if(rx >= 0 && rx < w && ry >= y && ry < y+h) 
                             DrawPixelRot180(disp, x + rx, ry, true);
                    }
                }
            }
            last_py = py;
        }
    }
}

static void DrawFilterCurve(OledDisplay<OledDriver> &disp, int x, int y, int w, int h, float val)
{
    for(int i=0; i<w; i++) DrawPixelRot180(disp, x+i, y+h-1, true);
    int last_py = y + h - 1;

    for(int i=0; i<w; i++)
    {
        float t = (float)i / (float)w; 
        float response = 1.0f;
        if (val < 0.45f) {
            float cutoff = val / 0.45f;
            if (t > cutoff) response = 1.0f - (t - cutoff) * 8.0f;
        } 
        else if (val > 0.55f) {
            float cutoff = (val - 0.55f) / 0.45f;
            if (t < cutoff) response = 1.0f - (cutoff - t) * 8.0f;
        }
        if(response < 0.0f) response = 0.0f;
        int py = (y + h - 1) - (int)(response * (h - 4));
        DrawPixelRot180(disp, x + i, py, true);
        if(i > 0 && abs(py - last_py) > 1) {
            int dir = (py > last_py) ? 1 : -1;
            for(int k = last_py; k != py; k += dir) DrawPixelRot180(disp, x+i, k, true);
        }
        last_py = py;
    }
}

void Screen::Init(DaisySeed &seed)
{
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.mode   = I2CHandle::Config::Mode::I2C_MASTER;
    disp_cfg.driver_config.transport_config.i2c_config.speed  = I2CHandle::Config::Speed::I2C_1MHZ;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed.GetPin(11);
    disp_cfg.driver_config.transport_config.i2c_address = 0x3C;
    display.Init(disp_cfg);
    display.Fill(false);
    display.Update();
}

void Screen::DrawStatus(Processing& proc, UiAction last_action, uint32_t time_since_act)
{
    display.Fill(false);
    
    // Header
    if(proc.IsMuted()) {
        DrawStringRot180(display, 0, 0, "[MUTE]", Font_7x10, true);
    } else {
        DrawStringRot180(display, 0, 0, proc.GetParamName(proc.GetCurrentParamIndex()), Font_7x10, true);
    }

    int p_idx = proc.GetCurrentParamIndex();
    
    if (proc.IsMuted()) {
        // Flat line
        DrawUnifiedWaveform(display, 0, 15, 128, 35, 0, 0, 0.0f, 0, 0, 0, 0, 0);
    }
    else if (p_idx == PARAM_FILTER) {
        DrawFilterCurve(display, 0, 15, 128, 35, proc.GetParamValue(PARAM_FILTER));
    }
    else {
        DrawUnifiedWaveform(display, 0, 15, 128, 35, 
            proc.GetParamValue(PARAM_FREQ),
            proc.GetParamValue(PARAM_WAVEFORM),
            proc.GetParamValue(PARAM_AMP),
            proc.GetParamValue(PARAM_DIST),
            proc.GetParamValue(PARAM_DETUNE),
            proc.GetParamValue(PARAM_PHASER),
            proc.GetParamValue(PARAM_REV_AMT),
            proc.GetParamValue(PARAM_WOB_AMT) // Passing Wobble here
        );
    }

    char tip[32] = "";
    if (proc.IsMuted()) snprintf(tip, sizeof(tip), "Press btn to unmute");
    else if (last_action == ACT_NONE || time_since_act > 5000) snprintf(tip, sizeof(tip), "Touch me pls");
    else if (last_action == ACT_ENC) snprintf(tip, sizeof(tip), "Select Param");
    else if (last_action == ACT_KNOB) {
         if(proc.IsParamLocked()) snprintf(tip, sizeof(tip), "Unlock -> Wiggle");
         else snprintf(tip, sizeof(tip), "Changing %s", proc.GetParamName(p_idx));
    }
    else if (last_action == ACT_BTN) snprintf(tip, sizeof(tip), "Mute Toggled");

    DrawStringRot180(display, 0, 54, tip, Font_6x8, true);
    display.Update();
}