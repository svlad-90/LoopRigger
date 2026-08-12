# Core Component

`core` owns engine state, commands, loopers, and deterministic performance
state transitions. It must stay independent from JUCE, Python, MIDI backends,
plugin hosting, and file formats.
