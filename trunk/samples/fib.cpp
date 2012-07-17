// Naive Fibonacci calculation

#include <active/shared.hpp>
#include <active/promise.hpp>
#include <iostream>

#ifdef ACTIVE_USE_BOOST
	#include <boost/make_shared.hpp>
#endif

typedef active::basic ao_type;

struct fib : public active::shared<fib, ao_type>, public active::sink<int>
{
	struct calculate
	{
		int value;
		active::sink<int>::sp result;
	};

	void active_method( calculate calculate )
	{
		if( calculate.value > 2 )
		{
			m_total=0;
			m_result = calculate.result;
			fib::calculate
				lhs = { calculate.value-1, shared_from_this() },
				rhs = { calculate.value-2, shared_from_this() };

			// Note: temporary AO destroyed only after its last message.
			(*active::platform::make_shared<fib>())(lhs);
			(*active::platform::make_shared<fib>())(rhs);
		}
		else
		{
			calculate.result->send(1);
		}
	}

	void active_method( int sub_result )
	{
		if( m_total ) m_result->send(m_total+sub_result);
		else m_total = sub_result;
	}

private:
	int m_total;
	sp m_result;
};

int main(int argc, char**argv)
{
	if( argc<2 ) { std::cout << "Usage: fib N\n"; return 1; }
	active::platform::shared_ptr<active::promise<int> > result = active::platform::make_shared<active::promise<int> >();
	fib::calculate calc = { atoi(argv[1]), result };
	(*active::platform::make_shared<fib>())(calc);
	active::run();
	std::cout << "Result = " << result->get() << std::endl;
}
