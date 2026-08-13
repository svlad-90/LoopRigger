# Audio Component

The `audio` component owns SDK-independent audio processing helpers and later
will own realtime engine contracts, loop buffers, transport, and routing.

Current scope:

- deterministic offline block-processing smoke support;
- block peak/RMS measurement helpers;
- no dependency on JUCE, plugin hosting, Python, or GUI code.

JUCE/VST adapters should convert their native buffers at the boundary and keep
plugin-specific logic outside this component.
