#pragma once
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h" // Using the working library driver
#include "processing.h"

using namespace daisy;

using OledDriver = daisy::SSD130xI2c128x64Driver;

enum UiAction {
    ACT_NONE, // Startup state
    ACT_ENC,
    ACT_KNOB,
    ACT_BTN
};

struct Screen
{
    void Init(daisy::DaisySeed &seed);
    void DrawStatus(Processing& proc, UiAction last_action, uint32_t time_since_act);
};