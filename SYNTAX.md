# rhythem generators

a rhythm is represented by a bitset

in functional programming, the states are "visible" from the outside, so you can manipulate and combine the states and use them to generate visualizations trivially.

remember, the syntax need to be LLM friendly, and constexpr everything

- clock: divide from raw ticks to dicrete steps
    - use c++ high resolution timer
    - midi clock sync

- euclid

# operators

- = (conversion) (done)

- boolean logic (done)
    - * (and)
    - + (or)
    - ! (not, invert)
    - - (subtract)

- % (modulo)

- comparator

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
- i reserved for step index

# rhythm <-> signal conversion

- adc: signal to beat

- dac: beat to signal

# randomness

- bernoulli gate
- velvet noise

# arppegiator


# comments

use // for comments

# system realtime

start, stop, continue

global bpm

# examples

- Steve Reich Piano Phase