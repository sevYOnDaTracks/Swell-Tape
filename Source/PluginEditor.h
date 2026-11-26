#pragma once
#include "PluginProcessor.h"

namespace swelltape
{
namespace colours
{
inline const juce::Colour background { 0xff141113 }, hero { 0xff0e0c0d }, raised { 0xff221e21 },
                          cream { 0xfff4eee6 }, muted { 0xff8f8781 }, accent { 0xfff2a65a }, ink { 0xff1a110b };
}

struct Fonts
{
    juce::Typeface::Ptr regular, semibold;
    juce::Font ui (float height, bool bold = false) const;
};

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (const Fonts&);
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override {}
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcut, const juce::Drawable*, const juce::Colour*) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override;
    juce::Font getPopupMenuFont() override { return fonts.ui (14.5f); }
private:
    const Fonts& fonts;
};

// Les bobines, la bande et la platine : la photo de la machine, animée.
class Deck final : public juce::Component, private juce::Timer
{
public:
    Deck (Processor&, const Fonts&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    void drawPack (juce::Graphics&, juce::Point<float> centre, float radius, float age) const;
    Processor& proc;
    const Fonts& fonts;
    juce::Image reel, plate;
    float angleL = 0.0f, angleR = 0.0f, running = 0.0f, jitter = 0.0f;
    double time = 0.0;
    juce::uint32 lastTick = 0;
    juce::Random random;
};

// Petit réglage texte de la ligne « Plus » : clic = menu (choix), glisser = valeur (dB).
class InlineValue final : public juce::Component
{
public:
    InlineValue (Processor&, const juce::String& paramId, const juce::String& label, const Fonts&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    int idealWidth() const;
private:
    juce::String valueText() const;
    juce::RangedAudioParameter& param;
    juce::String label;
    const Fonts& fonts;
    juce::ParameterAttachment attachment;
    float dragStart = 0.0f;
    bool dragging = false;
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void setMoreOpen (bool open); // aussi utilisé par les captures de test

private:
    struct Knob
    {
        juce::String id, label;
        bool big = false;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void timerCallback() override { repaint (0, 380, getWidth(), getHeight() - 380); }
    juce::String valueText (const Knob&) const;
    void showPresetMenu();
    void showMoreMenu();
    void stepPreset (int delta);
    juce::Rectangle<float> pillBounds() const { return { 280.0f, 17.0f, 400.0f, 42.0f }; }
    juce::Rectangle<float> dotsBounds() const { return { 900.0f, 22.0f, 36.0f, 32.0f }; }
    juce::Rectangle<float> plusBounds() const { return { 28.0f, 620.0f, 70.0f, 24.0f }; }

    Processor& proc;
    Fonts fonts;
    LookAndFeel look;
    juce::Image logo;
    Deck deck;
    std::vector<Knob> knobs;
    InlineValue hiss, speed, output;
    juce::TextButton stopButton;
    juce::ParameterAttachment stopAttachment;
    swell::LicenceBadge badge;
    bool moreOpen = false;
    int hover = 0; // 1 = flèche gauche, 2 = pastille, 3 = flèche droite, 4 = ••• , 5 = Plus
};
} // namespace swelltape
