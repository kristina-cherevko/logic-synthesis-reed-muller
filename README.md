# Area Minimization algorithms
This repository contains an algorithms for area minimization that use three canonical expansions - Shannon expansion, positive and negative Davio expansions.
The <a href="https://github.com/alanminko/iwls2022-ls-contest">benchmarks</a> from IWLS'22 Programming Contest are used for testing.
## Compiling
To compile algorithms download or copy code from `src/recsyn.cpp`. <br>

## Usage
 ```
printf("usage:  %s [-p] [-a] [-v] <string>\n", argv[0]);
printf("        this program synthesized circuits from truth tables\n");
printf("        -p : enables trying all variable permutations\n");
printf("        -a : enables using only and-gates (no xor-gates)\n");
printf("        -v : enables verbose output\n");
printf("  <string> : a truth table in hex notation or a file name\n");
```
To run Shannon expansion algorithm with fixed variable order 
Examples how to run different algorithms for one function or list of functions. <br>
Run algorithm that uses Shannon expansion and fixed variable order: `./recsyn -v 80`.
