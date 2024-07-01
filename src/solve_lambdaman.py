#!/usr/bin/env python
import heapq
import sys
from collections import deque
from pathlib import Path


class BitVec:
    def __init__(self, value=None):
        if value is None:
            self._value = bytearray()
            self._bitlen = 0
            self.bit_count = 0
        elif isinstance(value, BitVec):
            self._value = bytearray(value._value)
            self._bitlen = value._bitlen
            self.bit_count = value.bit_count
        elif isinstance(value, (list,tuple)):
            if all((isinstance(x, bool) or x == 0 or x == 1) for x in value):
                self._value = bytearray()
                self._bitlen = 0
                self.bit_count = 0
                for i,x in enumerate(value):
                    self[i] = x
            elif all(isinstance(x, int) for x in value):
                ...
            else:
                raise ValueError()
        elif isinstance(value, int):
            ...
        else:
            raise ValueError()

    def __getitem__(self, index):
        if index >= self._bitlen:
            raise IndexError()
        x = self._value[index // 8]
        return 1 if (x & (1 << (index % 8))) else 0

    def __setitem__(self, index, value):
        i,j = divmod(index, 8)
        if index >= self._bitlen:
            self._value.extend([0] * (i + 1 - len(self._value)))
            self._bitlen = index + 1
        x = self._value[i]
        m = 1 << j
        q = bool(x & m)
        t = bool(value)
        if q != t:
            x &= ~m
            x |= (t << j)
            self._value[i] = x
            self.bit_count += (t << 1) - 1


def solve_problem1(problem):
    s = problem.strip('\n')
    grid = {(y,x)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in '.L'}
    start = next((y,x)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in 'L')

    steps = ((0,1), (0,-1), (1,0), (-1,0))
    ops = 'RLDU'
    tr = dict(zip(ops, steps))

    visited = set()
    fringe = deque([(*start,)])
    seen = set()
    while fringe:
        pos = fringe.popleft()
        if pos in seen: continue
        seen.add(pos)
        py,px = pos
        for i in range(4):
            dy,dx = steps[i]
            qy = py + dy
            qx = px + dx
            q = (qy, qx)
            if q in grid:
                fringe.append(q)
    assert seen == grid
    print(f'{len(grid)} nodes', file=sys.stderr)

    goal = len(grid)
    fringe = deque([(*start, '')])
    seen = set()
    def _state(path):
        py,px = start
        visited = set()
        ps = set()
        ps.add((py,px))
        for m in path:
            dy,dx = tr[m]
            py += dy
            px += dx
            ps.add((py,px))
        return len(ps), f'{py},{px}-{sorted(ps)}'
    while fringe:
        if len(seen) % 10000 == 0:
            print(len(seen), file=sys.stderr)
        py,px,path = fringe.popleft()
        n,k = _state(path)
        if n == goal:
            return path
        if k in seen:
            continue
        seen.add(k)
        for i in range(4):
            dy,dx = steps[i]
            qy = py + dy
            qx = px + dx
            if (qy,qx) in grid:
                fringe.append((qy, qx, path + ops[i]))


def solve_problem2(problem):
    s = problem.strip('\n')
    grid = {(x+1j*y)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in '.L'}
    N = len(grid)
    start = next((x+1j*y)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in 'L')

    steps = (1, -1, 1j, -1j)
    ops = 'RLDU'
    otr = dict(zip(ops, steps))

    inf = float('inf')
    wei = dict()
    for p in grid:
        for q in grid:
            wei[p,q] = inf
            wei[q,p] = inf
    for p in grid:
        wei[p,p] = 0
        for s in steps:
            q = p + s
            if q in grid:
                wei[p,q] = 1
                wei[q,p] = 1
    for k in grid:
        for i in grid:
            for j in grid:
                w = wei[i,k] + wei[k,j]
                if w < wei[i,j]:
                    wei[i,j] = w
    assert all(v != inf for v in wei.values())
    npk = dict()
    for k in grid:
        npk[k] = sorted([(wei[k,q],q) for q in grid for w in [wei[k,q]] if w and w != inf],
            key=lambda p: p[0])

    def find_path(start, goal):
        fringe = deque([(start, '')])
        seen = set()
        while fringe:
            pos,path = fringe.popleft()
            if pos == goal:
                return path
            if pos in seen: continue
            seen.add(pos)
            for i in range(4):
                m = steps[i]
                q = pos + m
                if q in grid:
                    fringe.append((q, path + ops[i]))
        raise Exception(f'no path from {start} to {goal}')

    class Node:
        def __init__(self, wei, pos, path, visited):
            self.wei = wei
            self.pos = pos
            self.path = path
            self.visited = visited
            self._key = (wei, int(pos.imag), int(pos.real))
        def __hash__(self):
            return hash(self._key)
        def __eq__(self, other):
            return self._key == other._key
        def __lt__(self, other):
            return self._key < other._key

    fringe = [Node(0, start, tuple(), set())]
    seen = set()
    while fringe:
        if len(fringe) % 10000 == 0:
            print(len(fringe), file=sys.stderr)
        cur = heapq.heappop(fringe)
        if len(cur.visited) == N:
            break
        if cur in seen: continue
        seen.add(cur)
        for w,dest in npk[cur.pos]:
            if dest not in cur.visited:
                vis = set(cur.visited)
                vis.add(dest)
                heapq.heappush(fringe, Node(cur.wei+w, dest, cur.path + (dest,), vis))
    else:
        raise Exception('not found')
    pos = start
    moves = ''
    for p in cur.path:
        s = find_path(pos, p)
        moves += s
        pos = p
    return moves


def solve_problem(problem):
    s = problem.strip('\n')
    grid = {(x+1j*y)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in '.L'}
    N = len(grid)
    start = next((x+1j*y)
        for y,line in enumerate(s.splitlines())
        for x,c in enumerate(line)
        if c in 'L')

    pix = dict()
    ipx = dict()
    for i,p in enumerate(grid):
        pix[p] = i
        ipx[i] = p

    steps = (1, -1, 1j, -1j)
    ops = 'RLDU'
    otr = dict(zip(ops, steps))

    inf = float('inf')
    wei = dict()
    for p in range(N):
        for q in range(N):
            wei[p,q] = inf
            wei[q,p] = inf
    for p in range(N):
        wei[p,p] = 0
        for s in steps:
            q = ipx[p] + s
            if q in grid:
                k = pix[q]
                wei[p,k] = 1
                wei[k,p] = 1
    for k in range(N):
        for i in range(N):
            for j in range(N):
                w = wei[i,k] + wei[k,j]
                if w < wei[i,j]:
                    wei[i,j] = w
    assert all(v != inf for v in wei.values())
    npk = dict()
    for k in range(N):
        npk[k] = sorted([(wei[k,q],q) for q in range(N) for w in [wei[k,q]] if w and w != inf],
            key=lambda p: p[0])

    def find_path(start, goal):
        fringe = deque([(start, '')])
        seen = set()
        while fringe:
            pos,path = fringe.popleft()
            if pos == goal:
                return path
            if pos in seen: continue
            seen.add(pos)
            for i in range(4):
                m = steps[i]
                qp = ipx[pos] + m
                if (q := pix.get(qp)) is not None:
                    fringe.append((q, path + ops[i]))
        raise Exception(f'no path from {start} to {goal}')

    class Node:
        def __init__(self, wei, pos, path, visited):
            self.wei = wei
            self.pos = pos
            self.path = path
            self.visited = visited
            self._key = (wei, int(pos.imag), int(pos.real))
        def __hash__(self):
            return hash(self._key)
        def __eq__(self, other):
            return self._key == other._key
        def __lt__(self, other):
            return self._key < other._key

    fringe = [Node(0, pix[start], tuple(), BitVec([False]*N))]
    seen = set()
    while fringe:
        if len(fringe) % 10000 == 0:
            print(len(fringe), file=sys.stderr)
        cur = heapq.heappop(fringe)
        if cur.visited.bit_count == N:
            break
        if cur in seen: continue
        seen.add(cur)
        for w,dest in npk[cur.pos]:
            if not cur.visited[dest]:
                vis = BitVec(cur.visited)
                vis[dest] = True
                heapq.heappush(fringe, Node(cur.wei+w, dest, cur.path + (dest,), vis))
    else:
        raise Exception('not found')
    pos = pix[start]
    moves = ''
    for p in cur.path:
        s = find_path(pos, p)
        moves += s
        pos = p
    return moves


s4 = '''
#####################
#...#.#.........#...#
#.###.#.#####.###.###
#...#.#.....#.......#
###.#.#.###.#########
#.#....L..#.#.......#
#.#####.###.#.###.###
#.#.#...#.......#...#
#.#.#######.#######.#
#.#...#.#...#.#.....#
#.#.###.#.###.###.#.#
#.....#...#.......#.#
#.###.###.###.#####.#
#.#.#...#...#...#...#
###.#.#.#.#####.###.#
#...#.#...#.....#...#
#.###.#.#.#####.#####
#.....#.#.....#.#...#
#.###.#.#.#.#.#.#.###
#.#...#.#.#.#.#.....#
#####################
'''
s3 = '''
L...#.
#.#.#.
##....
...###
.##..#
....##
'''
s5 = '''
.....########...
....#...........
...#..######....
..#..#......#...
.#..#...##...#..
.#..#..#L.#...#.
.#...#....#...#.
..#...####...#..
...#........#...
....########....
................
'''
s7 = '''
############################
#............##............#
#.####.#####.##.#####.####.#
#.####.#####.##.#####.####.#
#.####.#####.##.#####.####.#
#..........................#
#.####.##.########.##.####.#
#.####.##.########.##.####.#
#......##....##....##......#
######.##############.######
######.##############.######
######.##..........##.######
######.##.###..###.##.######
######.##.#......#.##.######
#.........#......#.........#
######.##.#......#.##.######
######.##.########.##.######
######.##..........##.######
######.##.########.##.######
######.##.########.##.######
#............##............#
#.####.#####.##.#####.####.#
#.####.#####.##.#####.####.#
#...##........L.......##...#
###.##.##.########.##.##.###
###.##.##.########.##.##.###
#......##....##....##......#
#.##########.##.##########.#
#.##########.##.##########.#
#..........................#
############################
'''
t = solve_problem(s7)
print(t)


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
    parser.add_argument('problem', default='-', help='text of the lambdaman problem, or filename')
    args = parser.parse_args()
    solve(args)
