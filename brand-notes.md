# SimpleGain: look and feel notes

The colours, shapes and layout SimpleGain's editor uses today. The code is the source of truth
(`Source/PluginEditor.cpp`); this is a copy-paste reference.

## Colours

| Role                       | Hex       |
|----------------------------|-----------|
| Background, top            | `#18181B` |
| Background, bottom         | `#111113` |
| Track ring                 | `#2C2C31` |
| Accent (value arc)         | `#F2A766` |
| Text                       | `#ECEAE6` |
| Muted text                 | `#77757C` |

The background is a very subtle vertical gradient from top to bottom.

## The knob

- Flat: no face, no tick marks, no glow, no thumb dot.
- A slim **track ring** in the track colour, with one **accent arc** drawn over it from the
  start of the sweep to the current value.
- Ring thickness is 2.5% of the knob's diameter (never under 2 px), with rounded ends.
- The sweep is JUCE's default rotary range: 1.2π to 2.8π, so a gap at the bottom.
- The arc fills from the minimum (-60 dB), not from 0 dB.
- Dragging is relative: a click never moves the value, and the arc follows the angle you turn.

## Layout (all proportional to the window)

- Opening size 340 x 440. Resizable from 75% to 200%, always keeping that shape.
- Side and top margin: 1/12 of the width.
- **Value** (e.g. `-12.3 dB`): large, in a box centred in the knob, 74% of the diameter wide and
  30% tall, text height half the box height. Click it to type a value.
- **Caption** `GAIN`: small, muted, just under the value inside the ring (3% of the window height).
- **Title** `SimpleGain`: bold, muted, at the top (1/22 of the window height).
- **Credit line** along the bottom: muted, 3.5% of the window height.
- Double-click the knob to reset to 0 dB.
