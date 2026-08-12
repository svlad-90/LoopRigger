# Scripting Component

The scripting component owns the C++ contract for FL Studio-style device
scripts. The optional `livelooping_python_scripting` target embeds Python
behind the `ScriptHost` API.

Scripts are expected to translate device events into core controller commands.
They must not process audio or run in realtime audio callbacks.
