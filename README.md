# KNX
--------------------------------

This plugin adds support to connect nymea to a KNX NetIp Server.

## Usage

In order to set up KNX connectivty in nymea, the KNX network configuration
(.knxproj) file is required to be provided during setup. This file needs to
be copied to the same machine that is running nymea and the path can be provided
during the setup steps. If no path is given, the plugin will look for the file
in /etc/nymea/installation.knxproj by default.

Once the project is loaded, each KNX device in the network will appear as a
generic thing in nymea. Those can be tested in order to match nymea things
to the actual knx devices and then the addresses can be used to create more specialized
things in nymea. This will cause the generic things to disappear.

Example:
The plugin is set up using a knxproj file on a KNX network with a light bulb and
a wall switch. Once the setup is finished, there will be two generic things in nymea.
Read their bus/group/device addresses for the light bulb from the thing parameters 
and go to the thing setup to set up a new KNX light bulb using those addresses.
Once completed, the generic device will disappear and the light bulb will be configured
properly in nymea. Repeat the same for te wall switch.

## Supported Things

* KNX NetIp server
* KNX generic switch
* KNX generic UpDown
* KNX generic scale
* KNX generic temperature sensor
* KNX generic light sensor
* KNX generic wind speed sensor
* KNX trigger
* KNX shutter
* KNX light
* KNX dimmable light

## Requirements

* The package 'nymea-plugin-knx' must be installed.
* The KNX NetIp Server must be in the same network as nymea.
* ETS 5 project file.

## More

https://www.knx.org
