#!/bin/bash

estfile="cwnd.dat"
awk '$3=="CWND" {print $2,$6/1440}' $1 > /tmp/$estfile
