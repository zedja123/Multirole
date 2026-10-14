#include "Banlist.hpp"

namespace YGOPro
{

Banlist::Banlist(bool whitelist, DictType dict) noexcept :
	Banlist(whitelist, std::move(dict), false, 0U, {})
{}

Banlist::Banlist(bool whitelist, DictType dict, bool genesys,
                 uint32_t genesysCap, PointsType points) noexcept :
	whitelist(whitelist),
	dict(std::move(dict)),
	genesys(genesys),
	genesysCap(genesysCap),
	points(std::move(points))
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

} // namespace YGOPro
