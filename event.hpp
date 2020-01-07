#pragma once
#include <vector>
#include <unordered_map>
#include <xmmintrin.h>


template<typename F> struct Event;
template<typename F> struct ListenerBase;
template<auto EventPtr, auto CallbackPtr> struct Listener;

template<typename ... A>
struct ListenerBase<void(A...)>
{
	using CallbackFuncType = void(void*, A...);
	void* object = nullptr;
	CallbackFuncType* func = nullptr;
	ListenerBase* prev = nullptr;
	ListenerBase* next = nullptr;
	Event<void(A...)>* event = nullptr;
};

template<typename ... A>
struct Event<void(A...)>
{
	using CallbackFuncType = void(void*, A...);
	using ListenerType = ListenerBase<void(A...)>;

	~Event() 
	{
		for (auto listener = head; listener != nullptr; )
		{
			auto next = listener->next;
			listener->event = nullptr;
			listener->prev = nullptr;
			listener->next = nullptr;
			listener = next;
		}
	}

	template<typename ... ActualArgsT>
	void operator()(ActualArgsT&&... args)
	{
		if (first_func) {
			first_func(first_object, std::forward<ActualArgsT>(args)...);
		}
		else if (head) // at least two
		{
			auto listener = head, next = head;
			for (auto pre_tail = tail->prev; listener != pre_tail; listener = next)
			{
				next = listener->next;
				//this prefetches the next next pointer
				_mm_prefetch((const char*)next->next, _MM_HINT_T0);

				//this prefetches next function and object
				_mm_prefetch((const char*)next->func, _MM_HINT_T0);
				_mm_prefetch((const char*)next->object, _MM_HINT_T0);
				listener->func(listener->object, std::forward<ActualArgsT>(args)...);
			}

			//now listener is the pre-tail
			{
				_mm_prefetch((const char*)tail->func, _MM_HINT_T0);
				_mm_prefetch((const char*)tail->object, _MM_HINT_T0);
				listener->func(listener->object, std::forward<ActualArgsT>(args)...);
			}

			tail->func(tail->object, std::forward<ActualArgsT>(args)...);
		}
	}

	//Caches the callback of head as a small-object optimization for single callback
	CallbackFuncType* first_func = nullptr; 
	void* first_object = nullptr;

	ListenerType* head = nullptr;
	ListenerType* tail = nullptr;
	std::unordered_map<typename ListenerType::CallbackFuncType*, ListenerType*> mapping_by_func;

	void add(ListenerType* listener)
	{
		if (mapping_by_func.find(listener->func) == mapping_by_func.end())
		{
			if (!head)
			{
				head = tail = listener;
				listener->prev = listener->next = nullptr;
				first_func = listener->func;
				first_object = listener->object;
			}
			else
			{
				tail->next = listener;
				listener->prev = tail;
				listener->next = nullptr;
				tail = listener;
				first_func = nullptr; //no longer single function
				first_object = nullptr;
			}
			mapping_by_func[listener->func] = listener;
		}
		else //found the listener group of the same callback
		{
			ListenerType*& groupedHead = mapping_by_func[listener->func];
			listener->next = groupedHead->next;
			listener->prev = groupedHead;
			if (groupedHead->next)
				groupedHead->next->prev = listener;
			groupedHead->next = listener;
			if (groupedHead == tail)
				tail = listener;
			groupedHead = listener;
			first_func = nullptr; //no longer single function
			first_object = nullptr;
		}
	}

	void remove(ListenerType* listener)
	{
		if (tail == listener)
			tail = listener->prev;
		if (head == listener) {
			head = listener->next;
			first_func = head && head == tail ? head->func : nullptr;
			first_object = head && head == tail ? head->object : nullptr;
		}
		if (mapping_by_func[listener->func] == listener)
		{
			if (listener->prev && listener->prev->func == listener->func)
				mapping_by_func[listener->func] = listener->prev;
			else mapping_by_func.erase(listener->func);
		}

		if (listener->prev)
			listener->prev->next = listener->next;
		if (listener->next)
			listener->next->prev = listener->prev;
	}

	void replace(ListenerType* from, ListenerType* to)
	{
		if (mapping_by_func[from->func] == from)
			mapping_by_func[from->func] = to;
		if (tail == from)
			tail = to;
		if (head == from) {
			head = to;
			first_func = head && head == tail ? head->func : nullptr;
			first_object = head && head == tail ? head->object : nullptr;
		}
		if (from->prev)
			from->prev->next = to;
	}
};

template<
	class EventClass, typename... EventFuncArgsT, Event<void(EventFuncArgsT...)> EventClass:: * EventPtr,
	class CallbackClass, typename CallbackFuncType, CallbackFuncType CallbackClass:: * CallbackPtr
>
struct Listener<EventPtr, CallbackPtr> : public ListenerBase<void(EventFuncArgsT...)>
{
	using Base = ListenerBase<void(EventFuncArgsT...)>;

	Listener() = default;

	Listener(EventClass* eventSender, CallbackClass* eventReceiver)
	{
		connect(eventSender, eventReceiver);
	}

	Listener(const Listener&) = delete;
	Listener(Listener&& other) : Base(other)
	{
		//fix the object
		if (other.object)
			this->object = (void*)((size_t)this - ((size_t)&other - (size_t)other.object));

		//fix the links
		if (this->event)
			this->event->replace(&other, this);
		other.object = nullptr;
		other.func = nullptr;
		other.prev = nullptr;
		other.next = nullptr;
		other.event = nullptr;
	}

	void connect(EventClass* eventSender, CallbackClass* eventReceiver)
	{
		//potential optimization: don't store event, and disregard the ordering within the same callback group
		//potential optimization: use another indirection instead of this->event, so that when event goes out of the scope, we wouldn't need to reset every listener
		this->object = eventReceiver;
		this->func = call;
		this->event = &(eventSender->*EventPtr);
		this->event->add(this);
	}

	void disconnect()
	{
		if (this->event) this->event->remove(this);
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