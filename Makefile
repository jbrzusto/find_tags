## Makefile for linux

## PROFILING FLAGS (uncomment to enable profiling)
## PROFILING=-g -pg

## DEBUG FLAGS WITH DUMPING OF TREE (VERY VERBOSE)
## CPPFLAGS=-Wall -g3 -std=c++0x $(PROFILING) -DFIND_TAGS_DEBUG  -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS)
## DEBUG FLAGS:
##CPPFLAGS=-Wall -g3 -std=c++0x $(PROFILING) -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS)
## LDFLAGS=-lsqlite3

## PRODUCTION FLAGS:
CPPFLAGS=-Wall  -O3 -std=c++0x $(PROFILING) -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS)
LDFLAGS=-dl -lrt
PROGRAM_VERSION=\""$(shell git describe)\""
PROGRAM_BUILD_TS=$(shell date +%s)

all: find_tags_unifile

clean:
	rm -f *.o find_tags find_tags_unifile find_tags_motus

DB_Filer.o: DB_Filer.cpp DB_Filer.hpp find_tags_common.hpp

Freq_Setting.o: Freq_Setting.cpp Freq_Setting.hpp find_tags_common.hpp

Freq_History.o: Freq_History.cpp Freq_History.hpp find_tags_common.hpp

DFA_Node.o: DFA_Node.cpp DFA_Node.hpp find_tags_common.hpp

DFA_Graph.o: DFA_Graph.cpp DFA_Graph.hpp find_tags_common.hpp

Known_Tag.o: Known_Tag.cpp Known_Tag.hpp find_tags_common.hpp

Tag_Database.o: Tag_Database.cpp Tag_Database.hpp find_tags_common.hpp

Pulse.o: Pulse.cpp Pulse.hpp find_tags_common.hpp

Tag_Candidate.o: Tag_Candidate.hpp Tag_Candidate.cpp Tag_Finder.hpp find_tags_common.hpp

Tag_Finder.o: Tag_Finder.hpp Tag_Finder.cpp Tag_Candidate.hpp find_tags_common.hpp

Tag_Foray.o: Tag_Foray.hpp Tag_Foray.cpp find_tags_common.hpp

Rate_Limiting_Tag_Finder.o: Rate_Limiting_Tag_Finder.hpp find_tags_common.hpp

find_tags_unifile.o: find_tags_unifile.cpp find_tags_common.hpp Freq_History.hpp Freq_Setting.hpp DFA_Node.hpp DFA_Graph.hpp Known_Tag.hpp Tag_Database.hpp Pulse.hpp Burst_Params.hpp Bounded_Range.hpp Tag_Candidate.hpp Tag_Finder.hpp Rate_Limiting_Tag_Finder.hpp Tag_Foray.hpp

find_tags_unifile: Freq_Setting.o Freq_History.o DFA_Node.o DFA_Graph.o Known_Tag.o Tag_Database.o Pulse.o Tag_Candidate.o Tag_Finder.o Rate_Limiting_Tag_Finder.o find_tags_unifile.o Tag_Foray.o
	g++ $(PROFILING) -o find_tags_unifile $^ $(LDFLAGS)

find_tags_motus.o: find_tags_motus.cpp find_tags_common.hpp Freq_History.hpp Freq_Setting.hpp DFA_Node.hpp DFA_Graph.hpp Known_Tag.hpp Tag_Database.hpp Pulse.hpp Burst_Params.hpp Bounded_Range.hpp Tag_Candidate.hpp Tag_Finder.hpp Rate_Limiting_Tag_Finder.hpp Tag_Foray.hpp

find_tags_motus: Freq_Setting.o Freq_History.o DFA_Node.o DFA_Graph.o Known_Tag.o Tag_Database.o Pulse.o Tag_Candidate.o Tag_Finder.o Rate_Limiting_Tag_Finder.o find_tags_motus.o Tag_Foray.o DB_Filer.o
	g++ $(PROFILING) -o find_tags_motus $^ $(LDFLAGS)

sqlite3.o: sqlite3.c
	gcc -c -o sqlite3.o -O2 sqlite3.c

test7: test7.cpp Known_Tag.o Tag_Database.o sqlite3.o Freq_Setting.o
	g++ -O2 -std=c++0x  -g3  -o test7 test7.cpp Tag_Database.o sqlite3.o Known_Tag.o Freq_Setting.o -lrt -ldl
