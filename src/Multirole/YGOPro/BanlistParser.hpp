#ifndef YGOPRO_BANLIST_PARSER_HPP
#define YGOPRO_BANLIST_PARSER_HPP
#include <unordered_map>

#include "Banlist.hpp"

namespace YGOPro
{

using BanlistHash = uint32_t;
using BanlistMap = std::unordered_map<BanlistHash, BanlistPtr>;

} // namespace YGOPro

#endif // YGOPRO_BANLIST_PARSER_HPP

#ifdef YGOPRO_BANLIST_PARSER_IMPLEMENTATION
#ifndef YGOPRO_BANLIST_PARSER_IMPL_HPP
#define YGOPRO_BANLIST_PARSER_IMPL_HPP
#include <charconv>
#include <fmt/format.h>

namespace YGOPro
{

namespace Detail
{

constexpr const BanlistHash BANLIST_HASH_MAGIC = 0x7DFCEE6A;

constexpr BanlistHash Salt(BanlistHash hash, uint32_t code, int32_t count) noexcept
{
	constexpr uint32_t HASH_MAGIC_1 = 18U;
	constexpr uint32_t HASH_MAGIC_2 = 14U;
	constexpr uint32_t HASH_MAGIC_3 = 27U;
	constexpr uint32_t HASH_MAGIC_4 = 5U;
	return hash ^ ((code << HASH_MAGIC_1) | (code >> HASH_MAGIC_2)) ^
	       ((code << (HASH_MAGIC_3 + count)) | (code >> (HASH_MAGIC_4 - count)));
}

// Genesys hash contributions. They must stay identical to MASQPro's
// DeckManager::LoadLFListSingle so client and server agree on the hash.
constexpr BanlistHash GenesysCapSalt(uint32_t cap) noexcept
{
	return 0x47454E53U ^ (cap * 0x9E3779B1U);
}

constexpr BanlistHash GenesysPointsSalt(uint32_t code, uint32_t points) noexcept
{
	return ((code << 7U) | (code >> 25U)) ^ (points * 0x85EBCA77U);
}

} // namespace Detail

template<typename Stream>
void ParseForBanlists(Stream& stream, BanlistMap& banlists)
{
	BanlistHash hash = Detail::BANLIST_HASH_MAGIC;
	bool whitelist = false;
	bool genesys = false;
	uint32_t genesysCap = 0U;
	Banlist::DictType dict;
	Banlist::PointsType points;
	auto ConditionallyAdd = [&]()
	{
		if(hash == Detail::BANLIST_HASH_MAGIC)
			return;
		auto banlist = std::make_shared<Banlist>(whitelist, std::move(dict),
			genesys, genesysCap, std::move(points));
		banlists.emplace(std::piecewise_construct,
			std::forward_as_tuple(hash),
			std::forward_as_tuple(std::move(banlist))
		);
	};
	std::string l;
	std::size_t lc = 0U;
	auto MakeException = [&lc](std::string_view str)
	{
		return std::runtime_error(fmt::format("line {:d}: {:s}", lc, str));
	};
	while(++lc, std::getline(stream, l))
	{
		if(l.empty())
			continue;
		if(l.find("$whitelist") != std::string::npos)
		{
			whitelist = true;
			continue;
		}
		if(l.rfind("$genesys", 0U) == 0U)
		{
			if(genesys)
				continue;
			uint32_t cap = 100U;
			if(const auto b = l.find_first_of("0123456789"); b != std::string::npos)
			{
				auto e = l.find_first_not_of("0123456789", b);
				if(e == std::string::npos)
					e = l.size();
				std::from_chars(l.data() + b, l.data() + e, cap);
			}
			genesys = true;
			genesysCap = cap;
			hash ^= Detail::GenesysCapSalt(cap);
			continue;
		}
		switch(l[0U])
		{
		case '!':
		{
			ConditionallyAdd();
			// Reset state
			hash = Detail::BANLIST_HASH_MAGIC;
			whitelist = false;
			genesys = false;
			genesysCap = 0U;
			dict.clear();
			points.clear();
			continue;
		}
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		{
			uint32_t code;
			int32_t count;

			const auto separator = l.find(' ');
			if(separator == std::string::npos)
				throw MakeException("Card code separator not found");

			{
				const auto [_, error]
				{
					std::from_chars(l.data(), l.data() + separator, code)
				};
				if(error != std::errc())
					throw MakeException("Could not parse code");
				if(code == 0U)
					throw MakeException("Card code cannot be 0");
			}
			{
				static constexpr auto INT_CHARS = "-0123456789";
				const auto begin = l.find_first_of(INT_CHARS, separator);
				if(begin == std::string::npos)
					throw MakeException("Could not find count begin");
				auto end = l.find_first_not_of(INT_CHARS, begin);
				if(end == std::string::npos)
					end = l.size();
				const auto [_, error]
				{
					std::from_chars(l.data() + begin, l.data() + end, count)
				};
				if(error != std::errc())
					throw MakeException("Could not parse count");
				// Optional third column: Genesys points.
				if(const auto pb = l.find_first_not_of(" \t", end);
				   pb != std::string::npos && l[pb] >= '0' && l[pb] <= '9')
				{
					auto pe = l.find_first_not_of("0123456789", pb);
					if(pe == std::string::npos)
						pe = l.size();
					uint32_t value = 0U;
					std::from_chars(l.data() + pb, l.data() + pe, value);
					if(value > 0U)
					{
						points[code] = value;
						hash ^= Detail::GenesysPointsSalt(code, value);
					}
				}
			}

			hash = Detail::Salt(hash, code, count);
			dict[code] = count;
			continue;
		}
		}
	}
	ConditionallyAdd();
}

} // namespace YGOPro

#endif // YGOPRO_BANLIST_PARSER_IMPL_HPP
#endif // YGOPRO_BANLIST_PARSER_IMPLEMENTATION
