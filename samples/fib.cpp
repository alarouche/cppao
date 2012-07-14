// Naive Fibonacci calculation

#include <active/shared.hpp>
#include <active/promise.hpp>
#include <iostream>

typedef active::basic ao_type;

struct fib : public active::shared<fib, ao_type>, public active::sink<int>
{
	struct calculate
	{
		int value;
		active::sink<int>::sp result;
	};

	void active_method( calculate&&calculate )
	{
		if( calculate.value > 2 )
		{
			m_total=0;
			m_result = calculate.result;
			fib::calculate
				lhs = { calculate.value-1, shared_from_this() },
				rhs = { calculate.value-2, shared_from_this() };

			// Note: temporary AO destroyed only after its last message.
			(*std::make_shared<fib>())(lhs);
			(*std::make_shared<fib>())(rhs);
		}
		else
		{
			calculate.result->send(1);
		}
	}

	typedef int sub_result;

	void active_method( sub_result&&sub_result )
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
	auto result = std::make_shared<active::promise<int>>();
	fib::calculate calc = { atoi(argv[1]), result };
	(*std::make_shared<fib>())(calc);
	active::run();
	std::cout << "Result = " << result->get() << std::endl;
}
