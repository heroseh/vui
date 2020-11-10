#!/bin/sh

if test ! -f "$1.c"; then
	echo "there is no example with the file name $1"
	echo "here are a list of the examples:"
	ls | grep ".c"
	exit 1
fi

if test -f "$1"; then
	rm $1
fi

clang -D_GNU_SOURCE -o $1 $1.c -g -lm -lGL -pthread -lX11 -lXrandr -lSDL2

if test -f "$1"; then
	./$1
fi

