# TODO

- extend Stream type to do more things
- construct stream from a list of numbers
- sequencer
- function generator
- shift register
- geiger counter
- noise
- sample and hold
- accumulator → feedback → FFT

# framework

- visualize signal and internal states (imgui)
- tweak parameters by slider
    - imgui SliderFloat <-> local float -> Stream (next() just return the bounded float value)
    - thread safety?
- figure out how to do jit

