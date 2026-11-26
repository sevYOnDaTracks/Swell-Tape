// Banc de test hors-ligne de Swell Tape : le chemin complet du plugin (processBlock), avec des mesures chiffrées.
//   - Mix 0 : le son sec ressort intact, exactement retardé de la latence annoncée.
//   - Age 0 / Wow 0 / Drive 0 : presque transparent.
//   - Wow : la hauteur bouge (et pas du tout à Wow 0).
//   - Age : les aigus partent (cassette) ; Drive : la distorsion monte.
//   - Arrêt de bande : silence après l'arrêt, le son revient au redémarrage.
//   - Tous les presets, 44,1 et 96 kHz : pas de NaN, niveau raisonnable.
// Écrit des WAV à écouter et des captures PNG de l'interface.
//   SwellTapeTest <dossier de sortie> [fichier.wav à traiter]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace juce;
using swelltape::Processor;

static bool ok = true;
static String u (const char* s) { return String::fromUTF8 (s); }
static void check (bool cond, const String& what)
{
    std::printf ("  [%s] %s\n", cond ? "ok" : "FAIL", what.toRawUTF8());
    ok &= cond;
}

static void setParam (Processor& p, const char* id, float v)
{
    auto* prm = p.params.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void setAll (Processor& p, float drive, float age, float wow, float tone, float mix)
{
    setParam (p, "drive", drive); setParam (p, "age", age); setParam (p, "wow", wow);
    setParam (p, "tone", tone); setParam (p, "mix", mix); setParam (p, "output", 0.0f);
    setParam (p, "speed", 0.0f); setParam (p, "hiss", 1.0f); setParam (p, "stop", 0.0f);
}

static AudioBuffer<float> render (Processor& p, const AudioBuffer<float>& in, double sr, int block = 512,
                                  std::function<void (int)> beforeBlock = {})
{
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    AudioBuffer<float> out (in);
    MidiBuffer midi;
    for (int s = 0; s < out.getNumSamples(); s += block)
    {
        const int n = jmin (block, out.getNumSamples() - s);
        if (beforeBlock) beforeBlock (s);
        AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, n);
        p.processBlock (view, midi);
    }
    return out;
}

static AudioBuffer<float> sine (double sr, double seconds, double f, float amp)
{
    AudioBuffer<float> b (2, (int) (seconds * sr));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = amp * (float) std::sin (MathConstants<double>::twoPi * f * i / sr);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

static AudioBuffer<float> noise (double sr, double seconds, float amp)
{
    Random r (3);
    AudioBuffer<float> b (2, (int) (seconds * sr));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = amp * (r.nextFloat() * 2.0f - 1.0f);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

// Petit morceau : grosse caisse, caisse claire, basse, accords.
static AudioBuffer<float> music (double sr, double seconds)
{
    AudioBuffer<float> b (2, (int) (seconds * sr));
    b.clear();
    Random r (11);
    const double beat = 0.5;
    const double chords[4][3] { { 220.0, 261.6, 329.6 }, { 174.6, 220.0, 261.6 }, { 196.0, 246.9, 293.7 }, { 164.8, 207.7, 246.9 } };
    double bassPhase = 0.0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double tb = std::fmod (t, beat);
        const int bar = (int) (t / (beat * 4)) % 4;
        double kick = std::sin (MathConstants<double>::twoPi * (45.0 * tb + 60.0 * (1.0 - std::exp (-tb * 30.0)) / 30.0)) * std::exp (-tb * 9.0);
        const double ts = std::fmod (t + beat, beat * 2.0);
        double snare = (r.nextFloat() * 2.0 - 1.0) * std::exp (-ts * 22.0) * 0.5 + std::sin (MathConstants<double>::twoPi * 190.0 * ts) * std::exp (-ts * 30.0) * 0.3;
        if (std::fmod (t, beat * 2.0) < beat) snare = 0.0;
        bassPhase += chords[bar][0] * 0.5 / sr;
        const double bass = (2.0 * (bassPhase - std::floor (bassPhase)) - 1.0) * 0.18 * (0.6 + 0.4 * std::exp (-tb * 6.0));
        double pad = 0.0;
        for (double f : chords[bar]) pad += std::sin (MathConstants<double>::twoPi * f * t) + 0.3 * std::sin (MathConstants<double>::twoPi * 2.0 * f * t);
        const double hat = (r.nextFloat() * 2.0 - 1.0) * std::exp (-std::fmod (t, beat * 0.5) * 60.0) * 0.12;
        const double l = 0.55 * kick + 0.45 * snare + bass + 0.06 * pad + hat;
        const double rr = 0.55 * kick + 0.4 * snare + bass + 0.07 * pad + hat * 0.8;
        b.setSample (0, i, (float) (l * 0.8));
        b.setSample (1, i, (float) (rr * 0.8));
    }
    return b;
}

static float rms (const AudioBuffer<float>& b, int start, int len, int ch = 0)
{
    double s = 0.0;
    len = jmin (len, b.getNumSamples() - start);
    for (int i = start; i < start + len; ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (s / jmax (1, len));
}

static bool finite (const AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

// Taux de distorsion : résidu après retrait de la fondamentale (moindres carrés).
static float thd (const AudioBuffer<float>& b, double sr, double f, int start, int len)
{
    double ss = 0, sc = 0, cc = 0, xs = 0, xc = 0, xx = 0;
    for (int i = start; i < start + len; ++i)
    {
        const double ph = MathConstants<double>::twoPi * f * i / sr, s = std::sin (ph), c = std::cos (ph), x = b.getSample (0, i);
        ss += s * s; cc += c * c; sc += s * c; xs += x * s; xc += x * c; xx += x * x;
    }
    const double det = ss * cc - sc * sc;
    const double a = (xs * cc - xc * sc) / det, bb = (xc * ss - xs * sc) / det;
    double res = 0;
    for (int i = start; i < start + len; ++i)
    {
        const double ph = MathConstants<double>::twoPi * f * i / sr;
        const double e = b.getSample (0, i) - (a * std::sin (ph) + bb * std::cos (ph));
        res += e * e;
    }
    return (float) std::sqrt (res / jmax (1e-12, xx - res));
}

// Énergie au-dessus de ~8 kHz (différence d'échantillons répétée = passe-haut raide), relative au total.
static float highShareDb (const AudioBuffer<float>& b, int start, int len)
{
    double tot = 0, hi = 0;
    for (int i = start + 3; i < start + len; ++i)
    {
        const double x0 = b.getSample (0, i), x1 = b.getSample (0, i - 1), x2 = b.getSample (0, i - 2), x3 = b.getSample (0, i - 3);
        const double d = x0 - 3 * x1 + 3 * x2 - x3; // 3e différence : pente +18 dB/oct
        tot += x0 * x0; hi += d * d;
    }
    return (float) (10.0 * std::log10 (hi / jmax (1e-20, tot)));
}

// Écart de hauteur maximal (en %) d'une sinusoïde, par les passages à zéro.
static float pitchDeviationPct (const AudioBuffer<float>& b, double sr, double f, int start)
{
    std::vector<double> crossings;
    for (int i = start + 1; i < b.getNumSamples(); ++i)
    {
        const float a = b.getSample (0, i - 1), c = b.getSample (0, i);
        if (a < 0.0f && c >= 0.0f) crossings.push_back (i - 1 + a / (a - c));
    }
    double worst = 0.0;
    const int span = 20; // moyenne sur 20 périodes
    for (size_t k = span; k < crossings.size(); k += 5)
    {
        const double freq = span * sr / (crossings[k] - crossings[k - span]);
        worst = jmax (worst, std::abs (freq / f - 1.0));
    }
    return (float) (worst * 100.0);
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    std::unique_ptr<OutputStream> stream (std::move (os));
    if (auto w = wav.createWriterFor (stream, AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    swell::Licence::unlockForTesting();
    ScopedJuceInitialiser_GUI init;
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "test-out");
    outDir.createDirectory();
    const double sr = 44100.0;

    std::printf ("Swell Tape — banc de test\n");

    {
        std::printf ("Mix 0 et latence\n");
        Processor p;
        setAll (p, 80, 80, 100, 3, 0);
        const auto in = music (sr, 2.0);
        const auto out = render (p, in, sr);
        const int L = p.getLatencySamples();
        float err = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = L; i < in.getNumSamples(); ++i)
                err = jmax (err, std::abs (out.getSample (c, i) - in.getSample (c, i - L)));
        check (L > 0 && L < 1000, u ("latence annoncée : ") + String (L) + u (" échantillons"));
        check (err < 1.0e-6f, u ("Mix 0 = son sec retardé exactement (écart max ") + String (err, 9) + ")");
    }

    {
        std::printf ("Réglage neutre\n");
        Processor p;
        setAll (p, 0, 0, 0, 0, 100);
        setParam (p, "hiss", 0.0f);
        const auto in = music (sr, 3.0);
        const auto out = render (p, in, sr);
        const int L = p.getLatencySamples();
        const float levelDb = 20.0f * std::log10 (rms (out, (int) sr, (int) (2 * sr)) / rms (in, (int) sr - L, (int) (2 * sr)));
        String resp;
        bool flat = true;
        for (double f : { 60.0, 110.0, 400.0, 1000.0, 4000.0, 10000.0, 16000.0 })
        {
            Processor q; setAll (q, 0, 0, 0, 0, 100); setParam (q, "hiss", 0.0f);
            const auto s = render (q, sine (sr, 1.0, f, 0.1f), sr);
            const float db = 20.0f * std::log10 (rms (s, (int) (0.5 * sr), (int) (0.4 * sr)) / (0.1f / std::sqrt (2.0f)));
            resp << String (f, 0) << " Hz " << String (db, 1) << " / ";
            flat &= f == 110.0 ? (db > 0.5f && db < 4.0f) : f > 12000.0 ? (db > -4.0f && db < 1.0f) : std::abs (db) < 1.5f;
        }
        check (flat, u ("réponse quasi plate, bosse de tête vers 110 Hz : ") + resp);
        check (std::abs (levelDb) < 2.0f, u ("niveau conservé (") + String (levelDb, 2) + " dB)");
    }

    {
        std::printf ("Wow\n");
        Processor a, b;
        setAll (a, 0, 0, 0, 0, 100); setParam (a, "hiss", 0.0f);
        setAll (b, 0, 100, 100, 0, 100); setParam (b, "hiss", 0.0f);
        const auto in = sine (sr, 5.0, 1000.0, 0.3f);
        const float still = pitchDeviationPct (render (a, in, sr), sr, 1000.0, (int) (0.5 * sr));
        const float moving = pitchDeviationPct (render (b, in, sr), sr, 1000.0, (int) (0.5 * sr));
        check (still < 0.05f, "Wow 0 : hauteur stable (" + String (still, 3) + " %)");
        check (moving > 0.5f && moving < 6.0f, "Wow 100 / Age 100 : la hauteur ondule (" + String (moving, 2) + " %)");
        setAll (b, 0, 20, 30, 0, 100);
        const float mild = pitchDeviationPct (render (b, in, sr), sr, 1000.0, (int) (0.5 * sr));
        check (mild > 0.02f && mild < 0.6f, "preset studio : wow discret (" + String (mild, 3) + " %)");
    }

    {
        std::printf ("Age : perte d'aigus\n");
        Processor a, b;
        setAll (a, 0, 0, 0, 0, 100); setParam (a, "hiss", 0.0f);
        setAll (b, 0, 100, 0, 0, 100); setParam (b, "hiss", 0.0f);
        const auto in = noise (sr, 2.0, 0.2f);
        const float hiA = highShareDb (render (a, in, sr), (int) (0.5 * sr), (int) sr);
        const float hiB = highShareDb (render (b, in, sr), (int) (0.5 * sr), (int) sr);
        check (hiA - hiB > 12.0f, "cassette bien plus sombre que la bande studio (" + String (hiA - hiB, 1) + " dB d'aigus en moins)");
        setParam (b, "speed", 1.0f);
        const float hiFixed = highShareDb (render (b, in, sr), (int) (0.5 * sr), (int) sr);
        check (std::abs (hiFixed - hiA) < 3.0f, u ("Vitesse 30 ips fige le profil même à Age 100"));
    }

    {
        std::printf ("Drive\n");
        Processor a, b;
        setAll (a, 0, 0, 0, 0, 100); setParam (a, "hiss", 0.0f);
        setAll (b, 100, 0, 0, 0, 100); setParam (b, "hiss", 0.0f);
        const auto in = sine (sr, 2.0, 200.0, 0.5f);
        const float t0 = thd (render (a, in, sr), sr, 200.0, (int) sr, (int) (0.5 * sr));
        const float t1 = thd (render (b, in, sr), sr, 200.0, (int) sr, (int) (0.5 * sr));
        check (t1 > t0 * 3.0f && t1 > 0.03f, "la distorsion monte avec Drive (" + String (t0 * 100, 2) + u (" % → ") + String (t1 * 100, 1) + " %)");
    }

    {
        std::printf ("Arrêt de bande\n");
        Processor p;
        setAll (p, 30, 20, 20, 0, 100);
        const auto in = sine (sr, 4.0, 440.0, 0.3f);
        const auto out = render (p, in, sr, 512, [&] (int s)
        {
            if (s >= (int) (1.0 * sr) && s < (int) (2.5 * sr)) setParam (p, "stop", 1.0f);
            else setParam (p, "stop", 0.0f);
        });
        const float before = rms (out, (int) (0.5 * sr), (int) (0.4 * sr));
        const float stopped = rms (out, (int) (2.1 * sr), (int) (0.35 * sr));
        const float after = rms (out, (int) (3.3 * sr), (int) (0.5 * sr));
        check (stopped < 1.0e-4f, u ("silence une fois la bande arrêtée"));
        check (std::abs (20.0f * std::log10 (after / before)) < 2.0f, u ("le son revient au redémarrage"));
        check (finite (out), u ("pas de NaN pendant l'arrêt et la relance"));
        writeWav (outDir.getChildFile ("arret-de-bande.wav"), out, sr);
    }

    {
        std::printf ("Presets\n");
        const auto in = music (sr, 6.0);
        writeWav (outDir.getChildFile ("00-sec.wav"), in, sr);
        const float inRms = rms (in, (int) sr, (int) (4 * sr));
        for (int i = 0; i < (int) Processor::presets().size(); ++i)
        {
            Processor p;
            p.setCurrentProgram (i);
            const auto out = render (p, in, sr);
            float peak = out.getMagnitude (0, out.getNumSamples());
            const float db = 20.0f * std::log10 (rms (out, (int) sr, (int) (4 * sr)) / inRms);
            check (finite (out) && peak < 1.6f && std::abs (db) < 8.0f,
                   p.getProgramName (i) + u (" : crête ") + String (peak, 2) + ", niveau " + String (db, 1) + " dB");
            writeWav (outDir.getChildFile (String (i + 1).paddedLeft ('0', 2) + "-" + p.getProgramName (i).replaceCharacter (' ', '-') + ".wav"), out, sr);
        }
        Processor p;
        p.setCurrentProgram (6);
        const auto out96 = render (p, music (96000.0, 2.0), 96000.0, 256);
        check (finite (out96) && out96.getMagnitude (0, out96.getNumSamples()) < 1.6f, "96 kHz : cassette lo-fi stable");
    }

    if (argc > 2)
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        if (auto reader = std::unique_ptr<AudioFormatReader> (fm.createReaderFor (File::getCurrentWorkingDirectory().getChildFile (argv[2]))))
        {
            AudioBuffer<float> in (2, (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 30)));
            reader->read (&in, 0, in.getNumSamples(), 0, true, true);
            for (int i : { 0, 5, 6 })
            {
                Processor p; p.setCurrentProgram (i);
                writeWav (outDir.getChildFile ("fichier-" + p.getProgramName (i).replaceCharacter (' ', '-') + ".wav"), render (p, in, reader->sampleRate), reader->sampleRate);
            }
            std::printf ("  fichier traité : %s\n", argv[2]);
        }
    }

    {
        std::printf ("Captures\n");
        struct Shot { int preset; bool more, stop; const char* name; };
        for (auto s : { Shot { 0, false, false, "ui-studio.png" }, Shot { 5, false, false, "ui-demo-1987.png" },
                        Shot { 6, true, false, "ui-cassette-plus.png" }, Shot { 2, true, true, "ui-arret.png" } })
        {
            Processor p;
            p.setCurrentProgram (s.preset);
            setParam (p, "stop", s.stop ? 1.0f : 0.0f);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            if (auto* e = dynamic_cast<swelltape::Editor*> (ed.get())) e->setMoreOpen (s.more);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            auto f = outDir.getChildFile (s.name);
            f.deleteFile();
            PNGImageFormat png;
            if (auto os = std::unique_ptr<FileOutputStream> (f.createOutputStream())) png.writeImageToStream (img, *os);
            check (f.existsAsFile(), String ("capture ") + s.name);
        }
    }

    std::printf (ok ? "\nTout est bon.\n" : "\nDES CONTRÔLES ONT ÉCHOUÉ.\n");
    return ok ? 0 : 1;
}
