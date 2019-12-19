/*
 This file is part of micronau.
 Copyright (c) 2013 - David Smitley
 
 Permission is granted to use this software under the terms of the GPL v2 (or any later version)
 
 Details can be found at: www.gnu.org/licenses
*/

#ifndef __PLUGINPROCESSOR_H_CCAD67E2__
#define __PLUGINPROCESSOR_H_CCAD67E2__

#include "../JuceLibraryCode/JuceHeader.h"
#include "IonSysex.h"

#define MIDI_OUT_IDX 0
#define MIDI_IN_IDX 1

//==============================================================================
class ext_param {
public:
    ext_param(IonSysexParam *p, int idx) : param(p), index(idx) {
    }
    IonSysexParam *param;
    int index;
};

//==============================================================================
/**
*/
class MicronauAudioProcessor  : public AudioProcessor,
                                public MidiInputCallback
{
public:
    //==============================================================================
    MicronauAudioProcessor();
    ~MicronauAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

    const String getParameterName (int index);
    const String getParameterText (int index);

    float getParameterMinValue (int parameterIndex);
    float getParameterMaxValue (int parameterIndex);
    float getParameterDefaultValue (int parameterIndex);
    int getParameterNumSteps (int parameterIndex);
    bool isMetaParameter (int parameterIndex) const;

    const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;
    bool silenceInProducesSilenceOut() const;
    double getTailLengthSeconds() const;

    //==============================================================================
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

    //==============================================================================
    void handleIncomingMidiMessage (MidiInput* source, const MidiMessage& message);
   
    //==============================================================================
    // micronau specific from here on down
    void sync_via_nrpn();
    void sync_via_sysex();
    void send_request();
 
    String get_midi_port(int in_out);
    void set_midi_port(int in_out, String p);
    void set_midi_chan(unsigned int chan);
    unsigned int get_midi_chan();
    String get_prog_name();
    void set_prog_name(String s) {params->set_prog_name(s);}

    int index_of_nrpn(int nrpn) const;
    IonSysexParam *param_of_nrpn(int nrpn);
    bool get_progchange() {return prog_changed;}
    void set_progchange(bool v) {prog_changed = v;}
    
    static int midi_find_port_by_name(int idx, String nm);
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicronauAudioProcessor)

    static const int MAX_MIDI_PORT_NAME = 80;
    static const int SYSEX_LEN = 432;
    typedef struct {
        unsigned char sysex[SYSEX_LEN];
        unsigned int midi_out_chan;
        unsigned char midi_in_port[MAX_MIDI_PORT_NAME];
        unsigned char midi_out_port[MAX_MIDI_PORT_NAME];
        unsigned int bank;
        unsigned int patch;
    } preset;
    void send_nrpn(int nrpn, int value, bool send_bank=true);
    void init_from_sysex(unsigned char *sysex);
    void send_bank_patch();

    IonSysexParams *params;
    Array<IonSysexParam*> nrpns;
    HashMap<int, ext_param *> param_by_nrpn;

	double sample_rate; // used for midi thru timing

    std::unique_ptr<MidiOutput> midi_out;
    unsigned int midi_out_channel;
    String midi_out_port;
    CriticalSection midi_port_lock; // use this to ensure midi port is not halfway changed when process block runs

    std::unique_ptr<MidiInput> midi_in;
    String midi_in_port;
    bool prog_changed;
};
#endif  // __PLUGINPROCESSOR_H_CCAD67E2__
