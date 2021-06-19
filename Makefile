all: 
	clang-11 -Wall -Werror -std=c17 -g -O0 -o telemeter t.c -lhugetlbfs

