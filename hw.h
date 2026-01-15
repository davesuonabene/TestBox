#pragma once
#include "daisy_seed.h"

using namespace daisy;

class Hardware {
public:
    // Core Seed Object
    DaisySeed seed;

    // Components
    Encoder encoder;
    Switch button;
    AnalogControl pot;

    // Helper variable used in your snippet
    float sample_rate;

    void Init();
};