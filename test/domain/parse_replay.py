#!/usr/bin/env python3
"""Prints the decks of the YRP replay embedded in a Multirole YRPX replay and
checks that the YRP has exactly the size its header says (REPLAY_DOMAIN
0x400: a Deck Master block after each duelist's Extra Deck)."""
import lzma, struct, sys
data = open(sys.argv[1], 'rb').read()
typ, ver, flags, ts, size, h = struct.unpack_from('<6I', data, 0)
props = data[24:29]
lc_lp_pb, dict_size = props[0], struct.unpack('<I', props[1:5])[0]
lc = lc_lp_pb % 9; lp = (lc_lp_pb // 9) % 5; pb = lc_lp_pb // 45
dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[{'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp, 'pb': pb, 'dict_size': dict_size}])
body = dec.decompress(data[72:], max_length=size)
assert len(body) == size
p = 0
def rd(fmt):
	global p
	v = struct.unpack_from(fmt, body, p); p += struct.calcsize(fmt); return v
for t in range(2):
	n, = rd('<I'); p += 40 * n
flags64, = rd('<Q')
yrp = None
while p < len(body):
	mt, = rd('<B'); ln, = rd('<I'); m = body[p:p+ln]; p += ln
	if mt == 231: yrp = m
	else: print('msg', mt, m.hex())
q = 0
def rq(fmt):
	global q
	v = struct.unpack_from(fmt, yrp, q); q += struct.calcsize(fmt); return v
ytyp, yver, yflags, yts, ysize, yh = rq('<6I'); q = 72
print('YRP flags 0x%x size %d, actual %d' % (yflags, ysize, len(yrp) - 72))
counts = []
for t in range(2):
	n, = rq('<I'); counts.append(n); q += 40 * n
lp, hand, draw, df = rq('<IIIQ')
print('lp', lp, hand, draw, 'flags 0x%x' % df)
for d in range(sum(counts)):
	mc, = rq('<I'); main = rq('<%dI' % mc)
	ec, = rq('<I'); extra = rq('<%dI' % ec)
	master = ()
	if yflags & 0x400:
		dc, = rq('<I'); master = rq('<%dI' % dc)
	print('duelist', d, main, extra, master)
rc, = rq('<I'); print('rules', rq('<%dI' % rc))
while q < len(yrp):
	ln, = rq('<B'); print('response', yrp[q:q+ln].hex()); q += ln
assert q == len(yrp)
print('OK')
