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

    float L, R;
    for(size_t i = 0; i < size; i++)
    {
        engine.Process(L, R);
        out[0][i] = L;
        out[1][i] = R;
    }
}

int main(void)
{
    hw.Init();
    screen.Init(hw.seed);
    engine.Init(hw.sample_rate);
    hw.seed.StartAudio(AudioCallback);

    uint32_t last_ui_update = 0;
    UiAction last_action = ACT_NONE;
    uint32_t last_action_time = System::GetNow();
    float    last_pot_stored = 0.0f;

    uint32_t enc_hold_start = 0; bool enc_hold_fired = false;
    uint32_t btn_hold_start = 0; bool btn_hold_fired = false;

    while(1)
    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        int32_t inc = encoder_presses; encoder_presses = 0; 
        bool btn = button_pressed; button_pressed = false;
        float pot = pot_value;
        if (!primask) __enable_irq();

        uint32_t now = System::GetNow();

        if (btn) { last_action = ACT_BTN; last_action_time = now; }
        else if (inc != 0) { last_action = ACT_ENC; last_action_time = now; }
        else if (fabs(pot - last_pot_stored) > 0.01f) { last_action = ACT_KNOB; last_action_time = now; last_pot_stored = pot; }

        // HOLD ENCODER -> RANDOMIZE
        if (hw.encoder.Pressed()) {
            if (enc_hold_start == 0) enc_hold_start = now;
            else if ((now - enc_hold_start > 1000) && !enc_hold_fired) {
                engine.Randomize();
                enc_hold_fired = true;
                last_action = ACT_ENC; last_action_time = now;
            }
        } else { enc_hold_start = 0; enc_hold_fired = false; }

        // HOLD BUTTON -> RESET
        if (hw.button.Pressed()) {
            if (btn_hold_start == 0) btn_hold_start = now;
            else if ((now - btn_hold_start > 1000) && !btn_hold_fired) {
                engine.Reset();
                btn_hold_fired = true;
                last_action = ACT_BTN; last_action_time = now;
            }
        } else { btn_hold_start = 0; btn_hold_fired = false; }

        // IDLE -> RANDOMIZE (Self Gen)
        static bool idle_random_done = false;
        if (now - last_action_time > 20000) {
            if (!idle_random_done) { 
                engine.Randomize(); 
                idle_random_done = true; 
            }
        } else { 
            idle_random_done = false; 
        }

        engine.UpdateControls(inc, btn, pot);

        if(now - last_ui_update > 33) {
            last_ui_update = now;
            screen.DrawStatus(engine, last_action, now - last_action_time);
        }
    }
}