/*  Prime number sieve demonstration in cppao.
    Construct a chain of prime numbers using active objects.
    http://code.google.com/p/cppao
    Written by Calum Grant
*/

#include <active_object.hpp>
#include <iostream>


class Prime : public active::shared<Prime>
{
public:
    Prime(int p) : prime(p) { std::cout << p << "\n"; }

    typedef int filter;

    ACTIVE_METHOD(filter)
    {
        if(filter % prime)
        {
            if(next)
                (*next)(filter);
            else
                next = std::make_shared<Prime>(filter);
        }
    }

    // The only reason this is needed is because the std::shared_ptr
    // causes a stack overflow with 1 million active objects!
    struct destroy {};

    ACTIVE_METHOD(destroy)
    {
        if(next)
        {
            (*next)(destroy);
            next.reset();
        }
    }

private:
    ptr next;
    const int prime;
};

class Source : public active::object
{
public:
    typedef int number;

    Source(int m) : max(m) { }

    ACTIVE_METHOD(number)
    {
        if(head)
            (*head)(number);
        else
            head = std::make_shared<Prime>(number);

        if( number < max )
            (*this)(number+1);
        else
            (*head)(Prime::destroy());
    }
private:
    Prime::ptr head;
    const int max;
};

int main(int argc, char**argv)
{
    int max = argc<2 ? 100000 : atoi(argv[1]);
    Source source(max);
    source(2);
    active::run();
}
