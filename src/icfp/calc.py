#!/usr/bin/env python
import sys
from collections import ChainMap
from functools import cache


_b94abc = '!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
_abc94 = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!"#$%&\'()*+,-./:;<=>?@[\\]^_`|~ \n'
_comms_ascii_tr = str.maketrans(_b94abc, _abc94)
_ascii_comms_tr = str.maketrans(_abc94, _b94abc)


class IcfpCalc:
    def evaluate(self, text):
        tokens = iter(text.split())
        expr = self._read1(tokens)
        value = self._eval(expr, context=ChainMap())
        return value

    def encode_string(self, text):
        return 'S' + text.translate(_ascii_comms_tr)

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
                    n = self._eval(['I' + token[1:]])
                    l = f'L{n}'
                    return [l, a]
                case 'v':
                    n = self._eval(['I' + token[1:]])
                    v = f'v{n}'
                    return [v]
                case _:
                    raise Exception(f'! unhandled {token!r}')

    def _eval(self, expr, context=None):
        print('** eval', expr, file=sys.stderr)
        op = expr[0]
        match op[:1]:
            case 'F':
                return False
            case 'T':
                return True
            case 'I':
                v = 0
                for x in op[1:]:
                    x = ord(x) - ord('!')
                    v = v * 94 + x
                return v
            case 'S':
                text = op[1:].translate(_comms_ascii_tr)
                return text
            case 'U':
                op,a = expr
                x = self._eval(a, context)
                match op[1]:
                    case '-':
                        return -x
                    case '!':
                        return not x
                    case '#':
                        v = 0
                        for c in x:
                            v = v * 94 + _abc94.index(c)
                        return v
                    case '$':
                        if x <= 0:
                            return _abc94[0]
                        res = ''
                        while x:
                            x,t = divmod(x, 94)
                            s = _abc94[t]
                            res = s + res
                        return res
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case 'B':
                op,a,b = expr
                x = self._eval(a, context)
                y = self._eval(b, context)
                match op[1]:
                    case '+':
                        return x + y
                    case '-':
                        return x - y
                    case '*':
                        return x * y
                    case '/':
                        if x < 0 and y > 0:
                            return -((-x) // y)
                        return x // y
                    case '%':
                        if x < 0 and y > 0:
                            return -((-x) % y)
                        return x % y
                    case '<':
                        return x < y
                    case '>':
                        return x > y
                    case '=':
                        return x == y
                    case '|':
                        return x or y
                    case '&':
                        return x and y
                    case '.':
                        return x + y
                    case 'T':
                        return y[:x]
                    case 'D':
                        return y[x:]
                    case '$':
                        try:
                            return x(y, context)
                        except TypeError:
                            pass
                        import pdb; pdb.set_trace()
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case '?':
                op,t,a,b = expr
                if self._eval(t, context):
                    return self._eval(a, context)
                else:
                    return self._eval(b, context)
            case 'L':
                op,a = expr
                v = 'v' + op[1:]
                def wrap(v):
                    _eval = self._eval
                    ctx = context.new_child()
                    def f(u, context):
                        ctx[v] = u
                        return _eval(a, ctx)
                    return f
                return wrap(v)
            case 'v':
                v = 'v' + op[1:]
                return context[v]
            case _:
                raise Exception(f'! unhandled {op!r}')

    def self_check(self):
        s = '''? B= B$ B$ B$ B$ L$ L$ L$ L# v$ I" I# I$ I% I$ ? B= B$ L$ v$ I+ I+ ? B= BD I$ S4%34 S4 ? B= BT I$ S4%34 S4%3 ? B= B. S4% S34 S4%34 ? U! B& T F ? B& T T ? U! B| F F ? B| F T ? B< U- I$ U- I# ? B> I$ I# ? B= U- I" B% U- I$ I# ? B= I" B% I( I$ ? B= U- I" B/ U- I$ I# ? B= I# B/ I( I$ ? B= I' B* I# I$ ? B= I$ B+ I" I# ? B= U$ I4%34 S4%34 ? B= U# S4%34 I4%34 ? U! F ? B= U- I$ B- I# I& ? B= I$ B- I& I# ? B= S4%34 S4%34 ? B= F F ? B= I$ I$ ? T B. B. SM%,&k#(%#+}IEj}3%.$}z3/,6%},!.'5!'%y4%34} U$ B+ I# B* I$> I1~s:U@ Sz}4/}#,!)-}0/).43}&/2})4 S)&})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}k})3}./4}#/22%#4 S5.!29}k})3}./4}#/22%#4 S5.!29}_})3}./4}#/22%#4 S5.!29}a})3}./4}#/22%#4 S5.!29}b})3}./4}#/22%#4 S").!29}i})3}./4}#/22%#4 S").!29}h})3}./4}#/22%#4 S").!29}m})3}./4}#/22%#4 S").!29}m})3}./4}#/22%#4 S").!29}c})3}./4}#/22%#4 S").!29}c})3}./4}#/22%#4 S").!29}r})3}./4}#/22%#4 S").!29}p})3}./4}#/22%#4 S").!29}{})3}./4}#/22%#4 S").!29}{})3}./4}#/22%#4 S").!29}d})3}./4}#/22%#4 S").!29}d})3}./4}#/22%#4 S").!29}l})3}./4}#/22%#4 S").!29}N})3}./4}#/22%#4 S").!29}>})3}./4}#/22%#4 S!00,)#!4)/.})3}./4}#/22%#4 S!00,)#!4)/.})3}./4}#/22%#4'''
        t = self.evaluate(s)
        assert t.startswith('Self-check OK'), t


if __name__ == '__main__':
    # sys.setrecursionlimit(2000)
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser()
    parser.add_argument('file', nargs='?', help='ICFP value file')
    args = parser.parse_args()
    if not args.file or args.file == '-':
        text = sys.stdin.read()
    else:
        text = Path(args.file).read_text()

    calc = IcfpCalc()
    calc.self_check()
    val = calc.evaluate(text)
    print(val)

