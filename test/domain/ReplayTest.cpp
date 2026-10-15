// Domain format (MASQPro) replay test: serializes a small replay with
// YGOPro::Replay, with the Domain duel flag if an argument is given, and
// writes the YRPX bytes to stdout for parse_replay.py. Built without NDEBUG,
// so Serialize's size asserts are active.
#include <cstdio>

#include "../../src/Multirole/YGOPro/Replay.hpp"

int main(int argc, char** argv)
{
	(void)argv;
	const bool domain = argc > 1;
	YGOPro::HostInfo hi{};
	hi.startingLP = 8000U;
	hi.startingDrawCount = 5U;
	hi.drawCountPerTurn = 1U;
	hi.duelFlagsLow = 0x2E800U; // DUEL_MODE_MR5
	hi.duelFlagsHigh = domain ? 0x20U : 0U; // DUEL_DOMAIN >> 32
	YGOPro::Replay r(1700000000U, {1U, 2U, 3U, 4U}, hi, {153999999U});
	YGOPro::CodeVector master;
	if(domain)
		master.push_back(60948488U);
	r.AddDuelist(0U, 0U, {"Alice", {1U, 2U, 3U}, {4U}, master});
	r.AddDuelist(1U, 0U, {"Bob", {5U, 6U}, {}, {}});
	r.RecordMsg({2U, 204U, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}); // MSG_HINT HINT_DMZ
	r.RecordResponse({1U, 2U, 3U});
	r.Serialize();
	std::fwrite(r.Bytes().data(), 1U, r.Bytes().size(), stdout);
	return 0;
}
