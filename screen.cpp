#include "screen.h"
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace daisy;

static OledDisplay<OledDriver> display;

// --- ROTATION HELPERS ---
static void DrawPixelRot180(OledDisplay<OledDriver> &disp, int x, int y, bool on)
{
    int rx = disp.Width() - 1 - x;
    int ry = disp.Height() - 1 - y;
    if(rx >= 0 && ry >= 0 && rx < (int)disp.Width() && ry < (int)disp.Height())
    {
        disp.DrawPixel(rx, ry, on);
    }
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
                    if((row << j) & 0x8000)
                    {
                        DrawPixelRot180(disp, cx + j, y + i, on);
                    }
                }
            }
        }
        cx += font.FontWidth;
        ++str;
    }
}

// --- MORPHING WAVEFORM LOGIC ---

static float GetBaseSample(float phase, int wave_type)
{
    switch(wave_type) {
        case 0: return sinf(phase * TWOPI_F); // Sine
        case 1: return 1.0f - fabsf(fmodf(phase * 4.0f, 4.0f) - 2.0f); // Tri
        case 2: return 2.0f * (phase - floorf(phase + 0.5f)); // Saw
        case 3: return (phase < 0.5f) ? 0.8f : -0.8f; // Square
        default: return 0.0f;
    }
}

static float GetMorphSample(float phase, float morph_0_3)
{
    int idx_a = (int)morph_0_3;
    int idx_b = idx_a + 1;
    if (idx_a >= 3) { idx_a = 3; idx_b = 3; }
    
    float frac = morph_0_3 - (float)idx_a;
    
    float samp_a = GetBaseSample(phase, idx_a);
    float samp_b = GetBaseSample(phase, idx_b);
    
    return samp_a * (1.0f - frac) + samp_b * frac;
}

static void DrawWaveform(OledDisplay<OledDriver> &disp, int x, int y, int w, int h, float freq_01, float wave_01, float amp_01)
{
    float morph_val = wave_01 * 3.0f; 
    float density = 1.0f + (freq_01 * 3.5f); 
    
    float height_px = 0.0f;
    if (amp_01 > 0.001f) {
        height_px = 2.0f + (amp_01 * (h / 2.0f - 2.0f));
    }

    int mid_y = y + h / 2;
    int last_py = mid_y;
    
    for(int i = 0; i < w; i++)
    {
        float t = (float)i / (float)w;
        float phase = t * density;
        phase = phase - floorf(phase); 

        float sample = GetMorphSample(phase, morph_val);
        int py = mid_y - (int)(sample * height_px);
        
        if(py < y) py = y;
        if(py >= y + h) py = y + h - 1;

        DrawPixelRot180(disp, x + i, py, true);
        
        if(height_px > 1.0f && abs(py - last_py) > 1) {
            int dir = (py > last_py) ? 1 : -1;
            for(int k = last_py; k != py; k += dir) {
                 DrawPixelRot180(disp, x + i, k, true);
            }
        }
        last_py = py;
    }
}

// --- MAIN CLASS ---

void Screen::Init(DaisySeed &seed)
{
    OledDisplay<OledDriver>::Config disp_cfg;
    
    // Config from BlackBox
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
    
    // --- 1. HEADER ---
    // Only show Mute status, otherwise empty (clean look)
    if(proc.IsMuted()) {
        DrawStringRot180(display, 0, 0, "[MUTE]", Font_7x10, true);
    }

    // Param Number Top Right
    char p_str[10];
    snprintf(p_str, sizeof(p_str), "%d", proc.GetCurrentParamIndex());
    DrawStringRot180(display, 115, 0, p_str, Font_7x10, true);

    // --- 2. WAVEFORM ---
    float v_freq = proc.GetParamValue(PARAM_FREQ);
    float v_wave = proc.GetParamValue(PARAM_WAVEFORM);
    float v_amp  = proc.GetParamValue(PARAM_AMP);

    if(proc.IsMuted()) v_amp = 0.0f;

    DrawWaveform(display, 0, 15, 128, 35, v_freq, v_wave, v_amp);

    // --- 3. TIPS FOOTER ---
    const char* tip = "";
    
    // Logic: 
    // 1. Mute overrides everything.
    // 2. ACT_NONE (Startup) or > 5000ms idle -> "Touch me pls"
    // 3. Otherwise show specific action tip.

    if (proc.IsMuted()) {
        tip = "Press btn to unmute";
    }
    else if (last_action == ACT_NONE || time_since_act > 5000) { 
        tip = "Touch me pls";
    }
    else {
        switch(last_action) {
            case ACT_ENC:
                tip = "Select Param";
                break;
            case ACT_KNOB:
                if(proc.IsParamLocked()) tip = "Unlock -> Wiggle";
                else tip = "Changing Value";
                break;
            case ACT_BTN:
                tip = "Mute Toggled";
                break;
            default:
                break;
        }
    }

    DrawStringRot180(display, 0, 54, tip, Font_6x8, true);

    display.Update();
}