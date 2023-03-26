#!/usr/bin/env bash

if [[ $# -lt 3 ]]; then
  echo Usage: "$0" path/to/engine path/to/tests path/to/logs
  echo path/to/tests should be a directory containing test cases,
  echo so files matching path/to/tests/*.in will be used as test cases
  echo
  echo Logs will be placed in the directory path/to/logs/date_of_test
  echo
  echo Test results will be printed to stdout in csv format:
  echo date_of_test,engine,testfile,exitcode
  exit 1
fi

engine=$1
testfiles_path=$2
logs_path=$3
date_of_test="$(date +%y-%m-%d-%H-%M-%S)"
log_path="$logs_path/$date_of_test"

if [ ! \( -d "$testfiles_path" \) ]; then
  echo Please provide a valid path to the test cases directory
  echo path/to/tests should be a directory that contains files called xxxxx.in
  echo Each .in file will be used as a testcase for the grader
fi

mkdir -p "$log_path"

for testfile in "$testfiles_path"/*.in; do
  engine_basename="$(basename $engine)"
  testfile_basename="$(basename $testfile)"
  logfilename="$log_path/$date_of_test-$engine_basename-$testfile_basename"
  timeout 10s ./grader "$engine" < "$testfile" > "$logfilename.stdout" 2> "$logfilename.stderr"
  echo "$date_of_test,$engine_basename,$testfile_basename,$?"
done | tee $log_path-grade_all_testcases.csv
