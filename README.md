# Untitled Project

Locate sound sources using a microphone array! Fun for the whole family!

## Play with it

Prerequisites: FFTW (`libfftw3`), SDL (`libsdl1.2`), GLEW (`libglew`), OpenGL,
some 16-bit mono WAVs (split stereo into mono with audacity or something)

To build: `make`

To run:
    ./gen <output prefix> <input wav 1> [input wav 2...]
    ./view <input prefix> <number of sources>

## view

Plots estimates of sound source locations given audio streams from microphones
of known position.

## gen

Generates test audio streams for `view`.
