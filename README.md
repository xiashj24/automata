# Automata: generative music sequencing language

## next step

- make a c++ constexpr prototype library

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
- visual generator as first class citizen

## features

- rhythm visualization
- push the tick rate to audio rate for 8 bit sound generation
- web assembly build via emscripten
- x and y input as data source, normalized to mouse X and Y position on desktop
- how to handle timing offset and velocity? groove?
    - sub rhythms?
- generate modulation signal from rhythm using deterministic systems like cellular automata and Music Thing Modular Turing Machines
- lazy evaluation (what does it even mean?)
- support microtuning, frequency manipulation in hz
- integration with [hydra](https://hydra.ojack.xyz/)?

- Claude MCP server is a must!
    - but you need to figure out how to make agents receive audio feedback


## dependencies

- libremidi for midi input and output

## reference

- [Starting Forth](https://www.forth.com/wp-content/uploads/2018/01/Starting-FORTH.pdf)
- [Crafting Interpreters](https://craftinginterpreters.com/)
    - ch 1-3, 14

## academic studies

- Kurt James Werner
    - Generalizations of Velvet Noise and Their Use in 1-Bit Music
- minimalism
    - Ryoji Ikeda
- aethetic of aliasing
- 作って動かすALife
    - emergence, boid
- read about literature on algorithmic composition by computer music research people

## related projects 

- [sapf: Sound As Pure Form](https://github.com/lfnoise/sapf)
- tidal cycles and [strudel](https://strudel.cc/)
- ORCA
- isobar and signalflow
- [Reactable](https://reactable.com/)
- Faust
- Bespoke Synth
- Sugar Bytes Nest
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


## modular synths

- DivSkip
- [Intellijel Metropolix](https://intellijel.com/shop/eurorack/metropolix/)
- Mutable Instruments Marbles and grids
- Rob Hordik Benjolin
- Music Thing Modular Turing Machine with expanders
- Joranalogue Select 2