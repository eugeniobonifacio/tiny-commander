## A very small commander. Developed in C language with the assistance of AI.

I started developing this commander as I needed something similar to MC but a very basic one. I wanted to learn C programming in Linux too. So I put all together.

I had no time to start all from zero, so I asked AI to develop it. I revised the code and now it is being the base of this my own commander tool.

To build it, just launch from the source folder:
```
$ make
```

If you want to link ncurses statically, build your ncurses library apart and then build using the following:

```
make static-custom NCURSES_STATIC_PATH=/path/to/ncurses/static/lib/
```
