#! /bin/bash

all:
	make -C libs/ilclient
	make -C hello_triangle
	make -C hello_triangle2
	make -C hello_video
	make -C hello_videocube
	make -C hello_teapot
	make -C hello_dispmanx

clean:
	make -C libs/ilclient clean
	make -C hello_triangle clean
	make -C hello_triangle2 clean
	make -C hello_video clean
	make -C hello_videocube clean
	make -C hello_teapot clean
	make -C hello_dispmanx clean

clobber: clean

