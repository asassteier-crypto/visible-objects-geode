# Visible Objects

Visible Objects skips drawing level objects that are outside the current screen area while you play a level. It also culls objects in Geometry Dash's sprite-batch rendering path.

The mod leaves object updates, collision checks, and trigger behavior active. It focuses on reducing rendering work, so it may help most in levels where drawing is a large part of the slowdown.

Render culling is enabled by default. You can turn it off in the mod settings or adjust the off-screen margin. The default margin is 96 screen points, which keeps nearby objects ready to appear as the camera moves.

## Compatibility

- Geometry Dash 2.2081 on Windows
- Geode 5.11.2
