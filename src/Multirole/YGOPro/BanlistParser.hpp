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
#include <algorithm>
#include <charconv>
#include <string_view>
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

// Domain hash contributions and data lines. They must stay identical to
// MASQPro's DeckManager::LoadLFListSingle (gframe/domain_list.h) and to the
// reference implementation (LFListsZedja tools/domain_ref.py).
constexpr BanlistHash DOMAIN_SALT = 0x444F4D41U;

// Contribution of one "#@" line: rotl(fnv1a32(text), 1), where text is the
// line after "#@" without the carriage return and trailing spaces and tabs.
constexpr BanlistHash DomainLineSalt(std::string_view text) noexcept
{
	uint32_t h = 0x811C9DC5U;
	for(const char c : text)
	{
		h ^= static_cast<uint8_t>(c);
		h *= 0x01000193U;
	}
	return (h << 1U) | (h >> 31U);
}

constexpr std::string_view DomainLineText(std::string_view line) noexcept
{
	line.remove_prefix(2U); // "#@"
	if(const auto cr = line.find('\r'); cr != std::string_view::npos)
		line = line.substr(0U, cr);
	const auto end = line.find_last_not_of(" \t");
	return end == std::string_view::npos ? std::string_view{} : line.substr(0U, end + 1U);
}

// Unsigned number made only of digits of the given base, no sign or prefix,
// not greater than limit.
inline bool DomainParseUint(std::string_view text, uint64_t base, uint64_t limit, uint64_t& out) noexcept
{
	if(text.empty())
		return false;
	uint64_t value = 0U;
	for(const char c : text)
	{
		uint64_t digit = 0U;
		if(c >= '0' && c <= '9')
			digit = static_cast<uint64_t>(c - '0');
		else if(base == 16U && c >= 'a' && c <= 'f')
			digit = static_cast<uint64_t>(c - 'a') + 10U;
		else if(base == 16U && c >= 'A' && c <= 'F')
			digit = static_cast<uint64_t>(c - 'A') + 10U;
		else
			return false;
		if(value > (limit - digit) / base)
			return false;
		value = value * base + digit;
	}
	out = value;
	return true;
}

inline std::vector<std::string_view> DomainSplit(std::string_view text, std::string_view separators)
{
	std::vector<std::string_view> ret;
	std::size_t pos = 0U;
	while((pos = text.find_first_not_of(separators, pos)) != std::string_view::npos)
	{
		const auto end = text.find_first_of(separators, pos);
		ret.push_back(text.substr(pos, end == std::string_view::npos ? end : end - pos));
		pos = end;
	}
	return ret;
}

// Parses the text of a "#@" line of a Domain list:
//   forbid-dm <code>
//   dm <code> [a=<hex>] [r=<hex>] [s=<hex>,...] [n=<code>,...]
// A malformed line or an unknown directive is skipped (returns false). For a
// code with several "dm" lines the first one wins. Never throws on content.
inline bool ParseDomainLine(std::string_view text, Banlist::DomainType& masters,
                            Banlist::CodeSetType& forbiddenMasters)
{
	constexpr uint64_t U32_MAX = 0xFFFFFFFFU;
	constexpr uint64_t U64_MAX = ~uint64_t{0U};
	const auto tokens = DomainSplit(text, " \t\v\f\r\n");
	if(tokens.empty())
		return false;
	uint64_t code = 0U;
	if(tokens[0U] == "forbid-dm")
	{
		if(tokens.size() != 2U || !DomainParseUint(tokens[1U], 10U, U32_MAX, code) || code == 0U)
			return false;
		forbiddenMasters.insert(static_cast<uint32_t>(code));
		return true;
	}
	if(tokens[0U] != "dm" || tokens.size() < 2U ||
	   !DomainParseUint(tokens[1U], 10U, U32_MAX, code) || code == 0U)
		return false;
	DomainEntry entry;
	for(std::size_t i = 2U; i < tokens.size(); i++)
	{
		const auto eq = tokens[i].find('=');
		if(eq == std::string_view::npos)
			return false;
		const auto key = tokens[i].substr(0U, eq);
		const auto items = DomainSplit(tokens[i].substr(eq + 1U), ",");
		uint64_t value = 0U;
		if(key == "a" || key == "r")
		{
			const bool isA = key == "a";
			if(items.size() > 1U)
				return false;
			if(!items.empty() && !DomainParseUint(items[0U], 16U, isA ? U32_MAX : U64_MAX, value))
				return false;
			if(isA)
				entry.attribute = static_cast<uint32_t>(value);
			else
				entry.race = value;
		}
		else if(key == "s")
		{
			entry.setcodes.clear();
			for(const auto item : items)
			{
				if(!DomainParseUint(item, 16U, 0xFFFFU, value))
					return false;
				if(value != 0U)
					entry.setcodes.push_back(static_cast<uint16_t>(value));
			}
		}
		else if(key == "n")
		{
			entry.names.clear();
			for(const auto item : items)
			{
				if(!DomainParseUint(item, 10U, U32_MAX, value))
					return false;
				entry.names.push_back(static_cast<uint32_t>(value));
			}
		}
		// Unknown keys are ignored.
	}
	auto SortUnique = [](auto& v)
	{
		std::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
	};
	SortUnique(entry.setcodes);
	SortUnique(entry.names);
	masters.emplace(static_cast<uint32_t>(code), std::move(entry));
	return true;
}

} // namespace Detail

template<typename Stream>
void ParseForBanlists(Stream& stream, BanlistMap& banlists)
{
	BanlistHash hash = Detail::BANLIST_HASH_MAGIC;
	bool whitelist = false;
	bool genesys = false;
	uint32_t genesysCap = 0U;
	bool inList = false;
	bool domain = false;
	Banlist::DictType dict;
	Banlist::PointsType points;
	Banlist::DomainType masters;
	Banlist::CodeSetType forbiddenMasters;
	auto ConditionallyAdd = [&]()
	{
		if(hash == Detail::BANLIST_HASH_MAGIC)
			return;
		auto banlist = std::make_shared<Banlist>(whitelist, std::move(dict),
			genesys, genesysCap, std::move(points), domain, std::move(masters),
			std::move(forbiddenMasters));
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
		// Domain data lines: only inside a Domain list (after its "$domain"
		// line); anywhere else they are comments. Every one of them is part of
		// the hash, even when it is malformed and skipped.
		if(domain && l.rfind("#@", 0U) == 0U)
		{
			const auto text = Detail::DomainLineText(l);
			hash ^= Detail::DomainLineSalt(text);
			Detail::ParseDomainLine(text, masters, forbiddenMasters);
			continue;
		}
		if(l.find("$whitelist") != std::string::npos)
		{
			whitelist = true;
			continue;
		}
		// "$genesys" and "$domain" only count inside a list, and only the first
		// of them in a list does.
		if(l.rfind("$genesys", 0U) == 0U)
		{
			if(!inList || genesys || domain)
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
		if(l.rfind("$domain", 0U) == 0U)
		{
			if(!inList || genesys || domain)
				continue;
			domain = true;
			hash ^= Detail::DOMAIN_SALT;
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
			inList = true;
			domain = false;
			dict.clear();
			points.clear();
			masters.clear();
			forbiddenMasters.clear();
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
