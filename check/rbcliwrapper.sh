#!/bin/bash

# This script will work only if there is one .eval file in results/
# check/results should only contain results from one run

cd check/
./evalcheck_cluster.sh results/check.*.eval
rbcli up results/check.*.{err,out,set}