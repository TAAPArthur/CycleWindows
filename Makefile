CC := gcc
CPPFLAGS ?= -D_XOPEN_SOURCE=500
CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Wno-deprecated-declarations -Wno-missing-field-initializers -Os -g
LDFLAGS ?= -s -lX11 -lXtst -lXi -lxdo

HEADERS := CycleWindows.h

all: cycle-windows

cycle-windows: ${HEADERS} CycleWindows.c
	${CC} -o $@ ${CPPFLAGS} ${CFLAGS} CycleWindows.c ${LDFLAGS}


.PHONY: all clean check install
