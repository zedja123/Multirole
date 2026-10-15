#include "Banlist.hpp"

namespace YGOPro
{

Banlist::Banlist(bool whitelist, DictType dict) noexcept :
	Banlist(whitelist, std::move(dict), false, 0U, {})
{}

Banlist::Banlist(bool whitelist, DictType dict, bool genesys,
                 uint32_t genesysCap, PointsType points) noexcept :
	Banlist(whitelist, std::move(dict), genesys, genesysCap, std::move(points),
	        false, {}, {})
{}

Banlist::Banlist(bool whitelist, DictType dict, bool genesys,
                 uint32_t genesysCap, PointsType points, bool domain,
                 DomainType masters, CodeSetType forbiddenMasters) noexcept :
	whitelist(whitelist),
	dict(std::move(dict)),
	genesys(genesys),
	genesysCap(genesysCap),
	points(std::move(points)),
	domain(domain),
	masters(std::move(masters)),
	forbiddenMasters(std::move(forbiddenMasters))
{}

bool Banlist::IsWhitelist() const noexcept
{
	return whitelist;
}

const Banlist::DictType& Banlist::Dict() const noexcept
{
	return dict;
}

bool Banlist::IsGenesys() const noexcept
{
	return genesys;
}

uint32_t Banlist::GenesysCap() const noexcept
{
	return genesysCap;
}

const Banlist::PointsType& Banlist::Points() const noexcept
{
	return points;
}

bool Banlist::IsDomain() const noexcept
{
	return domain;
}

const DomainEntry* Banlist::FindDomain(uint32_t code, uint32_t alias) const noexcept
{
	auto it = masters.find(code);
	if(it == masters.end() && alias != 0U)
		it = masters.find(alias);
	return it != masters.end() ? &it->second : nullptr;
}

bool Banlist::IsForbiddenMaster(uint32_t code, uint32_t alias) const noexcept
{
	return forbiddenMasters.count(code) != 0U ||
	       (alias != 0U && forbiddenMasters.count(alias) != 0U);
}

} // namespace YGOPro
