#ifndef YGOPRO_BANLIST_HPP
#define YGOPRO_BANLIST_HPP
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace YGOPro
{

// Domain format (MASQPro): the domain of a Deck Master, as given by its
// "#@dm" line in a Domain list.
struct DomainEntry
{
	uint32_t attribute{};
	uint64_t race{};
	std::vector<uint16_t> setcodes; // Sorted, unique, never 0.
	std::vector<uint32_t> names; // Codes of the monsters named in its text, sorted.
};

class Banlist final
{
public:
	using DictType = std::unordered_map<uint32_t /*code*/, int32_t /*count*/>;
	using PointsType = std::unordered_map<uint32_t /*code*/, uint32_t /*points*/>;
	using DomainType = std::unordered_map<uint32_t /*code*/, DomainEntry>;
	using CodeSetType = std::unordered_set<uint32_t /*code*/>;

	Banlist(bool whitelist, DictType dict) noexcept;
	Banlist(bool whitelist, DictType dict, bool genesys, uint32_t genesysCap,
	        PointsType points) noexcept;
	Banlist(bool whitelist, DictType dict, bool genesys, uint32_t genesysCap,
	        PointsType points, bool domain, DomainType masters,
	        CodeSetType forbiddenMasters) noexcept;

	bool IsWhitelist() const noexcept;
	const DictType& Dict() const noexcept;
	// Genesys format (MASQPro): point list with a total point cap.
	bool IsGenesys() const noexcept;
	uint32_t GenesysCap() const noexcept;
	const PointsType& Points() const noexcept;
	// Domain format (MASQPro): one Deck Master per deck, singleton, and every
	// card within the domain of the Deck Master.
	bool IsDomain() const noexcept;
	// "#@dm" entry of a Deck Master: its own code first, then its alias (any
	// alias, not only alternate artworks). nullptr when there is none.
	const DomainEntry* FindDomain(uint32_t code, uint32_t alias) const noexcept;
	// Whether a card is in the list's "#@forbid-dm" lines (own code or alias).
	bool IsForbiddenMaster(uint32_t code, uint32_t alias) const noexcept;
private:
	const bool whitelist;
	DictType dict;
	const bool genesys;
	const uint32_t genesysCap;
	PointsType points;
	const bool domain;
	DomainType masters;
	CodeSetType forbiddenMasters;
};

using BanlistPtr = std::shared_ptr<Banlist>;

} // namespace Multirole

#endif // YGOPRO_BANLIST_HPP
