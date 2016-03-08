## Makefile for linux

CXX=~john/bin/gfilt

## PROFILING FLAGS (uncomment to enable profiling)
PROFILING=-g -pg

## DEBUG FLAGS WITH DUMPING OF TREE (VERY VERBOSE)
## CPPFLAGS=-Wall -g3 -std=c++0x $(PROFILING) -DFIND_TAGS_DEBUG  -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS)

## DEBUG FLAGS:
CPPFLAGS=-Wall -Wno-sign-compare -g3 -std=c++0x $(PROFILING) -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS) -I/home/src/boost_1_54_0

## PRODUCTION FLAGS:
##CPPFLAGS=-Wall -Wno-sign-compare -O3 -std=c++0x $(PROFILING) -DPROGRAM_VERSION=$(PROGRAM_VERSION) -DPROGRAM_BUILD_TS=$(PROGRAM_BUILD_TS) -I/home/src/boost_1_54_0

LDFLAGS=-ldl -lrt -lboost_serialization -lsqlite3
PROGRAM_VERSION=\""$(shell git describe)\""
PROGRAM_BUILD_TS=$(shell date +%s)

all: find_tags_motus testAddRemoveTag ## find_tags_unifile

OBJS=Ambiguity.o  Freq_Setting.o  History.o  Pulse.o Set.o Tag_Candidate.o  Tag_Finder.o  Tag.o Ticker.o DB_Filer.o Freq_History.o Graph.o Node.o Rate_Limiting_Tag_Finder.o Tag_Database.o Tag_Foray.o Data_Source.o Lotek_Data_Source.o SG_Data_Source.o

clean:
	rm -f $(OBJS) find_tags find_tags_unifile find_tags_motus  find_tags_motus.o  testAddRemoveTag.o

DB_Filer.o: DB_Filer.cpp DB_Filer.hpp find_tags_common.hpp

Freq_Setting.o: Freq_Setting.cpp Freq_Setting.hpp find_tags_common.hpp

Freq_History.o: Freq_History.cpp Freq_History.hpp find_tags_common.hpp

DFA_Node.o: DFA_Node.cpp DFA_Node.hpp find_tags_common.hpp

DFA_Graph.o: DFA_Graph.cpp DFA_Graph.hpp find_tags_common.hpp

Tag_Database.o: Tag_Database.cpp Tag_Database.hpp find_tags_common.hpp

Pulse.o: Pulse.cpp Pulse.hpp find_tags_common.hpp

Tag_Candidate.o: Tag_Candidate.hpp Tag_Candidate.cpp Tag_Finder.hpp find_tags_common.hpp

Tag_Finder.o: Tag_Finder.hpp Tag_Finder.cpp Tag_Candidate.hpp find_tags_common.hpp

Tag_Foray.o: Tag_Foray.hpp Tag_Foray.cpp find_tags_common.hpp

Rate_Limiting_Tag_Finder.o: Rate_Limiting_Tag_Finder.hpp find_tags_common.hpp

find_tags_unifile.o: find_tags_unifile.cpp find_tags_common.hpp Freq_History.hpp Freq_Setting.hpp DFA_Node.hpp DFA_Graph.hpp Tag.hpp Tag_Database.hpp Pulse.hpp Burst_Params.hpp Bounded_Range.hpp Tag_Candidate.hpp Tag_Finder.hpp Rate_Limiting_Tag_Finder.hpp Tag_Foray.hpp

find_tags_unifile: Freq_Setting.o Freq_History.o DFA_Node.o DFA_Graph.o Tag.o Tag_Database.o Pulse.o Tag_Candidate.o Tag_Finder.o Rate_Limiting_Tag_Finder.o find_tags_unifile.o Tag_Foray.o
	g++ $(PROFILING) -o find_tags_unifile $^ $(LDFLAGS)

find_tags_motus.o: find_tags_motus.cpp find_tags_common.hpp Freq_History.hpp Freq_Setting.hpp DFA_Node.hpp DFA_Graph.hpp Tag.hpp Tag_Database.hpp Pulse.hpp Burst_Params.hpp Bounded_Range.hpp Tag_Candidate.hpp Tag_Finder.hpp Rate_Limiting_Tag_Finder.hpp Tag_Foray.hpp Ambiguity.hpp

find_tags_motus: $(OBJS) find_tags_motus.o
	g++ $(PROFILING) -o find_tags_motus $^ $(LDFLAGS)

Data_Source.o: Data_Source.hpp find_tags_common.hpp

SG_Data_Source.o: SG_Data_Source.hpp Data_Source.hpp find_tags_common.hpp

Lotek_Data_Source.o: Lotek_Data_Source.hpp Data_Source.hpp find_tags_common.hpp

Tag.o: Tag.hpp Tag.cpp find_tags_common.hpp

Node.o: Node.hpp Node.cpp Tag.hpp find_tags_common.hpp

Set.o: Set.hpp find_tags_common.hpp

Graph.o: Graph.hpp Graph.cpp Set.hpp Node.hpp Tag.hpp find_tags_common.hpp

History.o: Event.hpp History.hpp History.cpp

Ticker.o: Ticker.hpp Ticker.cpp History.hpp

Ambiguity.o: Ambiguity.hpp Ambiguity.cpp

testAddRemoveTag.o: testAddRemoveTag.cpp Graph.o Tag_Database.o Freq_Setting.o DB_Filer.o

testAddRemoveTag: testAddRemoveTag.o Tag_Database.o Tag.o Node.o Set.o Graph.o Freq_Setting.o History.o Ticker.o Ambiguity.o DB_Filer.o Tag_Candidate.o
	g++ $(PROFILING) -o testAddRemoveTag $^ $(LDFLAGS)
