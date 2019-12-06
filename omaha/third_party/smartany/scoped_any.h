//+---------------------------------------------------------------------------
//
//  Copyright ( C ) Microsoft, 2002.
//
//  File:       scoped_any.h
//
//  Contents:   automatic resource management, a-la std::scoped_ptr
//
//  Classes:    scoped_any<> and various typedefs
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

#ifndef SCOPED_ANY
#define SCOPED_ANY
#include <cassert>
#include "smart_any_fwd.h"

#pragma warning(push)

// 4284 warning for operator-> returning non-pointer;
//      compiler issues it even if -> is not used for the specific instance
#pragma warning(disable: 4284)

namespace detail
{
    // friend function definitions go in scoped_any_helper
    template<typename T,class close_policy,class invalid_value,int unique>
    struct scoped_any_helper;
}

// wrap a resource to enforce strict ownership and ensure proper cleanup
template<typename T,class close_policy,class invalid_value,int unique>
class scoped_any
{
    // disallow copy and assignment
    scoped_any( scoped_any<T,close_policy,invalid_value,unique> const & );
    scoped_any<T,close_policy,invalid_value,unique> & operator=(
        scoped_any<T,close_policy,invalid_value,unique> const & );

    // disallow comparison of scoped_any's
    bool operator==( detail::safe_bool ) const;
    bool operator!=( detail::safe_bool ) const;

    typedef detail::safe_types<T,close_policy>  safe_types;

public:
    typedef typename detail::holder<T>::type    element_type;
    typedef close_policy                        close_policy_type;
    typedef typename safe_types::pointer_type   pointer_type;
    typedef typename safe_types::reference_type reference_type;

    // Fix-up the invalid_value type on older compilers
    typedef typename detail::fixup_invalid_value<invalid_value>::
        template rebind<T>::type invalid_value_type;

    friend struct detail::scoped_any_helper<T,close_policy,invalid_value,unique>;

    // construct from object pointer
    explicit scoped_any( T t = invalid_value_type() )
        : m_t( t )
    {
    }

    // destroy the object
    ~scoped_any()
    {
        if( valid() )
        {
            close_policy::close( m_t );
        }
    }

    // return pointer to class object (assume pointer)
    pointer_type operator->() const
    {
        #ifdef SMART_ANY_PTS
        // You better not be applying operator-> to a handle!
        static detail::smartany_static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        #endif
        assert( valid() );
        return safe_types::to_pointer( m_t );
    }

    // for use when scoped_any appears in a conditional
    operator detail::safe_bool() const
    {
        return valid() ? detail::safe_true : detail::safe_false;
    }

    // for use when scoped_any appears in a conditional
    bool operator!() const
    {
        return ! valid();
    }

    #ifdef SMART_ANY_PTS
    // if this scoped_any is managing an array, we can use operator[] to index it
    typename detail::deref<T>::type operator[]( int i ) const
    {
        static detail::smartany_static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        static detail::smartany_static_assert<!detail::is_delete<close_policy>::value> const accessed_like_an_array_but_not_deleted_like_an_array;
        assert( valid() );
        return m_t[ i ];
    }

    // unary operator* lets you write code like:
    // scoped_any<foo*,close_delete> pfoo( new foo );
    // foo & f = *pfoo;
    reference_type operator*() const
    {
        static detail::smartany_static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        assert( valid() );
        return safe_types::to_reference( m_t );
    }
    #endif

private:

    bool valid() const
    {
        // see if the managed resource is in the invalid state.
        return m_t != static_cast<T>( invalid_value_type() );
    }

    // the wrapped object
    element_type m_t;
};

namespace detail
{
    // friend function definitions go in scoped_any_helper
    template<typename T,class close_policy,class invalid_value,int unique>
    struct scoped_any_helper
    {
        // return wrapped pointer
        static T get( scoped_any<T,close_policy,invalid_value,unique> const & t )
        {
            return t.m_t;
        }

        // return wrapped pointer and give up ownership
        static T release( scoped_any<T,close_policy,invalid_value,unique> & t )
        {
            // Fix-up the invalid_value type on older compilers
            typedef typename detail::fixup_invalid_value<invalid_value>::
                template rebind<T>::type invalid_value_type;

            T tmpT = t.m_t;
            t.m_t = static_cast<T>( invalid_value_type() );
            return tmpT;
        }

        // destroy designated object and store new pointer
        static void reset( scoped_any<T,close_policy,invalid_value,unique> & t, T newT )
        {
            if( t.m_t != newT )
            {
                if( t.valid() )
                {
                    close_policy::close( t.m_t );
                }
                t.m_t = newT;
            }
        }

        typedef typename scoped_any<T,close_policy,invalid_value,unique>::element_type element_type;

        // return the address of the wrapped pointer
        static element_type* address( scoped_any<T,close_policy,invalid_value,unique> & t )
        {
            // check to make sure the wrapped object is in the invalid state
            assert( !t.valid() );
            return address_of( t.m_t );
        }
    };
}

// return wrapped resource
template<typename T,class close_policy,class invalid_value,int unique>
inline T get( scoped_any<T,close_policy,invalid_value,unique> const & t )
{
    return detail::scoped_any_helper<T,close_policy,invalid_value,unique>::get( t );
}

// return true if the scoped_any contains a currently valid resource
template<typename T,class close_policy,class invalid_value,int unique>
inline bool valid( scoped_any<T,close_policy,invalid_value,unique> const & t )
{
    return static_cast<bool>(t);
}

// return wrapped resource and give up ownership
template<typename T,class close_policy,class invalid_value,int unique>
inline T release( scoped_any<T,close_policy,invalid_value,unique> & t )
{
    return detail::scoped_any_helper<T,close_policy,invalid_value,unique>::release( t );
}

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique>
inline void reset( scoped_any<T,close_policy,invalid_value,unique> & t )
{
    typedef typename detail::fixup_invalid_value<invalid_value>::
        template rebind<T>::type invalid_value_type;
    detail::scoped_any_helper<T,close_policy,invalid_value,unique>::reset( t, invalid_value_type() );
}

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique,typename U>
inline void reset( scoped_any<T,close_policy,invalid_value,unique> & t, U newT )
{
    detail::scoped_any_helper<T,close_policy,invalid_value,unique>::reset( t, newT );
}

// return the address of the wrapped resource
// WARNING: this will assert if the value of the resource is
// anything other than invalid_value.
template<typename T,class close_policy,class invalid_value,int unique>
inline typename scoped_any<T,close_policy,invalid_value,unique>::element_type*
    address( scoped_any<T,close_policy,invalid_value,unique> & t )
{
    return detail::scoped_any_helper<T,close_policy,invalid_value,unique>::address( t );
}

#pragma warning(pop)

#endif

// This causes the scoped_* typedefs to be defined
DECLARE_SMART_ANY_TYPEDEFS(scoped)

#if defined(_OBJBASE_H_) & !defined(SCOPED_ANY_CO_INIT)
# define SCOPED_ANY_CO_INIT
  typedef scoped_any<HRESULT,close_co,co_not_init>                            scoped_co_close;

  // Helper class for balancing calls to CoInitialize and CoUninitialize
  struct scoped_co_init
  {
      explicit scoped_co_init( DWORD dwCoInit = COINIT_APARTMENTTHREADED )
          : m_hr( smart_co_init_helper( dwCoInit ) )
      {
      }
      HRESULT hresult() const
      {
          return get(m_hr);
      }
      scoped_co_close const m_hr;
  };
#endif
