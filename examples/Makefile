.PHONY: clean

EXAMPLES=$(filter-out Makefile, $(wildcard *))

all: ${EXAMPLES}
	for dir in $^ ; do \
		make -C $${dir} ; \
	done

clean: ${EXAMPLES}
	for dir in $^ ; do \
		make -C $${dir} clean; \
	done
	
