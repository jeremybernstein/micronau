/*
 This file is part of micronau.
 Copyright (c) 2013 - David Smitley
 
 Permission is granted to use this software under the terms of the GPL v2 (or any later version)
 
 Details can be found at: www.gnu.org/licenses
*/

#include "micronau.h"
#include "micronauEditor.h"
#include "gui/MicronSlider.h"
#include "gui/MicronToggleButton.h"
#include "gui/MicronTabBar.h"
#include "gui/LcdLabel.h"
#include "gui/LcdTextEditor.h"
#include "gui/StdComboBox.h"
#include "gui/LookAndFeel.h"
#include "tracking.h"

//==============================================================================
MicronauAudioProcessorEditor::MicronauAudioProcessorEditor (MicronauAudioProcessor* ownerFilter)
    : AudioProcessorEditor (ownerFilter)
{
    owner = ownerFilter;

	undo_cur = undo_history.begin();
	allowNewSnapshots = true;

	LookAndFeel::setDefaultLookAndFeel( PluginLookAndFeel::getInstance() );

	background = ImageCache::getFromMemory (BinaryData::background_png,
                                     BinaryData::background_pngSize);

    buttonOffImg = ImageCache::getFromMemory (BinaryData::led_button_off_png,
                                           BinaryData::led_button_off_pngSize);
    buttonHoverImg = ImageCache::getFromMemory (BinaryData::led_button_dim_png,
                                           BinaryData::led_button_dim_pngSize);
    buttonOnImg = ImageCache::getFromMemory (BinaryData::led_button_on_png,
                                           BinaryData::led_button_on_pngSize);

	setFocusContainer(true);

    // create all of the gui elements and hook them up to the processor
    inverted_button_lf = new InvertedButtonLookAndFeel();
    
	create_osc(OSCS_X, OSCS_Y);
	create_env(ENVS_X, ENVS_Y);
	create_prefilt(PREFILT_X, PREFILT_Y);
	create_postfilt(POSTFILT_X, POSTFILT_Y);
	create_filter(FILT_X, FILT_Y);
	create_mod(MODMAT_X, MODMAT_Y);
	create_fm(FM_X, FM_Y);
	create_voice(VOICE_X, VOICE_Y);
	create_portamento(PORTA_X, PORTA_Y);
	create_xyz(XYZ_X, XYZ_Y);
	create_output(OUTPUT_X, OUTPUT_Y);
	create_lfo(LFO_X, LFO_Y);
	create_fx_and_tracking_tabs(FX_X,FX_Y);

	create_randomizer(RANDOMIZER_X, RANDOMIZER_Y);

	add_label("sync", SYNC_X, SYNC_Y, 35, 15);

	add_label("nrpn", SYNC_X + 40, SYNC_Y + 13, 35, 15);
	sync_nrpn = create_guibutton(SYNC_X + 40, SYNC_Y);

	add_label("sysex", SYNC_X + 85, SYNC_Y + 13, 35, 15);
    sync_sysex = create_guibutton(SYNC_X + 85, SYNC_Y);

	add_label("request", SYNC_X + 115, SYNC_Y + 13, 65, 15);
	request = create_guibutton(SYNC_X + 130, SYNC_Y);

	add_label("undo", LOGO_X - 1, LOGO_Y + 78, 35, 15);
    undo_button = create_guibutton(LOGO_X - 1, LOGO_Y + 65);

	add_label("redo", LOGO_X + 34, LOGO_Y + 78, 35, 15);
    redo_button = create_guibutton(LOGO_X + 34, LOGO_Y + 65);

	param_display = new LcdLabel("panel", "micronAU\nretroware");
    param_display->setJustificationType (Justification::centredLeft);
    param_display->setEditable (false, false, false);
    param_display->setColour (TextEditor::textColourId, Colours::black);
    param_display->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    param_display->setFont (Font (18.00f, Font::plain));
	param_display->setBounds(875,LCD_Y,170,LCD_Y + LCD_H);
	addAndMakeVisible(param_display);

    midi_in_menu = new StdComboBox ();
    midi_in_menu->setEditableText (false);
    midi_in_menu->addListener(this);
    addAndMakeVisible (midi_in_menu);
    midi_in_menu->setBounds(MIDI_X, MIDI_IN_Y, 100, MIDI_H);
	add_label("midi in", MIDI_X - 64, MIDI_IN_Y+2, 75, MIDI_H);

    midi_out_menu = new StdComboBox ();
    midi_out_menu->setEditableText (false);
    midi_out_menu->addListener(this);
    addAndMakeVisible (midi_out_menu);
    midi_out_menu->setBounds(MIDI_X, MIDI_OUT_Y, 100, MIDI_H);
	add_label("midi out", MIDI_X - 60, MIDI_OUT_Y+2, 75, MIDI_H);
 
    midi_out_chan = new StdComboBox ();
    midi_out_chan->setEditableText (false);
    midi_out_chan->addListener(this);
    addAndMakeVisible (midi_out_chan);
    midi_out_chan->setBounds(MIDI_X + 105, MIDI_OUT_Y, 33, MIDI_H);
    for (int i = 0; i < 16; i++) {
        midi_out_chan->addItem(String(i+1), i+1);
    }

    prog_name = new LcdTextEditor();
    addAndMakeVisible (prog_name);
    prog_name->setBounds(910, PROG_NAME_Y, 120, 15);
    prog_name->addListener(this);
    add_box(666, 910, PROG_NAME_Y+20, 100,  NULL, 0, NULL); // category selector
    add_box(100, 910, PROG_NAME_Y+40, 30, "Bank", 1, NULL);
    add_box(101, 950, PROG_NAME_Y+40, 30, "Prgm", 1, NULL);

    logo = Drawable::createFromImageData (BinaryData::logo_svg, BinaryData::logo_svgSize);

	// whole gui size
    setSize (1060, 670);

    update_midi_menu(MIDI_IN_IDX, true);
    update_midi_menu(MIDI_OUT_IDX, true);

	// setup gui updating timer, ensure it only actually updates after parameter change notifications from the plugin
	owner->addListener(this);
	paramHasChanged = false;
	startTimer (50);

	updateGuiComponents();
}

MicronauAudioProcessorEditor::~MicronauAudioProcessorEditor()
{
	if (owner)
		owner->removeListener(this);
}

Button* MicronauAudioProcessorEditor::create_guibutton(int x, int y, bool wantMicronButton)
{
	if (wantMicronButton)
	{
		MicronToggleButton* button = new MicronToggleButton("");
		button->addListener(this);
		button->setBounds(x, y, 20, 20);
		addAndMakeVisible(button);
		return button;
	}
	else
	{
		ImageButton* button = new ImageButton();
		button->setImages (true, true, false,
							buttonOffImg, 1.0f, Colours::transparentWhite,
							buttonHoverImg, 1.0f, Colours::transparentWhite,
							buttonOnImg, 1.0f, Colours::transparentWhite, 0.0f);
		button->addListener(this);
		button->setBounds(x, y, 35, 15);
		addAndMakeVisible(button);
		return button;
	}
}

MicronSlider* MicronauAudioProcessorEditor::create_guiknob(int x, int y, const char *text)
{
    MicronSlider *s;
    s = new MicronSlider;
    s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);

    if (text) {
        s->setTextBoxStyle(Slider::TextBoxBelow, true, 40, 15);
        s->setLabel(text);
    }

    s->addListener (this);
    s->setBounds(x, y, 40, 40);
	addAndMakeVisible(s);
	
	return s;
}


void MicronauAudioProcessorEditor::add_knob(int nprn, int x, int y, const char *text, Component *parent = NULL) {
    ext_slider *s;
    s = new ext_slider(owner, nprn);
    sliders.add(s);
    s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);

    if (text) {
        s->setTextBoxStyle(Slider::TextBoxBelow, true, 40, 15);
        s->setLabel(text);
    }

    s->addListener (this);
    s->setBounds(x, y, 40, 40);
    if (parent) {
        parent->addAndMakeVisible(s);
    } else {
        addAndMakeVisible(s);
    }
}

void MicronauAudioProcessorEditor::add_box(int nrpn, int x, int y, int width, const char *text, int loc, Component *parent = NULL){
    ext_combo *c;
    
    c = new ext_combo(owner, nrpn);
    c->setBounds(x, y, width, 15);
    c->addListener(this);
    if (parent) {
        parent->addAndMakeVisible(c);
    } else {
        addAndMakeVisible(c);
    }
    boxes.add(c);
    
    if (text != NULL) {
        Label *l;
        switch (loc) {
            case 0:
                // to the right
                l = new back_label(text, x + width + 4, y, 55, 15);
                l->setJustificationType (Justification::centredLeft);
                break;
            case 1:
                // below
                l = new back_label(text, x, y + 15 + 2, width, 15);
                break;
            case 2:
                l = new back_label(text, x - 3 - 55, y, 55, 15);
                l->setJustificationType (Justification::centredRight);
                break;
        }
		
        if (parent) {
            parent->addAndMakeVisible(l);
        } else {
            addAndMakeVisible(l);
        }
		
		labelComponents.add(l);
    }
}

void MicronauAudioProcessorEditor::add_button(int nrpn, int x, int y, const char *text, bool invert, Component *parent = NULL)
{
    ext_button *b;
    if (invert) {
        b = new ext_button(owner, nrpn, inverted_button_lf);
    } else {
        b = new ext_button(owner, nrpn, NULL);
    }
    
    buttons.add(b);
    b->setBounds(x, y, 20, 20);
    b->addListener (this);
    if (parent) {
        parent->addAndMakeVisible(b);
    } else {
        addAndMakeVisible(b);
    }
    
    if (text) {
        Label *l = new back_label(text, x + 17, y + 3, 55, 15);
		labelComponents.add(l);
        l->setJustificationType (Justification::centredLeft);
        if (parent) {
            parent->addAndMakeVisible(l);
        } else {
            addAndMakeVisible(l);
        }
    }
}

void MicronauAudioProcessorEditor::create_mod(int x, int y)
{
	add_group_box("modulation matrix", x, y, MODMAT_W, MODMAT_H);

	Component* mods[2];

    int y_off = 12;
    for (int n = 0; n < 2; n++) {
        Component *c = new Component();
        mods[n] = c;
        for (int i = 0; i < 6; i++) {
            String s = "mod ";
            s += (i + (n*6)) + 1;
            Label *l = new back_label(s, 40 + (i*117), 0, 40, 15);
			labelComponents.add(l);
            c->addAndMakeVisible(l);
            add_knob((i*4)+(n*24)+694, (i*117), 0 + y_off, "level", c);
            add_knob((i*4)+(n*24)+695, (i*117), 40 + y_off, "offset", c);

            add_box((i*4) + (n*24) + 692, 40 + (i*117), 4 + y_off, 75, "source", 1, c);
            add_box((i*4) + (n*24) + 693, 40 + (i*117), 4 + 40 + y_off, 75, "dest", 1, c);
        }
        c->setBounds(x, y, 700, 100);
    }

	mod_tabs = new MicronTabBar(TabbedButtonBar::TabsAtBottom);

	mod_tabs->setTabBarDepth (42);
	mod_tabs->setTabBarMargins(25, 0);
	mod_tabs->addTab ("1 - 6", Colour (0x00d3d3d3), mods[0], false);
	mod_tabs->addTab ("7 - 12", Colour (0x00d3d3d3), mods[1], false);
	mod_tabs->setCurrentTabIndex (0);
    mod_tabs->setBounds (x, y, MODMAT_W, MODMAT_H);

	addAndMakeVisible(mod_tabs);
}

void MicronauAudioProcessorEditor::add_group_box(const String& labelText, int x, int y, int w, int h)
{
	GroupComponent* groupBox = new GroupComponent(labelText,labelText);
	group_boxes.add(groupBox);

    groupBox->setColour (GroupComponent::outlineColourId, Colour (0xffdf0000));
    groupBox->setColour (GroupComponent::textColourId, Colour (0xffdf0000));

    groupBox->setBounds (x - GROUP_BOX_MARGIN_X, y - GROUP_BOX_MARGIN_Y, w + GROUP_BOX_MARGIN_X, h + GROUP_BOX_MARGIN_Y);

	addAndMakeVisible(groupBox);
}

void MicronauAudioProcessorEditor::add_label(const String& labelText, int x, int y, int w, int h)
{
	Label* l = new back_label(labelText, x, y, w, h);
	labelComponents.add(l);
	addAndMakeVisible(l);
}

void MicronauAudioProcessorEditor::create_osc(int x, int y)
{
	add_group_box("oscillators", x, y, OSCS_W, OSCS_H);

	for (int n = 0; n < 3; ++n)
	{
		int y_base;
		const char *labels[] = {"shape","octave", "semi", "fine", "wheel"};
		
		y_base = y + n * 65;
		
		for (int i = 0; i < 5; i++) {
			add_knob((n*6)+i+524, x + (i*40), y_base + 20, labels[i]);
		}

		add_box((n*6)+523, x + 50, y_base, 55, "waveform", 0);

		String s = "osc";
		s += (n+1);

		add_label(s, x, y_base, 55, 15);
	}
}

void MicronauAudioProcessorEditor::create_prefilt(int x, int y)
{
	add_group_box("pre filter mix", x, y, PREFILT_W, PREFILT_H);

	x += 30;
	y += 7;

    const char *labels[] = {"osc1", "osc2", "osc3", "ring", "noise", "ext in"};
    for (int i = 0; i < 6; i++) {
        add_knob(541 + i, x, y + (i * 40), NULL);
        add_knob(547 + i, x + 50, y + (i * 40), NULL);
        add_label(labels[i], x-33, y + i*40 + 14, 40, 15);
    }

    add_label("level", x + 5, y - 12, 40, 15);

    add_label("balance", x+45, y - 12, 60, 15);
}

void MicronauAudioProcessorEditor::create_postfilt(int x, int y)
{
	add_group_box("post filter mix", x, y, POSTFILT_W, POSTFILT_H);

	x += 35;
	y += 10;

    const char *labels[] = {"filter1", "filter2", "prefilter"};
    for (int i = 0; i < 3; i++) {
        add_knob(566 + i, x, y + (i * 40), NULL);
        add_knob(569 + i, x + 40, y + (i * 40), NULL);
        add_label(labels[i], x-40, y + i*40 + 12, 40, 15);
    }
    add_box(573, x+100, y+5, 58, "polarity", 1);
    add_box(572, x+100, y+45, 58, "input", 1);
    
    add_label("level", x, y - 15, 40, 15);
    add_label("balance", x+40, y - 15, 60, 15);
}

void MicronauAudioProcessorEditor::create_filter(int x, int y)
{
	add_group_box("filters", x, y, FILT_W, FILT_H);

    for (int i = 0; i < 2; i++) {
        int x_offset = 50;
        
        add_knob((i*6)+556, x + x_offset, y+(i*80), "cutoff");
        add_knob((i*6)+557, x + x_offset + 40, y+(i*80), "res");
        add_knob((i*6)+559, x + x_offset + 80, y+(i*80), "envamt");
        add_knob((i*6)+558, x + x_offset + 120, y+(i*80), "keytrk");

        add_box((i*6)+555, x + 108, y + 42 + (i * 80), 100, "type", 2);
    }
    add_knob(553, x, y+17, "f1>f2");
    add_knob(670, x, y+62, "offset");

    add_button(560, x + 10, y + 100, NULL, false);
}

void MicronauAudioProcessorEditor::create_env(int x, int y)
{
	add_group_box("envelopes", ENVS_X, ENVS_Y, ENVS_W, ENVS_H);

	for (int n = 0; n < 3; ++n)
	{
		int v_space  = 65;
		const char *labels[] = {"attack", "decay", "sustain", "time", "release"};
		int base_nrpns[] = {578, 580, 583, 582, 584};
		int x_offset = x + 35;
		for (int i = 0; i < 5; i++) {
			add_knob(base_nrpns[i] + (n * 13), x_offset + (i * 40), y + (n * v_space), labels[i]);
		}
		
		add_knob(586 + (n*13), x + 245, y + (n * v_space), "velocity");

		add_box(579 + (n*13), x_offset, y + 40 + (n * v_space), 35, NULL, 0);
		add_box(581 + (n*13), x_offset + 40, y + 40 + (n * v_space), 35, NULL, 0);
		add_box(585 + (n*13), x_offset + 160, y + 40 + (n * v_space), 35, NULL, 0);
		add_button(590 + (n*13), x_offset + 80,  y + 35 + (n * v_space), "pedal", false);
		
		add_box(588 + (n*13), x + 255 + 57, y + (n * v_space), 28, "free run", 0);
		add_box(587 + (n*13), x + 255 + 33, y + 19 + (n * v_space), 52, "reset", 0);
		add_box(589 + (n*13), x + 255 + 40, y + 38 + (n * v_space), 45, "loop", 0);

		add_label("amp", x, y+6, 40, 15);
		add_label("filter", x, y+6+v_space, 40, 15);
		add_label("env3", x, y+6+(v_space * 2), 40, 15);
	}
}

void MicronauAudioProcessorEditor::create_fm(int x, int y)
{
	add_group_box("fm/sync/noise", x, y, FM_W, FM_H);

	x -= 15;
	y += 0;

    add_box(520, x + 20, y + 0, 90, "sync", 1);
    add_box(522, x + 20, y + 40, 90, "fm algorithm", 1);
    add_box(554, x + 150, y + 0, 45, "noise", 1);
    add_knob(521, x + 120, y + 35, "amount");
}

void MicronauAudioProcessorEditor::create_voice(int x, int y)
{
	add_group_box("voice", x, y, VOICE_W, VOICE_H);

	x -= 15;
	y += 0;

    add_box(512, x + 20, y + 0, 40, "poly", 0);
    add_box(513, x + 20, y + 25, 20, "unison", 0);
    add_box(518, x + 20, y + 50, 40, "pitch wheel", 0);
    add_knob(519, x + 145, y, "drift");
    add_knob(514, x + 180, y, "detune");
}

void MicronauAudioProcessorEditor::create_portamento(int x, int y)
{
	add_group_box("portamento", x, y, PORTA_W, PORTA_H);

	x -= 15;
	y += 0;

    add_box(515, x + 20, y + 0, 50, "mode", 0);
    add_box(516, x + 20, y + 25, 70, "type", 0);
    add_knob(517, x + 180, y, "time");

}

void MicronauAudioProcessorEditor::create_xyz(int x, int y)
{
	add_group_box("xyz assign", x, y, XYZ_W, XYZ_H);

	x += 5;
	y += 2;

    add_box(411, x, y + 0, 110, "X", 0);
    add_box(412, x, y + 20, 110, "Y", 0);
    add_box(413, x, y + 40, 110, "Z", 0);
}

void MicronauAudioProcessorEditor::create_output(int x, int y)
{
	add_group_box("output", x, y, OUTPUT_W, OUTPUT_H);

	x += 19;
	y += 0;

    add_knob(742, x + 35, y, "fx1/fx2");
    add_knob(577, x + 75, y, "dry/wet");
    add_knob(575, x + 75, y+45, "level");
    add_knob(576, x + 75, y+90, "level");
    add_box(574, x-15, y + 50, 85, "drive", 1);
    add_label("effects", x - 8, y + 7, 50, 15);
    add_label("program", x + 28, y + 95, 50, 15);
}

void MicronauAudioProcessorEditor::create_tracking(int x, int y, Component* parent)
{
	trackgen = new SliderBank(owner, this);
	trackgen->setTopLeftPosition(x+80, y);
	parent->addAndMakeVisible(trackgen);

	x += 5;
	y += 0;

    add_box(630, x, y, 60, "input", 1, parent);
    add_box(631, x, y + 40, 60, "preset", 1, parent);
    add_box(632, x, y + 80, 25, "points", 0, parent);
	
}

void MicronauAudioProcessorEditor::create_lfo(int x, int y)
{
	add_group_box("lfos", x, y, LFO_W, LFO_H);

	x += 30;
	y += 0;

    const char *labels[] = {"lfo1", "lfo2", "s&h"};
    
    for (int i = 0; i < 3; i++) {
        add_box(671 + i, x, y + (i*70) + 3, 65, labels[i], 2);
        add_knob(618 + (i*4), x+70, y + (i*70), "rate");
		if (i == 2)
			add_knob(629, x+110, y + (i*70), "smooth");
		else
			add_knob(620 + (i*4), x+110, y + (i*70), "m1");
        add_box(619 + (i*4), x+78, y + (i*70) + 40, 60, "reset", 2);
        add_button(617 + (i*4), x, y + 23 + (i* 70), "sync", true);
    }
    add_box(628, x, y+200, 60, "input", 0);
}

void MicronauAudioProcessorEditor::create_randomizer(int x, int y)
{
	add_group_box("randomize", x, y, RANDOMIZER_W, RANDOMIZER_H);

	add_label("randomize", x, y + 47, 70, 15);
	randomizeButton = create_guibutton(x + 68, y+47);

	x += 3;
	y += 3;

	add_label("lock pitch", x+54, y-5, 50, 15);
	randomizeLockPitchButton = create_guibutton(x+71, y+7, true);

	randomizeAmtSlider = create_guiknob(x + 2, y, "amt");
	randomizeAmtSlider->setRange(0.0f, 1.0f);
	randomizeAmtSlider->setValue(0.2f, dontSendNotification);
	randomizeAmtSlider->setDoubleClickReturnValue(true, 0.2f);
	randomizeAmtSlider->setMouseDragSensitivity( 100.0f );
}


void MicronauAudioProcessorEditor::create_fx_and_tracking_tabs(int x, int y)
{
	add_group_box("fx/tracking", x, y, FX_W, FX_H);

	Component* fx1Tab = new Component;
	Component* fx2Tab = new Component;
	Component* trackingTab = new Component;

	create_fx1(0, 0, fx1Tab);
	create_fx2(0, 0, fx2Tab);
	create_tracking(0, 0, trackingTab);

	fx_and_tracking_tabs = new MicronTabBar(TabbedButtonBar::TabsAtBottom);
	fx_and_tracking_tabs->setTabBarMargins(10,3);
	fx_and_tracking_tabs->setTabBarDepth (45);

	fx_and_tracking_tabs->addTab ("fx1", Colour (0x00d3d3d3), fx1Tab, true);
	fx_and_tracking_tabs->addTab ("fx2", Colour (0x00d3d3d3), fx2Tab, true);
	fx_and_tracking_tabs->addTab ("tgen", Colour (0x00d3d3d3), trackingTab, true);
	fx_and_tracking_tabs->setCurrentTabIndex (0);
    fx_and_tracking_tabs->setBounds (x, y, FX_W, FX_H);

	addAndMakeVisible(fx_and_tracking_tabs);
}

void MicronauAudioProcessorEditor::create_fx1(int x, int y, Component* parent)
{
    int i;
    int o = 0;

	add_box(800, 40, 10, 80, "type", 2, parent);

    for (i = 0; i < 7; i++) {
        int idx;
        Component *c = new Component();
		c->setInterceptsMouseClicks(false, true); // allows interaction with the fx1 type selector box

        c->setBounds(x, y, FX_W, FX_H);
        fx1[i] = c;

        if (i == 0) {
            continue;
        }
        if (i > 1) {
            o = -80;
        }

        idx = i - 1;
        if (idx < 5) {
			const int offsX = -60;
			
			add_knob(844 + (idx * 10), offsX + 150, 40, "feedbck", c);
			if (idx == 0) {
				add_knob(845 + (idx * 10), offsX + 150 + 40, 40, "notch", c);
				add_box(849 + (idx * 10), offsX + 245, 43, 40, "stages", 1, c);
			} else {
				add_knob(845 + (idx * 10), offsX + 150 + 40, 40, "delay", c);
			}
			add_knob(846 + (idx * 10), offsX + 310 + o, 40, "rate", c);
			add_knob(847 + (idx * 10), offsX + 310 + 40 + o, 40, "depth", c);
			add_box(848 + (idx * 10), offsX + 395 + o, 43, 45, "shape", 1, c);
			add_box(851 + (idx * 10), offsX + 340 + o, 15, 60, "sync", 2, c);
			if (idx == 0) {
				add_button(850, offsX + 405 + o, 14, NULL, true, c);
			} else {
				add_button(849 + (idx * 10), offsX + 405 + o, 14, NULL, true, c);
			}

        } else {
			const int offsX = -60;
			
            int v_x = 230;
            int v_y = 30;
            add_box(898, offsX + 170 - 50, 15 + 40, 45, "synth", 1, c);
            add_box(899, offsX + 170, 15 + 40, 45, "analysis", 1, c);
            add_knob(894, offsX + v_x, v_y, "gain", c);
            add_knob(895, offsX + v_x+40, v_y, "boost", c);
            add_knob(896, offsX + v_x+80, v_y, "decay", c);
            add_knob(897, offsX + v_x+120, v_y, "shift", c);
            add_knob(900, offsX + v_x+160, v_y, "mix", c);
        }
    }
    for (i = 0; i < 7; i++) {
        parent->addChildComponent(fx1[i]);
    }

    fx1[owner->param_of_nrpn(800)->getValue()]->setVisible(true);
}

void MicronauAudioProcessorEditor::create_fx2(int x, int y, Component* parent)
{
	add_box(801, 40, 10, 80, "type", 2, parent);

    int i;
    // 1, 2: sync rate, button, knobs: delay, regen, bright
    // 3: l delay , regen, bringh, r delay
    // 4, 5, 6: difuse, decay, bright, color
    for (i = 0; i < 7; i++) {
        int idx;
        Component *c = new Component();
		c->setInterceptsMouseClicks(false, true); // allows interaction with the fx2 type selector box
		
        c->setBounds(x, y, FX_W, FX_H);
        fx2[i] = c;
        
        if (i == 0) {
            continue;
        }

        idx = i - 1;
        int offsX = 160;
        switch (idx) {
            case 0:
            case 1:
                add_knob(920 + (idx * 5), offsX, 40, "delay", c);
                add_knob(921 + (idx * 5), offsX + 40, 40, "regen", c);
                add_knob(922 + (idx * 5), offsX + 80, 40, "bright", c);
                add_box(924 + (idx * 5), offsX - 70, 40, 60, NULL, 2, c);
                add_button(923 + (idx*5), offsX - 70, 60, "sync", false, c);
              break;
                
            case 2:
                offsX -= 70;
                add_knob(920 + (idx * 5), offsX, 40, "l delay", c);
                add_knob(921 + (idx * 5), offsX + 40, 40, "regen", c);
                add_knob(922 + (idx * 5), offsX + 80, 40, "bright", c);
                add_knob(923 + (idx * 5), offsX + 120, 40, "r delay", c);
               break;

            case 3:
            case 4:
            case 5:
                offsX -= 70;
                add_knob(920 + (idx * 5), offsX, 40, "diffuse", c);
                add_knob(921 + (idx * 5), offsX + 40, 40, "decay", c);
                add_knob(922 + (idx * 5), offsX + 80, 40, "bright", c);
                add_knob(923 + (idx * 5), offsX + 120, 40, "color", c);
                break;
        }

    }
    for (i = 0; i < 7; i++) {
        parent->addChildComponent(fx2[i]);
    }

    fx2[owner->param_of_nrpn(801)->getValue()]->setVisible(true);
}

//==============================================================================
void MicronauAudioProcessorEditor::paint (Graphics& g)
{
	g.drawImageWithin(background, 0, 0, getWidth(), getHeight(), RectanglePlacement(RectanglePlacement::stretchToFit));
    g.setColour (Colours::black);

    jassert (logo != 0);
    if (logo != 0)
        logo->drawWithin (g, Rectangle<float> (LOGO_X, LOGO_Y, LOGO_W, LOGO_Y+LOGO_H), RectanglePlacement::stretchToFit, 1.000f);
}

void MicronauAudioProcessorEditor::updateGuiComponents()
{
	for (int i = 0; i < sliders.size(); i++) {
		sliders[i]->setValue(sliders[i]->get_value(), dontSendNotification);
	}

	for (int i = 0; i < boxes.size(); i++) {
		boxes[i]->setSelectedItemIndex(boxes[i]->get_value(), dontSendNotification);
	}
	
    for (int i = 0; i < buttons.size(); i++) {
        if (buttons[i]->get_value() == 0) {
            buttons[i]->setToggleState(false, dontSendNotification);
        } else {
            buttons[i]->setToggleState(true, dontSendNotification);
        }
    }
    
    for (int i = 0; i < 7; i++) {
        fx1[i]->setVisible(false);
        fx2[i]->setVisible(false);
    }
    fx1[owner->param_of_nrpn(800)->getValue()]->setVisible(true);
    fx2[owner->param_of_nrpn(801)->getValue()]->setVisible(true);

    update_tracking();

	prog_name->setText(owner->get_prog_name(), false);
	midi_out_chan->setSelectedItemIndex(owner->get_midi_chan(), dontSendNotification);
}

void MicronauAudioProcessorEditor::timerCallback()
{
    // update gui if any parameters have changed
	if ((paramHasChanged) || (owner->get_progchange())) {
		updateGuiComponents();
		if (owner->get_progchange())
			takeUndoSnapshot(); // it turns out this will be the initial snapshot
        owner->set_progchange(false);
        paramHasChanged = false;
    }
    
	update_midi_menu(MIDI_IN_IDX, false);
	update_midi_menu(MIDI_OUT_IDX, false);
}

void MicronauAudioProcessorEditor::update_midi_menu(int in_out, bool init)
{
    ComboBox *menu;
    StringArray x;
    switch (in_out) {
        case MIDI_IN_IDX:
            x = MidiInput::getDevices();
            menu = midi_in_menu;
            break;
        case MIDI_OUT_IDX:
            x = MidiOutput::getDevices();
            menu = midi_out_menu;
            break;
        default:
            return;
    }

    bool midi_changed = false;
    if (x.size() + 1 != menu->getNumItems()) {
        midi_changed = true;
    } else {
        for (int i = 0; i < x.size(); i++) {
            if (x[i] != menu->getItemText(i+1)) {
                midi_changed = true;
                break;
            }
        }
    }
    if (midi_changed || init) {
        int idx = menu->getSelectedItemIndex();
        String current_midi;
        if (idx == -1) {
            current_midi = "None";
        } else {
            current_midi = menu->getItemText(idx);
        }
        menu->clear();
        menu->addItem("None", 1000);
        for (int i = 0; i < x.size(); i++) {
            menu->addItem(x[i], i+1);
        }
        if (init) {
            select_item_by_name(in_out, owner->get_midi_port(in_out));
        } else {
            select_item_by_name(in_out, current_midi);
        }
    } else {
        select_item_by_name(in_out, owner->get_midi_port(in_out));
    }
}

void MicronauAudioProcessorEditor::sliderValueChanged (Slider *slider)
{
	{	// handle nrpn editing sliders
		ext_slider *s = dynamic_cast<ext_slider*>( slider );
		if (s)
		{
			s->set_value(s->getValue());
			param_display->setText(s->get_name() + "\n" + s->get_txt_value(s->getValue()), dontSendNotification);
		}
	}
	
	{	// handle generic non-nrpn sliders
		MicronSlider *s = dynamic_cast<MicronSlider*>( slider );
		if (s)
		{
			if (s == randomizeAmtSlider)
				param_display->setText(String("Randomizer Amt\n") + String(s->getValue()), dontSendNotification);
		}
	}
}

void MicronauAudioProcessorEditor::sliderDragStarted (Slider* slider)
{	// when user just touches a slider, update its value so it may be seen in the parameter display box.
	sliderValueChanged(slider);
}

void MicronauAudioProcessorEditor::sliderDragEnded (Slider* slider)
{
	ext_slider *s = dynamic_cast<ext_slider*>( slider );
	if (s)
		takeUndoSnapshot();
}

void MicronauAudioProcessorEditor::buttonClicked (Button* button)
{
	String lcdTextMessage;

    ext_button *b = dynamic_cast<ext_button*>( button );
	if (b)
	{
		takeUndoSnapshot();

        int v = b->getToggleState();
		b->set_value(v);
		lcdTextMessage = b->get_name() + "\n" + b->get_txt_value(v);
	}
	else if (button == sync_nrpn) {
        owner->sync_via_nrpn();
		lcdTextMessage = "Sync prgm nrpn\nDone";
    }
    else if (button == sync_sysex) {
        owner->sync_via_sysex();
		lcdTextMessage = "Sync prgm sysex\nDone";
    }
    else if (button == request) {
        owner->send_request();
		lcdTextMessage = "Send prgm request\nDone";
    }
	else if (button == randomizeButton)
	{
		randomizeParams();
		lcdTextMessage = "Randomize\nDone";
	}
	else if (button == randomizeLockPitchButton)
	{
		lcdTextMessage = String("Randomizer\nLock pitch: ") + (button->getToggleState() ? "On" : "Off");
	}
	else if (button == undo_button)
	{
		if ( canUndo() )
		{
			restorePreviousUndoSnapshot();
			lcdTextMessage = "Undo";
		}
		else
		{
			lcdTextMessage = "Nothing to undo";
		}
	}
	else if (button == redo_button)
	{
		if ( canRedo() )
		{
			restorePreviousUndoSnapshot(true);
			lcdTextMessage = "Redo";
		}
		else
		{
			lcdTextMessage = "Nothing to redo";
		}
	}

	param_display->setText(lcdTextMessage, dontSendNotification);
}

ext_combo* MicronauAudioProcessorEditor::findBoxWithNrpn(int nrpn)
{
	ext_combo* box = 0;
	for (int j = 0; j < boxes.size(); j++)
	{
		if (boxes[j]->get_nrpn() == nrpn)
		{
			box = boxes[j];
			break;
		}
	}
	return box;
}

void MicronauAudioProcessorEditor::randomizeParams()
{
	allowNewSnapshots = false; // don't generate undo steps for each param modified

	bool lockPitch = randomizeLockPitchButton->getToggleState();

	const float sliderAmt = randomizeAmtSlider->getValue();

	// make random adjustments to the knob values, with a few tweaks/hacks to tend more toward viable sounds
	for (int i = 0; i < sliders.size(); i++) {
		const float curVal = (sliders[i]->get_value() - sliders[i]->getMinimum()) / (sliders[i]->getMaximum() - sliders[i]->getMinimum());

		float amt = 0.5f * sliderAmt;
		float defaultsBiasAmt = 0.8f*sliderAmt; // bias some/all params toward their default value by some strength level
	
		float defaultVal = 0.5f; // using halfway point as default value to bias toward
		float randOffset = 2.0f * randGen.nextFloat() - 1.0f;

		switch (sliders[i]->getInternalParam()->getNrpn())
		{
			case 576: // program level
				amt *= 0.5f; // less movement
				randOffset *= 0.5f; // less movement
				defaultVal = 0.75f; // bias toward a higher setting
				break;

			case 578: // env1 attack
				defaultVal = 0.3f; // bias toward a lower setting
				amt *= 0.7f; // less movement
				break;

			case 582: // env1 sustain time
				defaultVal = 1.0f; // bias toward a higher setting
				amt *= 0.7f; // less movement
				defaultsBiasAmt = 0.5f + 0.5f * sliderAmt; // stronger bias toward default
				break;

			case 583: // env1 sustain level
				defaultVal = 0.8f; // bias toward a higher setting
				amt *= 0.7f; // less movement
				break;

			case 584: // env1 release time
				defaultVal = 0.6f; // bias toward a higher setting
				amt *= 0.7f; // less movement
				break;

			case 517: // porta time
				defaultVal = 0.1f; // bias toward a lower setting
				amt *= 0.7f; // less movement
				break;
			
			case 526: // osc1 pitch semi
			case 532: // osc2 pitch semi
			case 538: // osc3 pitch semi
				if (lockPitch)
					continue; // skip changing osc semitone altogether to help preserve tonality of voice

			case 514: // unison detune
			case 519: // analog drift
				defaultVal = 0.0f; // bias toward a lower setting
				if (lockPitch)
				{
					amt *= 0.7f; // less movement
					defaultsBiasAmt = 0.5f + 0.5f * sliderAmt; // stronger bias toward default
				}
				break;

			case 527: // osc1 pitch fine
			case 533: // osc2 pitch fine
			case 539: // osc3 pitch fine
				if (lockPitch)
				{	// resist excessive osc detuning, may help preserve tonality of voice
					amt *= 0.5f; // less movement
					randOffset *= 0.5f; // less movement
					defaultsBiasAmt = 0.5f + 0.5f * sliderAmt; // stronger bias toward default
				}
				break;

			default:
				// constrain some mod matrix levels and offsets
				if ( sliders[i]->getInternalParam()->isModLevel() || sliders[i]->getInternalParam()->isModOffset() )
				{
					// find box containing dest param associated with current knob
					int associatedModDestNrpn = (sliders[i]->getInternalParam()->getNrpn() & 0xfffc) + 1;
					const ext_combo* box = findBoxWithNrpn(associatedModDestNrpn);

					if (box)
					{
						String boxString = box->getItemText(box->getSelectedItemIndex());
						if ( lockPitch && boxString.contains("Nar") )
						{	// constrain narrow pitch values
							amt *= 0.25f; // less movement
							randOffset *= 0.5f; // less movement
							defaultsBiasAmt = 1.0f; // strong bias toward default
						}
						else if ( lockPitch && boxString.contains("Pit") )
						{	// constrain broad range pitch values even more
							amt *= 0.1f; // less movement
							randOffset *= 0.1f; // less movement
							defaultsBiasAmt = 1.0f; // strong bias toward default
						}
						else if ( boxString.contains("PgmLvl") )
						{	// constrain pgmlevel values
							amt *= 0.5f; // less movement
							randOffset *= 0.5f; // less movement
							defaultsBiasAmt = 1.0f; // strong bias toward default
						}
						else if ( boxString.contains("Pan") )
						{
							amt *= 0.5f; // less movement
							defaultsBiasAmt = 0.5f + 0.5f * sliderAmt; // stronger bias toward default
						}
					}
				}
				break;
		}
		
		const float targetVal = ((1.0f-defaultsBiasAmt)*curVal + defaultsBiasAmt*defaultVal) + randOffset;
		const float finalMixedValue = ( (1.0f-amt) * curVal + amt * targetVal );
		double val = sliders[i]->getMinimum() + (sliders[i]->getMaximum() - sliders[i]->getMinimum()) * finalMixedValue;
		sliders[i]->setValue(val, sendNotificationSync);
	}

	// also modify combo box values if we are applying a large amount of randomization
	if (sliderAmt > 0.5f)
	{
		for (int i = 0; i < boxes.size(); i++) {
			const float curVal = ((float)boxes[i]->indexOfItemId(boxes[i]->getSelectedId())) / boxes[i]->getNumItems();

			float amt = 2.0f * (sliderAmt-0.5f);
			amt *= amt;
			float defaultsBiasAmt = 0.25f * amt; // bias some/all params toward their default value by some strength level
			amt *= 0.5f;
		
			float defaultVal = 0.5f; // using halfway point as default value to bias toward
			float randOffset = 2.0f * randGen.nextFloat() - 1.0f;
			
			// treat some combo boxes differently
			const int nrpn = boxes[i]->getInternalParam()->getNrpn();
			if (boxes[i]->getInternalParam()->isMatrixSource() ||
				nrpn == 628 || /* S/H input */
				nrpn == 630 ) /* trackgen input */
			{
				amt *= 0.4f; // less movement
				randOffset *= 0.4f; // less movement
				defaultVal = 0.0f; // bias toward a lower setting, avoid the CCs more or less
				defaultsBiasAmt *= 1.5f;
			}
			else if (boxes[i]->getInternalParam()->isMatrixDest())
			{
//				amt *= 0.5f; // less movement
				randOffset *= 0.5f; // less movement
			}
			else if (nrpn == 523 || /* osc 1 waveform */
					nrpn == 529 || /* osc 2 waveform */
					nrpn == 535) /* osc 3 waveform */
			{
				amt *= 2.0f; // more movement
				randOffset *= 2.0f;
			}
			else if (nrpn == 589 || /* env 1 loop */
					nrpn == 588) /* env 1 freerun */
			{
				amt *= 0.5f; // less movement
//				randOffset *= 0.5f; // less movement
			}
			else if (nrpn == 100 || /* program */
					nrpn == 101 || /* bank */
					nrpn == 666 || /* category */
					nrpn == 411 || /* knob x */
					nrpn == 412 || /* knob y */
					nrpn == 413 ) /* knob z */
			{
				continue;
			}
			
			const float targetVal = ((1.0f-defaultsBiasAmt)*curVal + defaultsBiasAmt*defaultVal) + randOffset;
			const float finalMixedValue = ( (1.0f-amt) * curVal + amt * targetVal );
			int val = round( finalMixedValue * boxes[i]->getNumItems() );
			if (val < 0)
				val = 0;
			if ( boxes[i]->getInternalParam()->isMatrixSource() || boxes[i]->getInternalParam()->isMatrixDest() )
			{
				if (val < 1)
					val = 1; // don't set any matrix src or dst to None (first item)
				if (boxes[i]->getSelectedItemIndex() == 0)
					continue; // slot was already set to None (first item), do not change it
			}
			if (val > boxes[i]->getNumItems()-1)
				val = boxes[i]->getNumItems()-1;
			boxes[i]->setSelectedItemIndex (val, sendNotificationSync);
		}
	}

	allowNewSnapshots = true;
	takeUndoSnapshot();
}


void MicronauAudioProcessorEditor::takeUndoSnapshot()
{
	if ( ! allowNewSnapshots )
		return;

	Snapshot snapshot;
	
	snapshot.progname_value = prog_name->getText().toStdString();
	for (int i = 0; i < sliders.size(); i++)
		snapshot.slider_values.push_back( sliders[i]->getValue() );
	for (int i = 0; i < boxes.size(); i++)
		snapshot.box_values.push_back( boxes[i]->getSelectedItemIndex() );
	for (int i = 0; i < buttons.size(); i++)
		snapshot.button_values.push_back( buttons[i]->getToggleState() );

	if ( ! undo_history.empty() )
		undo_history.erase(++undo_cur, undo_history.end());
	undo_history.push_back(snapshot);
	undo_cur = --undo_history.end();
}


bool MicronauAudioProcessorEditor::canUndo()
{
	return undo_cur != undo_history.begin();
}
bool MicronauAudioProcessorEditor::canRedo()
{
	return undo_cur != --undo_history.end();
}

void MicronauAudioProcessorEditor::restorePreviousUndoSnapshot(bool redo)
{
	if (redo)
	{
		if ( ! canRedo() )
			return; // can't redo past end
		undo_cur ++;
	}
	else // undo
	{
		if ( ! canUndo() )
			return; // can't undo past first initial snapshot
		undo_cur --;
	}

	Snapshot& snapshot = *undo_cur;
	
	allowNewSnapshots = false; // don't generate more snapshots while restoring current one

	owner->set_prog_name(snapshot.progname_value); // ensure name text box doesn't get clobbered with old data..
	prog_name->setText(snapshot.progname_value, true); // .. as the text box change notification is async.
	for (int i = 0; i < sliders.size(); i++)
		sliders[i]->setValue(snapshot.slider_values[i], sendNotificationSync);
	for (int i = 0; i < boxes.size(); i++)
		boxes[i]->setSelectedItemIndex(snapshot.box_values[i], sendNotificationSync);
	for (int i = 0; i < buttons.size(); i++)
		buttons[i]->setToggleState(snapshot.button_values[i], sendNotificationSync);
	
	allowNewSnapshots = true;
}

void MicronauAudioProcessorEditor::comboBoxChanged (ComboBox* box)
{
    int idx = box->getSelectedItemIndex();
    if (box == midi_out_menu) {
        owner->set_midi_port(MIDI_OUT_IDX, box->getItemText(idx));
    }
    if (box == midi_in_menu) {
        owner->set_midi_port(MIDI_IN_IDX, box->getItemText(idx));
    }
    if (box == midi_out_chan) {
        owner->set_midi_chan(box->getSelectedItemIndex());
    }
    ext_combo *b = dynamic_cast<ext_combo *>( box );
	if (b)
	{
		b->set_value(b->getSelectedItemIndex());
		param_display->setText(b->get_name() + "\n" + b->get_txt_value(b->getSelectedItemIndex()), dontSendNotification);
        if (b->get_nrpn() == 800) {
            int i;
            for (i = 0; i < 7; i++) {
                fx1[i]->setVisible(false);
            }
            fx1[b->getSelectedItemIndex()]->setVisible(true);
        }
        if (b->get_nrpn() == 801) {
            int i;
            for (i = 0; i < 7; i++) {
                fx2[i]->setVisible(false);
            }
            fx2[b->getSelectedItemIndex()]->setVisible(true);
        }
        if ((b->get_nrpn() == 631) || (b->get_nrpn() == 632)){
            update_tracking();
        }

		takeUndoSnapshot();
	}

	updateGuiComponents();
}

void MicronauAudioProcessorEditor::select_item_by_name(int in_out, String nm)
{
    int i;
    ComboBox *menu;
    switch (in_out) {
        case MIDI_IN_IDX:
            menu = midi_in_menu;
            break;
        case MIDI_OUT_IDX:
            menu = midi_out_menu;
            break;
        default:
            return;
    }
    for (i = 0; i < menu->getNumItems(); i++) {
        if (menu->getItemText(i) == nm) {
            menu->setSelectedItemIndex(i, dontSendNotification);
            return;
        }
    }
    menu->setSelectedItemIndex(0, dontSendNotification);
}

void MicronauAudioProcessorEditor::textEditorTextChanged (TextEditor &t) {
    String prog;
    prog = t.getText();
    
    owner->set_prog_name(prog.substring(0,14));
}

void MicronauAudioProcessorEditor::textEditorFocusLost (TextEditor &t) {
	takeUndoSnapshot();
}

void MicronauAudioProcessorEditor::update_tracking()
{
    int i;
    int preset, num_points;
    preset = owner->param_of_nrpn(631)->getValue();
    num_points = owner->param_of_nrpn(632)->getValue() * 6;
    if (num_points == 0) {
        trackgen->hide_12_16(true);
    } else {
        trackgen->hide_12_16(false);
    }
    
    switch (preset) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            {
                for (i = 0; i <= 32; i++) {
                    owner->param_of_nrpn(633 + i)->setValue(tracking[preset + num_points - 1][i]);
                }
            }
            break;
        case 7:
            // zero
            for (i = 633; i <= 665; i++) {
                owner->param_of_nrpn(i)->setValue(0);
            }
            break;
        case 8:
            // max
            for (i = 633; i <= 665; i++) {
                owner->param_of_nrpn(i)->setValue(100);
            }
            break;
        case 9:
            // min
            for (i = 633; i <= 665; i++) {
                owner->param_of_nrpn(i)->setValue(-100);
            }
            break;
        default:
			// using custom tracking values, so no need to update
            break;
    }
 }

// -------------------------------------------------------------------------------------------------------
// This code ensures that any time the user clicks away from something like a text editor which wants
// keyboard focus, the text editor will lose focus and nothing else will aquire focus.
//

// NOTE: we can seemingly just use a KeyboardFocusTraverser, but it seems more correct to make one which always returns nothing.
class NullKeyboardFocusTraverser : public KeyboardFocusTraverser
{
public:
    Component* getNextComponent (Component* current) { return 0; }
    Component* getPreviousComponent (Component* current) { return 0; }
	Component* getDefaultComponent (Component* parentComponent) { return 0; }
};

void MicronauAudioProcessorEditor::mouseDown(const MouseEvent& event)
{	// user clicked background, so take focus away from whatever was focused.
	Component::unfocusAllComponents();
}

#if 0
void ext_slider::mouseDoubleClick(const MouseEvent& event)
{
   set_value(param->getDefaultValue());
}
#endif

KeyboardFocusTraverser* MicronauAudioProcessorEditor::createFocusTraverser()
{	// user may have finished with something like a combo box, so make sure the next thing to focus on is nothing.

	// NOTE: we also have to unfocus all components, otherwise text editors don't seem to let go of focus after something else is clicked.
	Component::unfocusAllComponents();

	return new NullKeyboardFocusTraverser;
}

//
// -------------------------------------------------------------------------------------------------------


