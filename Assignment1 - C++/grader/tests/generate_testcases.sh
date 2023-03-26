#!/usr/bin/env bash

for testcase_generator in *.py; do
  testcase_filename="$(echo "$testcase_generator" | sed 's/.py/.in/')"
  python3 "$testcase_generator" > "$testcase_filename"
done
