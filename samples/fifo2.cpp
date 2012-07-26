#include <active/advanced.hpp>
#include <iostream>

class Consumer : public  active::object<Consumer, active::advanced>
{
public:
	Consumer(int queue_size, int buckets) : m_counts(buckets)
	{
		set_capacity(queue_size);
	}

	void active_method(int i)
	{
		++m_counts[i%m_counts.size()];
	}

	int get() const { return m_counts[0]; }

private:
	std::vector<int> m_counts;
};

class Producer : public active::object<Producer>
{
public:
	void active_method(Consumer * con, int size)
	{
		for(int i=0; i<size; ++i)
			(*con)(i);
	}
};


int main()
{
	Consumer consumer(100000, 10000);
	Producer producer;
	producer( &consumer, 10000000 );
	active::run();
	std::cout << consumer.get();
}
