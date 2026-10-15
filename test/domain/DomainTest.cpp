// Domain format (MASQPro) test driver for Multirole's real banlist parser and
// Domain deck check. Built and run by run.sh, see there.
//
//   DomainTest hash <file.lflist.conf>...
//     Parses each file with YGOPro::ParseForBanlists and prints every list
//     found as "<file>\t<hash>\t<domain|genesys|list>".
//   DomainTest show <file.lflist.conf> <code>...
//     Prints the parsed "#@dm" entry of each code in the file's Domain list.
//   DomainTest check <file.lflist.conf> <cards.cdb>... -- <deck.ydk>...
//     Checks each deck against the Domain list of the file, as
//     Room::Context::CheckDeck does in a Domain room (sizes Main 60-60,
//     Extra 0-15, Side 0-0, any allowed cards, content checked), and prints
//     "<deck>: OK" or "<deck>: <error type hex> <code>".
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#define YGOPRO_BANLIST_PARSER_IMPLEMENTATION
#include "../../src/Multirole/YGOPro/BanlistParser.hpp"
#include "../../src/Multirole/Room/DeckCheck.hpp"
#include "../../src/Multirole/YGOPro/CardDatabase.hpp"
#include "../../src/Multirole/YGOPro/Constants.hpp"
#include "../../src/Multirole/YGOPro/Deck.hpp"
#include "../../src/Multirole/YGOPro/MsgCommon.hpp"

using namespace Ignis::Multirole;

namespace
{

YGOPro::BanlistMap Parse(const char* path)
{
	YGOPro::BanlistMap map;
	std::ifstream f(path);
	YGOPro::ParseForBanlists(f, map);
	return map;
}

// Reads a .ydk as the client sends it: "#main"/"#extra" codes, "!side"
// codes, and the first code after "#master" as the Deck Master.
void ReadYdk(const char* path, std::vector<uint32_t>& main, std::vector<uint32_t>& side, uint32_t& master)
{
	std::ifstream f(path);
	std::string l;
	int section = 0; // 0 main, 1 side, 2 master
	while(std::getline(f, l))
	{
		while(!l.empty() && (l.back() == '\r' || l.back() == ' '))
			l.pop_back();
		if(l.empty())
			continue;
		if(l == "#master")
		{
			section = 2;
			continue;
		}
		if(l[0] == '#')
			continue;
		if(l[0] == '!')
		{
			section = 1;
			continue;
		}
		const auto code = static_cast<uint32_t>(std::stoul(l));
		if(section == 2)
		{
			if(master == 0U)
				master = code;
		}
		else
		{
			(section == 1 ? side : main).push_back(code);
		}
	}
}

// Context::LoadDeck: Main/Extra split by type, Tokens dropped, unknown
// codes dropped and reported.
YGOPro::Deck LoadDeck(const YGOPro::CardDatabase& cdb, const std::vector<uint32_t>& main,
                      const std::vector<uint32_t>& side, uint32_t master)
{
	YGOPro::CodeVector m, e, s;
	uint32_t err = 0U;
	for(const auto code : main)
	{
		const auto& data = cdb.DataFromCode(code);
		if(data.code == 0U)
		{
			err = code;
			continue;
		}
		if((data.type & TYPE_TOKEN) != 0U)
			continue;
		const bool extra = (data.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) != 0U ||
		                   ((data.type & TYPE_LINK) != 0U && (data.type & TYPE_MONSTER) != 0U);
		(extra ? e : m).push_back(code);
	}
	for(const auto code : side)
	{
		const auto& data = cdb.DataFromCode(code);
		if(data.code == 0U)
		{
			err = code;
			continue;
		}
		if((data.type & TYPE_TOKEN) == 0U)
			s.push_back(code);
	}
	return YGOPro::Deck(std::move(m), std::move(e), std::move(s), err, master);
}

// Context::CheckDeck in a Domain room with content checks: Deck Master,
// unknown cards, sizes, then the Domain content.
Room::DeckCheck::Result CheckDeck(const YGOPro::Deck& deck, const YGOPro::Banlist& bl,
                                  const YGOPro::CardDatabase& cdb)
{
	using namespace Room::DeckCheck;
	if(auto f = CheckDeckMaster(deck, bl, cdb, true); f)
		return f;
	if(deck.Error() != 0U)
		return Failure{Error::CARD_UNKNOWN, deck.Error()};
	std::size_t skills = 0U;
	for(const auto code : deck.Main())
		skills += (cdb.DataFromCode(code).type & TYPE_SKILL) != 0U;
	if(deck.Main().size() - skills != 60U)
		return Failure{Error::DECK_BAD_MAIN_COUNT, 0U};
	if(deck.Extra().size() > 15U)
		return Failure{Error::DECK_BAD_EXTRA_COUNT, 0U};
	if(!deck.Side().empty())
		return Failure{Error::DECK_BAD_SIDE_COUNT, 0U};
	return CheckContent(deck, bl, cdb, 0, YGOPro::ALLOWED_CARDS_ANY);
}

} // namespace

int main(int argc, char** argv)
{
	if(argc < 3)
	{
		std::fprintf(stderr, "usage: %s hash <list>... | check <list> <cdb>... -- <ydk>...\n", argv[0]);
		return 2;
	}
	const std::string cmd = argv[1];
	if(cmd == "hash")
	{
		for(int i = 2; i < argc; i++)
		{
			for(const auto& [hash, bl] : Parse(argv[i]))
			{
				std::printf("%s\t0x%08X\t%s\n", argv[i], hash,
				            bl->IsDomain() ? "domain" : bl->IsGenesys() ? "genesys" : "list");
			}
		}
		return 0;
	}
	if(cmd == "show")
	{
		for(const auto& [hash, bl] : Parse(argv[2]))
		{
			if(!bl->IsDomain())
				continue;
			for(int i = 3; i < argc; i++)
			{
				const auto code = static_cast<uint32_t>(std::stoul(argv[i]));
				const auto* e = bl->FindDomain(code, 0U);
				if(e == nullptr)
				{
					std::printf("%u: none%s\n", code, bl->IsForbiddenMaster(code, 0U) ? " forbidden" : "");
					continue;
				}
				std::printf("%u: a=%x r=%llx s=", code, e->attribute,
				            static_cast<unsigned long long>(e->race));
				for(const auto s : e->setcodes)
					std::printf("%x,", s);
				std::printf(" n=");
				for(const auto n : e->names)
					std::printf("%u,", n);
				std::printf("%s\n", bl->IsForbiddenMaster(code, 0U) ? " forbidden" : "");
			}
		}
		return 0;
	}
	if(cmd == "check")
	{
		YGOPro::BanlistPtr domain;
		for(const auto& [hash, bl] : Parse(argv[2]))
			if(bl->IsDomain())
				domain = bl;
		if(!domain)
		{
			std::fprintf(stderr, "%s: no Domain list\n", argv[2]);
			return 2;
		}
		YGOPro::CardDatabase cdb;
		int i = 3;
		for(; i < argc && std::string(argv[i]) != "--"; i++)
			if(!cdb.Merge(argv[i]))
				std::fprintf(stderr, "could not merge %s\n", argv[i]);
		int status = 0;
		for(i++; i < argc; i++)
		{
			std::vector<uint32_t> main, side;
			uint32_t master = 0U;
			ReadYdk(argv[i], main, side, master);
			const auto deck = LoadDeck(cdb, main, side, master);
			if(const auto f = CheckDeck(deck, *domain, cdb); f)
			{
				status = 1;
				std::printf("%s: 0x%X %u\n", argv[i], static_cast<unsigned>(f->type), f->code);
			}
			else
			{
				std::printf("%s: OK\n", argv[i]);
			}
		}
		return status;
	}
	return 2;
}
