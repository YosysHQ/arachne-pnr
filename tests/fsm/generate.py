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

def random_simple_expr(variables):
    c = random.choice(['uni', 'var', 'const'])
    if c == 'uni':
        op = random.choice(['+', '-', '~', '|', '&', '^', '~^', '!', '$signed', '$unsigned'])
        return "%s(%s)" % (op, random_simple_expr(variables))
    if c == 'var':
        return random.choice(variables)
    if c == 'const':
        bits = random.randint(1, 10)
        return "%d'd%s" % (bits, random.randint(0, 2**bits-1))
    raise AssertionError

def random_expr(variables):
    c = random.choice(['bin', 'uni', 'var', 'const'])
    if c == 'bin':
        op = random.choice(['+', '-', '*', '<', '<=', '==', '!=', '>=', '>', '<<', '>>', '<<<', '>>>', '|', '&', '^', '~^', '||', '&&'])
        return "(%s %s %s)" % (random_simple_expr(variables), op, random_simple_expr(variables))
    if c == 'uni':
        op = random.choice(['+', '-', '~', '|', '&', '^', '~^', '!', '$signed', '$unsigned'])
        return "%s(%s)" % (op, random_expr(variables))
    if c == 'var':
        return random.choice(variables)
    if c == 'const':
        bits = random.randint(1, 10)
        return "%d'd%s" % (bits, random.randint(0, 2**bits-1))
    raise AssertionError

for idx in range(25):
    with file('temp/uut_%05d.v' % idx, 'w') as f:
        with redirect_stdout(f):
            rst2 = random.choice([False, True])
            if rst2:
                print('module uut_%05d(clk, rst1, rst2, rst' % (idx), end="")
            else:
                print('module uut_%05d(clk, rst' % (idx), end="")
            variables=['a', 'b', 'c', 'x', 'y', 'z']
            io = {'a': 'input',
                  'b': 'input',
                  'c': 'input',
                  'x': 'output',
                  'y': 'output',
                  'z': 'output'}
            h = {}
            for v in variables:
                h[v] = random.randint(0, 12)
            for v in variables:
                for i in range(0,h[v]+1):
                    print(", %s%d" % (v, i), end="")
            print(');')
            if rst2:
                print('  input clk, rst1, rst2;')
                print('  output rst;')
                print('  assign rst = rst1 || rst2;')
            else:
                print('  input clk, rst;')
            for v in ['x', 'y', 'z']:
                print('  reg%s [%d:0] %s;' % (random.choice(['', ' signed']), h[v], v))
            for v in ['a', 'b', 'c']:
                for i in range(0,h[v]+1):
                    print('  %s %s%d;' % (io[v], v, i))
            for v in ['x', 'y', 'z']:
                for i in range(0,h[v]+1):
                    print('  %s %s%d = %s[%d];' % (io[v], v, i, v, i))
            for v in ['a', 'b', 'c']:
                print('  wire%s [%d:0] %s = {' % (random.choice(['', ' signed']), h[v], v), end="")
                for i in range(h[v],-1,-1):
                    if i < h[v]:
                        print(',', end="")
                    print('%s%d' % (v, i), end="")
                print('};')
            state_bits = random.randint(5, 16);
            print('  %sreg [%d:0] state;' % (random.choice(['', '(* fsm_encoding = "one-hot" *)',
                    '(* fsm_encoding = "binary" *)']), state_bits-1))
            states=[]
            for i in range(random.randint(2, 9)):
                n = random.randint(0, 2**state_bits-1)
                if n not in states:
                    states.append(n)
            print('  always @(posedge clk) begin')
            print('    if (%s) begin' % ('rst1' if rst2 else 'rst'))
            print('      x <= %d;' % random.randint(0, 2**31-1))
            print('      y <= %d;' % random.randint(0, 2**31-1))
            print('      z <= %d;' % random.randint(0, 2**31-1))
            print('      state <= %d;' % random.choice(states))
            print('    end else begin')
            print('      case (state)')
            for state in states:
                print('        %d: begin' % state)
                for var in ('x', 'y', 'z'):
                    print('            %s <= %s;' % (var, random_expr(variables)))
                next_states = states[:]
                for i in range(random.randint(0, len(states))):
                    next_state = random.choice(next_states)
                    next_states.remove(next_state)
                    print('            if ((%s) %s (%s)) state <= %s;' % (random_expr(variables),
                            random.choice(['<', '<=', '>=', '>']), random_expr(variables), next_state))
                print('          end')
            print('      endcase')
            if rst2:
                print('      if (rst2) begin')
                print('        x <= a;')
                print('        y <= b;')
                print('        z <= c;')
                print('        state <= %d;' % random.choice(states))
                print('      end')
            print('    end')
            print('  end')
            print('endmodule')
    with file('temp/uut_%05d.ys' % idx, 'w') as f:
        with redirect_stdout(f):
            print('rename uut_%05d gate' % idx)
            print('read_verilog temp/uut_%05d.v' % idx)
            print('rename uut_%05d gold' % idx)
            print('hierarchy; proc;;')
            print('miter -equiv -flatten -ignore_gold_x -make_outputs -make_outcmp gold gate miter')
            print('sat -verify-no-timeout -timeout 20 -seq 5 -set-at 1 %s_rst 1 -prove trigger 0 -prove-skip 1 -show-inputs -show-outputs miter' % ('gold' if rst2 else 'in'))
    with file('temp/uut_%05d_pp.ys' % idx, 'w') as f:
        with redirect_stdout(f):
            print('rename uut_%05d gate' % idx)
            print('read_verilog temp/uut_%05d.v' % idx)
            print('rename uut_%05d gold' % idx)
            print('hierarchy; proc;;')
            print('techmap -map +/adff2dff.v; opt;;')
            print('miter -equiv -flatten -ignore_gold_x -make_outputs -make_outcmp gold gate miter')
            print('sat -verify-no-timeout -timeout 20 -seq 5 -set-at 1 %s_rst 1 -prove trigger 0 -prove-skip 1 -show-inputs -show-outputs miter' % ('gold' if rst2 else 'in'))
