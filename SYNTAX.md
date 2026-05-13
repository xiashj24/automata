# rhythem generators

a rhythm is represented by a bitset

in functional programming, the states are "visible" from the outside, so you can manipulate and combine the states and use them to generate visualizations trivially.

remember, the syntax need to be LLM friendly, and constexpr everything

# operators

- = (conversion)

- boolean logic
    - * (and)
    - + (or)
    - ! (not, invert)
    - - (subtract)

- % (modulo)

- comparator

- @/d/z (delay)

- ~ feedback
    - insert one tick delay automatically

# signal input

- x and y reserved, z for touch pressure
- i reserved for step index

# rhythm <-> signal conversion

- adc: signal to beat
- dac: beat to signal

# randomness

- bernoulli gate
- velvet noise

# comments

use // for comments

# system realtime

start, stop, continue

global bpm

# example

- Music Thing Modular Turing Machine
- Steve Reich Piano Phase
- Euclid Arppegiator