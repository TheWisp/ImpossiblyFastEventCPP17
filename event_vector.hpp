#pragma once

namespace ifevec
{
	template<typename F> struct Event;

	struct ListenerBase
	{
		void* m_Event = nullptr;
		size_t m_idx;
	};

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
		};

		size_t m_Size = 0;
		size_t m_Capacity = 0;
		Callback* m_Callbacks = nullptr;
		ListenerBase** m_Listeners = nullptr;

		bool m_Calling = false; //represents if we are in the middle of calling back, no matter recursive or not. TODO use a single bit
		bool m_Dirty = false; //TODO use a single bit

		void add(void* object, CallbackFuncType* func, ListenerBase* pListener)
		{
			if (m_Size == m_Capacity)
			{
				//grow and copy
				size_t capacity = (m_Capacity * 3) / 2; //grow by 1.5
				if (!capacity) capacity = 2;
				Callback* callbacks = new Callback[capacity];
				ListenerBase** listeners = new ListenerBase * [capacity];

				for (size_t i = 0; i < m_Size; ++i)
				{
					callbacks[i] = m_Callbacks[i];
					listeners[i] = m_Listeners[i];
				}

				m_Capacity = capacity;
				delete[] m_Callbacks;
				m_Callbacks = callbacks;
				delete[] m_Listeners;
				m_Listeners = listeners;
			}

			m_Callbacks[m_Size].object = object;
			m_Callbacks[m_Size].func = func;
			m_Listeners[m_Size] = pListener;
			pListener->m_idx = m_Size++;
		}

		void remove(size_t idx)
		{
			m_Callbacks[idx].object = m_Callbacks[idx].func = nullptr;
			m_Listeners[idx] = nullptr;
			m_Dirty = true;
		}

		void replace(size_t idx, void* object, ListenerBase* listener)
		{
			m_Callbacks[idx].object = object;
			m_Listeners[idx] = listener;
		}

	public:

		~Event()
		{
			for (ListenerBase** l = m_Listeners, **end = m_Listeners + m_Size; l < end; ++l)
				if (*l) (*l)->m_Event = nullptr;
			delete[] m_Listeners;
			delete[] m_Callbacks;
		}

		template<typename ... ActualArgsT>
		void operator()(ActualArgsT&& ... args)
		{
			bool isRecursion = m_Calling;
			if (!m_Calling) m_Calling = true;

			//TODO: using index is necessary because of potential reallocation during callback. Can we make it faster? (yet another flag?)
			//TODO: alternatively, simply use another buffer for newly added ones during callback?
			for (size_t i = 0; i < m_Size; ++i)
			{
				auto& cb = m_Callbacks[i];
				if (cb.func)
					cb.func(cb.object, std::forward<ActualArgsT>(args)...);
			}

			if (!isRecursion)
			{
				m_Calling = false;

				if (m_Dirty)
				{
					m_Dirty = false;
					//remove all empty slots while patching the stored index in the listener
					size_t sz = 0;
					for (size_t i = 0; i < m_Size; ++i)
					{
						if (m_Listeners[i]) {
							m_Listeners[sz] = m_Listeners[i];
							m_Callbacks[sz] = m_Callbacks[i];
							m_Listeners[sz]->m_idx = sz;
							++sz;
						}
					}
					m_Size = sz;
				}
			}
		}
	};

	template<auto EventPtr, auto CallbackPtr> struct Listener;

	template<
		class EventClass, typename... EventFuncArgsT, Event<void(EventFuncArgsT...)> EventClass::* EventPtr,
		class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass::* CallbackPtr >
		struct Listener<EventPtr, CallbackPtr> : public ListenerBase
	{
		using EventType = Event<void(EventFuncArgsT...)>;

		CallbackClass* m_object = nullptr;

		Listener() = default;

		Listener(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			connect(eventSender, eventReceiver);
		}

		Listener(const Listener&) = delete;
		Listener(Listener&& other) : ListenerBase(other)
		{
			//fix the object
			if (other.m_object)
				m_object = (CallbackClass*)((size_t)this - ((size_t)& other - (size_t)other.m_object));

			//fix the links
			if (m_Event)
				static_cast<EventType*>(m_Event)->replace(m_idx, m_object, this);

			other.m_object = nullptr;
			other.m_Event = nullptr;
		}

		void connect(EventClass* eventSender, CallbackClass* eventReceiver)
		{
			m_object = eventReceiver;
			m_Event = &(eventSender->*EventPtr);
			static_cast<EventType*>(m_Event)->add(eventReceiver, call, this);
		}

		void disconnect()
		{
			if (m_Event) static_cast<EventType*>(m_Event)->remove(m_idx);
		}

		~Listener()
		{
			disconnect();
		}

		static void call(void* obj, EventFuncArgsT ... args)
		{
			(static_cast<CallbackClass*>(obj)->*CallbackPtr)(args...);
		}
	};
}
