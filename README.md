find_tags:  assemble putative pulses from coded ID tags and connect them into runs
of tag detections.

There are two versions of the program:  `find_tags_motus` and `find_tags_unifile`

find_tags_motus|find_tags_unifile
---|---
runs on motus data processing server|runs on sensorgnome receivers(*)
has access to full Lotek codeset and all registered tags|only detects tags registered by the receiver owner and stored in a database on the receiver
works from a tag event table that reflects deployment history and estimated lifespan of each tag|works from a static list of tags, assumed to always be active
reads input from archived raw receiver files, indexed in an .sqlite database|reads input from stdin
writes output in batch/run/hit format to an sqlite DB|writes csv-formatted output to stdout
works with files from both sensorgnomes and Lotek receivers|only works with output from the sensorgnome pulse detector
records receiver parameter settings, GPS fixes, pulseCounts|ignores non-pulse records except for frequency setting
records how it was run in the receiver database|no record of how it was run
can save state and resume processing when new data for the same receiver arrive (possibly months later)|cannot be paused and resumed
handles tag ambiguities wherein two or more indistinguishalbe tags are active during the same period; ambiguous detections are reported as such|static tag list **cannot** contain any indistinguishable pairs of tags
built from `master` branch by `git checkout master; make clean; make find_tags_motus`|built from `find_tags_unifile` tag by `git checkout find_tags_unifile;make clean; make find_tags_unifile`

(*): `find_tags_unifile` is also used in the tag registration process
on the data processing server; it uses the database of all Lotek ID
codes to detect tag bursts in user-submitted .WAV recordings.

The algorithm uses a collection of DFAs walking a directed graph whose
nodes are sets of tags, and whose edges are sets of gap-length ranges.
The graph changes dynamically in response to tag activation and
expiration events.  Each DFA is a `Tag_Candidate`, which represents a
possible real tag.  The DFA accepts sequences of inter-pulse gaps
compatible with the temporal pattern of one or more registered tags,
subject to filtering by signal strength and offset frequency
consistency.  As the DFA accepts more pulses, the set of tags
compatible with them shrinks, until eventually (hopefully) it reaches
a single tag.  The algorithm is greedy in that once a tag candidate
has reached a (selectable) degree of further confirmation
(i.e. accepted enough additional pulses), any other candidates sharing
pulses with it are discarded.  Such `confirmed` Tag_Candidates output
a summary of each burst of pulses from their tag, as these are
recognized.  This summary constitutes a *tag detection*.

A set of DFAs operating on a single antenna and nominal frequency is
called a `Tag_Finder`. The collection of all `Tag_Finders` is a
`Tag_Foray`.

This algorithm explores only a small portion of the full
"pulse-to-tag-assignment-problem" solution space, pruning many
potential solutions early on.  However, any pulse which passes
frequency-offset band filtering will be considered for inclusion in a
tag burst.  If the pulse is not accepted by any *confirmed*
`Tag_Candidate`, a new `Tag_Candidate` is created containing just that
pulse.
