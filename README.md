# Visible Objects

A Geode mod for Geometry Dash 2.2081 that skips visiting level objects whose bounds are outside the current screen area. It is intended to reduce rendering work in levels with many objects.

The mod only skips drawing while playing a level. Objects still update and remain active for collision, trigger, and gameplay logic, so this chiefly helps levels whose lag comes from rendering. Trigger objects are left alone for compatibility. A padded off-screen margin is adjustable in the mod settings; increase it if you notice objects appearing too late, or disable culling there.

## Install

Install `VisibleObjects.geode` with Geode's mod installer, then launch Geometry Dash. Culling is enabled by default and can be adjusted in the mod settings.

## Build

Build this folder with Geode SDK 5.11.2 and CMake using the Geode CMake workflow. The target is Windows Geometry Dash 2.2081. Set `GEODE_SDK` to the SDK directory before configuring. Use a non-Debug build such as `RelWithDebInfo`.
