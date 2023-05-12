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
[...]>./recsyn -p -v 80
The array contains 1 truth tables of size 1 words:
 0 : 8080808080808080
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
Result of synthesis by Shannon expansion algorithm used for all variable orders for function in file `test.truth` with truth table `10000100` in binary notation:
```
[...]>./recsyn -a -p -v ./inputs/test.truth
Finished entring 3-input 1-output function from file "./inputs/test.truth".
  0 : 0 1 2 : cost =   5
  1 : 0 2 1 : cost =   4
  2 : 1 0 2 : cost =   5
  3 : 1 2 0 : cost =   5
  4 : 2 0 1 : cost =   4
  5 : 2 1 0 : cost =   5
0000000000000000 n00 = 0
AAAAAAAAAAAAAAAA n01 = a
CCCCCCCCCCCCCCCC n02 = b
F0F0F0F0F0F0F0F0 n03 = c
1 8888888888888888 n04 = a & b
2 1111111111111111 n05 = ~a & ~b
3 6666666666666666 n06 = ~n04 & ~n05
4 9090909090909090 n07 = c & ~n06
9090909090909090 po0 = n07
The graph contains 4 nodes (4 ands and 0 xors) and spans 3 levels.
Verification succeeded.  Time =  0.00 sec
test.aig
Written graph with 3 inputs, 1 outputs, and 4 and-nodes into AIGER file "test.aig".
Added statistics for "test.aig" to the file "stats.txt".
```
To synthesize functions from list run: `./recsyn -a <filename>.filelist`
```
Solving problem "./inputs/ex00.truth".
Finished entring 6-input 1-output function from file "./inputs/ex00.truth".
The graph contains 41 nodes and spans 10 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 6 inputs, 1 outputs, and 41 and-nodes into AIGER file "ex00.aig".
Added statistics for "ex00.aig" to the file "stats.txt".

Solving problem "./inputs/ex01.truth".
Finished entring 6-input 1-output function from file "./inputs/ex01.truth".
The graph contains 42 nodes and spans 10 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 6 inputs, 1 outputs, and 42 and-nodes into AIGER file "ex01.aig".
Added statistics for "ex01.aig" to the file "stats.txt".

Solving problem "./inputs/ex02.truth".
Finished entring 8-input 1-output function from file "./inputs/ex02.truth".
The graph contains 136 nodes and spans 15 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 8 inputs, 1 outputs, and 136 and-nodes into AIGER file "ex02.aig".
Added statistics for "ex02.aig" to the file "stats.txt".

Finished solving 3 problems from the list "<filename>.filelist".
```
