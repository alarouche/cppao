#include <active/object.hpp>
#include <iostream>
#include <vector>

class fork;
class philosopher;

struct pickup_left{};
struct pickup_right{};
struct eats{};
struct putdown_right{};
struct putdown_left{};
struct pickup{};
struct putdown{};

class report : public active::object<report>
{
	friend active::object<report>;
	
	void active_method(eats, int id)
	{
		std::cout << "Philosopher " << id << " eats\n";
	}
	
	void active_method(pickup_left, int id)
	{
		std::cout << "Philosopher " << id << " gets left fork\n";
	}

	void active_method(pickup_right, int id)
	{
		std::cout << "Philosopher " << id << " gets right fork\n";
	}

	void active_method(putdown_left, int id)
	{
		std::cout << "Philosopher " << id << " puts down left fork\n";
	}
	
	void active_method(putdown_right, int id)
	{
		std::cout << "Philosopher " << id << " puts down right fork\n";
	}
};

class fork : public active::object<fork>
{
	friend active::object<fork>;
	
	void active_method(pickup, philosopher * recipient);
	void active_method(putdown);
	
	bool available;
	philosopher * waiting;
public:
	fork() : available(true), waiting(0) { }
};


class philosopher : public active::object<philosopher>
{
	friend active::object<philosopher>;
	
	void active_method(int id, report * report, fork * left, fork * right)
	{
		m_id=id;
		m_report=report;
		m_left=left;
		m_right=right;
		m_forks=0;
		(*m_left)(pickup(),this);
		(*m_right)(pickup(),this);
	}
	
	void active_method(pickup,fork*f)
	{
		if(f==m_left) (*m_report)(pickup_left(),m_id);
		else (*m_report)(pickup_right(), m_id);
		
		if(++m_forks==2)
		{
			(*m_report)(eats(), m_id);
		
			(*m_left)(putdown());
			(*m_report)(putdown_left(), m_id);
			(*m_right)(putdown());
			(*m_report)(putdown_right(), m_id);

			// Start again
			m_forks=0;
			(*m_left)(pickup(),this);
			(*m_right)(pickup(),this);
		}
	}
	
	int m_id;
	fork *m_left, *m_right;
	report * m_report;
	int m_forks;	// How many forks we are holding
};

void fork::active_method(pickup, philosopher * recipient)
{
	if(available) available=false, (*recipient)(pickup(),this);
	else waiting = recipient;
}

void fork::active_method(putdown)
{
	if(waiting) (*waiting)(pickup(),this), waiting=0;
	else available=true;
}


int main(int argc, char**argv)
{
	const int N=argc>1 ? atoi(argv[1]) : 3;
	std::vector<philosopher> philosophers(N);
	std::vector<fork> forks(N);
	report report;
	
	for(int p=0; p<N; ++p)
	{
		philosophers[p](p+1, &report, &forks[p], &forks[(p+1)%N]);
	}
	
	active::run();
	std::cout << "Deadlocked! (this is expected)\n";
}