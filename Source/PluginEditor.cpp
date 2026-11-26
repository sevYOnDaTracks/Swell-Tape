#include "PluginEditor.h"
#include "BinaryData.h"

namespace swelltape
{
using namespace juce;

namespace
{
constexpr int W = 960, H = 660;
const Rectangle<int> heroArea { 28, 76, 904, 322 };
constexpr float fracL = 0.9f, fracR = 0.6f;   // remplissage des deux galettes
constexpr float reelSize = 196.0f;
const Point<float> centreL { 92.0f + 98.0f, 16.0f + 98.0f }, centreR { 904.0f - 92.0f - 98.0f, 16.0f + 98.0f };

String ageName (float v) { return v < 25 ? "Studio" : v < 50 ? "Vintage" : v < 75 ? "Vieille bande" : "Cassette"; }
String ageDetail (float v)
{
    return String::fromUTF8 (v < 25 ? "bande 30 ips" : v < 50 ? "bande 15 ips" : v < 75 ? "7,5 ips, usée" : "type I, fatiguée");
}
String speedDetail (int speed, float age)
{
    switch (speed)
    {
        case 1: return "bande 30 ips";
        case 2: return "bande 15 ips";
        case 3: return "bande 7,5 ips";
        case 4: return "cassette";
        default: return ageDetail (age * 100.0f);
    }
}
String commaNumber (float v, int decimals) { return String (v, decimals).replaceCharacter ('.', ','); }
float textWidth (const Font& f, const String& s) { return GlyphArrangement::getStringWidth (f, s); }

void drawChevron (Graphics& g, Point<float> c, float size, int direction, Colour colour)
{
    Path p;
    if (direction == 2) { p.startNewSubPath (c.x - size, c.y - size * 0.5f); p.lineTo (c.x, c.y + size * 0.5f); p.lineTo (c.x + size, c.y - size * 0.5f); }
    else { const float d = (float) direction; p.startNewSubPath (c.x - d * size * 0.5f, c.y - size); p.lineTo (c.x + d * size * 0.5f, c.y); p.lineTo (c.x - d * size * 0.5f, c.y + size); }
    g.setColour (colour);
    g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}
} // namespace

Font Fonts::ui (float height, bool bold) const
{
    auto tf = bold ? semibold : regular;
    return tf != nullptr ? Font (FontOptions (tf).withHeight (height)) : Font (FontOptions (height));
}

// ------------------------------------------------------------------ look and feel

LookAndFeel::LookAndFeel (const Fonts& f) : fonts (f)
{
    setColour (PopupMenu::backgroundColourId, colours::raised);
    setColour (PopupMenu::textColourId, colours::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.16f));
    setColour (PopupMenu::highlightedTextColourId, colours::cream);
    setColour (AlertWindow::backgroundColourId, colours::raised);
    setColour (AlertWindow::textColourId, colours::cream);
    setColour (TextButton::buttonColourId, Colours::transparentBlack);
    setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    setColour (TextButton::textColourOffId, colours::cream);
    setColour (TextButton::textColourOnId, colours::accent);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& slider)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float ring = size * 0.435f;
    const float angle = start + pos * (end - start);
    const bool bipolar = slider.getProperties()["bipolar"];

    Path track, value;
    track.addCentredArc (c.x, c.y, ring, ring, 0.0f, start, end, true);
    const float from = bipolar ? (start + end) * 0.5f : start;
    value.addCentredArc (c.x, c.y, ring, ring, 0.0f, jmin (from, angle), jmax (from, angle), true);
    const PathStrokeType stroke (4.0f, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.strokePath (track, stroke);
    if (std::abs (angle - from) > 0.01f) { g.setColour (colours::accent); g.strokePath (value, stroke); }

    // capuchon blanc, bague sombre, ombre portée
    const float r = size * 0.36f;
    const auto cap = Rectangle<float> (r * 2, r * 2).withCentre (c);
    for (int i = 6; i > 0; --i)
    {
        g.setColour (Colours::black.withAlpha (0.06f));
        g.fillEllipse (cap.translated (0, 5.0f).expanded ((float) i * 1.6f));
    }
    g.setColour (Colour (0xff1b181a));
    g.fillEllipse (cap.expanded (6.0f));
    ColourGradient grad (Colours::white, cap.getX() + r * 0.84f, cap.getY() + r * 0.68f, Colour (0xffbfc4cc), cap.getRight(), cap.getBottom(), true);
    grad.addColour (0.4, Colour (0xffeef0f3));
    g.setGradientFill (grad);
    g.fillEllipse (cap);

    Path notch;
    notch.addRoundedRectangle (-1.5f, -r + 7.0f, 3.0f, r * 0.42f, 1.5f);
    g.setColour (Colour (0xff1c1d22));
    g.fillPath (notch, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool over, bool down)
{
    g.setFont (fonts.ui (14.0f, b.getToggleState()));
    g.setColour (b.getToggleState() || over || down ? colours::accent : colours::cream);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (8, 0), Justification::centredLeft, false);
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (colours::raised);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                     bool isTicked, bool, const String& text, const String&, const Drawable*, const Colour*)
{
    if (isSeparator)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect (area.reduced (12, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour (colours::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r.reduced (4.0f, 1.0f), 6.0f);
    }
    if (isTicked)
    {
        g.setColour (colours::accent);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 18.0f, r.getCentreY() }));
    }
    g.setColour (isActive ? colours::cream : colours::muted);
    g.setFont (fonts.ui (14.5f, isTicked));
    g.drawText (text, r.withTrimmedLeft (32.0f).withTrimmedRight (16.0f), Justification::centredLeft, true);
}

void LookAndFeel::getIdealPopupMenuItemSize (const String& text, bool isSeparator, int, int& w, int& h)
{
    if (isSeparator) { w = 50; h = 9; return; }
    w = roundToInt (textWidth (fonts.ui (14.5f, true), text)) + 64;
    h = 30;
}

// ------------------------------------------------------------------ platine animée

Deck::Deck (Processor& p, const Fonts& f) : proc (p), fonts (f)
{
    reel = ImageCache::getFromMemory (BinaryData::reel_png, BinaryData::reel_pngSize);
    plate = ImageCache::getFromMemory (BinaryData::transport_jpg, BinaryData::transport_jpgSize);
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
    lastTick = Time::getMillisecondCounter();
    startTimerHz (60);
}

void Deck::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = jlimit (0.0f, 0.05f, (float) (now - lastTick) * 0.001f);
    lastTick = now;
    time += dt;

    const auto s = proc.currentSettings();
    // Les bobines tournent quand le son passe ; l'arrêt de bande vient du moteur.
    const float target = proc.audioIsRunning() ? jlimit (0.0f, 1.0f, proc.transportSpeed()) : 0.0f;
    running += (target - running) * jmin (1.0f, dt * (target > running ? 5.0f : 2.5f));

    // Mouvement visible exagéré : la vraie modulation (quelques dixièmes de %) serait invisible.
    const float t = s.age, w = s.wow;
    const double wowHz = 0.55;
    const float wobble = 1.0f + w * (0.04f + 0.16f * t) * (float) std::sin (time * MathConstants<double>::twoPi * wowHz)
                              + w * (0.01f + 0.03f * t) * (float) std::sin (time * MathConstants<double>::twoPi * 7.3);
    const float base = (1.35f - 0.55f * t) * 0.32f * MathConstants<float>::twoPi; // radians / s
    const float vL = base * wobble * running;
    angleL += vL * dt;
    angleR += vL * (fracL / fracR) * dt;   // la galette la plus mince tourne plus vite
    jitter = w * t * 1.2f * (float) std::sin (time * MathConstants<double>::twoPi * wowHz) * running;
    repaint();
}

void Deck::drawPack (Graphics& g, Point<float> c, float radius, float age) const
{
    const Colour tape = Colour::fromRGB ((uint8) (40 + 30 * age), (uint8) (28 + 20 * age), (uint8) (22 + 12 * age));
    g.setColour (tape);
    g.fillEllipse (Rectangle<float> (radius * 2, radius * 2).withCentre (c));
    g.setColour (tape.darker (0.25f));
    for (float r = 22.0f; r < radius; r += 3.0f)
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 1.0f);
    ColourGradient sheen (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.4f), c.x + radius, c.y, true);
    sheen.addColour (0.9, Colour (0x0dffebd2));
    g.setGradientFill (sheen);
    g.fillEllipse (Rectangle<float> (radius * 2, radius * 2).withCentre (c));
}

void Deck::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    Path clip;
    clip.addRoundedRectangle (bounds, 12.0f);
    g.reduceClipRegion (clip);
    g.fillAll (colours::hero);

    const auto s = proc.currentSettings();
    const float age = s.age;

    g.saveState();
    g.addTransform (AffineTransform::translation (0.0f, jitter));

    // platine (photo), assombrie et fondue dans le fond
    const float plateTop = bounds.getHeight() - 146.0f;
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, 0, (int) plateTop, (int) bounds.getWidth(), 146, 0, 53, plate.getWidth(), 292);
    g.setColour (Colours::black.withAlpha (0.22f));
    g.fillRect (0.0f, plateTop, bounds.getWidth(), 146.0f);
    {
        const auto hero = colours::hero;
        ColourGradient top (hero.withAlpha (0.9f), 0, plateTop, hero.withAlpha (0.0f), 0, plateTop + 32.0f, false);
        g.setGradientFill (top); g.fillRect (0.0f, plateTop, bounds.getWidth(), 32.0f);
        ColourGradient bottom (hero.withAlpha (0.0f), 0, bounds.getBottom() - 30.0f, hero.withAlpha (0.85f), 0, bounds.getBottom(), false);
        g.setGradientFill (bottom); g.fillRect (0.0f, bounds.getBottom() - 30.0f, bounds.getWidth(), 30.0f);
        ColourGradient left (hero.withAlpha (0.7f), 0, 0, hero.withAlpha (0.0f), 72.0f, 0, false);
        g.setGradientFill (left); g.fillRect (0.0f, plateTop, 72.0f, 146.0f);
        ColourGradient right (hero.withAlpha (0.0f), bounds.getRight() - 72.0f, 0, hero.withAlpha (0.7f), bounds.getRight(), 0, false);
        g.setGradientFill (right); g.fillRect (bounds.getRight() - 72.0f, plateTop, 72.0f, 146.0f);
    }

    // la bande : du bord de chaque galette vers les bras de tension de la photo
    const float rl = reelSize * fracL * 0.5f, rr = reelSize * fracR * 0.5f;
    const Colour tapeColour = Colour::fromRGB ((uint8) (60 + 40 * age), (uint8) (42 + 28 * age), (uint8) (30 + 18 * age));
    g.setColour (tapeColour);
    const float armY = plateTop + 18.0f;
    g.drawLine (centreL.x - rl, centreL.y, 62.0f, armY, 1.6f);
    g.drawLine (centreR.x + rr, centreR.y, 842.0f, armY, 1.6f);

    // galettes puis bobines (vraie photo, détourée) qui tournent
    for (int side = 0; side < 2; ++side)
    {
        const auto c = side == 0 ? centreL : centreR;
        drawPack (g, c, side == 0 ? rl : rr, age);
        const auto box = Rectangle<float> (reelSize, reelSize).withCentre (c);
        for (int i = 5; i > 0; --i)
        {
            g.setColour (Colours::black.withAlpha (0.07f));
            g.fillEllipse (box.translated (0.0f, 9.0f).expanded ((float) i * 2.2f));
        }
        const float angle = side == 0 ? angleL : angleR;
        g.setOpacity (1.0f); // sinon l'image hérite de la transparence des ombres
        const float scale = reelSize / (float) reel.getWidth();
        g.drawImageTransformed (reel, AffineTransform::translation (-reel.getWidth() * 0.5f, -reel.getHeight() * 0.5f)
                                          .rotated (angle).scaled (scale).translated (c.x, c.y));
        g.setColour (Colours::black.withAlpha (0.08f));
        g.fillEllipse (box);
    }
    g.restoreState();

    // vieillissement : teinte chaude, un peu plus sombre, grain
    if (age > 0.001f)
    {
        g.setColour (Colour (0xff70421a).withAlpha (age * 0.16f));
        g.fillAll();
        g.setColour (Colours::black.withAlpha (age * 0.12f));
        g.fillAll();
    }
    if (age > 0.15f)
    {
        auto& rnd = const_cast<Random&> (random);
        const int dots = (int) (700.0f * (age - 0.15f));
        g.setColour (Colour (0xffffe9cc).withAlpha (jmin (0.18f, (age - 0.15f) * 0.25f)));
        for (int i = 0; i < dots; ++i)
            g.fillRect (rnd.nextFloat() * bounds.getWidth(), rnd.nextFloat() * bounds.getHeight(), 1.0f, 1.0f);
    }

    // légende centrée entre les bobines
    const auto nameFont = fonts.ui (14.0f, true), detailFont = fonts.ui (14.0f);
    const String name = ageName (age * 100.0f), detail = String (CharPointer_UTF8 (" \xc2\xb7 ")) + speedDetail (s.speed, age);
    const float w1 = textWidth (nameFont, name), w2 = textWidth (detailFont, detail);
    const float x0 = bounds.getCentreX() - (w1 + w2) * 0.5f;
    g.setFont (nameFont); g.setColour (colours::cream);
    g.drawText (name, Rectangle<float> (x0, 14.0f, w1 + 2.0f, 20.0f), Justification::centredLeft, false);
    g.setFont (detailFont); g.setColour (colours::muted);
    g.drawText (detail, Rectangle<float> (x0 + w1, 14.0f, w2 + 2.0f, 20.0f), Justification::centredLeft, false);
    const float wowPct = 0.02f + s.wow * (0.08f + age * 0.9f);
    const String wowText = s.stop ? String (CharPointer_UTF8 ("Bande arr\xc3\xaat\xc3\xa9" "e")) : "Wow " + commaNumber (wowPct, 2) + " %";
    g.drawText (wowText, Rectangle<float> (0.0f, 36.0f, bounds.getWidth(), 20.0f), Justification::centred, false);
}

// ------------------------------------------------------------------ réglages texte

InlineValue::InlineValue (Processor& p, const String& id, const String& l, const Fonts& f)
    : param (*p.params.getParameter (id)), label (l), fonts (f),
      attachment (param, [this] (float) { repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

String InlineValue::valueText() const
{
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (&param)) return choice->getCurrentChoiceName();
    const float v = param.convertFrom0to1 (param.getValue());
    return (v > 0.04f ? "+" : "") + commaNumber (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB";
}

int InlineValue::idealWidth() const
{
    return roundToInt (textWidth (fonts.ui (14.0f), label + " ") + textWidth (fonts.ui (14.0f), valueText()) + 6.0f);
}

void InlineValue::paint (Graphics& g)
{
    const auto f = fonts.ui (14.0f);
    g.setFont (f);
    const String l = label + " ";
    const float lw = textWidth (f, l);
    g.setColour (colours::muted);
    g.drawText (l, Rectangle<float> (0.0f, 0.0f, lw + 1.0f, (float) getHeight()), Justification::centredLeft, false);
    g.setColour (isMouseOverOrDragging() ? colours::accent : colours::cream);
    g.drawText (valueText(), Rectangle<float> (lw, 0.0f, (float) getWidth() - lw, (float) getHeight()), Justification::centredLeft, false);
}

void InlineValue::mouseDown (const MouseEvent&)
{
    dragStart = param.getValue();
    dragging = false;
}

void InlineValue::mouseDrag (const MouseEvent& e)
{
    if (dynamic_cast<AudioParameterChoice*> (&param) != nullptr || e.getDistanceFromDragStart() < 3) return;
    if (! dragging) { dragging = true; attachment.beginGesture(); }
    const float range = param.getNormalisableRange().getRange().getLength();
    const float delta = (float) -e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.02f : 0.1f) / range;
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, dragStart + delta)));
}

void InlineValue::mouseUp (const MouseEvent&)
{
    if (dragging) { attachment.endGesture(); dragging = false; return; }
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (&param))
    {
        PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        for (int i = 0; i < choice->choices.size(); ++i)
            menu.addItem (i + 1, choice->choices[i], true, i == choice->getIndex());
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, choice] (int r)
        {
            if (r > 0) attachment.setValueAsCompleteGesture ((float) (r - 1));
            ignoreUnused (choice);
        });
    }
}

void InlineValue::mouseDoubleClick (const MouseEvent&)
{
    if (dynamic_cast<AudioParameterChoice*> (&param) == nullptr)
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

// ------------------------------------------------------------------ éditeur

Editor::Editor (Processor& p)
    : AudioProcessorEditor (p), proc (p),
      look ((fonts.regular = Typeface::createSystemTypefaceFor (BinaryData::geist_regular_ttf, BinaryData::geist_regular_ttfSize),
             fonts.semibold = Typeface::createSystemTypefaceFor (BinaryData::geist_semibold_ttf, BinaryData::geist_semibold_ttfSize),
             fonts)),
      deck (p, fonts),
      hiss (p, "hiss", "Souffle", fonts), speed (p, "speed", "Vitesse", fonts), output (p, "output", "Sortie", fonts),
      stopAttachment (*p.params.getParameter ("stop"), [this] (float v)
      {
          stopButton.setToggleState (v > 0.5f, dontSendNotification);
          stopButton.setButtonText (String::fromUTF8 (v > 0.5f ? "Relancer la bande" : "Arrêt de bande"));
      }),
      badge (colours::muted, colours::cream)
{
    setLookAndFeel (&look);
    logo = ImageCache::getFromMemory (BinaryData::swell_mark_png, BinaryData::swell_mark_pngSize);
    addAndMakeVisible (deck);

    const std::initializer_list<std::tuple<const char*, const char*, bool>> defs {
        { "drive", "Drive", false }, { "wow", "Wow", false }, { "age", "Age", true }, { "tone", "Tone", false }, { "mix", "Mix", false } };
    for (auto& [id, label, big] : defs)
    {
        Knob k { id, label, big, std::make_unique<Slider> (Slider::RotaryVerticalDrag, Slider::NoTextBox), nullptr };
        k.slider->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        k.slider->setMouseDragSensitivity (220);
        k.slider->setVelocityBasedMode (false);
        if (String (id) == "tone") k.slider->getProperties().set ("bipolar", true);
        k.slider->onValueChange = [this] { repaint (0, 400, W, 220); };
        addAndMakeVisible (*k.slider);
        k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.params, id, *k.slider);
        if (auto* prm = proc.params.getParameter (id))
            k.slider->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        knobs.push_back (std::move (k));
    }

    for (auto* c : std::initializer_list<Component*> { &hiss, &speed, &output, &stopButton })
        addChildComponent (c);
    stopButton.setClickingTogglesState (false);
    stopButton.onClick = [this] { stopAttachment.setValueAsCompleteGesture (stopButton.getToggleState() ? 0.0f : 1.0f); };
    stopAttachment.sendInitialUpdate();
    addAndMakeVisible (badge);

    setSize (W, H);
    startTimerHz (15);
}

Editor::~Editor() { setLookAndFeel (nullptr); }

void Editor::resized()
{
    deck.setBounds (heroArea);

    // rangée de potards : comme la maquette, répartis sur 792 px, alignés en bas
    const float centres[] { 139.0f, 309.5f, 480.0f, 650.5f, 821.0f };
    const int bottomDial = 416 + 18 + 8 + 116; // bas du gros potard
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int size = knobs[i].big ? 116 : 92;
        knobs[i].slider->setBounds (Rectangle<int> (size, size).withCentre ({ roundToInt (centres[i]), bottomDial - size / 2 }));
    }

    int x = 116;
    for (auto* v : { &hiss, &speed, &output })
    {
        const int w = v->idealWidth() + 4;
        v->setBounds (x, 620, w, 24);
        x += w + 24;
    }
    stopButton.setBounds (x - 8, 618, 150, 28);
    badge.setBounds (W - 28 - 260, 620, 260, 24);
}

String Editor::valueText (const Knob& k) const
{
    const float v = (float) k.slider->getValue();
    if (k.id == "age") return ageName (v);
    if (k.id == "tone") return (v > 0.04f ? "+" : "") + commaNumber (std::abs (v) < 0.05f ? 0.0f : v, 1);
    return String (roundToInt (v)) + " %";
}

void Editor::paint (Graphics& g)
{
    g.fillAll (colours::background);

    // en-tête
    if (logo.isValid())
        g.drawImage (logo, Rectangle<float> (28.0f, 23.0f, 34.0f, 30.0f), RectanglePlacement::centred);
    g.setColour (colours::cream);
    g.setFont (fonts.ui (15.0f, true).withExtraKerningFactor (0.42f));
    g.drawText ("SWELL", Rectangle<float> (74.0f, 26.0f, 120.0f, 24.0f), Justification::centredLeft, false);
    g.setColour (colours::muted);
    g.setFont (fonts.ui (15.0f));
    g.drawText ("Tape", Rectangle<float> (164.0f, 26.0f, 80.0f, 24.0f), Justification::centredLeft, false);

    const auto pill = pillBounds();
    g.setColour (Colours::white.withAlpha (hover == 2 ? 0.09f : 0.06f));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    drawChevron (g, { pill.getX() + 20.0f, pill.getCentreY() }, 5.0f, -1, hover == 1 ? colours::cream : colours::muted);
    drawChevron (g, { pill.getRight() - 20.0f, pill.getCentreY() }, 5.0f, 1, hover == 3 ? colours::cream : colours::muted);
    const auto pf = fonts.ui (14.5f, true);
    const String pname = proc.getProgramName (proc.getCurrentProgram());
    const float pw = textWidth (pf, pname);
    g.setColour (colours::cream);
    g.setFont (pf);
    g.drawText (pname, Rectangle<float> (pill.getCentreX() - (pw + 14.0f) * 0.5f, pill.getY(), pw + 2.0f, pill.getHeight()), Justification::centredLeft, false);
    drawChevron (g, { pill.getCentreX() + (pw + 14.0f) * 0.5f - 3.0f, pill.getCentreY() + 1.0f }, 3.5f, 2, colours::muted);

    const auto dots = dotsBounds();
    g.setColour (hover == 4 ? colours::accent : colours::cream);
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre ({ dots.getCentreX() + (float) i * 7.5f, dots.getCentreY() }));

    // libellés et valeurs des potards
    for (auto& k : knobs)
    {
        const auto b = k.slider->getBounds().toFloat();
        g.setFont (k.big ? fonts.ui (15.0f, true) : fonts.ui (15.0f));
        g.setColour (k.big ? colours::cream : colours::muted);
        g.drawText (k.label, Rectangle<float> (b.getCentreX() - 60.0f, b.getY() - 26.0f, 120.0f, 18.0f), Justification::centred, false);
        g.setFont (fonts.ui (17.0f));
        g.setColour (colours::cream);
        g.drawText (valueText (k), Rectangle<float> (b.getCentreX() - 70.0f, b.getBottom() + 8.0f, 140.0f, 22.0f), Justification::centred, false);
    }

    // « Plus »
    const auto plus = plusBounds();
    const auto dot = Rectangle<float> (12.0f, 12.0f).withCentre ({ plus.getX() + 6.0f, plus.getCentreY() });
    if (moreOpen) { g.setColour (colours::accent); g.fillEllipse (dot); }
    else { g.setColour (hover == 5 ? colours::cream : colours::muted); g.drawEllipse (dot.reduced (1.0f), 2.0f); }
    g.setColour (hover == 5 ? colours::cream : colours::muted);
    g.setFont (fonts.ui (14.0f));
    g.drawText ("Plus", plus.withTrimmedLeft (21.0f), Justification::centredLeft, false);
}

void Editor::mouseMove (const MouseEvent& e)
{
    const auto p = e.position;
    const auto pill = pillBounds();
    int h = 0;
    if (pill.contains (p)) h = p.x < pill.getX() + 44.0f ? 1 : p.x > pill.getRight() - 44.0f ? 3 : 2;
    else if (dotsBounds().contains (p)) h = 4;
    else if (plusBounds().contains (p)) h = 5;
    if (h != hover) { hover = h; setMouseCursor (h ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor); repaint(); }
}

void Editor::mouseUp (const MouseEvent& e)
{
    switch (hover)
    {
        case 1: stepPreset (-1); break;
        case 3: stepPreset (1); break;
        case 2: showPresetMenu(); break;
        case 4: showMoreMenu(); break;
        case 5: setMoreOpen (! moreOpen); break;
        default: break;
    }
    ignoreUnused (e);
}

void Editor::setMoreOpen (bool open)
{
    moreOpen = open;
    for (auto* c : std::initializer_list<Component*> { &hiss, &speed, &output, &stopButton })
        c->setVisible (moreOpen);
    repaint();
}

void Editor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    repaint();
}

void Editor::showPresetMenu()
{
    PopupMenu menu;
    menu.setLookAndFeel (&look);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        menu.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    const auto pill = pillBounds().toNearestInt();
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (pill)).withMinimumWidth (pill.getWidth()),
                        [this] (int r) { if (r > 0) { proc.setCurrentProgram (r - 1); repaint(); } });
}

void Editor::showMoreMenu()
{
    PopupMenu menu;
    menu.setLookAndFeel (&look);
    menu.addItem (1, CharPointer_UTF8 ("\xc3\x80 propos de Swell Tape"));
    menu.addItem (2, "swell-audio.fr");
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (dotsBounds().toNearestInt())),
                        [] (int r)
    {
        if (r == 1)
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, "Swell Tape " SWELL_VERSION,
                String (CharPointer_UTF8 ("Bande studio et cassette lo-fi, de Swell Audio.\n\n"
                                          "Photo des bobines et de la platine : \xc2\xab Studer B67 reel-to-reel audio tape recorder \xc2\xbb, "
                                          "Robert Taylor (Stirling, ON, Canada), licence CC BY 2.0, d\xc3\xa9" "coup\xc3\xa9" "e et retouch\xc3\xa9" "e.")));
        else if (r == 2)
            URL ("https://swell-audio.fr").launchInDefaultBrowser();
    });
}
} // namespace swelltape
