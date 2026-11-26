#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace swelltape
{
using namespace juce;

const std::vector<Preset>& Processor::presets()
{
    static const std::vector<Preset> list {
        { "Colle de bus",      42, 20, 30,  0.5f, 100 },
        { "Voix chaude",       30, 12, 15, -1.0f,  80 },
        { "Batterie poussée",  78, 30, 10,  0.2f, 100 },
        { "Basse ronde",       55, 35, 12, -2.0f, 100 },
        { "Guitare vintage",   48, 45, 35, -1.5f, 100 },
        { "Démo 1987",         50, 62, 55, -2.4f, 100 },
        { "Cassette lo-fi",    55, 92, 80, -4.0f, 100 },
        { "Cassette oubliée",  35, 100, 100, -6.0f, 100 },
    };
    return list;
}

AudioProcessorValueTreeState::ParameterLayout Processor::layout()
{
    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%")
                                                          .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }));
    };
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (pct ("drive", "Drive", 42.0f));
    l.add (pct ("age", "Age", 20.0f));
    l.add (pct ("wow", "Wow", 30.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone", NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.5f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (pct ("mix", "Mix", 100.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Sortie", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "speed", 1 }, "Vitesse",
                                                   StringArray { "Suit Age", "30 ips", "15 ips", "7,5 ips", "Cassette" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hiss", 1 }, "Souffle", StringArray { String::fromUTF8 ("Coupé"), "Auto", "Fort" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "stop", 1 }, String::fromUTF8 ("Arrêt de bande"), false));
    return l;
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "SwellTape", layout())
{
}

bool Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.latencySamples());
}

Settings Processor::currentSettings() const
{
    auto v = [this] (const char* id) { return params.getRawParameterValue (id)->load(); };
    Settings s;
    s.drive = v ("drive") * 0.01f;
    s.age = v ("age") * 0.01f;
    s.wow = v ("wow") * 0.01f;
    s.tone = v ("tone") * 0.1f;
    s.mix = v ("mix") * 0.01f;
    s.outputDb = v ("output");
    s.speed = (int) v ("speed");
    s.hiss = (int) v ("hiss");
    s.stop = v ("stop") > 0.5f;
    return s;
}

void Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    lastBlockMs.store (Time::getMillisecondCounter());
    if (! licence->isUnlocked()) { buffer.clear(); return; }
    engine.process (buffer, currentSettings());
}

int Processor::getNumPrograms() { return (int) presets().size(); }
const String Processor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (CharPointer_UTF8 (presets()[(size_t) i].name)) : String(); }

void Processor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentPreset = i;
    const auto& p = presets()[(size_t) i];
    auto set = [this] (const char* id, float v)
    {
        auto* prm = params.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("drive", p.drive); set ("age", p.age); set ("wow", p.wow); set ("tone", p.tone); set ("mix", p.mix);
}

void Processor::getStateInformation (MemoryBlock& dest)
{
    auto state = params.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (params.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentPreset = jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("preset", 0));
            params.replaceState (tree);
            if (auto* stop = params.getParameter ("stop")) stop->setValueNotifyingHost (0.0f); // jamais rechargé à l'arrêt
        }
}

AudioProcessorEditor* Processor::createEditor()
{
    return swell::LicenceGuard::attach (new Editor (*this));
}
} // namespace swelltape

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new swelltape::Processor(); }
