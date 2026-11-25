#include "stdafx.h"

// A DLL must declare exactly one component version, otherwise foobar2000's
// troubleshooter cannot tell builds apart.
DECLARE_COMPONENT_VERSION("Output device switcher", "0.1.0",
                          "Shows the current output device in the layout and lets you switch it without opening Preferences.\n"
                          "Add it from the layout editor: Layout -> Add element -> Utilities -> Output device.");

// Stops the user (and other components) renaming the DLL, which breaks the
// troubleshooter and update checks.
VALIDATE_COMPONENT_FILENAME("foo_output_switcher.dll");

// Keeps cfg_var values readable when a newer build is rolled back to an older one.
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
