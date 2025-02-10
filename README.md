# Circuit Minimization Algorithms

This repository contains a C program implementing several algorithms for synthesizing minimal
circuit for completely-specified multi-output Boolean functions represented using truth tables. These algorithms were developed as part of my third-year coursework in the Bachelor's degree program in Applied Mathematics at National University of "Kyiv-Mohyla Academy". 
The algorithms are described in details in the
<a href="https://people.eecs.berkeley.edu/~alanmi/publications/2023/rm23_area.pdf">paper</a>
presented at <a href="http://www.lsi-cad.com/RM/RM2023/">Reed-Muller Workshop 2023</a>.

The benchmarks used to evaluate the algorithms are taken from the set of 100 testcases of
<a href="https://github.com/alanminko/iwls2022-ls-contest">IWLS Programming Contest 2022</a>.

## Compiling
To compile the program, download `src/recsyn.cpp` and compile it as follows: `g++ -o recsyn recsyn.cpp -std=c++11`. <br>
## Usage
To run the program, use the following command line: `./recsyn [-p] [-a] [-v] <string>` where<br>
`-p` enables trying all variable orders,<br>
`-a` enables using only and-gates (no xor-gates),<br>
`-v` enables verbose output,<br>
`<string>` is a truth table in the hexadecimal notation or a file name.<br>
## Examples
Here is the result of synthesis by the proposed algorithm based on the three canonical expansion 
(Shannon, Positive Davio, and Negative Davio) and all variable orders for the Boolean function 
of the three-input and-gate with the hexadecimal truth table `80`:
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
Here is the result of synthesis using only the Shannon expansion and all variable orders for a Boolean function 
with the binary truth table `10000100` listed in the file `test.truth`:
```
[...]>./recsyn -a -p -v test.truth
Finished entring 3-input 1-output function from file "test.truth".
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
The following is the result of running the program to several functions represented using 
their truth tables in the files listed in the given file name, one file name per line 
(the file should have extension .filelist): `./recsyn -a <filename>.filelist`
```
Solving problem "inputs/ex00.truth".
Finished entring 6-input 1-output function from file "inputs/ex00.truth".
The graph contains 41 nodes and spans 10 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 6 inputs, 1 outputs, and 41 and-nodes into AIGER file "ex00.aig".
Added statistics for "ex00.aig" to the file "stats.txt".

Solving problem "inputs/ex01.truth".
Finished entring 6-input 1-output function from file "inputs/ex01.truth".
The graph contains 42 nodes and spans 10 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 6 inputs, 1 outputs, and 42 and-nodes into AIGER file "ex01.aig".
Added statistics for "ex01.aig" to the file "stats.txt".

Solving problem "inputs/ex02.truth".
Finished entring 8-input 1-output function from file "inputs/ex02.truth".
The graph contains 136 nodes and spans 15 levels.
Verification succeeded.  Time =  0.00 sec
Written graph with 8 inputs, 1 outputs, and 136 and-nodes into AIGER file "ex02.aig".
Added statistics for "ex02.aig" to the file "stats.txt".

Finished solving 3 problems from the list "<filename>.filelist".
```
