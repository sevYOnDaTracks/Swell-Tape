#include "TapeEngine.h"
#include <cmath>

namespace swelltape
{
namespace
{
constexpr float pi = 3.14159265358979f;
constexpr double twoPi = 6.283185307179586;
constexpr int subBlock = 32;
constexpr double baseDelayMs = 4.0;   // centre de la modulation : c'est aussi la latence annoncée
constexpr double maxModMs = 3.4;      // excursion maximale du wow + flutter + dérive
constexpr double tapeSeconds = 6.0;   // assez pour un arrêt de bande complet
constexpr double stopSeconds = 0.95;
constexpr double startSeconds = 0.38;
constexpr double xfadeSeconds = 0.04;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
inline float logCosh (float x) noexcept
{
    const float a = std::abs (x);
    return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
}
} // namespace

// ------------------------------------------------------------------ SVF (Simper / Cytomic), stable en modulation

void TapeEngine::Svf::set (float f, float q, float sampleRate, float gScale)
{
    f = juce::jlimit (5.0f, 0.49f * sampleRate, f);
    const float g = std::tan (pi * f / sampleRate) * gScale;
    const float k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}
void TapeEngine::Svf::lowPass (float f, float q, float s) { set (f, q, s, 1.0f); m0 = 0; m1 = 0; m2 = 1; }
void TapeEngine::Svf::highPass (float f, float q, float s) { set (f, q, s, 1.0f); m0 = 1; m1 = -1.0f / q; m2 = -1; }
void TapeEngine::Svf::bell (float f, float q, float db, float s)
{
    const float A = std::pow (10.0f, db / 40.0f);
    set (f, q * A, s, 1.0f);
    m0 = 1; m1 = (1.0f / (q * A)) * (A * A - 1.0f); m2 = 0;
}
void TapeEngine::Svf::lowShelf (float f, float q, float db, float s)
{
    const float A = std::pow (10.0f, db / 40.0f);
    set (f, q, s, 1.0f / std::sqrt (A));
    m0 = 1; m1 = (1.0f / q) * (A - 1.0f); m2 = A * A - 1.0f;
}
void TapeEngine::Svf::highShelf (float f, float q, float db, float s)
{
    const float A = std::pow (10.0f, db / 40.0f);
    set (f, q, s, std::sqrt (A));
    m0 = A * A; m1 = (1.0f / q) * (1.0f - A) * A; m2 = 1.0f - A * A;
}

// ------------------------------------------------------------------ préparation

void TapeEngine::prepare (double sampleRate, int, int numChannels)
{
    sr = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    tapeSize = (int) std::ceil (tapeSeconds * sr) + 8;
    baseDelaySamples = (int) std::round (baseDelayMs * 0.001 * sr);
    maxModSamples = maxModMs * 0.001 * sr;
    drySize = baseDelaySamples + 4;
    for (int c = 0; c < 2; ++c)
    {
        tape[c].assign ((size_t) tapeSize, 0.0f);
        dry[c].assign ((size_t) drySize, 0.0f);
    }
    reset();
}

void TapeEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        std::fill (tape[c].begin(), tape[c].end(), 0.0f);
        std::fill (dry[c].begin(), dry[c].end(), 0.0f);
        prevX[c] = prevF[c] = 0.0f;
    }
    writeIndex = 0; dryIndex = 0; prevRunDelay = baseDelaySamples;
    for (auto* f : { &preEmph, &deEmph, &dcBlock, &lowCut, &bump, &hfLoss, &toneLow, &toneHigh, &hissShape })
        f->clear();
    first = true;
    wowPhase = flutterPhase = 0.0;
    drift = driftTarget = 0.0f;
    driftCountdown = 0;
    dropLeft = 0;
    transport = Transport::running;
    rate = 1.0;
    speedOut.store (1.0f);
}

float TapeEngine::speedProfile (const Settings& s) noexcept
{
    switch (s.speed)
    {
        case 1: return 0.0f;
        case 2: return 0.33f;
        case 3: return 0.66f;
        case 4: return 1.0f;
        default: return s.age;
    }
}

void TapeEngine::updateFilters (const Settings& p)
{
    const float fs = (float) sr;
    const float sp = speedProfile (p);
    const float age = p.age;

    // Les aigus saturent en premier, comme sur une vraie bande (pré-accentuation à l'enregistrement).
    const float emph = 3.0f + 5.0f * age;
    preEmph.highShelf (3000.0f, 0.7f, emph, fs);
    deEmph.highShelf (3000.0f, 0.7f, -emph, fs);
    dcBlock.highPass (12.0f, 0.707f, fs);

    // Bosse de tête : plus grave quand la bande ralentit.
    const float bumpF = 110.0f * std::pow (0.38f, sp);
    lowCut.highPass (bumpF * 0.32f, 0.6f, fs);
    bump.bell (bumpF, 1.0f, 2.2f + 1.3f * sp, fs);

    // Perte d'aigus : 22 kHz à 30 ips, ~5 kHz sur une cassette fatiguée.
    hfLoss.lowPass (26000.0f * std::pow (5200.0f / 26000.0f, sp), 0.62f, fs);

    toneHigh.highShelf (2800.0f, 0.7f, p.tone * 7.0f, fs);
    toneLow.lowShelf (180.0f, 0.7f, -p.tone * 4.0f, fs);
    hissShape.highPass (900.0f, 0.5f, fs);

    const float driveDb = p.drive * 24.0f + age * 4.0f;
    driveGain = 0.4f * dbToGain (driveDb);
    driveMakeup = 1.0f / (0.4f * dbToGain (driveDb * 0.85f));
    bias = (0.04f + 0.22f * age) * (0.3f + p.drive);
    biasDc = std::tanh (bias);

    hissGain = p.hiss == 0 ? 0.0f : dbToGain (-86.0f + 42.0f * std::pow (age, 1.2f) + (p.hiss == 2 ? 9.0f : 0.0f));
}

// ------------------------------------------------------------------ briques

float TapeEngine::saturate (float x, int ch) noexcept
{
    // ADAA seulement sur la part distordue r(x) = tanh(x) - x : la partie linéaire passe sans le moyennage
    // (qui sinon coupe les aigus du son propre, -7 dB à 16 kHz en 44,1 kHz).
    const float in = x * driveGain + bias;
    const float F = logCosh (in) - 0.5f * in * in;
    const float d = in - prevX[ch];
    const float mid = 0.5f * (in + prevX[ch]);
    const float res = std::abs (d) < 1.0e-4f ? std::tanh (mid) - mid : (F - prevF[ch]) / d;
    prevX[ch] = in;
    prevF[ch] = F;
    return (in + res - biasDc) * driveMakeup;
}

float TapeEngine::readTape (int ch, double pos) const noexcept
{
    while (pos < 0.0) pos += tapeSize;
    const int i = (int) pos;
    const float t = (float) (pos - i);
    const auto& b = tape[ch];
    auto at = [&] (int k) { k %= tapeSize; if (k < 0) k += tapeSize; return b[(size_t) k]; };
    const float xm = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
    const float c1 = 0.5f * (x1 - xm);
    const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

// ------------------------------------------------------------------ traitement

void TapeEngine::process (juce::AudioBuffer<float>& buffer, const Settings& target)
{
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (channels, buffer.getNumChannels());
    const float alpha = 1.0f - std::exp (-(float) subBlock / (0.03f * (float) sr));
    if (first) { smooth = target; first = false; }
    smooth.speed = target.speed;
    smooth.hiss = target.hiss;

    // Arrêt / démarrage demandés.
    if (target.stop && (transport == Transport::running || transport == Transport::starting))
    {
        varPos = transport == Transport::starting ? varPos : baseDelaySamples; // « varPos » = retard courant
        transport = Transport::stopping; elapsed = 0.0; rate = 1.0;
    }
    else if (! target.stop && (transport == Transport::stopping || transport == Transport::stopped))
    {
        transport = Transport::starting; elapsed = 0.0; rate = 0.0; xfade = 0.0;
        varPos = baseDelaySamples;
    }

    double speedSum = 0.0;
    const double dt = 1.0 / sr;

    for (int start = 0; start < n; start += subBlock)
    {
        const int len = juce::jmin (subBlock, n - start);
        auto lerp = [alpha] (float& a, float b) { a += (b - a) * alpha; };
        lerp (smooth.drive, target.drive); lerp (smooth.age, target.age); lerp (smooth.wow, target.wow);
        lerp (smooth.tone, target.tone); lerp (smooth.mix, target.mix); lerp (smooth.outputDb, target.outputDb);
        updateFilters (smooth);

        const float age = smooth.age, wow = smooth.wow;
        const double wowDepth = juce::jmin (maxModSamples * 0.75, (0.08 + 2.6 * age) * 0.001 * sr * wow);
        const double flutterDepth = (0.008 + 0.06 * age) * 0.001 * sr * wow;
        const double driftDepth = 0.6 * 0.001 * sr * wow * age;
        const float outGain = dbToGain (smooth.outputDb);
        const float mix = smooth.mix;
        const float dropChance = age > 0.5f ? std::pow ((age - 0.5f) / 0.5f, 1.5f) * 0.8f / (float) sr : 0.0f;

        for (int i = start; i < start + len; ++i)
        {
            // --- modulation commune aux deux canaux (une seule bande)
            if (--driftCountdown <= 0)
            {
                driftTarget = random.nextFloat() * 2.0f - 1.0f;
                driftCountdown = (int) (sr * (0.2 + 0.4 * random.nextDouble()));
                wowRate = juce::jlimit (0.42, 0.72, wowRate + (random.nextDouble() - 0.5) * 0.06);
            }
            drift += (driftTarget - drift) * (float) (twoPi * 3.0 * dt);
            wowPhase += twoPi * wowRate * dt;       if (wowPhase > twoPi) wowPhase -= twoPi;
            flutterPhase += twoPi * 7.3 * dt;       if (flutterPhase > twoPi) flutterPhase -= twoPi;
            const double mod = juce::jlimit (-maxModSamples, maxModSamples,
                                             wowDepth * std::sin (wowPhase) + flutterDepth * std::sin (flutterPhase) + driftDepth * drift);
            const double runDelay = baseDelaySamples + mod;

            // --- transport : retard de lecture et gain
            double delay = runDelay, delayB = runDelay;
            float gain = 1.0f, blend = 0.0f; // blend > 0 : fondu du chemin variable vers le chemin normal
            double speed = 1.0 - (runDelay - prevRunDelay);
            prevRunDelay = runDelay;
            switch (transport)
            {
                case Transport::running: break;
                case Transport::stopping:
                {
                    elapsed += dt;
                    const double t = juce::jmin (1.0, elapsed / stopSeconds);
                    rate = std::pow (1.0 - t, 1.5);
                    varPos = juce::jmin ((double) tapeSize - 8.0, varPos + (1.0 - rate));
                    delay = varPos; speed = rate;
                    gain = (float) juce::jmin (1.0, (1.0 - t) / 0.12);
                    if (t >= 1.0) transport = Transport::stopped;
                    break;
                }
                case Transport::stopped: delay = varPos; gain = 0.0f; speed = 0.0; break;
                case Transport::starting:
                {
                    elapsed += dt;
                    if (elapsed < startSeconds)
                    {
                        const double t = elapsed / startSeconds;
                        rate = std::pow (t, 1.2);
                        varPos = juce::jmin ((double) tapeSize - 8.0, varPos + (1.0 - rate));
                        delay = varPos; speed = rate;
                        gain = (float) juce::jmin (1.0, elapsed / 0.03);
                    }
                    else
                    {
                        xfade += dt / xfadeSeconds;
                        delay = varPos; delayB = runDelay;
                        blend = (float) juce::jmin (1.0, xfade);
                        if (xfade >= 1.0) transport = Transport::running;
                    }
                    break;
                }
            }
            speedSum += speed;

            // --- drops (bande usée)
            float dropGain = 1.0f;
            if (dropLeft > 0)
            {
                const float k = 1.0f - (float) dropLeft / (float) dropLength;
                const float s = std::sin (pi * k);
                dropGain = 1.0f - dropDepth * s * s;
                --dropLeft;
            }
            else if (dropChance > 0.0f && random.nextFloat() < dropChance)
            {
                dropLength = dropLeft = juce::jmax (1, (int) (sr * (0.025 + 0.085 * random.nextDouble())));
                dropDepth = 0.25f + 0.45f * random.nextFloat() * age;
            }

            for (int ch = 0; ch < nch; ++ch)
            {
                float* io = buffer.getWritePointer (ch);
                const float x = io[i];

                // enregistrement
                float s = preEmph.run (x, ch);
                s = saturate (s, ch);
                s = deEmph.run (s, ch);
                s = dcBlock.run (s, ch);
                s += hissShape.run (random.nextFloat() * 2.0f - 1.0f, ch) * hissGain;
                tape[ch][(size_t) writeIndex] = s;

                // lecture
                float r = readTape (ch, (double) writeIndex - delay);
                if (blend > 0.0f)
                    r = r * (1.0f - blend) + readTape (ch, (double) writeIndex - delayB) * blend;
                r *= gain;
                r = lowCut.run (r, ch);
                r = bump.run (r, ch);
                r = hfLoss.run (r, ch);
                r = toneLow.run (r, ch);
                r = toneHigh.run (r, ch);
                r *= dropGain;

                // sec retardé de la même latence que la bande
                auto& d = dry[ch];
                const int dw = dryIndex;
                d[(size_t) dw] = x;
                const float dx = d[(size_t) ((dw - baseDelaySamples + drySize) % drySize)];

                io[i] = (dx * (1.0f - mix) + r * mix) * outGain;
            }
            if (nch == 1 && buffer.getNumChannels() > 1)
                buffer.copyFrom (1, i, buffer, 0, i, 1);
            writeIndex = (writeIndex + 1) % tapeSize;
            dryIndex = (dryIndex + 1) % drySize;
        }
    }
    speedOut.store (n > 0 ? (float) (speedSum / n) : speedOut.load());
}
} // namespace swelltape
