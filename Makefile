
# build with default C/C++ compiler
# CC = clang
# CXX = clang++

# build optimized without -DNDEBUG
# OPTDEBUGFLAGS = -O0 -fno-inline -g
# OPTDEBUGFLAGS = -O3 -DNDEBUG
OPTDEBUGFLAGS ?= -MD -O2
SRC = src

# clang only: -Wglobal-constructors
CXXFLAGS += -I$(SRC) -std=c++11 $(OPTDEBUGFLAGS) -Wall -Wshadow -Wsign-compare -Werror
LIBS = -lm

DESTDIR ?=
PREFIX ?= /usr/local
ICEBOX ?= $(PREFIX)/share/icebox

# Cross-compile logic
HOST_CC ?= $(CC)
HOST_CXX ?= $(CXX)
HOST_CXXFLAGS += -I$(SRC) -std=c++11 $(OPTDEBUGFLAGS) -Wall -Wshadow -Wsign-compare -Werror
HOST_LIBS ?= $(LIBS)

IS_CROSS_COMPILING = no
ifneq ($(CC),$(HOST_CC))
	IS_CROSS_COMPILING = yes
endif
ifneq ($(CXX),$(HOST_CXX))
	IS_CROSS_COMPILING = yes
endif

.PHONY: all
all: bin/arachne-pnr$(EXE) share/arachne-pnr/chipdb-384.bin share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin share/arachne-pnr/chipdb-5k.bin

ARACHNE_VER = 0.1+$(shell test -e .git && echo `git log --oneline | wc -l`+`git diff --name-only HEAD | wc -l`)
GIT_REV = $(shell git rev-parse --verify --short HEAD 2>/dev/null || echo UNKNOWN)

VER_HASH = $(shell echo "$(ARACHNE_VER) $(GIT_REV)" | sum | cut -d ' ' -f -1)

src/version_$(VER_HASH).cc:
	echo "const char *version_str = \"arachne-pnr $(ARACHNE_VER) (git sha1 $(GIT_REV), $(notdir $(CXX)) `$(CXX) --version | tr ' ()' '\n' | grep '^[0-9]' | head -n1` $(filter -f% -m% -O% -DNDEBUG,$(CXXFLAGS)))\";" > src/version_$(VER_HASH).cc

bin/arachne-pnr$(EXE): src/arachne-pnr.o src/netlist.o src/blif.o src/pack.o src/place.o src/util.o src/io.o src/route.o src/chipdb.o src/location.o src/configuration.o src/line_parser.o src/pcf.o src/global.o src/constant.o src/designstate.o src/version_$(VER_HASH).o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

ifeq ($(IS_CROSS_COMPILING),yes)
bin/arachne-pnr-host: src/arachne-pnr.host-o src/netlist.host-o src/blif.host-o src/pack.host-o src/place.host-o src/util.host-o src/io.host-o src/route.host-o src/chipdb.host-o src/location.host-o src/configuration.host-o src/line_parser.host-o src/pcf.host-o src/global.host-o src/constant.host-o src/designstate.host-o src/version_$(VER_HASH).host-o
	$(HOST_CXX) $(HOST_CXXFLAGS) $(HOST_LDFLAGS) -o $@ $^ $(HOST_LIBS)
else
bin/arachne-pnr-host: bin/arachne-pnr$(EXE)
	cp $< $@
endif

%.host-o: %.cc
	$(HOST_CXX) -c $(HOST_CPPFLAGS) $(HOST_CXXFLAGS) -o $@ $<

ifeq ($(EXE),.js)
# Special hack to make sure chipdb is built first
bin/arachne-pnr$(EXE): | share/arachne-pnr/chipdb-384.bin share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin share/arachne-pnr/chipdb-5k.bin
endif

share/arachne-pnr/chipdb-384.bin: bin/arachne-pnr-host $(ICEBOX)/chipdb-384.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr-host -d 384 -c $(ICEBOX)/chipdb-384.txt --write-binary-chipdb share/arachne-pnr/chipdb-384.bin

share/arachne-pnr/chipdb-1k.bin: bin/arachne-pnr-host $(ICEBOX)/chipdb-1k.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr-host -d 1k -c $(ICEBOX)/chipdb-1k.txt --write-binary-chipdb share/arachne-pnr/chipdb-1k.bin

share/arachne-pnr/chipdb-8k.bin: bin/arachne-pnr-host $(ICEBOX)/chipdb-8k.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr-host -d 8k -c $(ICEBOX)/chipdb-8k.txt --write-binary-chipdb share/arachne-pnr/chipdb-8k.bin

share/arachne-pnr/chipdb-5k.bin: bin/arachne-pnr-host $(ICEBOX)/chipdb-5k.txt
	mkdir -p share/arachne-pnr
	bin/arachne-pnr-host -d 8k -c $(ICEBOX)/chipdb-5k.txt --write-binary-chipdb share/arachne-pnr/chipdb-5k.bin

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
	$(MAKE) share/arachne-pnr/chipdb-384.bin share/arachne-pnr/chipdb-1k.bin share/arachne-pnr/chipdb-8k.bin
	mv share/arachne-pnr/chipdb-384.bin arachne-pnr-win32/
	mv share/arachne-pnr/chipdb-1k.bin arachne-pnr-win32/
	mv share/arachne-pnr/chipdb-8k.bin arachne-pnr-win32/
	mv share/arachne-pnr/chipdb-5k.bin arachne-pnr-win32/
	$(MAKE) clean
	$(MAKE) CC=/usr/local/src/mxe/usr/bin/i686-w64-mingw32.static-gcc CXX=/usr/local/src/mxe/usr/bin/i686-w64-mingw32.static-g++ CXXFLAGS="$(CXXFLAGS) -DMXE_DIR_STRUCTURE" bin/arachne-pnr
	mv bin/arachne-pnr arachne-pnr-win32/arachne-pnr.exe
	zip -r arachne-pnr-win32.zip arachne-pnr-win32/
	rm -rf arachne-pnr-win32
	$(MAKE) clean

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp bin/arachne-pnr$(EXE) $(DESTDIR)$(PREFIX)/bin/arachne-pnr$(EXE)
	mkdir -p $(DESTDIR)$(PREFIX)/share/arachne-pnr
	cp share/arachne-pnr/chipdb-384.bin $(DESTDIR)$(PREFIX)/share/arachne-pnr/chipdb-384.bin
	cp share/arachne-pnr/chipdb-1k.bin $(DESTDIR)$(PREFIX)/share/arachne-pnr/chipdb-1k.bin
	cp share/arachne-pnr/chipdb-8k.bin $(DESTDIR)$(PREFIX)/share/arachne-pnr/chipdb-8k.bin
	cp share/arachne-pnr/chipdb-5k.bin $(DESTDIR)$(PREFIX)/share/arachne-pnr/chipdb-5k.bin

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/arachne-pnr$(EXE)
	rm -f $(DESTDIR)$(PREFIX)/share/arachne-pnr/*.bin

.PHONY: clean
clean:
	rm -f src/*.o src/*.host-o tests/*.o src/*.d tests/*.d bin/arachne-pnr$(EXE) bin/arachne-pnr-host
	rm -f tests/test_bv tests/test_us
	rm -f share/arachne-pnr/*.bin
	rm -f src/version_*
	$(MAKE) -C examples/rot clean
	rm -rf tests/combinatorial/temp tests/combinatorial/1k tests/combinatorial/8k
	rm -rf tests/fsm/temp tests/fsm/1k tests/fsm/8k
	rm -rf tests/regression/1k tests/regression/8k
	rm -rf tests/simple/txt.sum tests/simple/1k tests/simple/8k

.PHONY: emcc
emcc:
	$(MAKE) EXE=.js clean
	$(MAKE) CC=emcc CXX=emcc HOST_CC=gcc HOST_CXX=g++ PREFIX=/ LDFLAGS="--memory-init-file 0 --embed-file share -s TOTAL_MEMORY=256*1024*1024" EXE=.js
