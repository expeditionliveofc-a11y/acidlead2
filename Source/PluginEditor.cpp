#include "PluginEditor.h"

AcidLeadAudioProcessorEditor::AcidLeadAudioProcessorEditor (AcidLeadAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    const std::pair<const char*, const char*> defs[] =
    {
        { "wave",    "Wave"    }, { "detune", "Detune"  }, { "sub",    "Sub"     }, { "drive",  "Drive"   },
        { "analog",  "Analog"  }, { "glide",  "Glide"   }, { "volume", "Volume"  },
        { "hpf",     "HPF"     }, { "cutoff", "Cutoff"  }, { "reso",   "Reso"    }, { "envmod", "Env Mod" },
        { "decay",   "Decay"   }, { "accent", "Accent"  }, { "release","Release" }
    };

    for (const auto& d : defs)
    {
        auto k = std::make_unique<Knob>();

        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff9dff00));
        addAndMakeVisible (k->slider);

        k->label.setText (d.second, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (k->label);

        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, d.first, k->slider);

        knobs.push_back (std::move (k));
    }

    filterBox.addItemList ({ "Ladder (303)", "MS-20" }, 1);
    addAndMakeVisible (filterBox);
    filterLabel.setText ("Filter", juce::dontSendNotification);
    filterLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    filterLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (filterLabel);
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, "filter", filterBox);

    setSize (700, 380);
}

void AcidLeadAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15151a));

    g.setColour (juce::Colour (0xff9dff00));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("ACID LEAD", getLocalBounds().removeFromTop (40), juce::Justification::centred);
}

void AcidLeadAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (36);

    auto top = getLocalBounds().reduced (10).removeFromTop (36);
    filterBox.setBounds (top.removeFromRight (130).reduced (0, 6));
    filterLabel.setBounds (top.removeFromRight (50));

    constexpr int cols = 7, rows = 2;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + (int) (i % cols) * cellW,
                                          area.getY() + (int) (i / cols) * cellH,
                                          cellW, cellH).reduced (4);

        knobs[i]->label.setBounds (cell.removeFromTop (18));
        knobs[i]->slider.setBounds (cell);
    }
}
