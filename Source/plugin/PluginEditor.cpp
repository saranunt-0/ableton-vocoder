#include "PluginEditor.h"

namespace id = vv::params::id;
using namespace vv::ui;

namespace
{
constexpr int kWidth = 1000;
constexpr int kHeight = 650;
constexpr int kMargin = 16;
constexpr int kGap = 10;
constexpr int kTitleHeight = 22;
constexpr float kMeterFloorDb = -60.0f;
constexpr int kTimerHz = 30;

juce::Font font (float height, bool bold = false)
{
    auto options = juce::FontOptions (height);
    return juce::Font (bold ? options.withStyle ("Bold") : options);
}

juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz));
}
} // namespace

// ---------------------------------------------------------------------------
VintageLookAndFeel::VintageLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::cream);
    setColour (juce::ComboBox::backgroundColourId, colours::knob);
    setColour (juce::ComboBox::textColourId, colours::cream);
    setColour (juce::ComboBox::outlineColourId, colours::sectionEdge);
    setColour (juce::ComboBox::arrowColourId, colours::amber);
    setColour (juce::PopupMenu::backgroundColourId, colours::section);
    setColour (juce::PopupMenu::textColourId, colours::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour (juce::ToggleButton::tickColourId, colours::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::dimCream);
    setColour (juce::ToggleButton::textColourId, colours::cream);
}

void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Scale arc: dim track plus amber value arc.
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (colours::sectionEdge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::amber);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob cap with a soft top-light gradient.
    const float capRadius = radius * 0.72f;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (colours::knob.brighter (0.35f), centre.x, cap.getY(),
                                             colours::knob.darker (0.5f), centre.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);

    // Pointer.
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capRadius, 3.0f, capRadius * 0.55f, 1.5f);
    g.setColour (colours::cream);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

// ---------------------------------------------------------------------------
void BandMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (area, 6.0f);

    auto inner = area.reduced (12.0f, 10.0f);
    auto labels = inner.removeFromBottom (14.0f);

    const int n = juce::jmax (1, engine.getNumBands());
    const float slot = inner.getWidth() / static_cast<float> (n);
    const float barWidth = slot * 0.7f;
    constexpr int segments = 16;
    const float segHeight = inner.getHeight() / static_cast<float> (segments);

    for (int b = 0; b < n; ++b)
    {
        const float db = vv::gainToDb (engine.getBandLevel (b));
        const float norm = juce::jlimit (0.0f, 1.0f, (db - kMeterFloorDb) / -kMeterFloorDb);
        const int lit = juce::roundToInt (norm * segments);
        const float bx = inner.getX() + slot * static_cast<float> (b) + (slot - barWidth) * 0.5f;

        for (int s = 0; s < segments; ++s)
        {
            const auto colour = s >= lit ? colours::ledOff
                                         : (s >= segments - 2 ? juce::Colours::orangered
                                                              : (s >= segments - 5 ? colours::ledWarn : colours::amber));
            const float sy = inner.getBottom() - segHeight * static_cast<float> (s + 1);
            g.setColour (colour);
            g.fillRoundedRectangle (bx, sy + 1.0f, barWidth, segHeight - 2.0f, 1.5f);
        }
    }

    // Frequency labels for the first, middle and last band (VSM-201 spacing).
    g.setColour (colours::dimCream);
    g.setFont (font (11.0f));
    const float lo = vv::VocoderBank::kLowestBandHz, hi = vv::VocoderBank::kHighestBandHz;
    for (int b : { 0, n / 2, n - 1 })
    {
        const float hz = lo * std::pow (hi / lo, n > 1 ? static_cast<float> (b) / static_cast<float> (n - 1) : 0.0f);
        const auto cell = juce::Rectangle<float> (inner.getX() + slot * static_cast<float> (b), labels.getY(), slot, labels.getHeight());
        g.drawText (formatHz (hz), cell.expanded (12.0f, 0.0f), juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------
VintageVocoderEditor::Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text)
    : attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void VintageVocoderEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

VintageVocoderEditor::Choice::Choice (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text)
{
    // Items must exist before the attachment syncs the selection.
    box.addItemList (state.getParameter (paramId)->getAllValueStrings(), 1);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    addAndMakeVisible (box);
    addAndMakeVisible (label);
}

void VintageVocoderEditor::Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    box.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 8, 84), 26));
}

VintageVocoderEditor::Toggle::Toggle (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text)
    : attachment (state, paramId, button)
{
    button.setButtonText ("On");
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    addAndMakeVisible (button);
    addAndMakeVisible (label);
}

void VintageVocoderEditor::Toggle::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    button.setBounds (area.withSizeKeepingCentre (56, 26));
}

// ---------------------------------------------------------------------------
VintageVocoderEditor::VintageVocoderEditor (VintageVocoderProcessor& p)
    : AudioProcessorEditor (&p), processor (p), meter (p.engine)
{
    addAndMakeVisible (meter);

    sections = {
        { "MODULATOR", { knob (id::inGain, "Input"), knob (id::gate, "Gate") }, {} },
        { "VOCODER", { choice (id::bands, "Bands"), knob (id::resonance, "Resonance"), knob (id::formant, "Formant"),
                       knob (id::attack, "Env Attack"), knob (id::release, "Env Release"), knob (id::brightness, "Brightness"),
                       knob (id::sibilance, "Sibilance"), knob (id::air, "Air") }, {} },
        { "CARRIER", { knob (id::saw, "Saw"), knob (id::pulse, "Pulse"), knob (id::pulseWidth, "Width"), knob (id::sub, "Sub"),
                       knob (id::noise, "Noise"), knob (id::detune, "Detune"), knob (id::octave, "Octave") }, {} },
        { "OUTPUT", { knob (id::ensemble, "Ensemble"), knob (id::dry, "Dry Voice"), knob (id::outGain, "Output"),
                      toggle (id::listen, "Listen") }, {} },
        { "KEYBOARD", { knob (id::ampAttack, "Attack"), knob (id::ampRelease, "Release"), knob (id::glide, "Glide"),
                        choice (id::voiceMode, "Mode"), knob (id::bendRange, "Bend"), knob (id::vibRate, "Vib Rate"),
                        knob (id::vibDepth, "Vib Depth") }, {} },
    };

    // Set after the controls exist so every slider rebuilds its value box with these colours.
    setLookAndFeel (&lookAndFeel);
    setSize (kWidth, kHeight);
    startTimerHz (kTimerHz);
}

VintageVocoderEditor::~VintageVocoderEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Component* VintageVocoderEditor::knob (const char* paramId, const juce::String& label)
{
    controls.push_back (std::make_unique<Knob> (processor.state, paramId, label));
    addAndMakeVisible (*controls.back());
    return controls.back().get();
}

juce::Component* VintageVocoderEditor::choice (const char* paramId, const juce::String& label)
{
    controls.push_back (std::make_unique<Choice> (processor.state, paramId, label));
    addAndMakeVisible (*controls.back());
    return controls.back().get();
}

juce::Component* VintageVocoderEditor::toggle (const char* paramId, const juce::String& label)
{
    controls.push_back (std::make_unique<Toggle> (processor.state, paramId, label));
    addAndMakeVisible (*controls.back());
    return controls.back().get();
}

void VintageVocoderEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 0);
    headerArea = area.removeFromTop (60);
    meter.setBounds (area.removeFromTop (118));
    statusArea = area.removeFromTop (28);

    const int rowHeight = (area.getHeight() - kMargin - 2 * kGap) / 3;
    auto row1 = area.removeFromTop (rowHeight);
    area.removeFromTop (kGap);
    auto row2 = area.removeFromTop (rowHeight);
    area.removeFromTop (kGap);
    auto row3 = area.removeFromTop (rowHeight);

    // Each row is split between its sections in proportion to their control count.
    auto layoutRow = [this] (juce::Rectangle<int> row, std::initializer_list<size_t> indices, int extraCells)
    {
        int cells = extraCells;
        for (auto i : indices)
            cells += static_cast<int> (sections[i].controls.size());
        const int gaps = static_cast<int> (indices.size()) - 1 + (extraCells > 0 ? 1 : 0);
        const float cellWidth = static_cast<float> (row.getWidth() - gaps * kGap) / static_cast<float> (cells);

        for (auto i : indices)
        {
            auto& s = sections[i];
            const int w = juce::roundToInt (cellWidth * static_cast<float> (s.controls.size()));
            s.bounds = row.removeFromLeft (w);
            row.removeFromLeft (kGap);

            auto inner = s.bounds.reduced (4, 0);
            inner.removeFromTop (kTitleHeight);
            const int cw = inner.getWidth() / static_cast<int> (s.controls.size());
            for (auto* c : s.controls)
                c->setBounds (inner.removeFromLeft (cw).reduced (2, 4));
        }
        return row; // whatever is left over
    };

    layoutRow (row1, { 0, 1 }, 0);
    layoutRow (row2, { 2, 3 }, 0);
    routingArea = layoutRow (row3, { 4 }, 4);
}

void VintageVocoderEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // Header
    auto header = headerArea;
    g.setColour (colours::cream);
    g.setFont (font (26.0f, true));
    const juce::String title = VintageVocoderProcessor::isInstrument ? "VINTAGE VOCODER" : "VINTAGE VOCODER FX";
    g.drawText (title, header.removeFromLeft (360), juce::Justification::centredLeft);
    g.setColour (colours::dimCream);
    g.setFont (font (12.0f));
    g.drawText ("Channel vocoder  |  8-24 bands  |  MIDI carrier", header.removeFromLeft (300), juce::Justification::centredLeft);

    auto leds = headerArea.withTrimmedLeft (headerArea.getWidth() - 300).withSizeKeepingCentre (300, 24);
    const float inDb = vv::gainToDb (inputLevel);
    drawLed (g, leds.removeFromLeft (100), "VOICE", inDb > -50.0f ? colours::ledOn : (inDb > -70.0f ? colours::ledWarn : colours::ledOff));
    drawLed (g, leds.removeFromLeft (100), "MIDI", midiFlashFrames > 0 ? colours::ledOn : colours::ledOff);
    drawLed (g, leds, "KEYS " + juce::String (activeVoices), activeVoices > 0 ? colours::ledOn : colours::ledOff);

    // Routing hint: tells you which half of the Ableton routing is missing.
    juce::String hint;
    if (inDb <= -60.0f)
        hint = VintageVocoderProcessor::isInstrument
                   ? "No voice signal: open this device's Sidechain section and pick your vocal/mic track."
                   : "No voice signal: put this on the vocal/mic track and set its Monitor to In (or arm it).";
    else if (framesSinceMidi > 3 * kTimerHz && activeVoices == 0)
        hint = VintageVocoderProcessor::isInstrument
                   ? "Voice OK. Arm this MIDI track and play your keyboard."
                   : "Voice OK. On a MIDI track set 'MIDI To' -> this track -> Vintage Vocoder FX, arm it, then play.";
    else
        hint = "Voice and keys OK.";
    g.setColour (colours::dimCream);
    g.setFont (font (13.0f));
    g.drawText (hint, statusArea, juce::Justification::centredLeft);

    // Section panels
    for (const auto& s : sections)
    {
        g.setColour (colours::section);
        g.fillRoundedRectangle (s.bounds.toFloat(), 6.0f);
        g.setColour (colours::sectionEdge);
        g.drawRoundedRectangle (s.bounds.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (colours::amber);
        g.setFont (font (12.0f, true));
        g.drawText (s.title, s.bounds.withHeight (kTitleHeight).reduced (10, 0), juce::Justification::centredLeft);
    }

    // Quick reference panel
    if (! routingArea.isEmpty())
    {
        g.setColour (colours::section);
        g.fillRoundedRectangle (routingArea.toFloat(), 6.0f);
        g.setColour (colours::amber);
        g.setFont (font (12.0f, true));
        auto text = routingArea.reduced (10, 4);
        g.drawText ("TIPS", text.removeFromTop (kTitleHeight - 4), juce::Justification::centredLeft);
        g.setColour (colours::dimCream);
        g.setFont (font (12.0f));
        g.drawFittedText ("Listen = hear the carrier synth alone (checks MIDI).\n"
                          "Raise Gate if background noise drones.\n"
                          "Sibilance/Air bring back 's' and 't' sounds.\n"
                          "Mod wheel adds vibrato. Sustain pedal holds chords.",
                          text, juce::Justification::topLeft, 6);
    }
}

void VintageVocoderEditor::drawLed (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour) const
{
    auto dot = area.removeFromLeft (16).withSizeKeepingCentre (10, 10).toFloat();
    g.setColour (colour);
    g.fillEllipse (dot);
    if (colour != colours::ledOff)
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (3.0f));
    }
    g.setColour (colours::cream);
    g.setFont (font (12.0f, true));
    g.drawText (text, area.withTrimmedLeft (4), juce::Justification::centredLeft);
}

void VintageVocoderEditor::timerCallback()
{
    const auto midiCount = processor.engine.getMidiEventCount();
    if (midiCount != lastMidiCount)
    {
        lastMidiCount = midiCount;
        midiFlashFrames = kTimerHz / 6;
        framesSinceMidi = 0;
    }
    else
    {
        midiFlashFrames = juce::jmax (0, midiFlashFrames - 1);
        framesSinceMidi = juce::jmin (framesSinceMidi + 1, 1000000);
    }

    inputLevel = processor.engine.getInputLevel();
    activeVoices = processor.engine.getActiveVoiceCount();

    meter.repaint();
    repaint (headerArea);
    repaint (statusArea);
}
