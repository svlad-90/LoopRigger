# Scripting Component

The scripting component owns the C++ contract for FL Studio-style device
scripts. It does not embed Python yet; it defines the narrow API that a future
Python host will implement.

Scripts are expected to translate device events into core controller commands.
They must not process audio or run in realtime audio callbacks.
