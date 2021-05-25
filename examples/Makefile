.PHONY: clean

EXAMPLES=$(wildcard */.)

all: ${EXAMPLES}
	for dir in $^ ; do \
		make -C $${dir} DEBUG=$(DEBUG) ; \
	done

clean: ${EXAMPLES}
	for dir in $^ ; do \
		make -C $${dir} clean ; \
	done
