#include <active_object.hpp>
#include <iostream>


class Filter : public active::shared<Filter>
{
public:
    typedef int message;

    Filter(int p) : prime(p) { std::cout << p << "\n"; }

    ACTIVE_METHOD(message)
    {
        if(message % prime)
        {
            if(next)
                (*next)(message);
            else
                next = std::make_shared<Filter>(message);
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
            head = std::make_shared<Filter>(number);

        if( number < max ) (*this)(number+1);
    }
private:
    Filter::ptr head;
    const int max;
};

int main(int argc, char**argv)
{
    int max = argc<2 ? 1000000 : atoi(argv[1]);
    Source source(max);
    source(2);
    active::run();
}
