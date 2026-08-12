# Plugin Host Component

The plugin host component owns plugin discovery and plugin metadata. The core
API is independent of JUCE and realtime audio processing.

The optional JUCE scanner target is the first VST/AU-facing implementation. It
can discover plugin descriptions, but it does not instantiate plugins or process
audio yet.
