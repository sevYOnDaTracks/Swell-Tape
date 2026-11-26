#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

// Moteur de Swell Tape : une seule chaîne qui va de la bande studio propre à la vieille cassette.
//
//  enregistrement : pré-accentuation → saturation (tanh asymétrique, anti-repliement ADAA) → désaccentuation → souffle
//  la bande       : tampon circulaire lu à vitesse variable (wow, flutter, arrêt / démarrage de bande)
//  lecture        : coupe-bas + bosse de tête → perte d'aigus selon la vitesse → tonalité → drops (usure)
//
// « Age » fait glisser tout le profil (vitesse, usure, souffle, wow) ; « Vitesse » peut figer le profil de vitesse.
namespace swelltape
{
struct Settings
{
    float drive = 0.42f;   // 0..1
    float age = 0.2f;      // 0..1
    float wow = 0.3f;      // 0..1
    float tone = 0.0f;     // -1..1
    float mix = 1.0f;      // 0..1
    float outputDb = 0.0f; // -12..12
    int speed = 0;         // 0 suit Age, 1 = 30 ips, 2 = 15 ips, 3 = 7,5 ips, 4 = cassette
    int hiss = 1;          // 0 coupé, 1 auto, 2 fort
    bool stop = false;     // arrêt de bande
};

class TapeEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& settings);

    int latencySamples() const noexcept { return baseDelaySamples; }
    // Vitesse moyenne de défilement du dernier bloc (1 = nominale, 0 = arrêtée), pour les bobines de l'interface.
    float transportSpeed() const noexcept { return speedOut.load(); }

    static float speedProfile (const Settings& s) noexcept; // 0 = 30 ips … 1 = cassette usée

private:
    struct Svf
    {
        float a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
        float ic1[2] {}, ic2[2] {};
        void lowPass (float f, float q, float sr);
        void highPass (float f, float q, float sr);
        void bell (float f, float q, float db, float sr);
        void lowShelf (float f, float q, float db, float sr);
        void highShelf (float f, float q, float db, float sr);
        void clear() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }
        inline float run (float v0, int ch) noexcept
        {
            const float v3 = v0 - ic2[ch];
            const float v1 = a1 * ic1[ch] + a2 * v3;
            const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
            ic1[ch] = 2.0f * v1 - ic1[ch];
            ic2[ch] = 2.0f * v2 - ic2[ch];
            return m0 * v0 + m1 * v1 + m2 * v2;
        }
    private:
        void set (float f, float q, float sr, float gScale);
    };

    enum class Transport { running, stopping, stopped, starting };

    void updateFilters (const Settings& target);
    float saturate (float x, int ch) noexcept;
    float readTape (int ch, double pos) const noexcept;

    double sr = 44100.0;
    int channels = 2;

    // la bande
    std::vector<float> tape[2];
    int tapeSize = 0, writeIndex = 0;
    std::vector<float> dry[2];
    int drySize = 0, dryIndex = 0;
    double prevRunDelay = 0.0;
    int baseDelaySamples = 0;
    double maxModSamples = 0.0;

    // filtres
    Svf preEmph, deEmph, dcBlock, lowCut, bump, hfLoss, toneLow, toneHigh, hissShape;

    // saturation
    float prevX[2] {}, prevF[2] {};
    float driveGain = 1.0f, driveMakeup = 1.0f, bias = 0.0f, biasDc = 0.0f;

    // paramètres lissés (par bloc)
    Settings smooth;
    bool first = true;

    // wow & flutter
    double wowPhase = 0.0, flutterPhase = 0.0, wowRate = 0.55;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCountdown = 0;

    // drops
    int dropLeft = 0, dropLength = 1;
    float dropDepth = 0.0f;

    // souffle
    float hissGain = 0.0f;

    // arrêt / démarrage
    Transport transport = Transport::running;
    double varPos = 0.0, rate = 1.0, elapsed = 0.0, xfade = 0.0;
    std::atomic<float> speedOut { 1.0f };

    juce::Random random;
};
} // namespace swelltape
