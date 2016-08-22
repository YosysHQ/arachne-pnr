##
## Copyright (C) 2015-2016 Cotton Seed <cotton@alum.mit.edu>
## Copyright (C) 2016 Joel Holdsworth <joel@airwebreathe.org.uk>
##
## This file is part of arachne-pnr.  Arachne-pnr is free software;
## you can redistribute it and/or modify it under the terms of the GNU
## General Public License version 2 as published by the Free Software
## Foundation.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program. If not, see <http://www.gnu.org/licenses/>.
##

AM_CXXFLAGS = -I$(abs_top_srcdir)/src -std=c++11 -MD -Wall -Wshadow \
	-Wsign-compare -Werror

bin_PROGRAMS = bin/arachne-pnr
noinst_PROGRAMS = tests/test_bv tests/test_us

bin_arachne_pnr_SOURCES = \
	src/arachne-pnr.cc \
	src/blif.cc \
	src/chipdb.cc \
	src/configuration.cc \
	src/constant.cc \
	src/designstate.cc \
	src/global.cc \
	src/io.cc \
	src/line_parser.cc \
	src/location.cc \
	src/netlist.cc \
	src/pack.cc \
	src/pcf.cc \
	src/place.cc \
	src/route.cc \
	src/util.cc

tests_test_bv_SOURCES = tests/test_bv.cc
tests_test_us_SOURCES = tests/test_us.cc

arachne_pnr_datadir = $(datadir)/arachne-pnr
arachne_pnr_data_DATA = \
	share/arachne-pnr/chipdb-1k.bin \
	share/arachne-pnr/chipdb-8k.bin

share/arachne-pnr/chipdb-1k.bin: $(ICEBOX_DIR)/chipdb-1k.txt ./bin/arachne-pnr
	@$(MKDIR_P) share/arachne-pnr
	$(AM_V_GEN)./bin/arachne-pnr -q -d 1k -c $< --write-binary-chipdb $@

share/arachne-pnr/chipdb-8k.bin: $(ICEBOX_DIR)/chipdb-8k.txt ./bin/arachne-pnr
	@$(MKDIR_P) share/arachne-pnr
	$(AM_V_GEN)./bin/arachne-pnr -q -d 8k -c $< --write-binary-chipdb $@

if ENABLE_TESTS

simpletest: all ./tests/test_bv ./tests/test_us
	./tests/test_bv
	./tests/test_us
	cd tests/simple && bash run-test.sh
	cd tests/regression && bash run-test.sh
	cd tests/blif && bash run-test.sh
	cd tests/error && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

test: all tests/test_bv tests/test_us
	./tests/test_bv
	./tests/test_us
	$(MAKE) -C examples/rot clean && $(MAKE) -C examples/rot
	cd tests/simple && bash run-test.sh
	cd tests/regression && bash run-test.sh
	cd tests/blif && bash run-test.sh
	cd tests/error && bash run-test.sh
	cd tests/fsm && bash run-test.sh
	cd tests/combinatorial && bash run-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

endif

if ENABLE_DEVELOPER_MODE

testvg:
	cd tests/simple && bash run-valgrind-test.sh
	@echo
	@echo 'All tests passed.'
	@echo

endif

.PHONY: clean-local
clean-local:
	rm -f share/arachne-pnr/*.bin
