#include <iostream>
#include "event.hpp"

struct Foo
{
	Event<void(int)> evt;
};

Event<void(float)> g_evt2;

struct Bar
{
	void onEvt(int x)
	{
		std::cout << __PRETTY_FUNCTION__ << " " << x << " was called!\n";
	}

	void onEvt2(float delta)
	{
		std::cout << __PRETTY_FUNCTION__ << " " << delta << " was called!\n";
	}

	Listener<&Foo::evt, &Bar::onEvt> conn1;
	Listener<&g_evt2, &Bar::onEvt2> conn2;

	Bar(Foo& foo)
		: conn1(&foo, this)
		, conn2(this)
	{
	}
};

struct Baz
{
	void onEvt(int x)
	{
		std::cout << __PRETTY_FUNCTION__ << " " << x << " was called!\n";
	}

	Listener<&Foo::evt, &Baz::onEvt> conn;
};

int main()
{
	Baz baz1;
	{
		Foo foo;
		baz1.conn.connect(&foo, &baz1);
		{
			Baz baz2;
			baz2.conn.connect(&foo, &baz2);
			{
				Bar bar{ foo };
				foo.evt(3);
				g_evt2(3.14f);
			}
			foo.evt(42);
		}
		foo.evt(999);
	}
	//baz1 should not crash
}


