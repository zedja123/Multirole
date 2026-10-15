#!/bin/sh
# Domain format test for Multirole's banlist parser and Domain deck check.
#
#   test/domain/run.sh <LFListsZedja dir> <BabelCDB dir> [work dir]
#
# Builds DomainTest from the real sources, then:
#  1. compares the hash of every list of the LFListsZedja files, and of the
#     reference test vectors, with tools/domain_ref.py;
#  2. checks the decks written by make_decks.py and compares every verdict
#     (error type and card code) with the reference implementation;
#  3. checks a few cases the reference cannot see (Skill and Rush cards);
#  4. serializes replays with and without the Domain flag and checks the
#     Deck Master blocks and the sizes of the embedded YRP replay.
# Needs g++ (C++17), the fmt and sqlite3 libraries, and python3.
set -eu
LF=$1
BABEL=$2
WORK=${3:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../../src
mkdir -p "$WORK"
g++ -std=c++17 -O1 -Wall -Wextra -I"$SRC" -o "$WORK/DomainTest" "$HERE/DomainTest.cpp" \
	"$SRC/Multirole/YGOPro/Banlist.cpp" "$SRC/Multirole/YGOPro/Deck.cpp" \
	"$SRC/Multirole/YGOPro/CardDatabase.cpp" "$SRC/Multirole/Room/DeckCheck.cpp" \
	-lfmt -lsqlite3
fail=0

echo "== hashes"
python3 -I "$LF/tools/domain_ref.py" vectors "$WORK/vectors.conf" > /dev/null
for f in "$LF"/*.lflist.conf "$WORK/vectors.conf"; do
	"$WORK/DomainTest" hash "$f" | cut -f 2 | sort > "$WORK/mr.txt"
	python3 -I "$LF/tools/domain_ref.py" hash "$f" | cut -f 3 | sort > "$WORK/ref.txt"
	if cmp -s "$WORK/mr.txt" "$WORK/ref.txt"; then
		echo "ok   $(basename "$f") $(tr '\n' ' ' < "$WORK/mr.txt")"
	else
		echo "FAIL $(basename "$f"): multirole $(tr '\n' ' ' < "$WORK/mr.txt") reference $(tr '\n' ' ' < "$WORK/ref.txt")"
		fail=1
	fi
done

echo "== decks"
python3 -I "$HERE/make_decks.py" "$LF" "$BABEL" "$WORK/decks" "${DOMAIN_RANDOM_DECKS:-300}"
# The reference reads cards.cdb, release-*.cdb and prerelease-*.cdb (no
# Rush), the first file winning; Merge replaces, so merge them in reverse.
CDBS=$(ls "$BABEL"/cards.cdb "$BABEL"/release-*.cdb "$BABEL"/prerelease-*.cdb | grep -v -i rush | sort -r)
# shellcheck disable=SC2086
"$WORK/DomainTest" check "$LF/Domain.lflist.conf" $CDBS -- "$WORK"/decks/*.ydk > "$WORK/got.txt" || true
sort "$WORK/decks/expected.txt" > "$WORK/exp.sorted"
sort "$WORK/got.txt" > "$WORK/got.sorted"
if cmp -s "$WORK/exp.sorted" "$WORK/got.sorted"; then
	echo "ok   $(wc -l < "$WORK/got.sorted") decks, $(grep -c ': OK$' "$WORK/got.sorted") legal, same verdicts as the reference"
else
	echo "FAIL verdicts differ (< reference, > multirole):"
	diff "$WORK/exp.sorted" "$WORK/got.sorted" || true
	fail=1
fi
for name in legal_chronicle out_of_domain duplicate no_master token_master spell_master unknown_master \
	master_copy_in_main artwork_duplicate artwork_alias_ok main_59 extra_16 side_1 extra_0 legal_lo; do
	grep "/$name.ydk:" "$WORK/got.txt" | sed "s|$WORK/decks/||"
done

echo "== Skill and Rush cards"
# With the Skill and Rush databases loaded, a Skill is a forbidden type
# (0x9) and a Rush card is out of the domain (0x12); a Skill Deck Master is
# not a Deck Master (0x11).
set -- $(python3 -I - "$BABEL" "$WORK/decks/legal_chronicle.ydk" "$WORK" <<'PY'
import sqlite3, sys
babel, base, work = sys.argv[1:4]
def first(db, where):
	con = sqlite3.connect('file:%s/%s?mode=ro' % (babel, db), uri=True)
	return con.execute('select id from datas where %s order by id limit 1' % where).fetchone()[0]
skill = first('cards-skills.cdb', 'type & 0x8000000')
rush = first('cards-rush.cdb', 'type & 0x2 and alias = 0')
lines = open(base).read().split('\n')
main = lines.index('#main')
def write(name, lines):
	open('%s/%s.ydk' % (work, name), 'w').write('\n'.join(lines))
# A Skill as a 61st Main Deck card, a Rush Spell in place of a Main Deck
# card, and a Skill as Deck Master.
write('skill_in_main', lines[:main + 1] + [str(skill)] + lines[main + 1:])
write('rush_in_main', lines[:main + 1] + [str(rush)] + lines[main + 2:])
write('skill_master', lines[:lines.index('#master') + 1] + [str(skill)])
print(skill, rush)
PY
)
SKILL=$1
RUSH=$2
# shellcheck disable=SC2086
"$WORK/DomainTest" check "$LF/Domain.lflist.conf" "$BABEL/cards-skills.cdb" "$BABEL/cards-rush.cdb" $CDBS -- \
	"$WORK/skill_in_main.ydk" "$WORK/rush_in_main.ydk" "$WORK/skill_master.ydk" > "$WORK/extra.txt" || true
sed "s|$WORK/||" "$WORK/extra.txt"
grep -q "skill_in_main.ydk: 0x9 $SKILL" "$WORK/extra.txt" || fail=1
grep -q "rush_in_main.ydk: 0x12 $RUSH" "$WORK/extra.txt" || fail=1
grep -q "skill_master.ydk: 0x11 $SKILL" "$WORK/extra.txt" || fail=1

echo "== replays"
M=$SRC/Multirole/YGOPro
for c in LzmaEnc LzFind Alloc; do
	gcc -O1 -D_7ZIP_ST -c -o "$WORK/$c.o" "$M/LZMA/$c.c"
done
g++ -std=c++17 -O1 -I"$SRC" -o "$WORK/ReplayTest" "$HERE/ReplayTest.cpp" "$M/Replay.cpp" \
	"$M/StringUtils.cpp" "$WORK/LzmaEnc.o" "$WORK/LzFind.o" "$WORK/Alloc.o"
"$WORK/ReplayTest" domain > "$WORK/domain.yrpx"
"$WORK/ReplayTest" > "$WORK/plain.yrpx"
python3 -I "$HERE/parse_replay.py" "$WORK/domain.yrpx" > "$WORK/domain.txt" || fail=1
python3 -I "$HERE/parse_replay.py" "$WORK/plain.yrpx" > "$WORK/plain.txt" || fail=1
cat "$WORK/domain.txt"
grep -q "YRP flags 0x730" "$WORK/domain.txt" || fail=1
grep -q "duelist 0 (1, 2, 3) (4,) (60948488,)" "$WORK/domain.txt" || fail=1
grep -q "YRP flags 0x330" "$WORK/plain.txt" || fail=1
grep -q "^OK" "$WORK/plain.txt" || fail=1

if [ "$fail" -eq 0 ]; then echo "ALL OK"; else echo "FAILED"; fi
exit "$fail"
