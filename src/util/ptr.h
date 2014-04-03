// ptr.h:
// An instrusive reference-counted smart pointer implementation for Marmalade apps.
// Can create fairly robust smart pointers to any class that inherits from IRefCounted.
// Can create weak pointers to any class that inherits from IObservable.
//
// Created by the Get to Know Society
// Public domain

#pragma once

///// C++11 Null pointers are not implemented in GCC 4.4; only 4.6 /////
// So work around the issue using this cool code from the C++0x nullptr working paper:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2431.pdf
#if __GNUC__ < 5 && __GNUC_MINOR__ < 6 && !_MSC_VER && !__llvm__
const // this is a const object...
class nullptr_t {
public:
  template<class T> // convertible to any type
    operator T*() const // of null non-member
    { return 0; } // pointer...
  template<class C, class T> // or any type of null
    operator T C::*() const // member pointer...
    { return 0; }
private:
  void operator&() const; // whose address can't be taken
} nullptr = {}; // and whose name is nullptr
#else
typedef decltype(nullptr) nullptr_t;
#endif

// Any object that can be used with ObservingPtr<> must implement IObservable:
class IObservable {
public:
	IObservable() : m_firstObserver(nullptr) {}
	virtual ~IObservable() { SetObserversNull(); }
private:
	void* m_firstObserver; // A pointer to an ObservingPtr object
	template <class T> friend class ObservingPtr;
	inline void SetObserversNull();
};
// Any object that can be used with Ptr<> must implement IRefCounted:
class IRefCounted : public IObservable {
public:
	IRefCounted() : m_refCount(0) {}
	virtual ~IRefCounted() { }
	
private:
	unsigned int m_refCount;
	template <class T> friend class Ptr;
};


template <class ObjectType>
class Ptr {
public:
	Ptr(ObjectType* ptr) : m_ptr(ptr) { up(); }
	Ptr(nullptr_t ptr) : m_ptr(nullptr) {}
	Ptr() : m_ptr(nullptr) {}
	~Ptr() { down(); }
	Ptr(const Ptr& r) throw() { m_ptr = r.m_ptr; up(); }
	Ptr& operator=(const Ptr& r)
	{
		if (this != &r) {// protect against invalid self-assignment
			down();
			m_ptr = r.m_ptr;
			up();
		}
		return *this;
	}
	//ObjectType operator*() const throw() { return *m_ptr; }
	ObjectType* operator->() const throw() { return m_ptr; }
	ObjectType* ptr() const throw() { return m_ptr; }
	int count() const throw() { return m_ptr->m_refCount; }
	operator bool() const throw() { return m_ptr != nullptr; }
	bool operator ==(nullptr_t x) const throw() { return m_ptr == nullptr; }
	bool operator ==(Ptr x) const throw() { return x.m_ptr == m_ptr; }
	template <class SubclassedType> bool isOfType() { return dynamic_cast<SubclassedType*>(m_ptr) != nullptr; }
	template<class Subclass> Ptr(const Ptr<Subclass>& pSubclass) throw() : m_ptr(pSubclass.ptr()) { up(); } // Allow converting pointers to inherited classes
	// Comparator for use in std::map etc.
	struct Comp { bool operator() (const Ptr<ObjectType>& x, const Ptr<ObjectType>& y) const {return x.ptr() < y.ptr();} };
private:
	ObjectType* m_ptr;
	inline void up() const throw() { if (m_ptr) { m_ptr->m_refCount++; } }
	inline void down() const throw();/* { 
		if (m_ptr && --(m_ptr->m_refCount) == 0) { 
			while (ObservingPtr<ObjectType>* o = (ObservingPtr<ObjectType>*)m_ptr->m_firstObserver) { 
				o->m_ptr = nullptr;
				m_ptr->m_firstObserver = (void*)o->m_next;
			}
			delete m_ptr; 
		}
	}*/
};

template <class ObjectType>
class ObservingPtr {
	// These observing pointers do not increase the reference count and will be automagically set NULL
	// when the object they point to gets deleted.
public:
	ObservingPtr(ObjectType* ptr) : m_ptr(ptr), m_next(nullptr) { up(); }
	ObservingPtr(Ptr<ObjectType> ptr) : m_ptr(ptr.ptr()), m_next(nullptr) { up(); }
	ObservingPtr(nullptr_t ptr) : m_ptr(nullptr), m_next(nullptr) {}
	ObservingPtr() : m_ptr(nullptr), m_next(nullptr) {}
	~ObservingPtr() { down(); }
	ObservingPtr(const ObservingPtr& r) throw() : m_ptr(r.m_ptr), m_next(nullptr) { up(); }
	ObservingPtr& operator=(const ObservingPtr& r)
	{
		if (this != &r) {// protect against invalid self-assignment
			down();
			m_ptr = r.m_ptr;
			up();
		}
		return *this;
	}
	//ObjectType operator*() const throw() { return *m_ptr; }
	ObjectType* operator->() const throw() { return m_ptr; }
	ObjectType* ptr() const throw() { return m_ptr; }
	operator Ptr<ObjectType>() const throw() { return Ptr<ObjectType>(m_ptr); }
	operator bool() const throw() { return m_ptr != nullptr; }
	bool operator ==(nullptr_t x) const throw() { return m_ptr == nullptr; }
	bool operator !=(nullptr_t x) const throw() { return m_ptr != nullptr; }
	bool operator ==(Ptr<ObjectType> x) const throw() { return x.m_ptr == m_ptr; }
	bool operator ==(ObservingPtr x) const throw() { return x.m_ptr == m_ptr; }
	template <class SubclassedType> bool isOfType() { return dynamic_cast<SubclassedType*>(m_ptr) != nullptr; }
private:
	ObjectType* m_ptr;
	ObservingPtr* m_next;
	ObservingPtr* FirstObserver() const throw() { return static_cast<ObservingPtr*>(m_ptr->m_firstObserver); }
	inline void up() const throw() { if (m_ptr) { const_cast<ObservingPtr*>(this)->m_next = FirstObserver(); m_ptr->m_firstObserver = const_cast<void*>((void* const)this); } }
	inline void down() const throw() { 
		if (m_ptr && FirstObserver() == this)
			m_ptr->m_firstObserver = static_cast<void*>(m_next);
		else if (m_ptr) {
			ObservingPtr* i=FirstObserver();
			while (i->m_next!=this) { i=i->m_next; }
			i->m_next = this->m_next;
		}
	}
	friend void IObservable::SetObserversNull();
	friend class Ptr<ObjectType>;
};

template <class ObjectType>
inline void Ptr<ObjectType>::down() const throw() { 
	if (m_ptr && --(m_ptr->m_refCount) == 0)
		delete m_ptr; 
}

void IObservable::SetObserversNull() {
	// We've been deleted, so mark all observing pointers as NULL.
	// All Ptr<> objects are presumably gone.
	ObservingPtr<void>* o = (ObservingPtr<void>*)m_firstObserver;
	while (o) { 
		o->m_ptr = nullptr;
		o = o->m_next;
	}
}
