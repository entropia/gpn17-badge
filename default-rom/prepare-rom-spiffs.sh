#!/bin/bash
rm -rv data
cp -rv data_raw data
echo "Compressing"
gzip data/deflt/web/*
gzip data/deflt/web/**/*
echo "Done."
