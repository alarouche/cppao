// Naive fibonacci calculation

#include <active/shared.hpp>
#include <active/promise.hpp>

#include <iostream>

#include <active/advanced.hpp>
#include <active/fast.hpp>
typedef active::fast ao_type;

struct fib : public active::shared<fib, ao_type>, public active::sink<int>
{
	struct calculate
	{
		int value;
		active::sink<int>::sp result;
	};

	ACTIVE_METHOD( calculate )
	{
		if( calculate.value > 2 )
		{
			m_total=0;
			m_result = calculate.result;
			fib::calculate 
				lhs = { calculate.value-1, shared_from_this() }, 
				rhs = { calculate.value-2, shared_from_this() };
			
			// Note: AO destroyed after its last message.
			(*std::make_shared<fib>())(lhs);
			(*std::make_shared<fib>())(rhs);
		}
		else 
		{
			(*calculate.result)(1);
		}
	}
	
	typedef int sub_result;
	
	ACTIVE_METHOD( sub_result )
	{
		if( m_total ) (*m_result)(m_total+sub_result); //, m_result.reset();
		else m_total = sub_result;
	}
	
private:
	int m_total;
	sp m_result;
};

int main(int argc, char**argv)
{
	if( argc<2 ) {std::cout << "Usage: fib N\n"; return 1; }
	auto result = std::make_shared<active::promise<int>>();
	fib::calculate calc = { atoi(argv[1]), result };
	(*std::make_shared<fib>())(calc);
	active::run();
	std::cout << "Result = " << result->get() << std::endl;
}