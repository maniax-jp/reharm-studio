# Test Design

## Unit Tests

### ChordProgressionGenerator Tests

1. **generateProgression_Major**
   - Input: rootNote=0 (C), scale="Major"
   - Expected: "I - IV - V - I"

2. **generateProgression_Minor**
   - Input: rootNote=0 (C), scale="Minor"
   - Expected: "i - iv - v - i"

3. **applySubstitution_Tritone**
   - Input: "I - IV - V - I"
   - Expected: "I - IV - vii° - I"

### MainComponent Tests

1. **keyComboBox_Change**
   - Change key combo box
   - Verify progression display updates

2. **scaleComboBox_Change**
   - Change scale combo box
   - Verify progression display updates

## Integration Tests

1. **LoadVST3Plugin**
   - Simulate file selection
   - Verify plugin instance created
   - Verify AudioProcessorPlayer set

2. **PlayProgression**
   - Load plugin
   - Click play button
   - Verify MIDI events sent to plugin
   - Verify audio output

## Manual Tests

1. **UI Responsiveness**
   - Verify all buttons and combo boxes work
   - Verify text display updates

2. **Audio Output**
   - Load a VST3 instrument (e.g., simple synth)
   - Play progression
   - Verify sound output matches chords

3. **Plugin Compatibility**
   - Test with different VST3 plugins
   - Verify no crashes or audio issues

## Performance Tests

1. **Audio Latency**
   - Measure round-trip latency
   - Ensure < 10ms for real-time feel

2. **CPU Usage**
   - Monitor CPU during playback
   - Ensure < 20% on modern hardware