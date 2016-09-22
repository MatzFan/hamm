gcc -O3 -Wall -Wextra -Werror -std=gnu99 tree.c -o tree

@ECHO OFF
REM The parameters 288 and 112 were found by some testing, trying to optimize
REM for maximum distance 3 (that's what Dietrich had done, and it also makes
REM sense now that I know that the maximum maximum distance in the application
REM of the StackOverflow question was about 4-5).
@ECHO ON

tree bk     288 100000000 1 1 2 3 4 5 6 7 8 9 10 > test_bk1.txt
tree vp     112 100000000 1 1 2 3 4 5 6 7 8 9 10 > test_vp1.txt
tree linear   0 100000000 1 1 2 3 4 5 6 7 8 9 10 > test_linear1.txt
tree bitset   0 100000000 1 1 2 3 4 5 6 7 8 9 10 > test_bitset1.txt

tree bk     288 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_bk2.txt
tree vp     112 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_vp2.txt
tree linear   0 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_linear2.txt
tree bitset   0 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_bitset2.txt

tree bk     288 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_bk3.txt
tree vp     112 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_vp3.txt
tree linear   0 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_linear3.txt
tree bitset   0 100000000 10 1 2 3 4 5 6 7 8 9 10 > test_bitset3.txt
