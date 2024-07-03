#!/usr/bin/env python
import sys
from collections import ChainMap


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
                        except TypeError as err:
                            print(err)
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
                def wrap(v, context):
                    capture = context.new_child()
                    _eval = self._eval
                    def f(u, context):
                        ctx = context.new_child(capture)
                        ctx[v] = u
                        return _eval(a, ctx)
                    return f
                return wrap(v, context)
            case 'v':
                return context[op]
            case _:
                raise Exception(f'! unhandled {op!r}')

    def _emit_py(self, expr):
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
                return repr(text)
            case 'U':
                op,a = expr
                x = self._emit_py(a)
                match op[1]:
                    case '-':
                        return f'-{x}'
                    case '!':
                        return f'not {x}'
                    case '#':
                        return f'_icfp_a2n({x})'
                    case '$':
                        return f'_icfp_n2a({x})'
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case 'B':
                op,a,b = expr
                x = self._emit_py(a)
                y = self._emit_py(b)
                match op[1]:
                    case '+':
                        return f'({x} + {y})'
                    case '-':
                        return f'({x} - {y})'
                    case '*':
                        return f'({x} * {y})'
                    case '/':
                        return f'_icfp_idiv({x}, {y})'
                    case '%':
                        return f'_icfp_imod({x}, {y})'
                    case '<':
                        return f'({x} < {y})'
                    case '>':
                        return f'({x} > {y})'
                    case '=':
                        return f'({x} == {y})'
                    case '|':
                        return f'({x} or {y})'
                    case '&':
                        return f'({x} and {y})'
                    case '.':
                        return f'({x} + {y})'
                    case 'T':
                        return f'{y}[:{x}]'
                    case 'D':
                        return f'{y}[{x}:]'
                    case '$':
                        return f'{x}({y})'
                    case _:
                        raise Exception(f'! unhandled {op!r} in {expr!r}')
            case '?':
                op,t,a,b = expr
                q = self._emit_py(t)
                x = self._emit_py(a)
                y = self._emit_py(b)
                return f'({x} if {q} else {y})'
            case 'L':
                op,a = expr
                v = op[1:]
                x = self._emit_py(a)
                return f'(lambda v{v}: {x})'
            case 'v':
                return str(op)
            case _:
                raise Exception(f'! unhandled {op!r}')

    def _eval_py(self, expr):
        def _icfp_a2n(x):
            v = 0
            for c in x:
                v = v * 94 + _abc94.index(c)
            return v
        def _icfp_n2a(x):
            assert x >= 0
            if x == 0:
                return _abc94[0]
            res = ''
            while x:
                x,t = divmod(x, 94)
                s = _abc94[t]
                res = s + res
            return res
        def _icfp_idiv(x, y):
            if x < 0 and y > 0:
                return -((-x) // y)
            return x // y
        def _icfp_imod(x, y):
            if x < 0 and y > 0:
                return -((-x) % y)
            return x % y

        s = self._emit_py(expr)
        print(s)
        return eval(s)

    def self_check(self):
        s = '''? B= B$ B$ B$ B$ L$ L$ L$ L# v$ I" I# I$ I% I$ ? B= B$ L$ v$ I+ I+ ? B= BD I$ S4%34 S4 ? B= BT I$ S4%34 S4%3 ? B= B. S4% S34 S4%34 ? U! B& T F ? B& T T ? U! B| F F ? B| F T ? B< U- I$ U- I# ? B> I$ I# ? B= U- I" B% U- I$ I# ? B= I" B% I( I$ ? B= U- I" B/ U- I$ I# ? B= I# B/ I( I$ ? B= I' B* I# I$ ? B= I$ B+ I" I# ? B= U$ I4%34 S4%34 ? B= U# S4%34 I4%34 ? U! F ? B= U- I$ B- I# I& ? B= I$ B- I& I# ? B= S4%34 S4%34 ? B= F F ? B= I$ I$ ? T B. B. SM%,&k#(%#+}IEj}3%.$}z3/,6%},!.'5!'%y4%34} U$ B+ I# B* I$> I1~s:U@ Sz}4/}#,!)-}0/).43}&/2})4 S)&})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}q})3}./4}#/22%#4 S").!29}k})3}./4}#/22%#4 S5.!29}k})3}./4}#/22%#4 S5.!29}_})3}./4}#/22%#4 S5.!29}a})3}./4}#/22%#4 S5.!29}b})3}./4}#/22%#4 S").!29}i})3}./4}#/22%#4 S").!29}h})3}./4}#/22%#4 S").!29}m})3}./4}#/22%#4 S").!29}m})3}./4}#/22%#4 S").!29}c})3}./4}#/22%#4 S").!29}c})3}./4}#/22%#4 S").!29}r})3}./4}#/22%#4 S").!29}p})3}./4}#/22%#4 S").!29}{})3}./4}#/22%#4 S").!29}{})3}./4}#/22%#4 S").!29}d})3}./4}#/22%#4 S").!29}d})3}./4}#/22%#4 S").!29}l})3}./4}#/22%#4 S").!29}N})3}./4}#/22%#4 S").!29}>})3}./4}#/22%#4 S!00,)#!4)/.})3}./4}#/22%#4 S!00,)#!4)/.})3}./4}#/22%#4'''
        t = self.evaluate(s)
        assert t.startswith('Self-check OK'), t


if __name__ == '__main__':
    # sys.setrecursionlimit(5000)
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

