#include "DeckCheck.hpp"

#include <algorithm>
#include <array>
#include <map>

#include "../YGOPro/CardDatabase.hpp"
#include "../YGOPro/Constants.hpp"
#include "../YGOPro/Deck.hpp"
#include "../YGOPro/MsgCommon.hpp"

namespace Ignis::Multirole::Room::DeckCheck
{

namespace
{

constexpr uint32_t ATTRIBUTE_DIVINE = 0x40U;
constexpr uint64_t RACE_DIVINE = 0x200000U;

// Card archetype setcode against a domain archetype, as the core matches
// setcodes (card::match_setcode).
constexpr bool MatchSetcode(uint16_t archetype, uint16_t setcode) noexcept
{
	return (archetype & 0xFFFU) == (setcode & 0xFFFU) &&
	       (archetype & setcode) == archetype;
}

bool InArchetypes(const uint16_t* setcodes, const YGOPro::DomainEntry& domain) noexcept
{
	if(setcodes == nullptr)
		return false;
	for(; *setcodes != 0U; setcodes++)
		for(const auto archetype : domain.setcodes)
			if(MatchSetcode(archetype, *setcodes))
				return true;
	return false;
}

} // namespace

std::optional<Error::DeckOrCard> ScopeError(uint32_t scope, uint8_t allowed) noexcept
{
	using namespace YGOPro;
	// Unofficial cards not allowed.
	switch(allowed)
	{
	case ALLOWED_CARDS_OCG_ONLY:
	case ALLOWED_CARDS_TCG_ONLY:
	case ALLOWED_CARDS_OCG_TCG:
		if(scope > SCOPE_OCG_TCG)
			return Error::CARD_UNOFFICIAL;
		break;
	case ALLOWED_CARDS_WITH_PRERELEASE:
		if((scope & (~SCOPE_OFFICIAL)) != 0U)
			return Error::CARD_UNOFFICIAL;
		break;
	case ALLOWED_CARDS_OCG_CUSTOM:
		if((scope & SCOPE_OCG_CUSTOM) == 0U)
			return Error::CARD_UNOFFICIAL;
		break;
	case ALLOWED_CARDS_TCG_CUSTOM:
		if((scope & SCOPE_TCG_CUSTOM) == 0U)
			return Error::CARD_UNOFFICIAL;
		break;
	case ALLOWED_CARDS_OCG_TCG_CUSTOM:
		if((scope & SCOPE_OCG_TCG_CUSTOM) == 0U)
			return Error::CARD_UNOFFICIAL;
		break;
	case ALLOWED_CARDS_PRERELEASE_CUSTOM:
		if((scope & SCOPE_PRERELEASE_CUSTOM) == 0U)
			return Error::CARD_UNOFFICIAL;
		break;
	default:
		break;
	}
	// Prerelease cards not allowed.
	if(allowed == ALLOWED_CARDS_WITH_PRERELEASE && (scope & SCOPE_OFFICIAL) == 0U)
		return Error::CARD_UNOFFICIAL;
	// Only OCG cards allowed and the card is not OCG (it is TCG).
	if(allowed == ALLOWED_CARDS_OCG_ONLY && (scope & SCOPE_OCG) == 0U)
		return Error::CARD_TCG_ONLY;
	// Only TCG cards allowed and the card is not TCG (it is OCG).
	if(allowed == ALLOWED_CARDS_TCG_ONLY && (scope & SCOPE_TCG) == 0U)
		return Error::CARD_OCG_ONLY;
	return std::nullopt;
}

Result CheckDeckMaster(const YGOPro::Deck& deck, const YGOPro::Banlist& banlist,
                       const YGOPro::CardDatabase& cdb, bool checkContent) noexcept
{
	const uint32_t code = deck.Master();
	if(code == 0U)
		return Failure{Error::DOMAIN_DECK_MASTER, 0U};
	const auto& data = cdb.DataFromCode(code);
	if(data.code == 0U || (data.type & TYPE_MONSTER) == 0U ||
	   (data.type & (TYPE_TOKEN | TYPE_SKILL)) != 0U)
		return Failure{Error::DOMAIN_DECK_MASTER, code};
	if(checkContent && banlist.IsForbiddenMaster(code, data.alias))
		return Failure{Error::DOMAIN_FORBIDDEN_DM, code};
	return std::nullopt;
}

Result CheckContent(const YGOPro::Deck& deck, const YGOPro::Banlist& banlist,
                    const YGOPro::CardDatabase& cdb, int32_t forbiddenTypes,
                    uint8_t allowed) noexcept
{
	const YGOPro::CodeVector master{deck.Master()};
	const std::array<const YGOPro::CodeVector*, 4U> piles
	{
		&master,
		&deck.Main(),
		&deck.Extra(),
		&deck.Side()
	};
	// Skills are never allowed in a Domain deck.
	const auto forb = static_cast<uint32_t>(forbiddenTypes) | TYPE_SKILL;
	for(const auto* pile : piles)
		for(const auto code : *pile)
			if((cdb.DataFromCode(code).type & forb) != 0U)
				return Failure{Error::CARD_FORBIDDEN_TYPE, code};
	// Rush and Legend cards are outside every domain, whatever the allowed
	// cards setting.
	for(const auto* pile : piles)
		for(const auto code : *pile)
			if((cdb.ExtraFromCode(code).scope & (SCOPE_RUSH | SCOPE_LEGEND)) != 0U)
				return Failure{Error::DOMAIN_OUT_OF_DOMAIN, code};
	// Card by card: known, allowed, one copy (by alias, so "always treated
	// as" cards and alternate artworks count as the card they are), and the
	// list's card lines.
	const auto& dict = banlist.Dict();
	std::map<uint32_t, int32_t> counts;
	for(const auto* pile : piles)
	{
		for(const auto code : *pile)
		{
			const auto& data = cdb.DataFromCode(code);
			if(data.code == 0U)
				return Failure{Error::CARD_UNKNOWN, code};
			if(const auto error = ScopeError(cdb.ExtraFromCode(code).scope, allowed); error)
				return Failure{*error, code};
			const int32_t count = ++counts[data.alias != 0U ? data.alias : code];
			if(count > 1)
				return Failure{Error::DOMAIN_SINGLETON, code};
			// Card lines follow the alias, except whitelists, which follow
			// alternate artworks only (as Context::CheckDeck does).
			auto it = dict.find(code);
			if(it == dict.end() && data.alias != 0U)
			{
				constexpr uint32_t ID_MAX_DISTANCE_HALF = 10U;
				const bool inArtworkRange = (data.alias - code < ID_MAX_DISTANCE_HALF) ||
				                            (code - data.alias < ID_MAX_DISTANCE_HALF);
				if(!banlist.IsWhitelist() || inArtworkRange)
					it = dict.find(data.alias);
			}
			if((it != dict.end() && count > it->second) ||
			   (it == dict.end() && banlist.IsWhitelist()))
				return Failure{Error::CARD_BANLISTED, code};
		}
	}
	const auto domain = GetDomain(banlist, cdb.DataFromCode(deck.Master()));
	for(const auto* pile : {&deck.Main(), &deck.Extra()})
		for(const auto code : *pile)
			if(!IsInDomain(cdb, cdb.DataFromCode(code), domain))
				return Failure{Error::DOMAIN_OUT_OF_DOMAIN, code};
	return std::nullopt;
}

YGOPro::DomainEntry GetDomain(const YGOPro::Banlist& banlist, const OCG_CardData& dm) noexcept
{
	YGOPro::DomainEntry domain;
	if(const auto* entry = banlist.FindDomain(dm.code, dm.alias); entry != nullptr)
		domain = *entry;
	// The card data may be newer than the list: the Deck Master's own
	// attribute, race and archetypes always belong to its domain.
	domain.attribute |= dm.attribute;
	domain.race |= dm.race;
	if(dm.setcodes != nullptr)
		for(const auto* sc = dm.setcodes; *sc != 0U; sc++)
			domain.setcodes.push_back(*sc);
	std::sort(domain.setcodes.begin(), domain.setcodes.end());
	domain.setcodes.erase(std::unique(domain.setcodes.begin(), domain.setcodes.end()),
	                      domain.setcodes.end());
	return domain;
}

bool IsInDomain(const YGOPro::CardDatabase& cdb, const OCG_CardData& card,
                const YGOPro::DomainEntry& domain) noexcept
{
	// Spells and Traps are in every domain, other non-monsters in none.
	if((card.type & (TYPE_SPELL | TYPE_TRAP)) != 0U)
		return true;
	if((card.type & TYPE_MONSTER) == 0U)
		return false;
	// So are DIVINE monsters and Divine-Beasts.
	if((card.attribute & ATTRIBUTE_DIVINE) != 0U || (card.race & RACE_DIVINE) != 0U)
		return true;
	// Monsters named by the Deck Master.
	auto IsNamed = [&](uint32_t code)
	{
		return std::binary_search(domain.names.begin(), domain.names.end(), code);
	};
	if(IsNamed(card.code) || (card.alias != 0U && IsNamed(card.alias)))
		return true;
	// Archetypes of the card's own row and of its alias row.
	if(InArchetypes(card.setcodes, domain))
		return true;
	if(card.alias != 0U && InArchetypes(cdb.DataFromCode(card.alias).setcodes, domain))
		return true;
	return (card.attribute & domain.attribute) != 0U || (card.race & domain.race) != 0U;
}

} // namespace Ignis::Multirole::Room::DeckCheck
