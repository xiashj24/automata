- make a c++ constexpr prototype library
- make the main loop
- use a library for bmp image

signals constructed by filters are stateful, so you should probably make a new SequentialSignal type that does not allow random access.

more primitives
- shift register
- geiger
- signal (0-255) generation
- noise
- sample and hold

