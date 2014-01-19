#ifndef UTILH
#define UTILH

//------------------------------------------------------------------------------
// File: ComBase.h
//------------------------------------------------------------------------------

/* Version of IUnknown that is renamed to allow a class to support both
non delegating and delegating IUnknowns in the same COM object */

#ifndef INONDELEGATINGUNKNOWN_DEFINED
DECLARE_INTERFACE(INonDelegatingUnknown)
{
	STDMETHOD(NonDelegatingQueryInterface) (THIS_ REFIID, LPVOID *) PURE;
	STDMETHOD_(ULONG, NonDelegatingAddRef)(THIS)PURE;
	STDMETHOD_(ULONG, NonDelegatingRelease)(THIS)PURE;
};
#define INONDELEGATINGUNKNOWN_DEFINED
#endif

typedef INonDelegatingUnknown *PNDUNKNOWN;


/* An object that supports one or more COM interfaces will be based on
this class. It supports counting of total objects for DLLCanUnloadNow
support, and an implementation of the core non delegating IUnknown */

class AM_NOVTABLE MyUnknown : public INonDelegatingUnknown
{
private:
	const LPUNKNOWN m_pUnknown; /* Owner of this object */

protected:                      /* So we can override NonDelegatingRelease() */
	volatile LONG m_cRef;       /* Number of reference counts */

public:

	MyUnknown();
	virtual ~MyUnknown() {};

	/* Return the owner of this object */

	LPUNKNOWN GetOwner() const {
		return m_pUnknown;
	};

	/* Non delegating unknown implementation */

	STDMETHODIMP NonDelegatingQueryInterface(REFIID, void **);
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();
	STDMETHODIMP_(ULONG) NonDelegatingRelease();
};

/*
#if (_MSC_VER <= 1200)
#pragma warning(disable:4211)

// The standard InterlockedXXX functions won't take volatiles
static inline LONG WINAPI InterlockedIncrement( volatile LONG * plong )
{ return InterlockedIncrement( const_cast<LONG*>( plong ) ); }

static inline LONG WINAPI InterlockedDecrement( volatile LONG * plong )
{ return InterlockedDecrement( const_cast<LONG*>( plong ) ); }

#pragma warning(default:4211)
#endif
*/

/* Return an interface pointer to a requesting client
performing a thread safe AddRef as necessary */

STDAPI GetInterface(LPUNKNOWN pUnk, void **ppv);


/* You must override the (pure virtual) NonDelegatingQueryInterface to return
interface pointers (using GetInterface) to the interfaces your derived
class supports (the default implementation only supports IUnknown) */

#define DECLARE_IUNKNOWN                                        \
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {      \
	return GetOwner()->QueryInterface(riid,ppv);            \
};                                                          \
	STDMETHODIMP_(ULONG) AddRef() {                             \
	return GetOwner()->AddRef();                            \
};                                                          \
	STDMETHODIMP_(ULONG) Release() {                            \
	return GetOwner()->Release();                           \
};






#undef ASSERT
#undef EXECUTE_ASSERT
#undef DbgBreak

#ifdef DEBUG

void WINAPI DbgBreakPoint(const TCHAR *pCondition, const TCHAR *pFileName, INT iLine);

#define ASSERT(_x_) if (!(_x_))         \
	DbgBreakPoint(TEXT(#_x_),TEXT(__FILE__),__LINE__)

//  Put up a message box informing the user of a halt
#define DbgBreak(_x_)                   \
	DbgBreakPoint(TEXT(#_x_),TEXT(__FILE__),__LINE__)

#define EXECUTE_ASSERT(_x_) ASSERT(_x_)

#else

#define ASSERT(_x_) ((void)0)
#define DbgBreak(_x_) ((void)0)
#define EXECUTE_ASSERT(_x_) ((void)(_x_))

#endif

#define CheckPointer(p) {if((p)==NULL) return E_POINTER;}

#endif