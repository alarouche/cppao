// Naive Fibonacci calculation

#include <active/shared.hpp>
#include <active/promise.hpp>
#include <iostream>

#ifdef ACTIVE_USE_BOOST
#include <boost/make_shared.hpp>
#endif

typedef active::basic ao_type;

struct fib_result
{
	virtual void sub_result(int value)=0;
};

struct fib : public active::shared<fib, ao_type>, public fib_result
{
	void calculate(int value, active::platform::shared_ptr<fib_result> result)
	{
		active_fn([=]
		{
			if( value > 2 )
			{
				this->m_total=0;
				this->m_result = result;
				active::platform::make_shared<fib>()->calculate(value-1,shared_from_this());
				active::platform::make_shared<fib>()->calculate(value-2,shared_from_this());
			}
			else
			{
				result->sub_result(1);
			}
		});
	}

	void sub_result(int value)
	{
		active_fn([=]
		{
			if(this->m_total)
				this->m_result->sub_result(m_total+value);
			else
				this->m_total = value;
		});
	}

private:
	int m_total;
	active::platform::shared_ptr<fib_result> m_result;
};

struct display : public fib_result
{
	void sub_result(int i)
	{
		std::cout << "Result = " << i << std::endl;
	}
};

int main(int argc, char**argv)
{
	if( argc<2 ) { std::cout << "Usage: fib N\n"; return 1; }
	active::platform::make_shared<fib>()->calculate(atoi(argv[1]), active::platform::make_shared<display>());
	active::run();
}
