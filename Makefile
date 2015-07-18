
# build with default C/C++ compiler
# CC = clang
# CXX = clang++

# build optimized without -DNDEBUG
# OPTDEBUGFLAGS = -O0 -fno-inline -g
OPTDEBUGFLAGS = -O2 # -DNDEBUG

# clang only: -Wglobal-constructors
CXXFLAGS = -Isrc -std=c++11 -MD $(OPTDEBUGFLAGS) -Wall -Wshadow -Wsign-compare -Werror
LIBS = -lm

CHIPDBS = share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin

.PHONY: all
all: bin/arachne-pnr

bin/arachne-pnr: src/arachne-pnr.o src/netlist.o src/blif.o src/pack.o src/place.o src/util.o src/io.o src/route.o src/chipdb.o src/location.o src/configuration.o src/line_parser.o src/pcf.o src/global.o src/constant.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

share/arachne-pnr/chipdb-1k.bin: bin/arachne-pnr
	mkdir -p share/arachne-pnr
	bin/arachne-pnr -d 1k -c /usr/local/share/icebox/chipdb-1k.txt --write-binary-chipdb share/arachne-pnr/chipdb-1k.bin

share/arachne-pnr/chipdb-8k.bin: bin/arachne-pnr
	mkdir -p share/arachne-pnr
	bin/arachne-pnr -d 8k -c /usr/local/share/icebox/chipdb-8k.txt --write-binary-chipdb share/arachne-pnr/chipdb-8k.bin

tests/test_bv: tests/test_bv.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

tests/test_us: tests/test_us.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

# assumes icestorm installed
simpletest: bin/arachne-pnr $(CHIPDBS) tests/test_bv tests/test_us
	./tests/test_bv
	./tests/test_us
	cd tests/simple && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

# assumes icestorm, yosys and valgrind installed
test: bin/arachne-pnr $(CHIPDBS) tests/test_bv
	./tests/test_bv
	./tests/test_us
	cd tests/simple && bash run-test.sh
	cd tests/regression && bash run-test.sh
	cd tests/simple && bash run-valgrind-test.sh
	cd tests/fsm && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

-include src/*.d

.PHONY: install
install: bin/arachne-pnr
	cp bin/arachne-pnr /usr/local/bin/arachne-pnr

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/arachne-pnr

.PHONY: clean
clean:
	rm -f src/*.o tests/*.o src/*.d tests/*.d bin/arachne-pnr tests/test_bv
	rm -f share/arachne-pnr/*.bin
