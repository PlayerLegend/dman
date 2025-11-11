# dman

`dman` is a display configuration utility that fingerprints displays via EDID in order to save and restore configurations irrespective to whether the displays have changed ports. Toggle/enable/disable behavior is also able to be done via human-friendly names specified in config files.

Graphics tablet configuration is also planned, but is currently to-do.

# Display config files

Configuration files are composed of lines in this format:

`EDID_HASH KEY1=VAL1 KEY2=VAL2 ...`

Config files are intended to be auto generated, but it may be helpful to alter 
the `name=something` key/value pair to be a familiar name. Generated configs
provide names derived from the monitor's EDID.

# Example usage

```
# Save the current display configuration
dman --output /some/file

# Restore a previous display configuration
dman --input /some/file     # Restores a previous display configuration

# Toggles a monitor named 'Secondary' in the configuration file
dman --input /some/file --toggle Secondary

# List outputs, select one using dmenu, and toggle it.
# Empty inputs are ignored, so this is escape-friendly. 
dman --list-config-outputs /some/file --list-active-outputs | dmenu -i | dman --input /some/file --toggle -
```
