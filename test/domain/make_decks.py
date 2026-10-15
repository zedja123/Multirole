#!/usr/bin/env python3
"""Writes Domain test decks (.ydk with "#master") for run.sh.

  make_decks.py <LFListsZedja dir> <BabelCDB dir> <out dir> [random count]

Uses LFListsZedja's reference implementation (tools/domain_ref.py) to pick
cards, so that the decks cover both legal decks and each kind of error.
Writes <out>/expected.txt with the reference verdict of every deck:
"<deck>: OK" or "<deck>: 0x<type> <code>".
"""
import os
import random
import sys


def main():
	lfdir, babel, out = sys.argv[1:4]
	nrandom = int(sys.argv[4]) if len(sys.argv) > 4 else 200
	sys.path.insert(0, os.path.join(lfdir, 'tools'))
	import domain_ref as R
	lst = R.find_domain_list(os.path.join(lfdir, 'Domain.lflist.conf'))
	cards = R.load_cards(R.babel_cdbs(babel))
	os.makedirs(out, exist_ok=True)
	rng = random.Random(1234)

	def is_extra(c):
		return bool(c.type & R.TYPES_EXTRA and c.type & R.TYPE_MONSTER)

	deckable = [c for c in sorted(cards.values(), key=lambda c: c.code)
		if c.alias == 0 and not c.type & (R.TYPE_TOKEN | R.TYPE_SKILL)
		and c.type & (R.TYPE_MONSTER | R.TYPE_SPELL | R.TYPE_TRAP)]
	spells = [c for c in deckable if c.type & (R.TYPE_SPELL | R.TYPE_TRAP)]
	monsters = [c for c in deckable if c.type & R.TYPE_MONSTER]

	decks = {}

	def legal(dm_code, seed):
		r = random.Random(seed)
		dm = cards[dm_code]
		dom = R.deck_master_domain(lst, dm)
		inside = [c for c in monsters if c.code != dm_code and R.in_domain(c, dom, cards)]
		main_m = [c.code for c in r.sample([c for c in inside if not is_extra(c)], 25)]
		extra = [c.code for c in r.sample([c for c in inside if is_extra(c)], 15)]
		main_s = [c.code for c in r.sample(spells, 60 - len(main_m))]
		return dict(master=dm_code, main=main_m + main_s, extra=extra, side=[])

	CHRONICLE = 60948488
	DARK_MAGICIAN = 46986414
	DARK_MAGICIAN_ART = 36996508
	BEWD = 89631139
	BEWD_ART = 89631140
	base = legal(CHRONICLE, 1)
	base['main'][:3] = [DARK_MAGICIAN, BEWD, 30208479]  # Magician of Black Chaos
	decks['legal_chronicle'] = base

	def variant(name, **kw):
		d = {k: list(v) if isinstance(v, list) else v for k, v in base.items()}
		d.update(kw)
		decks[name] = d
		return d

	dom = R.deck_master_domain(lst, cards[CHRONICLE])
	outside = next(c.code for c in monsters if not is_extra(c) and not R.in_domain(c, dom, cards))
	variant('out_of_domain', main=base['main'][:-1] + [outside])
	variant('duplicate', main=base['main'][:-1] + [base['main'][-2]])
	variant('no_master', master=None)
	token = next(c.code for c in sorted(cards.values(), key=lambda c: c.code) if c.type & R.TYPE_TOKEN)
	variant('token_master', master=token)
	variant('spell_master', master=spells[0].code)
	variant('unknown_master', master=1)
	variant('master_copy_in_main', main=base['main'][:-1] + [CHRONICLE])
	variant('artwork_duplicate', main=base['main'][:-1] + [DARK_MAGICIAN_ART])
	variant('artwork_alias_ok', main=[BEWD_ART if c == BEWD else c for c in base['main']])
	variant('main_59', main=base['main'][:-1])
	variant('extra_16', extra=base['extra'] + [next(c.code for c in monsters if is_extra(c)
		and c.code not in base['extra'] and R.in_domain(c, dom, cards))])
	variant('side_1', side=[spells[-1].code])
	variant('extra_0', extra=[])
	# Rulebook example: Lo, the Prayers of the Voiceless Voice.
	decks['legal_lo'] = legal(25801745, 2)
	decks['legal_ecclesia'] = legal(55273560, 3)
	decks['legal_malicious'] = legal(9411399, 4)

	# Random decks: a random Deck Master, cards mostly from its domain.
	pool = [c for c in monsters if not R.is_forbidden_dm(lst, c)]
	for i in range(nrandom):
		dm = rng.choice(pool)
		d = legal(dm.code, 100 + i)
		roll = rng.random()
		if roll < 0.3:
			# Any monster, in or out of the domain.
			d['main'][rng.randrange(60)] = rng.choice([c for c in monsters if not is_extra(c)]).code
		elif roll < 0.4:
			d['extra'][rng.randrange(15)] = rng.choice([c for c in monsters if is_extra(c)]).code
		elif roll < 0.5:
			# An alternate artwork or "always treated as" row.
			alias_rows = [c for c in cards.values() if c.alias and c.type & R.TYPE_MONSTER
				and not c.type & R.TYPE_TOKEN and not is_extra(c)]
			d['main'][rng.randrange(60)] = rng.choice(alias_rows).code
		elif roll < 0.55:
			d['main'][rng.randrange(60)] = dm.code
		decks['random_%03d' % i] = d

	with open(os.path.join(out, 'expected.txt'), 'w') as exp:
		for name, d in decks.items():
			path = os.path.join(out, name + '.ydk')
			with open(path, 'w') as f:
				f.write('#created by make_decks.py\n#main\n')
				for c in d['main'] + d['extra']:
					f.write('%d\n' % c)
				f.write('!side\n')
				for c in d['side']:
					f.write('%d\n' % c)
				if d['master'] is not None:
					f.write('#master\n%d\n' % d['master'])
			err, code = R.check_deck(R.read_ydk(path, cards), lst, cards)
			exp.write('%s: %s\n' % (path, 'OK' if not err else '0x%X %d' % (err, code)))
	print('%d decks written to %s' % (len(decks), out))


if __name__ == '__main__':
	main()
