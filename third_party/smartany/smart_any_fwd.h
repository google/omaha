//+---------------------------------------------------------------------------
//
//  Copyright ( C ) Microsoft, 2002.
//
//  File:       smart_any_fwd.h
//
//  Contents:   automatic resource management
//
//  Classes:    auto_any, scoped_any and shared_any
//
//  Functions:  get
//              reset
//              release
//              valid
//              address
//
//  Author:     Eric Niebler ( ericne@microsoft.com )
//
//----------------------------------------------------------------------------

#ifndef SMART_ANY_FWD
#define SMART_ANY_FWD

#ifdef _MANAGED
#pragma warning( push )
#pragma warning( disable : 4244 )
#include <vcclr.h>
#pragma warning( pop )
#endif

// Check to see if partial template specialization is available
#if _MSC_VER >= 1310
#define SMART_ANY_PTS
#endif

// forward declare some invalid_value policy classes
struct null_t;

template<typename T,T value = T(0)>
struct value_const;

struct close_release_com;

//
// TEMPLATE CLASS auto_any
//
template<typename T,class close_policy,class invalid_value = null_t,int unique = 0>
class auto_any;

// return wrapped resource
template<typename T,class close_policy,class invalid_value,int unique>
T  get( auto_any<T,close_policy,invalid_value,unique> const & t );

// return true if the auto_any contains a currently valid resource
template<typename T,class close_policy,class invalid_value,int unique>
bool valid( auto_any<T,close_policy,invalid_value,unique> const & t );

// return wrapped resource and give up ownership
template<typename T,class close_policy,class invalid_value,int unique>
T release( auto_any<T,close_policy,invalid_value, unique> & t );

// destroy designated object
template<typename T,class close_policy,class invalid_value,int unique>
void reset( auto_any<T,close_policy,invalid_value,unique> & t );

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique,typename U>
void reset( auto_any<T,close_policy,invalid_value,unique> & t, U newT );

// swap the contents of two shared_any objects
template<typename T,class close_policy,class invalid_value,int unique>
void swap( auto_any<T,close_policy,invalid_value,unique> & left,
           auto_any<T,close_policy,invalid_value,unique> & right );

// return the address of the wrapped resource
// WARNING: this will assert if the value of the resource is
// anything other than invalid_value.
//template<typename T,class close_policy,class invalid_value,int unique>
//T* address( auto_any<T,close_policy,invalid_value,unique> & t );

//
// TEMPLATE CLASS shared_any
//
template<typename T,class close_policy,class invalid_value = null_t,int unique = 0>
class shared_any;

// return wrapped resource
template<typename T,class close_policy,class invalid_value,int unique>
T  get( shared_any<T,close_policy,invalid_value,unique> const & t );

// return true if the auto_any contains a currently valid resource
template<typename T,class close_policy,class invalid_value,int unique>
bool valid( shared_any<T,close_policy,invalid_value,unique> const & t );

// destroy designated object
template<typename T,class close_policy,class invalid_value,int unique>
void reset( shared_any<T,close_policy,invalid_value,unique> & t );

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique,typename U>
void reset( shared_any<T,close_policy,invalid_value,unique> & t, U newT );

// swap the contents of two shared_any objects
template<typename T,class close_policy,class invalid_value,int unique>
void swap( shared_any<T,close_policy,invalid_value,unique> & left,
           shared_any<T,close_policy,invalid_value,unique> & right );


//
// TEMPLATE CLASS scoped_any
//
template<typename T,class close_policy,class invalid_value = null_t,int unique = 0>
class scoped_any;

// return wrapped resource
template<typename T,class close_policy,class invalid_value,int unique>
T  get( scoped_any<T,close_policy,invalid_value,unique> const & t );

// return true if the auto_any contains a currently valid resource
template<typename T,class close_policy,class invalid_value,int unique>
bool valid( scoped_any<T,close_policy,invalid_value,unique> const & t );

// return wrapped resource and give up ownership
template<typename T,class close_policy,class invalid_value,int unique>
T release( scoped_any<T,close_policy,invalid_value, unique> & t );

// destroy designated object
template<typename T,class close_policy,class invalid_value,int unique>
void reset( scoped_any<T,close_policy,invalid_value,unique> & t );

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique,typename U>
void reset( scoped_any<T,close_policy,invalid_value,unique> & t, U newT );

// return the address of the wrapped resource
// WARNING: this will assert if the value of the resource is
// anything other than invalid_value.
//template<typename T,class close_policy,class invalid_value,int unique>
//T* address( scoped_any<T,close_policy,invalid_value,unique> & t );

// close policy for objects allocated with new
struct close_delete;

namespace detail
{
    typedef char (&yes)[1];
    typedef char (&no) [2];

    struct dummy_struct
    {
        void dummy_method() {}
    };

    typedef void (dummy_struct::*safe_bool)();
    safe_bool const safe_true  = &dummy_struct::dummy_method;
    safe_bool const safe_false = 0;

    // Because of older compilers, we can't always use
    // null_t when we would like to.
    template<class invalid_value>
    struct fixup_invalid_value
    {
        template<typename> struct rebind { typedef invalid_value type; };
    };

    // for compile-time assertions
    template<bool>
    struct static_assert;

    template<>
    struct static_assert<true>
    {
        static_assert() {}
    };

    template<typename T>
    struct static_init
    {
        static T const value;
    };

    template<typename T>
    T const static_init<T>::value = T();

    template<bool>
    struct null_helper // unmanaged
    {
        template<typename T>
        struct inner
        {
            static T const get()
            {
                return static_init<T>::value;
            }
        };
    };

    template<>
    struct null_helper<true> // managed
    {
        template<typename T>
        struct inner
        {
            static T const get()
            {
                return 0;
            }
        };
    };

    typedef char (&yes_t)[1];
    typedef char (& no_t)[2];

    template<bool>
    struct select_helper
    {
        template<typename T,typename>
        struct inner { typedef T type; };
    };

    template<>
    struct select_helper<false>
    {
        template<typename,typename U>
        struct inner { typedef U type; };
    };

    template<bool F,typename T,typename U>
    struct select
    {
        typedef typename select_helper<F>::template inner<T,U>::type type;
    };


    template< bool >
    struct holder_helper
    {
        template<typename T>
        struct inner
        {
            typedef T type;
        };
    };

    template< typename T >
    struct remove_ref
    {
        typedef T type;
    };

    #ifdef SMART_ANY_PTS
    template< typename T >
    struct remove_ref<T&>
    {
        typedef T type;
    };
    #endif

    template<typename T>
    T* address_of( T & v )
    {
        return reinterpret_cast<T*>(
            &const_cast<char&>(
                reinterpret_cast<char const volatile &>(v)));
    }

    #ifndef _MANAGED

    template<typename T>
    struct is_managed
    {
        static bool const value = false;
    };

    #else

    struct managed_convertible
    {
        managed_convertible( System::Object const volatile __gc* );
        managed_convertible( System::Enum const volatile __gc* );
        managed_convertible( System::ValueType const volatile __gc* );
        managed_convertible( System::Delegate const volatile __gc* );
    };

    template<typename T>
    struct is_managed
    {
    private:
        static yes_t check( managed_convertible );
        static no_t __cdecl check( ... );
        static typename remove_ref<T>::type & make();
    public:
        static bool const value = sizeof( yes_t ) == sizeof( check( make() ) );
    };

    #ifdef SMART_ANY_PTS
    template<typename T>
    struct is_managed<T __gc&>
    {
        static bool const value = true;
    };
    template<typename T>
    struct is_managed<T __gc*>
    {
        static bool const value = true;
    };
    template<typename T>
    struct is_managed<T __gc*const>
    {
        static bool const value = is_managed<T __gc*>::value;
    };
    template<typename T>
    struct is_managed<T __gc*volatile>
    {
        static bool const value = is_managed<T __gc*>::value;
    };
    template<typename T>
    struct is_managed<T __gc*const volatile>
    {
        static bool const value = is_managed<T __gc*>::value;
    };
    #endif
    template<>
    struct is_managed<System::Void __gc*>
    {
        static bool const value = true;
    };
    template<>
    struct is_managed<System::Void const __gc*>
    {
        static bool const value = true;
    };
    template<>
    struct is_managed<System::Void volatile __gc*>
    {
        static bool const value = true;
    };
    template<>
    struct is_managed<System::Void const volatile __gc*>
    {
        static bool const value = true;
    };

    template<>
    struct holder_helper<true>
    {
        template<typename T>
        struct inner
        {
            typedef gcroot<T> type;
        };
    };
    #endif

    template<typename T>
    struct holder
    {
        typedef typename holder_helper<is_managed<T>::value>::template inner<T>::type type;
    };

    template<typename T>
    struct is_delete
    {
        static bool const value = false;
    };

    template<>
    struct is_delete<close_delete>
    {
        static bool const value = true;
    };

    // dummy type, don't define
    struct smart_any_cannot_dereference;

    // For use in implementing unary operator*
    template<typename T>
    struct deref
    {
        typedef smart_any_cannot_dereference type; // will cause a compile error by default
    };

    #ifndef SMART_ANY_PTS

    // Old compiler needs extra help
    template<>
    struct fixup_invalid_value<null_t>
    {
        template<typename T> struct rebind { typedef value_const<T> type; };
    };

    #else

    template<typename T,typename U>
    struct same_type
    {
        static const bool value = false;
    };

    template<typename T>
    struct same_type<T,T>
    {
        static const bool value = true;
    };

    // Handle reference types
    template<typename T>
    struct deref<T&>
    {
        typedef typename deref<T>::type type;
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T*>
    {
        typedef T& type; // The result of dereferencing a T*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T*const>
    {
        typedef typename deref<T*>::type type; // The result of dereferencing a T*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T*volatile>
    {
        typedef typename deref<T*>::type type; // The result of dereferencing a T*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T*const volatile>
    {
        typedef typename deref<T*>::type type; // The result of dereferencing a T*
    };

    // Fully specialize for void*
    template<>
    struct deref<void*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a void*
    };

    // Fully specialize for void const*
    template<>
    struct deref<void const*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a void*
    };

    // Fully specialize for void volatile*
    template<>
    struct deref<void volatile*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a void*
    };

    // Fully specialize for void const volatile*
    template<>
    struct deref<void const volatile*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a void*
    };

    #ifdef _MANAGED
    // Handle reference types
    template<typename T>
    struct deref<T __gc&>
    {
        typedef typename deref<T>::type type;
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T __gc*>
    {
        typedef T __gc& type; // The result of dereferencing a T __gc*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T __gc*const>
    {
        typedef typename deref<T __gc*>::type type; // The result of dereferencing a T __gc*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T __gc*volatile>
    {
        typedef typename deref<T __gc*>::type type; // The result of dereferencing a T __gc*
    };

    // Partially specialize for pointer types
    template<typename T>
    struct deref<T __gc*const volatile>
    {
        typedef typename deref<T __gc*>::type type; // The result of dereferencing a T __gc*
    };

    // Fully specialize for void*
    template<>
    struct deref<System::Void __gc*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a System::Void __gc*
    };

    // Fully specialize for void const*
    template<>
    struct deref<System::Void const __gc*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a System::Void __gc*
    };

    // Fully specialize for void volatile*
    template<>
    struct deref<System::Void volatile __gc*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a System::Void __gc*
    };

    // Fully specialize for void const volatile*
    template<>
    struct deref<System::Void const volatile __gc*>
    {
        typedef smart_any_cannot_dereference type; // cannot dereference a System::Void __gc*
    };
    #endif

    // The DECLARE_HANDLE macro in winnt.h defines a handle to be a pointer
    // to a struct containing one member named "unused" of type int. We can
    // use that information to make auto_any safer by disallowing actions like
    // dereferencing a handle or calling delete on a handle.
    template<typename T>
    struct has_unused
    {
    private:
        template<class U,int U::*> struct wrap_t;
        template<typename U> static yes_t check( wrap_t<U,&U::unused>* );
        template<typename U> static no_t  __cdecl check( ... );
    public:
        static bool const value = ( sizeof(check<T>(0)) == sizeof(yes_t) );
    };

    template<typename T>
    struct is_handle_helper
    {
        static bool const value = ( sizeof(T)==sizeof(int) && has_unused<T>::value );
    };

    #ifdef _MANAGED
    template<typename T>
    struct is_handle_helper<T __gc&>
    {
        static bool const value = false;
    };
    #endif

    template<>
    struct is_handle_helper<smart_any_cannot_dereference>
    {
        static bool const value = false;
    };

    // used to see whether a given type T is a handle type or not.
    template<typename T>
    struct is_handle
    {
    private:
        typedef typename remove_ref<typename deref<T>::type>::type deref_t;
    public:
        static bool const value =
            ( same_type<T,void*>::value || is_handle_helper<deref_t>::value );
    };
    #endif

    template<typename T,class close_policy>
    struct safe_types
    {
        typedef T pointer_type;
        typedef typename deref<T>::type reference_type;

        static pointer_type to_pointer( T t )
        {
            return t;
        }
        static reference_type to_reference( T t )
        {
            return *t;
        }
    };

    #ifdef SMART_ANY_PTS
    template<typename T>
    class no_addref_release : public T
    {
        unsigned long __stdcall AddRef();
        unsigned long __stdcall Release();
    };

    // shouldn't be able to call AddRef or Release
    // through a smart COM wrapper
    template<typename T>
    struct safe_types<T*,close_release_com>
    {
        typedef no_addref_release<T>* pointer_type;
        typedef no_addref_release<T>& reference_type;

        static pointer_type to_pointer( T* t )
        {
            return static_cast<pointer_type>( t );
        }
        static reference_type to_reference( T* t )
        {
            return *static_cast<pointer_type>( t );
        }
    };
    #endif
}

// a generic close policy that uses a ptr to a function
template<typename Fn, Fn Pfn>
struct close_fun
{
    template<typename T>
    static void close( T t )
    {
        Pfn( t );
    }
};

// free an object allocated with new by calling delete
struct close_delete
{
    template<typename T>
    static void close( T * p )
    {
        // This will fail only if T is an incomplete type.
        static detail::static_assert<0 != sizeof( T )> const cannot_delete_an_incomplete_type;

        #ifdef SMART_ANY_PTS
        // This checks to make sure we're not calling delete on a HANDLE
        static detail::static_assert<!detail::is_handle<T*>::value> const cannot_delete_a_handle;
        #endif

        delete p;
    }

    #ifdef _MANAGED
    template<typename T>
    static void close( gcroot<T __gc*> const & p )
    {
        delete static_cast<T __gc*>( p );
    }
    #endif
};

// free an array allocated with new[] by calling delete[]
struct close_delete_array
{
    template<typename T>
    static void close( T * p )
    {
        // This will fail only if T is an incomplete type.
        static detail::static_assert<0 != sizeof( T )> const cannot_delete_an_incomplete_type;

        #ifdef SMART_ANY_PTS
        // This checks to make sure we're not calling delete on a HANDLE
        static detail::static_assert<!detail::is_handle<T*>::value> const cannot_delete_a_handle;
        #endif

        delete [] p;
    }

    //#ifdef _MANAGED
    // This is broken because of compiler bugs
    //template<typename T>
    //static void close( gcroot<T __gc* __gc[]> const & p )
    //{
    //    delete [] static_cast<T __gc* __gc[]>( p );
    //}
    //#endif
};

// for releasing a COM object
struct close_release_com
{
    template<typename T>
    static void close( T p )
    {
        p->Release();
    }
};

// for releasing a __gc IDisposable object
struct close_dispose
{
    template<typename T>
    static void close( T p )
    {
        p->Dispose();
    }
};

// some generic invalid_value policies

struct null_t
{
    template<typename T>
    operator T const() const
    {
        return detail::null_helper<detail::is_managed<T>::value>::template inner<T>::get();
    }
};

template<typename T,T value>
struct value_const
{
    operator T const() const
    {
        return value;
    }
};

template<typename T,T const* value_ptr>
struct value_const_ptr
{
    operator T const&() const
    {
        return *value_ptr;
    }
};

#ifdef SMART_ANY_PTS
template<typename T, T const& value>
struct value_ref
{
    operator T const&() const
    {
        return value;
    }
};
#endif

#endif // SMART_ANY_FWD


//
// Define some other useful close polcies
//

#if defined(_INC_STDLIB) | defined(_INC_MALLOC)
typedef void (__cdecl *pfn_free_t)( void* );
typedef close_fun<pfn_free_t,static_cast<pfn_free_t>(&free)>                close_free;
#endif

#if defined(_INC_STDIO) & !defined(SMART_CLOSE_FILE_PTR)
# define SMART_CLOSE_FILE_PTR
  // don't close a FILE* if it is stdin, stdout or stderr
  struct close_file_ptr
  {
      static void close( FILE * pfile )
      {
          if( pfile != stdin && pfile != stdout && pfile != stderr )
          {
              fclose( pfile );
          }
      }
  };
#endif

#ifdef _WINDOWS_

# ifndef SMART_VIRTUAL_FREE
# define SMART_VIRTUAL_FREE
  // free memory allocated with VirtualAlloc
  struct close_virtual_free
  {
      static void close( void * p )
      {
          ::VirtualFree( p, 0, MEM_RELEASE );
      }
  };
# endif

  typedef close_fun<BOOL (__stdcall *)( HANDLE ),CloseHandle>               close_handle;
  typedef close_fun<BOOL (__stdcall *)( HANDLE ),FindClose>                 close_find;
  typedef close_fun<BOOL (__stdcall *)( HANDLE ),FindCloseChangeNotification> close_find_change_notification;
  typedef close_fun<BOOL (__stdcall *)( HINSTANCE ),FreeLibrary>            close_library;
  typedef close_fun<LONG (__stdcall *)( HKEY ),RegCloseKey>                 close_regkey;
  typedef close_fun<BOOL (__stdcall *)( LPCVOID ),UnmapViewOfFile>          close_file_view;
  typedef close_fun<BOOL (__stdcall *)( HICON ),DestroyIcon>                close_hicon;
  typedef close_fun<BOOL (__stdcall *)( HGDIOBJ ),DeleteObject>             close_hgdiobj;
  typedef close_fun<BOOL (__stdcall *)( HACCEL ),DestroyAcceleratorTable>   close_haccel;
  typedef close_fun<BOOL (__stdcall *)( HDC ),DeleteDC>                     close_hdc;
  typedef close_fun<BOOL (__stdcall *)( HMENU ),DestroyMenu>                close_hmenu;
  typedef close_fun<BOOL (__stdcall *)( HCURSOR ),DestroyCursor>            close_hcursor;
  typedef close_fun<BOOL (__stdcall *)( HWND ),DestroyWindow>               close_window;
  typedef close_fun<BOOL (__stdcall *)( HANDLE ),HeapDestroy>               close_heap_destroy;
  typedef close_fun<HLOCAL (__stdcall *)( HLOCAL ),LocalFree>               close_local_free;
  typedef close_fun<BOOL (__stdcall *)( HDESK ),CloseDesktop>               close_hdesk;
  typedef close_fun<BOOL (__stdcall *)( HHOOK ),UnhookWindowsHookEx>        close_hhook;
  typedef close_fun<BOOL (__stdcall *)( HWINSTA ),CloseWindowStation>       close_hwinsta;
  typedef close_fun<BOOL (__stdcall *)( HANDLE ),DeregisterEventSource>     close_event_source;
  typedef close_fun<HGLOBAL (__stdcall *)( HGLOBAL ),GlobalFree>            close_global_free;

  typedef value_const<HANDLE,INVALID_HANDLE_VALUE>                          invalid_handle_t;
#endif

#ifdef _OLEAUTO_H_
  typedef close_fun<void (__stdcall *)(BSTR),SysFreeString>                 close_bstr;
#endif

#ifdef __MSGQUEUE_H__
  typedef close_fun<BOOL (__stdcall *)(HANDLE),CloseMsgQueue>               close_msg_queue;
#endif

#if defined(_WININET_) | defined(_DUBINET_)
  typedef close_fun<BOOL (__stdcall *)(HINTERNET),InternetCloseHandle>      close_hinternet;
#endif

#ifdef _RAS_H_
  typedef close_fun<DWORD (__stdcall *)( HRASCONN ),RasHangUp>              close_hrasconn;
#endif

#if defined(__RPCDCE_H__) & !defined(SMART_ANY_RPC)
# define SMART_ANY_RPC
  // for releaseing an rpc binding
  struct close_rpc_binding
  {
      static void close( RPC_BINDING_HANDLE & h )
      {
          ::RpcBindingFree( &h );
      }
  };
  // for releaseing an rpc binding vector
  struct close_rpc_vector
  {
      static void close( RPC_BINDING_VECTOR __RPC_FAR * & p )
      {
          ::RpcBindingVectorFree( &p );
      }
  };
  // for releasing a RPC string
  struct close_rpc_string
  {
      static void close( unsigned char __RPC_FAR * & p )
      {
          ::RpcStringFreeA(&p);
      }
      static void close( unsigned short __RPC_FAR * & p )
      {
          ::RpcStringFreeW(&p);
      }
  };
#endif

#ifdef _WINSVC_
  typedef close_fun<BOOL (__stdcall *)( SC_HANDLE ),CloseServiceHandle>     close_service;
  typedef close_fun<BOOL (__stdcall *)( SC_LOCK ),UnlockServiceDatabase>    unlock_service;
#endif

#ifdef _WINSOCKAPI_
  typedef int (__stdcall *pfn_closock_t)( SOCKET );
  typedef close_fun<pfn_closock_t,static_cast<pfn_closock_t>(&closesocket)> close_socket;
  typedef value_const<SOCKET,INVALID_SOCKET>                                invalid_socket_t;
#endif

#ifdef _OBJBASE_H_
  // For use when releasing memory allocated with CoTaskMemAlloc
  typedef close_fun<void (__stdcall*)( LPVOID ),CoTaskMemFree>              close_co_task_free;
#endif


//
// Below are useful smart typedefs for some common Windows/CRT resource types.
//

#undef DECLARE_SMART_ANY_TYPEDEFS_STDIO
#undef DECLARE_SMART_ANY_TYPEDEFS_WINDOWS
#undef DECLARE_SMART_ANY_TYPEDEFS_OLEAUTO
#undef DECLARE_SMART_ANY_TYPEDEFS_MSGQUEUE
#undef DECLARE_SMART_ANY_TYPEDEFS_WININET
#undef DECLARE_SMART_ANY_TYPEDEFS_RAS
#undef DECLARE_SMART_ANY_TYPEDEFS_RPCDCE
#undef DECLARE_SMART_ANY_TYPEDEFS_WINSVC
#undef DECLARE_SMART_ANY_TYPEDEFS_WINSOCKAPI
#undef DECLARE_SMART_ANY_TYPEDEFS_OBJBASE

#define DECLARE_SMART_ANY_TYPEDEFS_STDIO(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_WINDOWS(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_OLEAUTO(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_MSGQUEUE(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_WININET(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_RAS(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_RPCDCE(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_WINSVC(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_WINSOCKAPI(prefix)
#define DECLARE_SMART_ANY_TYPEDEFS_OBJBASE(prefix)

#ifdef _INC_STDIO
# undef  DECLARE_SMART_ANY_TYPEDEFS_STDIO
# define DECLARE_SMART_ANY_TYPEDEFS_STDIO(prefix)                                                                       \
  typedef prefix ## _any<FILE*,close_file_ptr>                                    prefix ## _file_ptr;
#endif

#ifdef _WINDOWS_
# undef  DECLARE_SMART_ANY_TYPEDEFS_WINDOWS
# define DECLARE_SMART_ANY_TYPEDEFS_WINDOWS(prefix)                                                                     \
  typedef prefix ## _any<HKEY,close_regkey>                                       prefix ## _hkey;                      \
  typedef prefix ## _any<HANDLE,close_find,invalid_handle_t>                      prefix ## _hfind;                     \
  typedef prefix ## _any<HANDLE,close_find_change_notification,invalid_handle_t>  prefix ## _hfind_change_notification; \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t>                    prefix ## _hfile;                     \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,1>                  prefix ## _communications_device;     \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,2>                  prefix ## _console_input;             \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,3>                  prefix ## _console_input_buffer;      \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,4>                  prefix ## _console_output;            \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,5>                  prefix ## _mailslot;                  \
  typedef prefix ## _any<HANDLE,close_handle,invalid_handle_t,6>                  prefix ## _pipe;                      \
  typedef prefix ## _any<HANDLE,close_handle>                                     prefix ## _handle;                    \
  typedef prefix ## _any<HANDLE,close_handle,null_t,1>                            prefix ## _access_token;              \
  typedef prefix ## _any<HANDLE,close_handle,null_t,2>                            prefix ## _event;                     \
  typedef prefix ## _any<HANDLE,close_handle,null_t,3>                            prefix ## _file_mapping;              \
  typedef prefix ## _any<HANDLE,close_handle,null_t,4>                            prefix ## _job;                       \
  typedef prefix ## _any<HANDLE,close_handle,null_t,5>                            prefix ## _mutex;                     \
  typedef prefix ## _any<HANDLE,close_handle,null_t,6>                            prefix ## _process;                   \
  typedef prefix ## _any<HANDLE,close_handle,null_t,7>                            prefix ## _semaphore;                 \
  typedef prefix ## _any<HANDLE,close_handle,null_t,8>                            prefix ## _thread;                    \
  typedef prefix ## _any<HANDLE,close_handle,null_t,9>                            prefix ## _timer;                     \
  typedef prefix ## _any<HANDLE,close_handle,null_t,10>                           prefix ## _completion_port;           \
  typedef prefix ## _any<HDC,close_hdc>                                           prefix ## _hdc;                       \
  typedef prefix ## _any<HICON,close_hicon>                                       prefix ## _hicon;                     \
  typedef prefix ## _any<HMENU,close_hmenu>                                       prefix ## _hmenu;                     \
  typedef prefix ## _any<HCURSOR,close_hcursor>                                   prefix ## _hcursor;                   \
  typedef prefix ## _any<HPEN,close_hgdiobj,null_t,1>                             prefix ## _hpen;                      \
  typedef prefix ## _any<HRGN,close_hgdiobj,null_t,2>                             prefix ## _hrgn;                      \
  typedef prefix ## _any<HFONT,close_hgdiobj,null_t,3>                            prefix ## _hfont;                     \
  typedef prefix ## _any<HBRUSH,close_hgdiobj,null_t,4>                           prefix ## _hbrush;                    \
  typedef prefix ## _any<HBITMAP,close_hgdiobj,null_t,5>                          prefix ## _hbitmap;                   \
  typedef prefix ## _any<HPALETTE,close_hgdiobj,null_t,6>                         prefix ## _hpalette;                  \
  typedef prefix ## _any<HACCEL,close_haccel>                                     prefix ## _haccel;                    \
  typedef prefix ## _any<HWND,close_window>                                       prefix ## _window;                    \
  typedef prefix ## _any<HINSTANCE,close_library>                                 prefix ## _library;                   \
  typedef prefix ## _any<LPVOID,close_file_view>                                  prefix ## _file_view;                 \
  typedef prefix ## _any<LPVOID,close_virtual_free>                               prefix ## _virtual_ptr;               \
  typedef prefix ## _any<HANDLE,close_heap_destroy>                               prefix ## _heap;                      \
  typedef prefix ## _any<HLOCAL,close_local_free>                                 prefix ## _hlocal;                    \
  typedef prefix ## _any<HDESK,close_hdesk>                                       prefix ## _hdesk;                     \
  typedef prefix ## _any<HHOOK,close_hhook>                                       prefix ## _hhook;                     \
  typedef prefix ## _any<HWINSTA,close_hwinsta>                                   prefix ## _hwinsta;                   \
  typedef prefix ## _any<HANDLE,close_event_source>                               prefix ## _event_source;              \
  typedef prefix ## _any<HGLOBAL,close_global_free>                               prefix ## _hglobal;
#endif

//
// Define some other useful typedefs
//

#ifdef _OLEAUTO_H_
# undef  DECLARE_SMART_ANY_TYPEDEFS_OLEAUTO
# define DECLARE_SMART_ANY_TYPEDEFS_OLEAUTO(prefix)                                                                     \
  typedef prefix ## _any<BSTR,close_bstr>                                         prefix ## _bstr;
#endif

#ifdef __MSGQUEUE_H__
# undef  DECLARE_SMART_ANY_TYPEDEFS_MSGQUEUE
# define DECLARE_SMART_ANY_TYPEDEFS_MSGQUEUE(prefix)                                                                    \
  typedef prefix ## _any<HANDLE,close_msg_queue>                                  prefix ## _msg_queue;
#endif

#if defined(_WININET_) | defined(_DUBINET_)
# undef  DECLARE_SMART_ANY_TYPEDEFS_WININET
# define DECLARE_SMART_ANY_TYPEDEFS_WININET(prefix)                                                                     \
  typedef prefix ## _any<HINTERNET,close_hinternet>                               prefix ## _hinternet;
#endif

#ifdef _RAS_H_
# undef  DECLARE_SMART_ANY_TYPEDEFS_RAS
# define DECLARE_SMART_ANY_TYPEDEFS_RAS(prefix)                                                                         \
  typedef prefix ## _any<HRASCONN,close_hrasconn>                                 prefix ## _hrasconn;
#endif

#ifdef __RPCDCE_H__
# undef DECLARE_SMART_ANY_TYPEDEFS_RPCDCE
# ifdef UNICODE
#   define DECLARE_SMART_ANY_TYPEDEFS_RPCDCE(prefix)                                                                    \
    typedef prefix ## _any<RPC_BINDING_HANDLE,close_rpc_binding>                  prefix ## _rpc_binding;               \
    typedef prefix ## _any<RPC_BINDING_VECTOR __RPC_FAR*,close_rpc_vector>        prefix ## _rpc_binding_vector;        \
    typedef prefix ## _any<unsigned char __RPC_FAR*,close_rpc_string>             prefix ## _rpc_string_A;              \
    typedef prefix ## _any<unsigned short __RPC_FAR*,close_rpc_string>            prefix ## _rpc_string_W;              \
    typedef prefix ## _rpc_string_W                                               prefix ## _rpc_string;
# else
#   define DECLARE_SMART_ANY_TYPEDEFS_RPCDCE(prefix)                                                                    \
    typedef prefix ## _any<RPC_BINDING_HANDLE,close_rpc_binding>                  prefix ## _rpc_binding;               \
    typedef prefix ## _any<RPC_BINDING_VECTOR __RPC_FAR*,close_rpc_vector>        prefix ## _rpc_binding_vector;        \
    typedef prefix ## _any<unsigned char __RPC_FAR*,close_rpc_string>             prefix ## _rpc_string_A;              \
    typedef prefix ## _any<unsigned short __RPC_FAR*,close_rpc_string>            prefix ## _rpc_string_W;              \
    typedef prefix ## _rpc_string_A                                               prefix ## _rpc_string;
# endif
#endif

#ifdef _WINSVC_
# undef  DECLARE_SMART_ANY_TYPEDEFS_WINSVC
# define DECLARE_SMART_ANY_TYPEDEFS_WINSVC(prefix)                                                                      \
  typedef prefix ## _any<SC_HANDLE,close_service>                                 prefix ## _service;                   \
  typedef prefix ## _any<SC_LOCK,unlock_service>                                  prefix ## _service_lock;
#endif

#ifdef _WINSOCKAPI_
# undef  DECLARE_SMART_ANY_TYPEDEFS_WINSOCKAPI
# define DECLARE_SMART_ANY_TYPEDEFS_WINSOCKAPI(prefix)                                                                  \
  typedef prefix ## _any<SOCKET,close_socket,invalid_socket_t>                    prefix ## _socket;
#endif

#if defined(_OBJBASE_H_) & !defined(SMART_ANY_CO_INIT)
# define SMART_ANY_CO_INIT
  inline HRESULT smart_co_init_helper( DWORD dwCoInit )
  {
      (void) dwCoInit;
#     if (_WIN32_WINNT >= 0x0400 ) | defined(_WIN32_DCOM)
          return ::CoInitializeEx(0,dwCoInit);
#     else
          return ::CoInitialize(0);
#     endif
  }
  inline void smart_co_uninit_helper( HRESULT hr )
  {
      if (SUCCEEDED(hr))
          ::CoUninitialize();
  }
  typedef close_fun<void(*)(HRESULT),smart_co_uninit_helper>                      close_co;
  typedef value_const<HRESULT,CO_E_NOTINITIALIZED>                                co_not_init;
# undef  DECLARE_SMART_ANY_TYPEDEFS_OBJBASE
# define DECLARE_SMART_ANY_TYPEDEFS_OBJBASE(prefix)                                                                     \
  typedef prefix ## _any<LPVOID,close_co_task_free>                               prefix ## _co_task_ptr;
#endif


#define DECLARE_SMART_ANY_TYPEDEFS(prefix)                                                                              \
    DECLARE_SMART_ANY_TYPEDEFS_STDIO(prefix)                                                                            \
    DECLARE_SMART_ANY_TYPEDEFS_WINDOWS(prefix)                                                                          \
    DECLARE_SMART_ANY_TYPEDEFS_OLEAUTO(prefix)                                                                          \
    DECLARE_SMART_ANY_TYPEDEFS_MSGQUEUE(prefix)                                                                         \
    DECLARE_SMART_ANY_TYPEDEFS_WININET(prefix)                                                                          \
    DECLARE_SMART_ANY_TYPEDEFS_RAS(prefix)                                                                              \
    DECLARE_SMART_ANY_TYPEDEFS_RPCDCE(prefix)                                                                           \
    DECLARE_SMART_ANY_TYPEDEFS_WINSVC(prefix)                                                                           \
    DECLARE_SMART_ANY_TYPEDEFS_WINSOCKAPI(prefix)                                                                       \
    DECLARE_SMART_ANY_TYPEDEFS_OBJBASE(prefix)
