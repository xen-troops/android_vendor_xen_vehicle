# Vehicle HAL 2.0

Vehicle HAL which complies Android Vehicle interface https://android.googlesource.com/platform/hardware/interfaces/+/refs/tags/android-cts-9.0_r7/automotive/vehicle/2.0

It was implemented according to: https://www.w3.org/TR/vehicle-information-service

as a client of VIS server.

## Dependencies:

# uWebSockets
Forked and hosted at:
 https://github.com/xen-troops/uWebSockets/tree/android-9.0.0_r3-xt0.2

This is an indirect dependency.

# libvisclient
Will be moved to separate project

## Configuration

You can define VIS uri by setting Android property:
```persist.vis.uri```
The default value of it is: ```wss://wwwivi:8088``` (it is DomD IP).
It is preferred to use a hostname and define it in resolver.

Static VIS to Android property mapping is done in: ``` DefaultConfig.h```
But you can provide json configuration file(like ```cfg/vehicle-mappings.json```) and override static mappings. The path to this file is configurable using Android property: ```persist.vehicle.prop-mapping```. Default path is: ```/vendor/etc/vehicle/vehicle-mappings.json```.

