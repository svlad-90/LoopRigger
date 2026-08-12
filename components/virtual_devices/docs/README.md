# Virtual Devices Component

The virtual devices component owns renderer-independent pseudo-device
descriptors and event contracts.

A virtual device describes logical widgets and can be rendered by a JUCE
window, a CLI harness, or another test surface. It does not decide what a
button means; scripted adapters and the control component translate events
into core commands.
