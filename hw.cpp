#include "hw.h"

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // ADC: Pot on Pin 15
    AdcChannelConfig adc_config;
    adc_config.InitSingle(seed.GetPin(15));
    seed.adc.Init(&adc_config, 1);
    seed.adc.Start();

    // Init Pot with flip=true based on your snippet
    // Note: ensure your libDaisy version supports the 'flip' boolean arg in Init
    pot.Init(seed.adc.GetPtr(0), seed.AudioCallbackRate(), true);

    // Encoder: Pin 1 (A), Pin 28 (B), Pin 2 (Click)
    encoder.Init(seed.GetPin(1), seed.GetPin(28), seed.GetPin(2), seed.AudioCallbackRate());

    // Button: Pin 18
    button.Init(seed.GetPin(18), seed.AudioCallbackRate());
}