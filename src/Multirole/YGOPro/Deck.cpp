#include "Deck.hpp"

namespace YGOPro
{

Deck::Deck() noexcept :
	main(),
	extra(),
	side(),
	error(0),
	master(0)
{}

Deck::Deck(CodeVector&& m, CodeVector&& e, CodeVector&& s, uint32_t err)  noexcept:
	Deck(std::move(m), std::move(e), std::move(s), err, 0U)
{}

Deck::Deck(CodeVector&& m, CodeVector&& e, CodeVector&& s, uint32_t err,
           uint32_t master) noexcept :
	main(std::move(m)),
	extra(std::move(e)),
	side(std::move(s)),
	error(err),
	master(master)
{}

const CodeVector& Deck::Main() const noexcept
{
	return main;
}

const CodeVector& Deck::Extra() const noexcept
{
	return extra;
}

const CodeVector& Deck::Side() const noexcept
{
	return side;
}

uint32_t Deck::Error() const noexcept
{
	return error;
}

uint32_t Deck::Master() const noexcept
{
	return master;
}

CodeMap Deck::GetCodeMap() const noexcept
{
	CodeMap map;
	auto AddToMap = [&map](const CodeVector& from)
	{
		for(auto code : from)
			map[code]++;
	};
	AddToMap(main);
	AddToMap(extra);
	AddToMap(side);
	return map;
}

} // namespace YGOPro
