#!/usr/bin/env python
import sys
from pathlib import Path


def solve_problem(problem):
    s = problem.strip('\n')
    grid = {(y,x) for line in s.splitlines()
        for x,y in [tuple(map(int, line.split()))]}
    display(grid)


def display(grid):
    minx = min(0, min(x for y,x in grid))
    maxx = max(0, max(x for y,x in grid))
    miny = min(0, min(y for y,x in grid))
    maxy = max(0, max(y for y,x in grid))
    so = ''
    for y in range(miny, maxy+1):
        s = ['.o'[(y,x) in grid] for x in range(minx, maxx+1)]
        if y == 0:
            s[-minx] = '@'
        so += ''.join(s) + '\n'

    print(so)


def solve(args):
    if args.problem == '-':
        problem = sys.stdin.read()
    elif '\n' in args.problem:
        problem = args.problem
    else:
        fn = Path(args.problem)
        problem = fn.read_text()
    print(repr(problem), file=sys.stderr)
    sol = solve_problem(problem)
    print(sol)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('problem', default='-', help='text of the spaceship problem, or a filename')
    args = parser.parse_args()
    solve(args)
