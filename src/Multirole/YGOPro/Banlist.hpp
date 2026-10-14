#ifndef YGOPRO_BANLIST_HPP
#define YGOPRO_BANLIST_HPP
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace YGOPro
{

class Banlist final
{
public:
	using DictType = std::unordered_map<uint32_t /*code*/, int32_t /*count*/>;
	using PointsType = std::unordered_map<uint32_t /*code*/, uint32_t /*points*/>;

	Banlist(bool whitelist, DictType dict) noexcept;
	Banlist(bool whitelist, DictType dict, bool genesys, uint32_t genesysCap,
	        PointsType points) noexcept;

	bool IsWhitelist() const noexcept;
	const DictType& Dict() const noexcept;
	// Genesys format (MASQPro): point list with a total point cap.
	bool IsGenesys() const noexcept;
	uint32_t GenesysCap() const noexcept;
	const PointsType& Points() const noexcept;
private:
	const bool whitelist;
	DictType dict;
	const bool genesys;
	const uint32_t genesysCap;
	PointsType points;
};

using BanlistPtr = std::shared_ptr<Banlist>;

} // namespace Multirole

#endif // YGOPRO_BANLIST_HPP
