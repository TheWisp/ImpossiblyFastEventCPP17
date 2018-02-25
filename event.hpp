#pragma once
#include <vector>
#include <variant>

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

	std::variant<std::nullptr_t, Callback, std::vector<Callback>> m_Callbacks;

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
		Callback cb = {
			eventReceiver,
			[](void* obj, A... args) -> R
			{
				return (static_cast<CallbackClassType*>(obj)->*CallbackPtr)(args...);
			}
		};

		if (m_Callbacks.index() == 0)
		{
			m_Callbacks.template emplace<1>(cb);
		}
		else if (m_Callbacks.index() == 1)
		{
			Callback single = std::get<1>(m_Callbacks);
			m_Callbacks.template emplace<2>();
			std::get<2>(m_Callbacks).push_back(single);
			std::get<2>(m_Callbacks).push_back(cb);
		}
		else
		{
			std::get<2>(m_Callbacks).push_back(cb);
		}
	}

	void remove_listener(std::size_t callbackIndex)
	{
		if (m_Callbacks.index() == 1)
		{
			m_Callbacks.template emplace<0>();
		}
		else if (m_Callbacks.index() == 2)
		{
			auto& vec = std::get<2>(m_Callbacks);
			vec.erase(vec.begin() + callbackIndex);
			if (vec.size() == 1)
			{
				m_Callbacks.template emplace<1>(vec[0]);
			}
		}
	}

public:

	~Event();

	template<typename ... ActualArgsT>
	void operator()(ActualArgsT&&... args)
	{
		if (m_Callbacks.index() == 1)
		{
			auto& cb = std::get<1>(m_Callbacks);
			cb.func(cb.object, std::forward<ActualArgsT>(args)...);
		}
		else if (m_Callbacks.index() == 2)
		{
			for (auto &[object, func] : std::get<2>(m_Callbacks))
			{
				func(object, std::forward<ActualArgsT>(args)...);
			}
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
		//TODO problem is m_CallbackIndex can be invalidated by other deletion!
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
