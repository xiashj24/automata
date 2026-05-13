# Automata: generative music sequencing language

A minimal, RPN-based, feedback-capable interpreted music language, designed for LLM, human live coding, web sharing, and embedded hardware — with cellular automata as first-class primitives.

## design goals

- functional programming philosophy
- easy to read and write for both humans and LLMs
    - syntax should resemble natural language?
    - Reverse Polish Notation?
- suitable for live coding
- simple type system: interger only, not floats
- modular: build complex rhythms from basic building blocks
    - a basic rhythm generator is a pure function of uint->bool, which maps step index to trigger on/off
    - basic building blocks: euclid, rotate, reverse, interleave, boolean logic (* for and, + for or) (make a list!)
    - random source: bernoulli gate, velvet noise, with hardware sourced randomness
    - quantizer, selector
    - counter? system with memory?
    - 8 bit ADC and DAC
- get chaos-like behavior via feedback patching
- discrete timing model: ticks->steps->trigger
- immediate visual feedback (Bret Victor, shadertoy)
- visual generator as first class citizen

## features

- rhythm visualization
- push the tick rate to audio rate for sound generation
- web assembly build via emscripten
- x and y input as data source, normalized to mouse X and Y position on desktop
- how to handle timing offset and velocity? groove?
    - sub rhythms?
- generate modulation signal from rhythm using deterministic systems like cellular automata and Music Thing Modular Turing Machines
- lazy evaluation (what does it even mean?)
- microtuning, frequency manipulation in hz
- integration with [hydra](https://hydra.ojack.xyz/)?

- Claude MCP server is a must!
    - but you need to figure out how to make agents receive audio feedback


## dependencies

- libremidi for midi input and output

## reference

- [Starting Forth](https://www.forth.com/wp-content/uploads/2018/01/Starting-FORTH.pdf)
- [Crafting Interpreters](https://craftinginterpreters.com/)
    - ch 1-3, 14
- MAKE: electronic music from scratch
    - to learn about logic ICs

## [logic ICs](https://en.wikipedia.org/wiki/List_of_4000-series_integrated_circuits)

- mux, demux
- counter
- schmidt trigger
- PLL
- Bonus: figure out how the WASP filter does integration using CMOS logic
    - the language should be good enough for acid filters, because why not?

## academic studies

- Kurt James Werner
    - Generalizations of Velvet Noise and Their Use in 1-Bit Music
- minimalism
    - Ryoji Ikeda
- aethetics of aliasing
- 作って動かすALife
    - emergence, boid
- read about literature on algorithmic composition by computer music research people
- [Bret Victor: Inventing on Principle](https://www.youtube.com/watch?v=PUv66718DII)
    - https://thebookofshaders.com/edit.php
    - shadertoy
- number theory, combinitorial mathematics, and pseudo-random number generators
- [Algorithmic Pattern project](https://algorithmicpattern.org/) by Alex Mclean
- music theory: son clave, tresillo, world music
    - The Geometry of Musical Rhythm by Godfried T. Toussaint

## Books

- Generative Art: A practical guide using Processing
- A New Kind of Science by Stephen Wolfram

## related projects

- [sapf: Sound As Pure Form](https://github.com/lfnoise/sapf) and SuperCollider
- tidal cycles and [strudel](https://strudel.cc/)
- ORCA
- isobar and signalflow
- [Klang](https://github.com/nashaudio/klang)
    - the idea of using c++ to express signal flow is interesting
- [Reactable](https://reactable.com/)
- Faust
- Bespoke Synth
- games of Zachtronics
- a whole bunch of similar algorave projects (dang! people have thought about this before...)
    - https://github.com/tarpit-collective/cane
    - https://github.com/pd3v/line
    - https://xn--melrse-egb.org/
    - https://machiaworx.net/recode/doku.php?id=start
    - https://github.com/irritant/serialist (inactive)
    - https://tweakable.org/examples
    - https://github.com/echolevel/wulfcode
    - https://github.com/sdclibbery/limut
    - https://github.com/Mdashdotdashn/krill
    - https://github.com/nnirror/facet
    - https://github.com/lucretiomsp/Coypu

## Sugar Bytes Nest

inspired by a breakboard prototype of logical ICs

this maps easily to an object oriented approach

- 4051 multiplexer
- shift register
- 8 bit DAC
- 8 bit ADC
- clock division: 1/2/4/8/16/32/64/128 (with swing built in)
- delay
- clock divider
- envelope
- S&H
- counters
- logic gates: or, nor, xor, xnor, and, nand, >, <, ==, !=
- gate to trigger
- flip-flop
- geiger: Binary waveform (Low/High) with random occurrence.
- MIDI note, gate, velocity, cc, pitchbend, play
- arpeggiator with up/down trigger input
- scale

These building blocks corresponds to different abstractions in a functional approach.

## music hardware

- DivSkip
- [Intellijel Metropolix](https://intellijel.com/shop/eurorack/metropolix/)
- Mutable Instruments Marbles and grids
- Rob Hordik Benjolin
- Music Thing Modular Turing Machine with expanders
- Joranalogue Select 2
- Neutral Labs LUNA