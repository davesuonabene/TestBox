#include "daisy_seed.h"
#include "daisysp.h"
#include "hw.h"
#include "processing.h"
#include "screen.h"

using namespace daisy;
using namespace daisysp;

Hardware hw;
Processing engine;
Screen screen;

// Interrupt shared data
volatile int32_t encoder_presses = 0;
volatile bool    button_pressed = false;
volatile float   pot_value = 0.0f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.encoder.Debounce();
    hw.button.Debounce();
    
    encoder_presses += hw.encoder.Increment();

    static uint32_t last_btn_time = 0;
    if(hw.button.RisingEdge())
    {
        uint32_t now = System::GetNow();
        if(now - last_btn_time > 200) 
        {
            button_pressed = true;
            last_btn_time = now;
        }
    }
    
    pot_value = hw.pot.Process();

    for(size_t i = 0; i < size; i++)
    {
        float sig = engine.Process();
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

int main(void)
{
    hw.Init();
    screen.Init(hw.seed);
    engine.Init(hw.sample_rate);
    hw.seed.StartAudio(AudioCallback);

    // Activity Tracking
    uint32_t last_ui_update = 0;
    
    // Start with ACT_NONE so "Touch me pls" shows up immediately
    UiAction last_action = ACT_NONE;
    uint32_t last_action_time = System::GetNow();
    float    last_pot_stored = 0.0f;

    while(1)
    {
        // 1. Fetch Data
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        
        int32_t inc = encoder_presses;
        encoder_presses = 0; 
        
        bool btn = button_pressed;
        button_pressed = false;
        
        float pot = pot_value;
        
        if (!primask) __enable_irq();

        // 2. Activity Detection
        uint32_t now = System::GetNow();

        if (btn) {
            last_action = ACT_BTN;
            last_action_time = now;
        }
        else if (inc != 0) {
            last_action = ACT_ENC;
            last_action_time = now;
        }
        else if (fabs(pot - last_pot_stored) > 0.01f) {
            last_action = ACT_KNOB;
            last_action_time = now;
            last_pot_stored = pot;
        }

        // 3. Engine Update
        engine.UpdateControls(inc, btn, pot);

        // 4. Draw Screen
        if(now - last_ui_update > 33)
        {
            last_ui_update = now;
            uint32_t idle_ms = now - last_action_time;
            screen.DrawStatus(engine, last_action, idle_ms);
        }
    }
}