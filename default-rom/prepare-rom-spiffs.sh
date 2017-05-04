#!/bin/bash
rm -rv data
cp -rv data_raw data
echo "Compressing"
gzip data/system/web/*
gzip data/system/web/**/*
echo "Done."
