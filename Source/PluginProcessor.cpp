#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Range = juce::NormalisableRange<float>;

    std::unique_ptr<juce::AudioParameterFloat> makeParam (const char* id, const char* name,
                                                          Range range, float def,
                                                          const juce::String& suffix = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (suffix));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AcidLeadAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeParam ("wave",    "Wave (Saw>Sq)", Range (0.0f, 1.0f),                 0.0f));
    layout.add (makeParam ("detune",  "Detune",        Range (0.0f, 30.0f),                8.0f,  " ct"));
    layout.add (makeParam ("sub",     "Sub",           Range (0.0f, 1.0f),                 0.0f));
    layout.add (makeParam ("cutoff",  "Cutoff",        Range (20.0f, 18000.0f, 0.0f, 0.3f), 800.0f, " Hz"));
    layout.add (makeParam ("reso",    "Resonance",     Range (0.0f, 1.0f),                 0.7f));
    layout.add (makeParam ("envmod",  "Env Mod",       Range (0.0f, 1.0f),                 0.6f));
    layout.add (makeParam ("decay",   "Decay",         Range (30.0f, 1500.0f, 0.0f, 0.5f), 250.0f, " ms"));
    layout.add (makeParam ("accent",  "Accent",        Range (0.0f, 1.0f),                 0.7f));
    layout.add (makeParam ("glide",   "Glide",         Range (0.0f, 400.0f, 0.0f, 0.5f),   60.0f, " ms"));
    layout.add (makeParam ("drive",   "Drive",         Range (0.0f, 1.0f),                 0.3f));
    layout.add (makeParam ("release", "Release",       Range (5.0f, 600.0f, 0.0f, 0.5f),   40.0f, " ms"));
    layout.add (makeParam ("hpf",     "HPF",           Range (20.0f, 2000.0f, 0.0f, 0.35f), 20.0f, " Hz"));
    layout.add (makeParam ("analog",  "Analog",        Range (0.0f, 1.0f),                 0.3f));
    layout.add (makeParam ("volume",  "Volume",        Range (-24.0f, 6.0f),               -6.0f, " dB"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "filter", 1 }, "Filter",
        juce::StringArray { "Ladder (303)", "MS-20" }, 1));

    return layout;
}

AcidLeadAudioProcessor::AcidLeadAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

AcidParams AcidLeadAudioProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    AcidParams p;
    p.wave        = get ("wave");
    p.detuneCents = get ("detune");
    p.subLevel    = get ("sub");
    p.cutoffHz    = get ("cutoff");
    p.reso        = get ("reso");
    p.envMod      = get ("envmod");
    p.decayMs     = get ("decay");
    p.accent      = get ("accent");
    p.glideMs     = get ("glide");
    p.drive       = get ("drive");
    p.releaseMs   = get ("release");
    p.hpfHz       = get ("hpf");
    p.analog      = get ("analog");
    p.filterType  = (int) std::lround (get ("filter"));
    return p;
}

void AcidLeadAudioProcessor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
}

bool AcidLeadAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void AcidLeadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const auto params = readParams();
    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("volume")->load());

    buffer.clear();
    float* out = buffer.getWritePointer (0);

    int pos = 0;
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventPos > pos)
        {
            voice.render (out + pos, eventPos - pos, params);
            pos = eventPos;
        }

        if (msg.isNoteOn())
            voice.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            voice.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            voice.allNotesOff();
    }

    if (pos < numSamples)
        voice.render (out + pos, numSamples - pos, params);

    buffer.applyGain (0, 0, numSamples, gain);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* AcidLeadAudioProcessor::createEditor()
{
    return new AcidLeadAudioProcessorEditor (*this);
}

void AcidLeadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AcidLeadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidLeadAudioProcessor();
}
