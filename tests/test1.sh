#!/bin/bash

## This tests whether we get the same results from a set of files
## when we run it as a pause/resume pair of sessions, versus
## running all files in one go.

## Relative paths assume this script is run from its directory.

SQL=sqlite3
RCVDB=test1/test1.sqlite
FINDTAGS=../src/find_tags_motus
OPTIONS="--pulses_to_confirm=8 --frequency_slop=0.5 --min_dfreq=0 --max_dfreq=12 --pulse_slop=1.5 --burst_slop=4 --burst_slop_expansion=1 --use_events --max_skipped_bursts=20 --default_freq=166.376 --bootnum=176  --src_sqlite $RCVDB"

tar -xjvf test1.tar.bz2

## break files into two sets; we know some runs cross between them
$SQL $RCVDB <<EOF
create table save_files as select * from files where fileID >= 15600;
delete from files where fileID>=15600;
EOF

## run 1st set of files
$FINDTAGS $OPTIONS $RCVDB >/dev/null 2>&1

## restore 2nd set of files
$SQL $RCVDB <<EOF
insert into files select * from save_files;
drop table save_files;
EOF

## resume processing of previous boot session (i.e. 2nd set of files)
$FINDTAGS --resume $OPTIONS $RCVDB >/dev/null 2>&1

## re-run same boot session (all files)
$FINDTAGS $OPTIONS $RCVDB >/dev/null 2>&1

$SQL $RCVDB <<EOF
select "numHits, numRuns correct: " ||
   case when
       (select numHits from batches where batchID=3) = 127
       and (select count(*) from runs where batchIDbegin=3) = 2
   then "PASS"
   else "FAIL"
   end;

select "pause/resume numHits equal: " ||
   case when
         (select sum(numHits) from batches where batchID < 3)
       = (select numHits from batches where batchID=3)
   then "PASS"
   else "FAIL"
   end;

select "pause/resume numRuns equal: " ||
   case when
         (select count(*) from runs where batchIDbegin < 3)
       = (select count(*) from runs where batchIDbegin=3)
   then "PASS"
   else "FAIL"
   end;

select "all detections ambiguous: " ||
   case when
       (select sum(motusTagID=-1) from runs) = 4
   then "PASS"
   else "FAIL"
   end;


EOF
