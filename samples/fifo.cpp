#include <active/object.hpp>
#include <active/synchronous.hpp>

#include <iostream>
#include <deque>

struct ready{};

template<typename T>
struct queue
{
	// Wait until push is possible.
	virtual void wait(active::sink<ready>*)=0;
	virtual void push(T value)=0;
	virtual void pop(active::sink<T>*)=0;
};


template<typename T>
class FIFO :
	public active::object<FIFO<T>,active::synchronous>,
	public queue<T>
{
public:
	FIFO(std::size_t capacity) : m_capacity(capacity), m_input(0), m_output(0)
	{
	}

	void push(T value)
	{
		(*this)(value);
	}

	void pop(active::sink<T>*p)
	{
		(*this)(p);
	}

	void wait(active::sink<ready>*w)
	{
		(*this)(w);
	}

	void active_method(T value)
	{
		if( m_queue.empty() && m_output )
		{
			m_output->send(value);
			m_output=0;
		}
		else
			m_queue.push_back(value);
	}

	// Source is ready to send: Send now or later.
	void active_method( active::sink<ready>* input )
	{
		if( m_queue.size() < m_capacity )
			input->send(ready());
		else
			m_input=input;
	}

	// Sink is ready to receive: send now or later
	void active_method( active::sink<T> * output )
	{
		if( !m_queue.empty() )
		{
			output->send(active::platform::move(m_queue.front()));
			m_queue.pop_front();
			if( m_input ) m_input->send(ready()), m_input=0;
		}
		else
		{
			m_output=output;
		}
	}

private:
	const std::size_t m_capacity;
	std::deque<T> m_queue;
	active::sink<ready> * m_input;
	active::sink<T> * m_output;
};


class Producer : public active::object<Producer>, public active::sink<ready>
{
public:
	Producer(int count, queue<int> & consumer) : m_count(count), m_consumer(consumer)
	{
		consumer.wait(this);
	}

	void active_method( ready )
	{
		m_consumer.push(m_count);
		if( --m_count>0 ) m_consumer.wait(this);
	}

private:
	int m_count;
	queue<int> & m_consumer;
};

class Consumer :
	public active::object<Consumer, active::basic>,
	public active::sink<int>
{
public:
	Consumer(int size, queue<int>&source) : m_source(source), m_counts(size)
	{
		m_source.pop(this);
	}

	void active_method(int i)
	{
		++m_counts[i%m_counts.size()];
		m_source.pop(this);
	}

	int get() const { return m_counts[0]; }

private:
	queue<int> & m_source;
	std::vector<int> m_counts;
};


int main()
{
	FIFO<int> fifo(100000);
	Producer producer(10000000, fifo);
	Consumer consumer(10000, fifo);
	active::run();
	std::cout << consumer.get();
}

