//NOTE: intended to be included multiple times

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
		std::cout << "Calling " << this << std::endl;
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

	//reentrance and modification safety
	ReentranceTest::test();

	//move
	MoveTest::test();
}
