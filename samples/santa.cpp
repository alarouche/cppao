/*	The "Santa Claus Problem"
	http://www.crsr.net/Notes/SantaClausProblem.html
	Santa repeatedly sleeps until wakened by either all of his nine reindeer,
	back from their holidays, or by a group of three of his ten elves.
	If awakened by the reindeer, he harnesses each of them to his sleigh, delivers
	toys with them and finally unharnesses them (allowing them to go off on holiday).
	If awakened by a group of elves, he shows each of the group into his study,
	consults with them on toy R&D and finally shows them each out (allowing them to
	go back to work). Santa should give priority to the reindeer in the case that there
	is both a group of elves and a group of reindeer waiting.

	This solution uses an active objects library for C++, cppao.

	Written by Calum Grant 16th April 2013.
  */

#include <active/object.hpp>
#include <active/advanced.hpp>	// For message priority

const int num_elves=10;
const int num_reindeer=9;
const int min_elves=3;

class Elf;
class Reindeer;

namespace active
{
	// Set the priority of a Reindeer
	template<> int priority(Reindeer* const&)
	{
		return 1;	// Higher priority than an Elf
	}
}

class Santa : public active::object<Santa, active::advanced>
{
public:
	// A reindeer has turned up
	void active_method(Reindeer* deer);

	// An elf has turned up
	void active_method(Elf* elf);

private:
	std::vector<Elf*> m_elves_waiting;
	std::vector<Reindeer*> m_deer_waiting;
};

class Elf : public active::object<Elf>
{
public:
	// Santa sends the elf on his way.
	void active_method(Santa* santa);
};

class Reindeer : public active::object<Reindeer>
{
public:
	// Santa sends the reindeer on holiday.
	void active_method(Santa* santa);
};

void Santa::active_method(Reindeer* deer)
{
	printf("Reindeer is back from holiday\n");
	m_deer_waiting.push_back(deer);
	if( m_deer_waiting.size() == num_reindeer )
	{
		printf("Delivering presents\n");
		for(std::vector<Reindeer*>::iterator d=m_deer_waiting.begin();
			d!=m_deer_waiting.end();
			++d)
		{
			(**d)(this);
		}
		m_deer_waiting.clear();
	}
}

void Santa::active_method(Elf* elf)
{
	printf("An elf turns up\n");
	m_elves_waiting.push_back(elf);
	if( m_elves_waiting.size() == min_elves )
	{
		for(std::vector<Elf*>::iterator e=m_elves_waiting.begin();
			e!=m_elves_waiting.end();
			++e)
		{
			printf("Consulting with elf\n");
			(**e)(this);	// Send it back to work
		}
		m_elves_waiting.clear();
	}
}

void Elf::active_method(Santa* santa)
{
	printf("Elf going back to work\n");
	(*santa)(this);	// Go see Santa again
}

void Reindeer::active_method(Santa* santa)
{
	printf("Reindeer is on holiday\n");
	(*santa)(this);	// Go back to Santa
}

int main()
{
	Santa santa;
	Elf elf[num_elves];
	Reindeer reindeer[num_reindeer];

	// Start off each elf
	for(int e=0; e<num_elves; ++e) elf[e](&santa);

	// Start off each reindeer
	for(int d=0; d<num_reindeer; ++d) reindeer[d](&santa);

	// Process all messages (never exits in this case).
	active::run();
}
