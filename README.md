# modfm

This is a simple synthesizer which is an attempt to implement the "Modified Frequency Modulation" algorithm described by
Victor Lazzarini and Joseph Timoney in their paper
@ https://mural.maynoothuniversity.ie/4697/1/JAES_V58_6_PG459hirez.pdf

The ModFM method produces a sound which is less "nasal" than traditional FM (and its phase modulation variant) as the
amplitude of the sidebands are said to vary in a more linear fashion.

This synth currently works as a standlone program (not a VSTi or etc.) using PortAudio to produce sound and PortMidi to
receive MIDI note events. It has a simple GUI written in ImGui which enables basic parameter editing.

It should in theory be portable to multiple platforms but I have so far run it only on Linux.

It is currently 8 voice polyphonic, but monotimbral. Amplitude mixing for multiple voices is not ideal, as it produces
clipping on chords. Legato, portamento etc have not been implemented. The envelope generator needs tweeking. There is no
support for saving or loading patches yet. There is no support for MIDI continuous controller input yet.

There are undoubtably bugs, and I can't guarantee my implementation of the math described in the paper is correct. It
has also not been optimized for performance at this time.

