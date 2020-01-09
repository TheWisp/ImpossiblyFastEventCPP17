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

namespace TestEventList
{
#include "Test.h"
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
}

namespace TestEventVector
{
	using ifevec::Event;
	using ifevec::Listener;
#include "Test.h"
}

namespace BenchEventList
{
#include "BenchEventVsCall.h"
}

namespace BenchEventVector
{
	using ifevec::Event;
	using ifevec::Listener;
#include "BenchEventVsCall.h"
}

int main()
{
	srand((unsigned)time(NULL));
	//TestEventList::test();
	//TestEventList::GroupingTest::test();
	TestEventVector::test();

	//simpleBench();

	std::cout << "\n";
	std::cout << "Benchmarking implementation using linked list:\n";
	BenchEventList::test();
	std::cout << "\n";
	std::cout << "Benchmarking implementation using vector:\n";
	BenchEventVector::test();
}


