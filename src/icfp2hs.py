#!/usr/bin/env python
import json
import sys


_b94abc = '!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
_abc94 = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!"#$%&\'()*+,-./:;<=>?@[\\]^_`|~ \n'
_comms_ascii_tr = str.maketrans(_b94abc, _abc94)
_ascii_comms_tr = str.maketrans(_abc94, _b94abc)


class IcfpParser:
    def process(self, text):
        tokens = iter(text.split())
        expr = self._read1(tokens)
        s = self._emit_hs(expr)
        print(s)

    def encode_string(self, text):
        return 'S' + text.translate(_ascii_comms_tr)

    def _decode_number(self, text):
        v = 0
        for x in text:
            x = ord(x) - ord('!')
            v = v * 94 + x
        return v

    def _read1(self, tokens):
        for token in tokens:
            op = token[:1]
            match op:
                case 'F' | 'T' | 'I' | 'S':
                    return [token]
                case 'U':
                    a = self._read1(tokens)
                    return [token, a]
                case 'B':
                    a = self._read1(tokens)
                    b = self._read1(tokens)
                    return [token, a, b]
                case '?':
                    t = self._read1(tokens)
                    a = self._read1(tokens)
                    b = self._read1(tokens)
                    return [token, t, a, b]
                case 'L':
                    a = self._read1(tokens)
                    n = self._decode_number([token[1:]])
                    l = f'L{n}'
                    return [l, a]
                case 'v':
                    n = self._decode_number([token[1:]])
                    v = f'v{n}'
                    return [v]
                case _:
                    raise Exception(f'! unhandled {token!r}')

    def _emit_hs(self, expr):
        op = expr[0]
        match op[:1]:
            case 'F':
                return 'False'
            case 'T':
                return 'True'
            case 'I':
                v = 0
                for x in op[1:]:
                    x = ord(x) - ord('!')
                    v = v * 94 + x
                return str(v)
            case 'S':
                text = op[1:].translate(_comms_ascii_tr)
                return json.dumps(text)
            case 'U':
                op,a = expr
                x = self._emit_hs(a)
                match op[1]:
                    case '-':
                        return f'(-{x})'
                    case '!':
                        return f'(not {x})'
                    case '#':
                        return f'_icfp_a2n({x})'
                    case '$':
                        return f'_icfp_n2a({x})'
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case 'B':
                op,a,b = expr
                x = self._emit_hs(a)
                y = self._emit_hs(b)
                match op[1]:
                    case '+':
                        return f'({x} + {y})'
                    case '-':
                        return f'({x} - {y})'
                    case '*':
                        return f'({x} * {y})'
                    case '/':
                        return f'(div {x} {y})'
                    case '%':
                        return f'(rem {x} {y})'
                    case '<':
                        return f'({x} < {y})'
                    case '>':
                        return f'({x} > {y})'
                    case '=':
                        return f'({x} == {y})'
                    case '|':
                        return f'({x} || {y})'
                    case '&':
                        return f'({x} && {y})'
                    case '.':
                        return f'({x} ++ {y})'
                    case 'T':
                        return f'(take {x} {y})'
                    case 'D':
                        return f'(drop {x} {y})'
                    case '$':
                        return f'({x} {y})'
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case '?':
                op,t,a,b = expr
                q = self._emit_hs(t)
                x = self._emit_hs(a)
                y = self._emit_hs(b)
                return f'(if {q} then {x} else {y})'
            case 'L':
                op,a = expr
                v = op[1:]
                x = self._emit_hs(a)
                return f'(\\v{v} -> {x})'
            case 'v':
                return str(op)
            case _:
                raise Exception(f'! unhandled {op!r}')


if __name__ == '__main__':
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser()
    parser.add_argument('file', nargs='?', help='ICFP value file')
    args = parser.parse_args()
    if not args.file or args.file == '-':
        text = sys.stdin.read()
    else:
        text = Path(args.file).read_text()

    parser = IcfpParser()
    parser.process(text)

