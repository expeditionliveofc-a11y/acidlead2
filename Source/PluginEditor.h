#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AcidLeadAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcidLeadAudioProcessorEditor (AcidLeadAudioProcessor&);
    ~AcidLeadAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::vector<std::unique_ptr<Knob>> knobs;

    juce::ComboBox filterBox;
    juce::Label filterLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidLeadAudioProcessorEditor)
};
