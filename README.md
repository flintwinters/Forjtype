# Forj

Forj is a typed lisp variant.  It is stack based, and types are first-class and follow the homoiconicity of a usual lisp implementation.

It is written in freestanding C, using a reference counter for memory safety, so it can run on anything.
The only dependency is some sort of malloc.

## Building

download `git clone git@github.com:flintwinters/Forjtype.git`

enter the directory `cd Forjtype`

Run either `make fj` to compile the binary itself.

Or `make val` to run valgrind
