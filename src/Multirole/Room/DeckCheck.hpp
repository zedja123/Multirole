#ifndef ROOM_DECKCHECK_HPP
#define ROOM_DECKCHECK_HPP
#include <cstdint>
#include <optional>

#include "../STOCMsgFactory.hpp"
#include "../YGOPro/Banlist.hpp"
#include "../../ocgapi_types.h"

namespace YGOPro
{

class CardDatabase;
class Deck;

} // namespace YGOPro

namespace Ignis::Multirole::Room
{

// Deck checks shared by Context::CheckDeck and the Domain format (MASQPro).
// The Domain rules must stay identical to MASQPro's DeckManager (client) and
// to the reference implementation (LFListsZedja tools/domain_ref.py).
namespace DeckCheck
{

struct Failure
{
	Error::DeckOrCard type;
	uint32_t code;
};

using Result = std::optional<Failure>;

// Error for a card whose scope is not allowed by the room's allowed cards
// setting (YGOPro::AllowedCards), or std::nullopt.
std::optional<Error::DeckOrCard> ScopeError(uint32_t scope, uint8_t allowed) noexcept;

// Domain: the deck has a Deck Master that is a known monster card, not a
// Token nor a Skill (DOMAIN_DECK_MASTER), and, if content is checked, that is
// not forbidden as Deck Master (DOMAIN_FORBIDDEN_DM).
Result CheckDeckMaster(const YGOPro::Deck& deck, const YGOPro::Banlist& banlist,
                       const YGOPro::CardDatabase& cdb, bool checkContent) noexcept;

// Domain: content of a deck whose Deck Master passed CheckDeckMaster and
// whose sizes were checked. In this order, Deck Master included: forbidden
// types (Skills always), Rush and Legend cards, then card by card (Deck
// Master, Main, Extra, Side) unknown cards, allowed cards scope, a second copy
// of a card (by its alias if it has one) and the list's card lines, then the
// domain of every Main and Extra Deck card.
Result CheckContent(const YGOPro::Deck& deck, const YGOPro::Banlist& banlist,
                    const YGOPro::CardDatabase& cdb, int32_t forbiddenTypes,
                    uint8_t allowed) noexcept;

// Domain of a Deck Master: its "#@dm" line (own code, then alias), combined
// with its own attribute, race and archetypes.
YGOPro::DomainEntry GetDomain(const YGOPro::Banlist& banlist, const OCG_CardData& dm) noexcept;

// Whether a card may be in the Main or Extra Deck of a Deck Master with the
// given domain.
bool IsInDomain(const YGOPro::CardDatabase& cdb, const OCG_CardData& card,
                const YGOPro::DomainEntry& domain) noexcept;

} // namespace DeckCheck

} // namespace Ignis::Multirole::Room

#endif // ROOM_DECKCHECK_HPP
