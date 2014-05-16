all: matrix

/home/diego/customXScreensaver/matrix matrix: matrix.c
	gcc -o matrix matrix.c -L/usr/X11R6/lib -lX11
	cp ./matrix /home/diego/customXScreensaver/matrix
