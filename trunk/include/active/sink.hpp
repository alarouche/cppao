#ifndef ACTIVE_SINK_INCLUDED
#define ACTIVE_SINK_INCLUDED

#ifndef ACTIVE_OBJECT_INCLUDED
#include "object.hpp"
#endif

namespace active
{
#ifdef ACTIVE_USE_VARIADIC_TEMPLATES
    template<typename... Args>
	struct sink
	{
		typedef platform::shared_ptr<sink<Args...> > sp;
		typedef platform::weak_ptr<sink<Args...> > wp;
		virtual void send(Args...)=0;
	};
	
    template<typename Derived, typename... Args>
	struct handle : public sink<Args...>
	{
        void send(Args... args) { static_cast<Derived&>(*this)(args...); }
	};
#else
	template<typename A1=void, typename A2=void, typename A3=void,typename A4=void, typename A5=void>
    struct sink
    {
        typedef platform::shared_ptr<sink<A1,A2,A3,A4,A5> > sp;
        typedef platform::weak_ptr<sink<A1,A2,A3,A4,A5> > wp;
        virtual void send(A1,A2,A3,A4,A5)=0;
    };
	
	
	template<typename Derived, typename A1=void, typename A2=void, typename A3=void,typename A4=void, typename A5=void>
    struct handle : public sink<A1,A2,A3,A4,A5>
    {
        void send(A1 a1,A2 a2,A3 a3,A4 a4,A5 a5) { static_cast<Derived&>(*this)(a1,a2,a3,a4,a5); }
    };
	
	
    template<>
    struct sink<>
    {
        typedef platform::shared_ptr<sink<> > sp;
        typedef platform::weak_ptr<sink<> > wp;
        virtual void send()=0;
    };
	
    template<typename A1>
    struct sink<A1>
	{
        typedef platform::shared_ptr<sink<A1> > sp;
        typedef platform::weak_ptr<sink<A1> > wp;
        virtual void send(A1)=0;
	};
	
    template<typename A1, typename A2>
    struct sink<A1,A2>
    {
        typedef platform::shared_ptr<sink<A1,A2> > sp;
        typedef platform::weak_ptr<sink<A1,A2> > wp;
        virtual void send(A1,A2)=0;
    };
	
    template<typename A1, typename A2, typename A3>
    struct sink<A1,A2,A3>
    {
        typedef platform::shared_ptr<sink<A1,A2,A3> > sp;
        typedef platform::weak_ptr<sink<A1,A2,A3> > wp;
        virtual void send(A1,A2,A3)=0;
    };
	
    template<typename A1, typename A2, typename A3, typename A4>
    struct sink<A1,A2,A3,A4>
    {
        typedef platform::shared_ptr<sink<A1,A2,A3,A4> > sp;
        typedef platform::weak_ptr<sink<A1,A2,A3,A4> > wp;
        virtual void send(A1,A2,A3,A4)=0;
    };
	
    template<typename Derived>
    struct handle<Derived> : public sink<>
    {
        void send() { static_cast<Derived&>(*this)(); }
    };
	
    template<typename Derived, typename A1>
    struct handle<Derived,A1> : public sink<A1>
	{
        void send(A1 a1) { static_cast<Derived&>(*this)(a1); }
	};
	
    template<typename Derived, typename A1, typename A2>
    struct handle<Derived,A1,A2> : public sink<A1,A2>
    {
        void send(A1 a1,A2 a2) { static_cast<Derived&>(*this)(a1,a2); }
    };
	
    template<typename Derived, typename A1, typename A2, typename A3>
    struct handle<Derived,A1,A2,A3> : public sink<A1,A2,A3>
    {
        void send(A1 a1,A2 a2,A3 a3) { static_cast<Derived&>(*this)(a1,a2,a3); }
    };
	
    template<typename Derived, typename A1, typename A2, typename A3, typename A4>
    struct handle<Derived,A1,A2,A3,A4> : public sink<A1,A2,A3,A4>
    {
        void send(A1 a1,A2 a2,A3 a3,A4 a4) { static_cast<Derived&>(*this)(a1,a2,a3,a4); }
    };
	
#endif	
}

#endif