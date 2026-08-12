# Profile IO Component

`profile_io` owns file-backed controller profiles and control-surface layouts.
It may depend on `control`, but it must remain independent from JUCE renderers
and realtime audio code.
