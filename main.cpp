#include <vector>

template<typename F> struct Event;

template<typename R, typename ... A> 
struct Event<R(A...)>
{
private:
	template<auto, auto> friend struct Listener;

    using CallbackFuncType = R(void*, A...);

    struct Callback
    {
        void* object;
        CallbackFuncType* func;

		Callback(void* object, CallbackFuncType* func)
			: object(object)
			, func(func)
		{}
    };

    //TODO small buffer optimization
	//TODO use something else to make Event constexpr-able
    std::vector<Callback> m_Callbacks;

	std::vector<void**> m_Cleanups;

	//adds listener that needs to be notified at destruction time of event
    template<auto CallbackPtr, typename CallbackClassType>
    std::size_t add_listener(CallbackClassType* eventReceiver, void** cleanup)
    {
		m_Cleanups.push_back(cleanup);
		return add_listener<CallbackPtr, CallbackClassType>(eventReceiver);
    }

	//adds listener that doesn't need to be notified about event destruction
	template<auto CallbackPtr, typename CallbackClassType>
	std::size_t add_listener(CallbackClassType* eventReceiver)
	{
		m_Callbacks.emplace_back(eventReceiver, [](void* obj, A... args) -> R
		{
			(static_cast<CallbackClassType*>(obj)->*CallbackPtr)(args...);
		});
		return m_Callbacks.size() - 1;
	}
    
    void remove_listener(std::size_t callbackIndex)
    {
		m_Callbacks.erase(m_Callbacks.begin() + callbackIndex);
    }

public:
	~Event();

    template<typename ... ActualArgsT>
    void operator()(ActualArgsT... args)
    {
        for (auto &[object, func] : m_Callbacks)
        {
            func(object, args...);
        }
    }
};

template<auto EventPtr, auto CallbackPtr> struct Listener;

template<
	class EventClass, typename EventFuncType, Event<EventFuncType> EventClass::* EventPtr,
	class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass::* CallbackPtr
>
struct Listener<EventPtr, CallbackPtr>
{
	std::size_t m_CallbackIndex;
	EventClass* m_EventSender = nullptr;

	Listener() = default;

	Listener(EventClass* eventSender, CallbackClass* eventReceiver)
	{
		connect(eventSender, eventReceiver);
	}

    void connect(EventClass* eventSender, CallbackClass* eventReceiver)
    {
        m_EventSender = eventSender;
		m_CallbackIndex = (eventSender->*EventPtr).template add_listener<CallbackPtr>(eventReceiver, (void**)&m_EventSender);
    }
    
    ~Listener()
    {
        if (m_EventSender)
        {
			(m_EventSender->*EventPtr).remove_listener(m_CallbackIndex);
        }
    }
};

template<
	typename EventFuncType, Event<EventFuncType>* EventPtr/*just static*/,
	class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass::* CallbackPtr
>
struct Listener<EventPtr, CallbackPtr>
{
	std::size_t m_CallbackIndex;

	Listener(CallbackClass* eventReceiver)
	{
		m_CallbackIndex = EventPtr->template add_listener<CallbackPtr>(eventReceiver);
	}

	~Listener()
	{
		EventPtr->remove_listener(m_CallbackIndex);
	}
};

template<typename R, typename ...A>
inline Event<R(A...)>::~Event()
{
	for (void** cleanup : m_Cleanups)
	{
		*cleanup = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
#include <iostream>

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


