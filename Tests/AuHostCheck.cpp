// Charge le composant AU installé comme un hôte (FL Studio, Logic) : instanciation, lecture audio, fenêtre.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
using namespace juce;
int main()
{
    ScopedJuceInitialiser_GUI init;
    AudioUnitPluginFormat fmt;
    OwnedArray<PluginDescription> found;
    fmt.findAllTypesForFile (found, "AudioUnit:Effects/aufx,Stap,Sevy");
    std::printf ("types trouvés : %d\n", found.size());
    if (found.isEmpty()) return 1;
    String err;
    auto inst = fmt.createInstanceFromDescription (*found[0], 44100.0, 512, err);
    std::printf ("instance : %s %s\n", inst ? "ok" : "ÉCHEC", err.toRawUTF8());
    if (! inst) return 1;
    std::printf ("bus : in %d out %d, latence %d\n", inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels(), inst->getLatencySamples());
    inst->prepareToPlay (44100.0, 512);
    AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer m;
    for (int i = 0; i < 20; ++i) inst->processBlock (b, m);
    std::printf ("processBlock : ok\n");
    std::unique_ptr<AudioProcessorEditor> ed (inst->createEditorIfNeeded());
    std::printf ("éditeur : %s (%dx%d)\n", ed ? "ok" : "ÉCHEC", ed ? ed->getWidth() : 0, ed ? ed->getHeight() : 0);
    MemoryBlock state; inst->getStateInformation (state); inst->setStateInformation (state.getData(), (int) state.getSize());
    std::printf ("état : %d octets, rechargé\n", (int) state.getSize());
    ed.reset(); inst->releaseResources(); inst.reset();
    std::printf ("fermeture : ok\n");
    return 0;
}
