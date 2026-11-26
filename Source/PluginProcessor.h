#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "TapeEngine.h"
#include "Licence/SwellLicence.h"

namespace swelltape
{
struct Preset
{
    const char* name;
    float drive, age, wow, tone, mix; // drive/age/wow/mix en %, tone en dB (-10..10)
};

class Processor final : public juce::AudioProcessor
{
public:
    Processor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swell Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static const std::vector<Preset>& presets();

    juce::AudioProcessorValueTreeState params;
    juce::SharedResourcePointer<swell::Licence> licence;

    // Pour l'interface : vitesse de la bande et dernier passage audio.
    float transportSpeed() const noexcept { return engine.transportSpeed(); }
    bool audioIsRunning() const noexcept { return juce::Time::getMillisecondCounter() - lastBlockMs.load() < 250u; }
    Settings currentSettings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    TapeEngine engine;
    int currentPreset = 0;
    std::atomic<juce::uint32> lastBlockMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
} // namespace swelltape
