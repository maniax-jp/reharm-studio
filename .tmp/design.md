# Design

## Architecture

### Components

1. **MainComponent** (Main GUI)
   - Inherits from AudioAppComponent for audio processing
   - Manages UI elements (buttons, combo boxes, text editor)
   - Handles user interactions
   - Coordinates between chord generator and plugin host

2. **ChordProgressionGenerator** (Logic)
   - Generates basic progressions (I-IV-V-I)
   - Applies music theory substitutions
   - Returns progression as string

3. **VST3 Host Integration**
   - Uses JUCE's AudioPluginFormat for VST3 loading
   - AudioProcessorPlayer for audio routing
   - MidiBuffer for MIDI event scheduling

### Data Flow

1. User selects key and scale
2. ChordProgressionGenerator creates progression string
3. Display in TextEditor
4. User loads VST3 plugin via FileChooser
5. Plugin instance created and set to AudioProcessorPlayer
6. User clicks play: MIDI events generated and sent to plugin
7. Audio output routed to system

### Audio Pipeline

- AudioDeviceManager -> AudioProcessorPlayer -> VST3 Plugin -> System Output
- MIDI: Chord notes -> MidiBuffer -> Plugin processBlock

## UI Layout

- Top: Load VST3 button, Play button
- Middle: Key combo box, Scale combo box
- Bottom: Progression display text area

## Music Theory Implementation

- Scales: Major (I ii iii IV V vi vii°), Minor (i ii° III iv v VI VII)
- Basic progression: I - IV - V - I
- Substitution: V -> vii° (tritone substitution)