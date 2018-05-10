small-scheme
============

A small implementation of a scheme interpreter to practise implementing a
garbage collector. This I wanted to do before starting work on the more
complicated garbage collector that would need to be written for the intml
ml-like language.

The interpreter is in the [reader](reader) subdirectory of this repository,
the garbage collector in runtime.c

Requirements
------------
The reader project requires libbsd on linux. On debian based systems, install
as so:

```shell
# apt-get install libbsd-dev -y
```

