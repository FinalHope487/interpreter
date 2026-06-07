# OurC & OurScheme interpreter

完整的 OurC 和 OurScheme 直譯器程式，已經完全通過 Project 4，僅做為紀錄之用

請勿下載程式在提交系統上測試，不管改了多少內容

Notes : 有部分 spec 內容和實際測資有出入，要自己去注意到

## OurC Features

- Variables (int, float, char, string, bool)
- Arrays
- Functions (including lexical scope, recursion, pass params by reference)
- Environments (support for nested scopes)
- Conditional statements (if, if-else)
- Loops (while, do-while)
- Standard input/output (cin, cout)
- Formatted output (see example p4-12-1)
- Error handling
- Debug mode

## OurScheme Features

- Data types (number, symbol, string, boolean, nil, error-object)
- Lists and pairs (standard lists, dotted pairs)
- Functional forms (quote, begin, and, or, cond, if)
- Functions (user-defined procedures, lambda, recursive calls, lexical scope)
- Environments (support for nested local environments using let)
- Variable binding and mutation (define, set!)
- Primitives (cons, car, cdr, eqv?, equal?, string operations, arithmetic & relational operators)
- Standard input/output (read, write, display-string, newline, type conversions)
- Verbose mode (verbose, verbose? to control define and clean-environment feedback)
- Error handling & recovery (precise line/column lexing errors, runtime checks, no-return-value tracking)

## How to Compile

```bash
g++ p4.cpp -o p4.exe
```

## How to Run

```bash
./p4.exe < inputs/p4-01.txt
```
