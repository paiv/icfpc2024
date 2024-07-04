ICFPC 2024
==
Team: paiv

[![standwithukraine](docs/StandWithUkraine.svg)](https://ukrainewar.carrd.co/)


ICFP compiler
--
Compiles toy ICF language in the style of:
```scheme
{- combinator -}
(define (Y f) (
  (\ (x) (f (x x)))
  (\ (x) (f (x x)))
))

(define (S x)
  (Y (\ (z y) (? (= y 1) x (. x (z (- y 1))))))
)

(. "A" ((S "w") 9))
```

```
usage: icfpc [-a] [-t] [-v] [file...]

ICFP document compiler

Options:
  -a,--asserts  generate asserts
  -t,--text     generate ICFP code
  -v,--verbose  set verbose logging
```
