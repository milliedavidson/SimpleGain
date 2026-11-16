# quick start

## Like you're five

**simply gain is a volume knob.** Put it on a track, a bus or your whole mix, and it makes the
sound louder or quieter. That's all it does.

- **Turn the ring** clockwise for louder, anticlockwise for quieter. Grab it anywhere: it moves
  from where it is, so a click never makes it jump.
- **Drag the number** up or down for small, fine changes. Scrolling over it works too.
- **Click the number** to type an exact value, like -3 or 2.5.
- **Double-click the ring** to go back to 0 dB.
- **0 dB means no change.** The sound comes out exactly as it went in. Below zero is quieter, and
  -60 dB is nearly silent. Above zero is louder, up to +30 dB.

**The colours show where you are.** The ring is pale at the quiet end and apricot in the middle.
It warms to amber as you push hard, then deep red at the very top. When **Approaching the limit.**
appears, you've turned it up a long way. Listen for distortion in whatever comes next.

## Good for

- **Gain staging:** setting the level going into a compressor, saturator or amp sim, so it gets
  the level it was designed for.
- **Riding levels:** automate it to lift a quiet verse or tame a loud chorus, without touching
  your faders.
- **Fair comparisons:** match the volume before and after an effect. Louder always sounds better
  at first, so matching the levels keeps your ears honest.

## For the nerds

- **The maths.** Every sample is multiplied by one number: `gain = 10^(dB / 20)`. So +6 dB is
  about ×2, -6 dB about ×0.5, and -60 dB is ×0.001.
- **The range** is -60 to +30 dB in 0.1 dB steps. It's linear in decibels, so the top of the ring
  is -15 dB.
- **No zipper noise.** A change glides to its new value over 20 ms, a linear ramp of the gain
  worked out for every sample. Fast drags and automation never click.
- **No clipping inside.** The audio is 32-bit floating point, so going over 0 dBFS here is
  harmless. Whatever comes next might clip, and so will your DAW's output.
- **Zero latency**, no lookahead and no tail. The cost is one multiply per sample.
- **Mono or stereo**, with the same gain on every channel.
- **One parameter**, Gain (its ID is `gain`). Hosts record each drag as one automation move, and
  your project saves the exact value.
- **The colours are zones on the ring.** Amber blends in around 85% of the range (+16.5 dB), and
  red around 95% (+25.5 dB), where the readout appears.
- **Formats:** Audio Unit and VST3, for Intel and Apple Silicon Macs on macOS 11 or later. Built
  with JUCE 8 and C++17, and open source under the MIT licence.
