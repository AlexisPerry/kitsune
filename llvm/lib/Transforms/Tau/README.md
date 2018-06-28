Adding TAU profiling calls via LLVM pass
========================================

Building
--------

This pass (in `Instrument.cpp`) can be built as part of the greater
Kitsune project, part of an LLVM build or independently, linked against
the LLVM libraries on your system.

### As part of Kitsune

Simply follow the Kitsune build instructions. You can find the shared
library at `<kitsune-build-dir>/llvm/lib/Tau_Profiling.so`.

### As part of a standard LLVM build

Copy this directory into `<llvm-root>/lib/Transform` and add the
following line to `<llvm-root>/lib/Transform/CMakeLists.txt`:

``` cmake
add_subdirectory(Tau)
```

### <span class="todo TODO">TODO</span> Test this

-   make sure the resulting library is in the expected `build/lib/`
    directory.

### Independently

Starting at `llvm/lib/Transform/Tau` as the working directory:

``` bash
mkdir -p build
cd build
cmake .. -DDTAU_PROF_STANDALONE=1
cmake --build .
```

Usage
-----

At the moment, there are three source files in the `sandbox/tau_profile`
directory that can be used for simple tests.

-   `rtlib.c` defines the two functions that are called by instructions
    inserted in the LLVM pass.
-   `example.c` is a Hello World C program to test the pass on
-   `example.cc` is a Hello World C++ program with some OO features to
    see what kinds of calls are visible after lowering to LLVM IR.

The following instructions assume `TAU_Profiling.so` and
`Tau_Profiling_CXX.so` have been built against the system installed
LLVM. If this is not the case, replace invocations of `clang` and
`clang++` with the appropriate versions.

The runtime library must be compiled first:

``` bash
clang -c rtlib.c
# produces rtlib.o
```

To compile and link the example C program with the plugin:

``` bash
clang -fplugin=path/to/TAU_Profiling.so example.c rtlib.o
```

To compile and link the example C++ program with the plugin:

``` bash
clang++ -fplugin=path/to/TAU_Profiling_CXX.so example.cc rtlib.o
```

The plugin currently produces a great deal of output logging what it's
looking at.

References
----------

-   [Writing an LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html)
-   [Adrian Sampson's LLVM pass
    guide (2015)](https://www.cs.cornell.edu/~asampson/blog/llvm.html)