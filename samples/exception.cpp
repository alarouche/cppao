#include <active/object.hpp>
#include <iostream>

struct ex_object : public active::object<ex_object>
{
	struct msg {};

	struct the_unthinkable : public std::runtime_error
	{
		the_unthinkable() : std::runtime_error("This could never happen") { }
	};

	void active_method(msg)
	{
		throw the_unthinkable();
	}

	void exception_handler() throw()
	{
		try
		{
			throw;
		}
		catch(the_unthinkable&)
		{
			std::clog << "The unthinkable has happened!\n";
		}
		catch(...)
		{
			// Invoke default handler
			active::basic::exception_handler();
		}
	}
};

int main()
{
	ex_object obj;
	obj(ex_object::msg());
	active::run();
}
