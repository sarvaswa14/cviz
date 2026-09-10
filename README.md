# cviz — an inspectable C compiler

A compiler for a defined subset of ISO C (C99 baseline), written in C++,
which emits a structured trace of every phase alongside its normal output.
A browser-based visualiser reads that trace.

BCSE307P Compiler Design Laboratory — Sarvaswa Bhanti, 24BCE0333

## Layout

    src/       compiler sources
    include/   headers
    spec/      language and trace-format specifications
    tests/     valid/ and invalid/ test corpora
    viz/       trace visualiser (single page, no build step)
    docs/      proposal and development log

## Build

    make

## Usage

    cviz file.c --emit-trace trace.json