# rhythem generators

a beat is a rhythm, represented by an array of bits
- variable length?
- is it a vector<bool> or bitset?

in functional programming, the states are "visible" from the outside, so you can manipulate and combine the states and use them to generate visualizations trivially.

remember, the syntax need to be AI friendly, for AI improvisation

- clock: divide from raw ticks to dicrete steps 

- euclid

# operators

- = (assignment)

- boolean logic
    - * (and)
    - + (or)
    - ! (not, invert)
    - - (subtract)

- % (modulo)

- @/d/z (delay)

- ~ feedback (from Faust)
    - insert one sample delay automatically

- min and max


# rhythm manipulation

- shuffle
- delay
- cascade

# signal input

- x and y reserved, z for touch pressure

# signal manipulation

- bitcrush: >>, <<

# rhythm -> signal conversion

- adc(signal) -> beat

- dac(beat) -> uint8_t

# routing, selecting

- bernoulli
- mux, demux

# arppegiator

arp([0, 3, 5, 9], up_down)

# comments

use // for comments

# system realtime

start, stop, continue

global bpm

# examples

- Steve Reich Piano Phase