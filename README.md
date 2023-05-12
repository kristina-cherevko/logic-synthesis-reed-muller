# Area Minimization algorithms
This repository contains an algorithms for area minimization that use three canonical expansions - Shannon expansion, positive and negative Davio expansions. The algorithms use truth tables for functional representation.
The <a href="https://github.com/alanminko/iwls2022-ls-contest">benchmarks</a> from IWLS'22 Programming Contest are used for testing.
## Compiling
To compile algorithms download or copy code from `src/recsyn.cpp`. <br>
## Usage
In terminal run: `./recsyn [-p] [-a] [-v] <string>, argv[0]`.<br>
`-p` enables trying all variable permutations.<br>
`-a` enables using only and-gates (no xor-gates).<br>
`-v` enables verbose output.<br>
`<string>` - a truth table in hex notation or a file name.<br>
## Examples
Result of synthesis by algorithm with three canonical expansion and all variable orders for function with truth table `80` in hex notation:
```
Finished entring 3-input 1-output function.
  0 : 0 1 2 : cost =   2
  1 : 0 2 1 : cost =   2
  2 : 1 0 2 : cost =   2
  3 : 1 2 0 : cost =   2
  4 : 2 0 1 : cost =   2
  5 : 2 1 0 : cost =   2
0000000000000000 n00 = 0
AAAAAAAAAAAAAAAA n01 = a
CCCCCCCCCCCCCCCC n02 = b
F0F0F0F0F0F0F0F0 n03 = c
1 8888888888888888 n04 = a & b
2 8080808080808080 n05 = c & n04
8080808080808080 po0 = n05
The graph contains 2 nodes (2 ands and 0 xors) and spans 2 levels.
Verification succeeded.  Time =  0.00 sec
80.aig
Written graph with 3 inputs, 1 outputs, and 2 and-nodes into AIGER file "80.aig".
Added statistics for "80.aig" to the file "stats.txt".
```
