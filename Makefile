
# build with default C/C++ compiler
# CC = clang
# CXX = clang++

# build optimized without -DNDEBUG
# OPTDEBUGFLAGS = -O0 -fno-inline -g
# OPTDEBUGFLAGS = -O3 -DNDEBUG
OPTDEBUGFLAGS = -O2
SRC = src

# clang only: -Wglobal-constructors
CXXFLAGS = -I$(SRC) -std=c++11 -MD $(OPTDEBUGFLAGS) -Wall -Wshadow -Wsign-compare -Werror
LIBS = -lm

DESTDIR = /usr/local
ICEBOX = /usr/local/share/icebox

.PHONY: all
all: bin/arachne-pnr share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin

VER = 0.1+$(shell echo `git log --oneline | wc -l`)+$(shell echo `git diff --name-only HEAD | wc -l`)
GIT_REV = $(shell git rev-parse --verify --short HEAD)

VER_HASH = $(shell echo "$(VER) $(GIT_REV)" | sum | cut -d ' ' -f -1)

src/version_$(VER_HASH).cc:
	echo "const char *version_str = \"arachne-pnr $(VER) (git sha1 $(GIT_REV), $(notdir $(CXX)) `$(CXX) --version | tr ' ()' '\n' | grep '^[0-9]' | head -n1` $(filter -f% -m% -O% -DNDEBUG,$(CXXFLAGS)))\";" > src/version_$(VER_HASH).cc

bin/arachne-pnr: src/arachne-pnr.o src/netlist.o src/blif.o src/pack.o src/place.o src/util.o src/io.o src/route.o src/chipdb.o src/location.o src/configuration.o src/line_parser.o src/pcf.o src/global.o src/constant.o src/designstate.o src/version_$(VER_HASH).o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

share/arachne-pnr/chipdb-1k.bin: bin/arachne-pnr $(ICEBOX)/chipdb-1k.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr -d 1k -c $(ICEBOX)/chipdb-1k.txt --write-binary-chipdb share/arachne-pnr/chipdb-1k.bin

share/arachne-pnr/chipdb-8k.bin: bin/arachne-pnr $(ICEBOX)/chipdb-8k.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr -d 8k -c $(ICEBOX)/chipdb-8k.txt --write-binary-chipdb share/arachne-pnr/chipdb-8k.bin

tests/test_bv: tests/test_bv.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

tests/test_us: tests/test_us.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

# assumes icestorm installed
simpletest: all tests/test_bv tests/test_us
	./tests/test_bv
	./tests/test_us
	cd tests/simple && ICEBOX=$(ICEBOX) bash run-test.sh
	cd tests/regression && bash run-test.sh
	cd tests/blif && bash run-test.sh
	cd tests/error && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

# assumes icestorm, yosys installed
test: all tests/test_bv ./tests/test_us
	./tests/test_bv
	./tests/test_us
	make -C examples/rot clean && make -C examples/rot
	cd tests/simple && ICEBOX=$(ICEBOX) bash run-test.sh
	cd tests/regression && bash run-test.sh
	cd tests/blif && bash run-test.sh
	cd tests/error && bash run-test.sh
	cd tests/fsm && bash run-test.sh
	cd tests/combinatorial && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

# assumes valgrind installed
testvg:
	cd tests/simple && bash run-valgrind-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

fuzz-pcf: all
	afl-fuzz -t 2500 -m 500 -x fuzz/pcf/pcf.dict -i fuzz/pcf/testcases -o fuzz/pcf/findings bin/arachne-pnr -p @@ fuzz/pcf/rot.blif -o /dev/null

fuzz-blif: all
	afl-fuzz -t 2500 -m 500 -x fuzz/blif/blif.dict -i fuzz/blif/testcases -o fuzz/blif/findings bin/arachne-pnr @@ -o /dev/null

-include src/*.d

.PHONY: mxebin
mxebin:
	$(MAKE) clean
	rm -rf arachne-pnr-win32
	rm -f arachne-pnr-win32.zip
	mkdir -p arachne-pnr-win32
	$(MAKE) share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin
	mv share/arachne-pnr/chipdb-1k.bin arachne-pnr-win32/
	mv share/arachne-pnr/chipdb-8k.bin arachne-pnr-win32/
	$(MAKE) clean
	$(MAKE) CC=/usr/local/src/mxe/usr/bin/i686-w64-mingw32.static-gcc CXX=/usr/local/src/mxe/usr/bin/i686-w64-mingw32.static-g++ bin/arachne-pnr
	mv bin/arachne-pnr arachne-pnr-win32/arachne-pnr.exe
	zip -r arachne-pnr-win32.zip arachne-pnr-win32/
	rm -rf arachne-pnr-win32
	$(MAKE) clean

.PHONY: install
install: all
	mkdir -p $(DESTDIR)/bin
	cp bin/arachne-pnr $(DESTDIR)/bin/arachne-pnr
	mkdir -p $(DESTDIR)/share/arachne-pnr
	cp share/arachne-pnr/chipdb-1k.bin $(DESTDIR)/share/arachne-pnr/chipdb-1k.bin
	cp share/arachne-pnr/chipdb-8k.bin $(DESTDIR)/share/arachne-pnr/chipdb-8k.bin

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)/bin/arachne-pnr
	rm -f $(DESTDIR)/bin/share/arachne-pnr/*.bin

.PHONY: clean
clean:
	rm -f src/*.o tests/*.o src/*.d tests/*.d bin/arachne-pnr
	rm -f tests/test_bv tests/test_us
	rm -f share/arachne-pnr/*.bin
	rm -f src/version_*
