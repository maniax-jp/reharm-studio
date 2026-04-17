# Requirements

## Functional Requirements

1. **Chord Progression Generation**
   - Generate chord progressions based on music theory
   - Support major and minor scales
   - Allow selection of root key (C, C#, D, etc.)
   - Display generated progression in Roman numerals

2. **Chord Substitution**
   - Apply theoretical substitutions to progressions
   - Support tritone substitution (V -> vii°)
   - Allow user to enable/disable substitutions

3. **VST3 Plugin Hosting**
   - Load and host VST3 instrument plugins
   - Route MIDI from chord progression to plugin
   - Output audio from plugin to system audio

4. **Audio Playback**
   - Play generated chord progression through loaded VST3 plugin
   - Support tempo and timing control
   - Real-time audio output

5. **User Interface**
   - macOS native GUI using JUCE
   - Key and scale selection dropdowns
   - Progression display text area
   - Load VST3 button
   - Play progression button

## Non-Functional Requirements

- Platform: macOS
- Framework: JUCE (C++)
- Plugin Format: VST3
- Audio: Real-time, low latency
- UI: Intuitive, responsive