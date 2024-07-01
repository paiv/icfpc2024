#!/usr/bin/env python
import sys
from pathlib import Path


def solve(fn):
    text = fn.read_text().strip()
    grid = {(x+1j*y) for y,line in enumerate(text.splitlines())
        for x,c in enumerate(line) if c in '.L'}
    start = next((x+1j*y) for y,line in enumerate(text.splitlines())
        for x,c in enumerate(line) if c in 'L')

    bui = lambda g: (lambda n: g(g)(n))
    mk_add = lambda v: (lambda f: (lambda n: v if n == 1 else v + f(f)(n-1)))
    circle = lambda n: (
        bui(mk_add('D'))(n+2) +
        bui(mk_add('L'))(n+2) +
        bui(mk_add('U'))(n+4) +
        bui(mk_add('R'))(n+4)
        )
    mk_crl = lambda f: (
        (lambda n: circle(0) if n == 0 else f(f)(n-1) + circle(n*4))
    )
    prog = lambda: bui(mk_crl)(24)


    prog = (lambda bui: (
        (lambda mk_add:
            (lambda mk_crl:
                bui(mk_crl)(24)
            )(
                (lambda circle:
                    lambda f: (lambda n: circle(0) if n == 0 else f(f)(n-1) + circle(n*4))
                )(
                    lambda n: (
                        bui(mk_add('D'))(n+2) +
                        bui(mk_add('L'))(n+2) +
                        bui(mk_add('U'))(n+4) +
                        bui(mk_add('R'))(n+4)
                    )
                )
            )
        )(
            lambda v: (lambda f: (lambda n: v if n == 1 else v + f(f)(n-1)))
        )
    ))(
        lambda g: (lambda n: g(g)(n))
    )


    moves = 'DLUR'
    opsd = dict(zip(moves, [1j, -1, -1j, 1]))
    def prog0():
        def rep(v, n):
            return v * n
        def blo(n):
            so = ''
            n += 2
            so += rep('D', n)
            so += rep('L', n)
            n += 2
            so += rep('U', n)
            so += rep('R', n)
            return so
        def mai(n):
            if n == 0:
                return blo(0)
            else:
                return mai(n - 1) + blo(n * 4)
        return mai(24)

    pos = start
    seen = {pos}
    print(pos, file=sys.stderr)
    print(prog)
    for m in prog:
        p = pos + opsd[m]
        if p in grid:
            pos = p
            seen.add(pos)
    print(pos, file=sys.stderr)
    if seen == grid:
        return 'ok'
    return 'poop', len(grid) - len(seen)


fn = Path(__file__).parent.parent / 'task' / 'lambdaman' / 'lambdaman8.txt'
sol = solve(fn)
print(sol)
