Title: examples: make Distortion example less aggressive (fixes #89)

This minimal change replaces the DC-producing expression in the Distortion example with a true phase inversion for the right channel.

- Use -out_l instead of 1 - out_l to avoid producing a DC offset when processing silence.

Closes #89.
