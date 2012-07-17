/*  Prime number sieve demonstration in cppao.
 Construct a chain of prime numbers using active objects.
 http://code.google.com/p/cppao
 Written by Calum Grant
 */

#include <active/shared.hpp>
#include <iostream>

class Prime : public active::shared<Prime>
{
public:
	Prime(int p) : prime(p) { std::cout << p << "\n"; }

	void filter(int i)
	{
		active_fn( [=]
		{
			if( i % prime )
			{
				if(next) next->filter(i);
				else next.reset( new Prime(i) );
			}
		} );
	}

	void destroy()
	{
		active_fn([=]{next->destroy();next.reset();});
	}

private:
	ptr next;
	const int prime;
};

class Source : public active::object<Source>
{
public:
	Source(int m) : max(m) { }

	void source(int number)
	{
		active_fn( [=]
		{
			if(head)
				head->filter(number);
			else
				head.reset( new Prime(number) );
			if(number<max)
				source(number+1);
			else
				head->destroy();
		} );
	}

private:
	Prime::ptr head;
	const int max;
};

int main(int argc, char**argv)
{
	int max = argc<2 ? 100000 : atoi(argv[1]);
	Source source(max);
	source.source(2);
	active::run();
}
