#!/bin/bash

git apply patches/fd9f2036f26d83f9fcfe93042fb952e5a7fe2167.patch
python build/gen.py
ninja -C out