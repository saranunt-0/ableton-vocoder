#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace vv::ui
{
namespace colours
{
    const juce::Colour panel { 0xff1c1916 };
    const juce::Colour section { 0xff27221d };
    const juce::Colour sectionEdge { 0xff3d352c };
    const juce::Colour cream { 0xffeadfc8 };
    const juce::Colour dimCream { 0xff9d927f };
    const juce::Colour amber { 0xffe8883a };
    const juce::Colour knob { 0xff35302a };
    const juce::Colour ledOn { 0xff7fe36a };
    const juce::Colour ledWarn { 0xffe8c23a };
    const juce::Colour ledOff { 0xff3a332b };
} // namespace colours

class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Bar graph of the modulator band envelopes, like the LED ladders on 70s vocoders.
class BandMeter : public juce::Component
{
public:
    explicit BandMeter (vv::VocoderEngine& e) : engine (e) {}
    void paint (juce::Graphics&) override;

private:
    vv::VocoderEngine& engine;
};

struct Section
{
    juce::String title;
    std::vector<juce::Component*> controls;
    juce::Rectangle<int> bounds;
};
} // namespace vv::ui

class VintageVocoderEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VintageVocoderEditor (VintageVocoderProcessor&);
    ~VintageVocoderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob : juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label);
        void resized() override;
        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    struct Choice : juce::Component
    {
        Choice (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label);
        void resized() override;
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Toggle : juce::Component
    {
        Toggle (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label);
        void resized() override;
        juce::ToggleButton button;
        juce::Label label;
        juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
    };

    void timerCallback() override;
    juce::Component* knob (const char* paramId, const juce::String& label);
    juce::Component* choice (const char* paramId, const juce::String& label);
    juce::Component* toggle (const char* paramId, const juce::String& label);
    void drawLed (juce::Graphics&, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour) const;

    VintageVocoderProcessor& processor;
    vv::ui::VintageLookAndFeel lookAndFeel;
    vv::ui::BandMeter meter;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<vv::ui::Section> sections;

    uint32_t lastMidiCount = 0;
    int midiFlashFrames = 0;
    int framesSinceMidi = 1000000;
    float inputLevel = 0.0f;
    int activeVoices = 0;
    juce::Rectangle<int> headerArea, statusArea, routingArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageVocoderEditor)
};
