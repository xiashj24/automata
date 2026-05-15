## How to build and run unit tests
```
cmake --preset develop
cmake --build build/develop
ctest --test-dir build/develop --output-on-failure
```

## misc

- don't use single short variable names unless it's conventional, e.g. x, y, z
    - don't use rhy as shorthand for Rhythm, use more descriptive names

- constexpr whenever possible