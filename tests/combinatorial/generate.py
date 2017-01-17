#!/usr/bin/python

from __future__ import division
from __future__ import print_function

import sys
import random
from contextlib import contextmanager

random.seed(1)

@contextmanager
def redirect_stdout(new_target):
    old_target, sys.stdout = sys.stdout, new_target
    try:
        yield new_target
    finally:
        sys.stdout = old_target

def random_term(variables):
    n_inputs = random.randint(4, 9)
    inputs = [random.choice(variables) for i in range(0, n_inputs)]
    n_terms = random.randint(3, 5)
    term = ' | '.join([
        ('(' 
         + ' & '.join([
             random.choice([v, '~' + v])
             for v in inputs])
         + ')')
        for i in range(0, n_terms)])
    return term

for idx in range(25):
    with file('temp/uut_%05d.v' % idx, 'w') as f:
        with redirect_stdout(f):
            pins = 96
            
            n_inputs = random.randint(3, pins / 2)
            n_outputs = random.randint(3, pins / 2)
            
            print('module uut_%05d(' % (idx), end="")
            
            variables = ['i0']
            print('input i0', end='')
            for i in range(1, n_inputs+1):
                v = 'i%d' % (i)
                print(', input %s' % (v), end='')
                variables.append(v)
            for i in range(0, n_outputs+1):
                print(', output o%d' % (i), end='')
            print(');')
            
            n_temps = random.randint(3,50)
            for i in range(0, n_temps):
                p = random.random()
                if p < 0.05:
                    width = random.randint(3, 16)
                    a = ('{' 
                         + ', '.join([random.choice(variables) for j in range(0,  width)])
                         + '}')
                    b = ('{' 
                         + ', '.join([random.choice(variables) for j in range(0,  width)])
                         + '}')
                    op = random.choice(['+', '-'])
                    print('  wire [%d:0] t%d = %s %s %s;'
                          % (width - 1, i, a, op, b))
                    for j in range(0, width):
                        variables.append('t%d[%d]' % (i, j))
                elif p < 0.1:
                    width = random.randint(3, 16)
                    a = ('{' 
                         + ', '.join([random.choice(variables) for j in range(0,  width)])
                         + '}')
                    b = ('{' 
                         + ', '.join([random.choice(variables) for j in range(0, width)])
                         + '}')
                    op = random.choice(['<', '<=', '>', '>=', '==', '!='])
                    print('  wire t%d = %s %s %s;'
                          % (i, a, op, b))
                    variables.append('t%d' % (i))
                else:
                    term = random_term(variables)
                    print('  wire t%d = %s;' % (i, term))
                    variables.append('t%d' % (i))
            
            for i in range(0, n_outputs+1):
                term = random_term(variables)
                print('  assign o%d = %s;' % (i, term))
            
            print('endmodule')
    with file('temp/uut_%05d.ys' % idx, 'w') as f:
        with redirect_stdout(f):
            print('rename uut_%05d gate' % idx)
            print('read_verilog temp/uut_%05d.v' % idx)
            print('rename uut_%05d gold' % idx)
            print('hierarchy; proc;;')
            print('miter -equiv -flatten -ignore_gold_x -make_outputs -make_outcmp gold gate miter')
            print('sat -verify-no-timeout -timeout 20 -prove trigger 0 -show-inputs -show-outputs miter')
    with file('temp/uut_%05d_pp.ys' % idx, 'w') as f:
        with redirect_stdout(f):
            print('rename uut_%05d gate' % idx)
            print('read_verilog temp/uut_%05d.v' % idx)
            print('rename uut_%05d gold' % idx)
            print('hierarchy; proc;;')
            print('techmap -map +/adff2dff.v; opt;;')
            print('miter -equiv -flatten -ignore_gold_x -make_outputs -make_outcmp gold gate miter')
            print('sat -verify-no-timeout -timeout 20 -prove trigger 0 -show-inputs -show-outputs miter')
