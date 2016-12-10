RESULTS=tests/test-array.r tests/test-bound_left.r tests/test-bound_right.r tests/test-deep_brackets.r tests/test-helloworld.r tests/test-ignore.r tests/test-io.r tests/test-stress.r

.PRECIOUS: %.o

bfc: bfc.c
	gcc bfc.c -o bfc

install: bfc
	cp bfc /usr/bin/

test: $(RESULTS)

%.r: %.check %.o tests/diffcheck
	@tests/diffcheck $*

tests/diffcheck: tests/diffcheck.c
	@gcc tests/diffcheck.c -o tests/diffcheck

%.o: bfc %.b %.in
	@./bfc $*.b -o $*
	-@./$* < $*.in > $*.o
	@rm $*

clean:
	rm -f bfc
	rm -f tests/*.o
	rm -rf *.ld
	make -C tests clean

uninstall:
	rm /usr/bin/bfc