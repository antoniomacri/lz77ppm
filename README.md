
lz77ppm
=======

A simple implementation of the LZ77 compression algorithm in C.

Academic project, _Progettazione e Produzione Multimediale_, Università di Pisa.

This project is written in C, following the C99 standard, using GCC, `make` and `doxygen`.


Overview
--------

This project implements an LZ77 compressor, consisting of a static library (which contains all the core functionalities) and an executable program (which acts as a command-line interface to the compression algorithm).

The compressor follows the standard LZ77 algorithm. A **look-ahead buffer** points to a certain position on the input data — to the beginning at the start. The algorithm looks for the longest match between the first bytes of the look-ahead buffer and any block of the same length inside a **sliding window** (which is moved over the data itself). If a match is found, the substring is replaced with a **pointer** to the start of the match (i.e. the offset from the beginning of the window) and the **length** of the match; otherwise, a symbol is copied to the output. Then, the window is shifted by the number of consumed bytes, i.e. the length of the match or the size of a symbol.


The library
-----------

The core compression routines are implemented as a small C library. They can be used to compress and decompress data from/to streams which are backed by a memory buffer or by a socket/file descriptor. That is, one can compress data stored in a buffer and send the output directly through a socket to a receiver, which in turn can then decompresses the socket stream directly to, e.g., a file.

The main requirements of the library can be summarized as follows:

  * it _shall_ be able to (de)compress in-memory data;
  * it _shall_ be able to (de)compress data from/to a file or socket;
  * it _shall_ handle files up to 4 GiB (and possibly greater);
  * it _shall_ run on Ubuntu and FreeBSD and _should_ be as much portable as possible.


The command line interface
--------------------------

A command line program is also provided, which can be used to compress and decompress files up to (and actually greater than) 4 GiB. Furthermore, it can be instructed to receive the input from stdin and send the output to stdout, enabling it to be chained with other commands using pipes.

The main requirements of the CLI program can be summarized as follows:

  * it _shall_ handle files up to 4 GiB (and possibly greater);
  * it _shall_ support input and output from/to a pipe;
  * it _shall_ run on Ubuntu and FreeBSD and _should_ be as much portable as possible.

The memory footprint of the executable file (with the library linked statically) is around 160 KiB (on a 64-bit system).


Directory structure
-------------------

  * `liblz77ppm/`: contains all source files of the library;
  * `liblz77ppm-test/`: contains a few tests for the library;
  * `lz77ppm/`: contains source files of the command line interface to the library;
  * `doc/`: will contain documentation produced by `Doxygen` and other documentation files:
  * `bin/`: will contain all executables;
  * `lib/`: will contain the static library `liblz77ppm.a`;
  * `Doxyfile`: configuration file for `Doxygen`;
  * `Makefile`: used by `make` to build everything.


Compiling
---------

 *Target*                    | *Group* | *Output*
-----------------------------|:-------:|--------------------------------------
 `library`                   | `all`   | `lib/liblz77ppm.a`
 `cli`                       | `all`   | `bin/lz77ppm`
 `test-library`              | `test`  | `bin/liblz77ppm-test`
 `documentation`             | –       | `doc/html/...`
 `clean`                     | –       | Remove object and dependency files
 `distclean`                 | –       | Remove all generated files

Required dependencies:

  * `m` (_C math library_)

For faster execution, make sure to build (on branch master) without assertions and with optimization flags enabled:

```
#!bash
CFLAGS='-O3 -DNDEBUG' make distclean cli
```

The whole project has been built and tested on the following operating systems and architectures:

  * Ubuntu 14.04, x86_64

        #!bash
        $ uname -srpo
        Linux 3.13.0-32-generic x86_64 GNU/Linux
        $ python -c "import sys; print(sys.byteorder)"
        little
        $ bash --version
        GNU bash, version 4.3.11(1)-release (x86_64-pc-linux-gnu)
        $ make --version
        GNU Make 3.81
        $ gcc --version
        gcc (Ubuntu 4.8.2-19ubuntu1) 4.8.2
        $ ar --version
        GNU ar (GNU Binutils for Ubuntu) 2.24

  * FreeBSD 10.0, x86_64 (VirtualBox)

        #!bash
        $ uname -srpo
        FreeBSD 10.0-RELEASE amd64
        $ python -c "import sys; print(sys.byteorder)"
        little
        $ bash --version
        GNU bash, version 4.3.24(0)-release (amd64-portbld-freebsd10.0)
        $ gmake --version
        GNU Make 3.82
        $ cc --version
        FreeBSD clang version 3.3 (tags/RELEASE_33/final 183502) 20130610
        Target: x86_64-unknown-freebsd10.0
        Thread model: posix
        $ ar --version
        BSD ar 1.1.0 - libarchive 3.1.2

  * Debian Wheezy, PowerPC (QEMU)

        #!bash
        $ uname -srpo
        Linux 3.2.0-4-powerpc unknown GNU/Linux
        $ python -c "import sys; print(sys.byteorder)"
        big
        $ bash --version
        GNU bash, version 4.2.37(1)-release (powerpc-unknown-linux-gnu)
        $ make --version
        GNU Make 3.81
        $ gcc --version
        gcc (Debian 4.6.3-14) 4.6.3
        $ ar --version
        GNU ar (GNU Binutils for Debian) 2.22


Running
-------

After building the project, all programs are placed in the `bin/` folder. It also contains many example files and a couple of shell scripts that can be used to perform the compression and decompression on a given file and check that they execute successfully:

    #!bash
    bin/liblz77ppm-test
    bin/lz77ppm
    bin/test-file.sh
    bin/test-pipe.sh



Implementation details
======================

Without claiming to be exaustive, in this section are discussed a few key aspects regarding the implementation of the compressor.


The input and output buffers
----------------------------

In order to avoid unnecessary copies, the sliding window and the look-ahead buffer are implemented as pointers to the input data buffer. The input data can be obtained either from a file descriptor or from a given memory buffer. In the former case, the algorithm allocates a fixed-size buffer, which is filled as needed with data read from the file descriptor. In the latter case, instead, no buffer is allocated, since the algorithm directly works on the original one.

Also the output can be backed by a memory buffer or by a file descriptor. In the former case, the user can provide a preallocated buffer of a certain size, or let the algorithm allocate it as needed. If the output does not fit into the size of the buffer, the algorithm needs to reallocate it, but this is not always possible with a user-allocated buffer (the user himself specifies so to the algorithm).


The sliding window and the look-ahead buffer
--------------------------------------------

As already stated, in order to avoid unnecessary copies, the sliding window and the look-ahead buffer are implemented as pointers to the input data buffer. Their size can change from file to file — when compressing a file, it can be specified by the user — enabling a better compression based on the properties of the data. Actually, the user specifies the _maximum_ size of the sliding window and look-ahead buffer, since their _current_ size can vary during the execution of the algorithm depending on the iteration.

For instance, at the beginning the window is empty and its size is zero, since no data has been already processed. Notice that this differs from many other implementations of the LZ77 compressor, in which the window is initially filled by a given "dictionary". This would enable a slightly better compression for the first bytes, but would make impossibile to work on a given user buffer without further copies.

The figure below shows how the sliding window and look-ahead buffer change during the execution of the algorithm.

| *W*max = 4, *L*max = 2 |
|:----------------------:|
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-0.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-1.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-2.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-3.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-4.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-5.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-6.png) ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-curr-buffers-7.png) |


Encoding the match length with Huffman
--------------------------------------

Since matches in the sliding window are more likely to have short lengths, a Huffman compressor can be employed to compress them. However, a generic static Huffman is not feasible, since it would require two passes (the first pass for counting the number of occurrencies) and therefore the need to store the produced output or to process again the whole input. Similarly, an adaptive version of Huffman would be too overwhelming. The chosen implementation is a precomputed table.

Explicit codes are created for values smaller than a given maximum (8), assuming greater probabilities (and so shorter codes) for smaller values, as shown in the following table.

| Value | Code    | N. of bits |
|:------|:--------|:----------:|
| 0     | 000 000 | 6          |
| 2     | 11      | 2          |
| 3     | 10      | 2          |
| 4     | 01      | 2          |
| 5     | 001     | 3          |
| 6     | 000 1   | 4          |
| 7     | 000 01  | 5          |
| 8+    | 000 001 | 6          |

Notice that a value of 1 is never produced, since that match would be encoded as a symbol token. Instead, the length of 0 is used to mark the end of the stream, and hence it has a long code.

If the length of the match is greater than or equal to 8, the code of 8 is first produced (acts as a prefix). Then the difference between the actual length and 8 is emitted on a fixed number of bits, which is properly sized to accomodate values up to *L*max − 8. The number of bits used for this difference can change from file to file, since the size of the look-ahead buffer *L*max is chosen by the user.

Actually, the implemented version is slightly different, because not only the maximum difference *L*max − 8 can vary, but also the minimum possible encoded value (`min_value`). Since the size of the sliding window and of the look-ahead buffer is specified by the user, the minimum encoded value for the length is not always 2, but depends on the relation between the size of a phrase token and a sequence of symbol tokens. As the sliding window and the look-ahead buffer scale up, the widths of their respective fields in a phrase token increase too, causing a sequence of (for instance) 2 symbols to be better encoded using two symbol tokens instead of a single phrase token. Therefore, a value of 2 would never be used, and its code can be reused for values of 3, allowing a little improvement in the efficiency of the compressor.

| Value                  | Code    | N. of bits |
|:-----------------------|:--------|:----------:|
| 0                      | 000 000 | 6          |
| `min_value`            | 11      | 2          |
| `min_value`+1          | 10      | 2          |
| `min_value`+2          | 01      | 2          |
| `min_value`+3          | 001     | 3          |
| `min_value`+4          | 000 1   | 4          |
| `min_value`+5          | 000 01  | 5          |
| `min_value`+6 ÷ *L*max | 000 001 | 6          |


Mantaining a tree to speed-up search in the window
--------------------------------------------------

Looking up in the sliding window using a simple linear search is an expensive operation, since it costs O(*W* * *L*). Therefore, a binary search tree is constructed on top of the sliding window, reducing the operation to a logarithmic complexity.

In principle, the tree contains all the words of the window, with lengths ranging from 1 to *L*max. However, since any word can be considered as a prefix of another maximum-length word, the size of the tree can be scaled down by keeping only words with length *L*. The dictionary has therefore a maximum size determined solely by the size of the sliding window *W* and can be allocated just once as a single array.

Each node in the tree contains a word (of length *L*) and the indices to the parent and child nodes. Actually, instead of storing the whole word, a simple offset to the beginning of the word inside the sliding window is sufficient. Furthermore, the information about the offset can be associated implicitly to each element of the array.

    #!cpp
    struct tree_node {
        uint16_t parent;
        uint16_t smaller_child;
        uint16_t larger_child;
    };

When a search in the tree is performed, at each step the algorithm needs to obtain the word associated to the current node, and besides, any new word from the window has to be added to the tree in a specific position of the array of nodes. Suppose that the word (of length *L*) starting at position *k* in the sliding window is implicitly associated, with a certain function, to the *i*-th element of the array. Such function is not simply *i = k*, because the window actually slides over the input data and hence *k* increases far beyond the maximum allowed *i* (i.e., *W* − 1).

If *b* is the offset of the window inside the input data buffer, the word at position *k* in the window is added to the array of nodes at position *i* = (*b* + *k*) mod *W*. In other words, the array of nodes is used like a circular array.

On the other hand, when the algorithm steps through the tree during a search and needs to obtain the word associated to the current node, the offset is calculated as *k* = (*i* − (*b* mod *W*) + *W*) mod *W*, where *W* is added to ensure that the dividend of the outer modulo operation is always positive (so that, regardless of the C standard, the result is always positive).

| Data | Tree |
|:----:|:----:|
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-0.png)  | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-1.png)  |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-2.png)  | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-3.png)  |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-4.png)  | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-5.png)  |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-6.png)  | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-7.png)  |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-8.png)  | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-9.png)  |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-10.png) | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-11.png) |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-12.png) | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-13.png) |
| ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-14.png) | ![data](https://bitbucket.org/antoniomacri/lz77ppm/raw/master/doc/img/print-tree-15.png) |
