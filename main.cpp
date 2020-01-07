#include <chrono>
#include <iostream>
#include "event.hpp"
#include "event_vector.hpp"
#include <functional>
#include <string>
#include <algorithm>
#include <cassert>
#include <memory>
#include <functional>
#include <array>

struct Foo
{
	Event<void(float)> evt;
};

struct Bar
{
	void onEvt(float delta)
	{
		total += delta;
	}

	Listener<&Foo::evt, &Bar::onEvt> listener;

	Bar(Foo& foo)
		: listener(&foo, this)
	{
	}

	volatile float total = 0.f;
};

Foo g_foo;
std::function<void(float)> g_func;


namespace ReentranceTest
{
	struct Foo
	{
		Event<void(int count)> evt;
	};

	struct Bar
	{
		void onEvt(int count);

		Listener<&Foo::evt, &Bar::onEvt> listener;
		Foo* m_foo;
		int x = 0;
		bool flag;

		Bar(Foo& foo, bool flag = true)
			: listener(&foo, this)
			, flag(flag)
		{
			m_foo = &foo;
		}
	};

	std::vector<std::unique_ptr<Bar>> dyn_listeners;

	void test()
	{
		dyn_listeners.clear();

		Foo foo;
		for (int i = 0, n = rand() % 10; i < n; i++)
			dyn_listeners.emplace_back(new Bar(foo, false));

		Bar bar(foo);

		foo.evt(3);
		assert(bar.x == 3);
	}

	inline void Bar::onEvt(int count)
	{
		if (flag) {
			if (count > 0) {
				this->x++;

				//purposely add or remove listeners
				if (rand() & 1)
					dyn_listeners.emplace_back(new Bar(*m_foo, false));
				else if (!dyn_listeners.empty())
					dyn_listeners.erase(dyn_listeners.begin() + rand() % dyn_listeners.size());
				m_foo->evt(count - 1);
			}
		}
	}
}

namespace MoveTest
{
	void test()
	{
		Foo foo;
		Bar bar(foo);
		Bar bar2 = std::move(bar);
		foo.evt(1);
		assert(bar2.total == 1);
	}
}

namespace GroupingTest
{
	template<int x>
	struct Bar
	{
		void onEvt(float f) {
			str += std::to_string(x);
		}

		Listener<&Foo::evt, &Bar::onEvt> listener;

		Bar(Foo& foo, std::string& str)
			: listener(&foo, this)
			, str(str)
		{

		}

		std::string& str;
	};

	void test()
	{
		Foo foo;
		std::string str;

		Bar<1> bar1(foo, str);
		{
			Bar<2> bar2(foo, str);
			{
				Bar<1> bar3(foo, str);
				Bar<2> bar4(foo, str);
				//bar2 and bar4 should be grouped together
				foo.evt(1);
				assert(str == "1122" || str == "2211");
			}
			str.clear();
			foo.evt(1);
			assert(str == "12" || str == "21");
		}
		str.clear();
		Bar<1> bar2(foo, str);
		foo.evt(1);
		assert(str == "11");
	}
}

void test()
{
	//basic usage
	{
		Foo foo;
		Bar bar(foo);
		foo.evt(3.14f);
		assert(bar.total == 3.14f);
		foo.evt(3.14f);
		assert(bar.total == 6.28f);
	}

	//default construct
	{
		Listener<&Foo::evt, &Bar::onEvt> listener;
		(void)listener;
	}

	//scope safety (listener)
	{
		Foo foo;
		{
			Bar bar1(foo), * bar3;
			{
				Bar bar2(foo);
				bar3 = new Bar(foo);
				foo.evt(1.f);
				assert(bar1.total == 1);
				assert(bar2.total == 1);
				assert(bar3->total == 1);
			}
			foo.evt(1.f);
			assert(bar1.total == 2);
			assert(bar3->total == 2);
		}
		foo.evt(1.f);
	}

	//grouping
	GroupingTest::test();

	//reentrance and modification safety
	ReentranceTest::test();

	//move
	MoveTest::test();
}

void simpleBench()
{
	Bar bar{ g_foo };

	//with event
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 1'000'000; ++i)
		{
			g_foo.evt(0.001f);
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> diff = end - start;
		std::cout << "Event: " << diff.count() << " s\n";
	}

	//direct call
	{
		auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 1'000'000; ++i)
		{
			bar.onEvt(0.001f);
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> diff = end - start;
		std::cout << "Call: " << diff.count() << " s\n";
	}

	//std::function
	{
		g_func = [&bar](float delta)
		{
			bar.onEvt(delta);
		};
		auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 1'000'000; ++i)
		{
			g_func(0.001f);
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> diff = end - start;
		std::cout << "std::function: " << diff.count() << " s\n";
	}
}

namespace benchEventVsCall_List
{
#include "BenchEventVsCall.h"
}

namespace benchEventVsCall_Vector
{
	using ifevec::Event;
	using ifevec::Listener;
#include "BenchEventVsCall.h"
}

int main()
{
	srand((unsigned)time(NULL));
	test();

	//simpleBench();

	std::cout << "\n";
	std::cout << "Benchmarking implementation using linked list:\n";
	benchEventVsCall_List::test();
	std::cout << "\n";
	std::cout << "Benchmarking implementation using vector:\n";
	benchEventVsCall_Vector::test();
}


