#!/usr/bin/python

import os
import sys

def isFloat(s):
    result = True
    try:
        float(s)
    except ValueError:
        result = False

    return result

curr_benchmark = None

for line in sys.stdin:
    tokens = line.split()
    if len(tokens) >= 1 and tokens[0] == 'Running':
        curr_benchmark = ' '.join(tokens[1:])
        print('')
        print(curr_benchmark)
    elif len(tokens) == 3 and isFloat(tokens[0]) and isFloat(tokens[1]) and isFloat(tokens[2]):
        print(tokens[0] + ',' + tokens[1])

