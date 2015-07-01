//+---------------------------------------------------------------------------
//
//  Copyright ( C ) Microsoft, 2002.
//
//  File:       shared_any.h
//
//  Contents:   automatic resource management
//
//  Classes:    shared_any<> and various typedefs
//
//  Functions:  get
//              reset
//              valid
//
//  Author:     Eric Niebler ( ericne@microsoft.com )
//
//----------------------------------------------------------------------------


#ifndef SHARED_ANY
#define SHARED_ANY

#include <cassert>
#include <functional>  // for std::less
#include <algorithm>   // for std::swap
#include "smart_any_fwd.h"

namespace detail
{
    class ref_count_allocator
    {
        struct  node;
        node   *m_list_blocks;
        node   *m_last_alloc;
        node   *m_last_free;
        
        ref_count_allocator();
        ~ref_count_allocator();
    public:
        void finalize();
        long volatile *alloc();
        long volatile *alloc( long val );
        void free( long volatile *refcount );

        static ref_count_allocator instance;
    };

    template<typename T,class close_policy,class invalid_value,int unique>
    struct shared_any_helper;

    template<typename Super>
    struct shared_holder : Super
    {
        explicit shared_holder( typename Super::type t )
            : Super( t )
        {
        }

        shared_holder( shared_holder const & that )
            : Super( that )
        {
            if( Super::valid() )
            {
                Super::inc_ref();
            }
        }

        ~shared_holder()
        {
            if( Super::valid() )
            {
                Super::dec_ref();
            }
        }
    };

    template<typename T,class invalid_value_type>
    struct intrusive
    {
        typedef T type;

        explicit intrusive( T t )
            : m_t( t )
        {
        }

        bool valid() const
        {
            return m_t != static_cast<T>( invalid_value_type() );
        }
        
        void inc_ref()
        {
            m_t->AddRef();
        }
        
        void dec_ref()
        {
            if( 0 == m_t->Release() )
            {
                m_t = static_cast<T>( invalid_value_type() );
            }
        }
        
        T m_t;
    };
    
    template<typename T,class close_policy,class invalid_value_type>
    struct nonintrusive
    {
        typedef T type;
        
        explicit nonintrusive( T t )
            : m_t( t ),
              m_ref( 0 )
        {
            if( valid() )
            {
                m_ref = ref_count_allocator::instance.alloc(1L);
                if( ! m_ref )
                {
                    m_t = static_cast<T>( invalid_value_type() );
                    throw std::bad_alloc();
                }
            }
        }
        
        bool valid() const
        {
            return m_t != static_cast<T>( invalid_value_type() );
        }
        
        void inc_ref()
        {
            ::InterlockedIncrement( m_ref );
        }
        
        void dec_ref()
        {
            if( 0L == ::InterlockedDecrement( m_ref ) )
            {
                ref_count_allocator::instance.free( m_ref );
                m_ref = 0;
                close_policy::close( m_t );
                m_t = static_cast<T>( invalid_value_type() );
            }
        }
        
        typename holder<T>::type    m_t;
        long volatile              *m_ref;
    };

    template<class close_policy>
    struct is_close_release_com
    {
        static bool const value = false;
    };
    template<>
    struct is_close_release_com<close_release_com>
    {
        static bool const value = true;
    };

    // credit Rani Sharoni for showing me how to implement
    // is_com_ptr on VC7. This is deeply magical code.
    template<typename T>
    struct is_com_ptr
    {
    private:
        struct maybe
        {
            operator IUnknown*() const;
            operator T();
        };

        template<typename U>
        static yes    check(T, U);    
        static no     check(IUnknown*, int);
        static maybe  get();
    public:
        static bool const value = sizeof(check(get(),0)) == sizeof(yes);
    };

    template<>
    struct is_com_ptr<IUnknown*>
    {
        static bool const value = true;
    };
}

template<typename T,class close_policy,class invalid_value,int unique>
class shared_any
{
    typedef detail::safe_types<T,close_policy>  safe_types;

    // disallow comparison of shared_any's
    bool operator==( detail::safe_bool ) const;
    bool operator!=( detail::safe_bool ) const;

public:
    typedef typename detail::holder<T>::type    element_type;
    typedef close_policy                        close_policy_type;
    typedef typename safe_types::pointer_type   pointer_type;
    typedef typename safe_types::reference_type reference_type;

    // Fix-up the invalid_value type on older compilers
    typedef typename detail::fixup_invalid_value<invalid_value>::
        template rebind<T>::type invalid_value_type;

    friend struct detail::shared_any_helper<T,close_policy,invalid_value,unique>;

    // default construct
    shared_any()
        : m_held( static_cast<T>( invalid_value_type() ) )
    {
    }

    // construct from object. If we fail to allocate a reference count,
    // then the T object is closed, and a bad_alloc exception is thrown.
    explicit shared_any( T t )
    try : m_held( t )
    {
    }
    catch( std::bad_alloc & )
    {
        close_policy::close( t );
        throw;
    }

    // construct from another shared_any, incrementing ref count.
    // Only throws if T's copy-c'tor throws, in which case, ref-count
    // is unchanged.
    shared_any( shared_any<T,close_policy,invalid_value,unique> const & right )
        : m_held( right.m_held )
    {
    }

    // construct from an auto_any, taking ownership. If allocation
    // fails, auto_any retains ownership.
    shared_any( auto_any<T,close_policy,invalid_value,unique> & right )
        : m_held( get( right ) )
    {
        release( right );
    }

    // assign from another shared_any
    shared_any<T,close_policy,invalid_value,unique> & operator=(
        shared_any<T,close_policy,invalid_value,unique> const & right )
    {
        shared_any<T,close_policy,invalid_value,unique>( right ).swap( *this );
        return *this;
    }

    // assign from an auto_any
    shared_any<T,close_policy,invalid_value,unique> & operator=(
        auto_any<T,close_policy,invalid_value,unique> & right )
    {
        shared_any<T,close_policy,invalid_value,unique>( right ).swap( *this );
        return *this;
    }

    operator detail::safe_bool() const
    {
        return m_held.valid() ? detail::safe_true : detail::safe_false;
    }

    bool operator!() const
    {
        return ! m_held.valid();
    }

    // return pointer to class object (assume pointer)
    pointer_type operator->() const
    {
        #ifdef SMART_ANY_PTS
        // You better not be applying operator-> to a handle!
        static detail::static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        #endif
        assert( m_held.valid() );
        return safe_types::to_pointer( m_held.m_t );
    }

    #ifdef SMART_ANY_PTS
    // if this shared_any is managing an array, we can use operator[] to index it
    typename detail::deref<T>::type operator[]( int i ) const
    {
        static detail::static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        static detail::static_assert<!detail::is_delete<close_policy>::value> const accessed_like_an_array_but_not_deleted_like_an_array;
        assert( m_held.valid() );
        return m_held.m_t[ i ];
    }

    // unary operator* lets you write code like:
    // shared_any<foo*,close_delete> pfoo( new foo );
    // foo & f = *pfoo;
    reference_type operator*() const
    {
        static detail::static_assert<!detail::is_handle<T>::value> const cannot_dereference_a_handle;
        assert( m_held.valid() );
        return safe_types::to_reference( m_held.m_t );
    }
    #endif

private:

    void swap( shared_any<T,close_policy,invalid_value,unique> & right )
    {
        using std::swap;
        swap( m_held, right.m_held );
    }

    // if we are wrapping a COM object, then use COM's reference counting.
    // otherwise, use our own reference counting.
    typedef typename detail::select<
        detail::is_com_ptr<T>::value && detail::is_close_release_com<close_policy>::value,
        detail::intrusive<T,invalid_value_type>,
        detail::nonintrusive<T,close_policy,invalid_value_type> >::type holder_policy;

    detail::shared_holder<holder_policy> m_held;
};

namespace detail
{
    template<typename T,class close_policy,class invalid_value,int unique>
    struct shared_any_helper
    {
        static T get( shared_any<T,close_policy,invalid_value,unique> const & t )
        {
            return t.m_held.m_t;
        }

        static void reset( shared_any<T,close_policy,invalid_value,unique> & t, T newT )
        {
            shared_any<T,close_policy,invalid_value,unique>( newT ).swap( t );
        }

        static void swap( shared_any<T,close_policy,invalid_value,unique> & left,
                          shared_any<T,close_policy,invalid_value,unique> & right )
        {
            left.swap( right );
        }
    };
}

// return wrapped resource
template<typename T,class close_policy,class invalid_value,int unique>
inline T get( shared_any<T,close_policy,invalid_value,unique> const & t )
{
    return detail::shared_any_helper<T,close_policy,invalid_value,unique>::get( t );
}

// return true if the shared_any contains a currently valid resource
template<typename T,class close_policy,class invalid_value,int unique>
inline bool valid( shared_any<T,close_policy,invalid_value,unique> const & t )
{
    return t;
}

// destroy designated object
template<typename T,class close_policy,class invalid_value,int unique>
inline void reset( shared_any<T,close_policy,invalid_value,unique> & t )
{
    typedef typename detail::fixup_invalid_value<invalid_value>::
        template rebind<T>::type invalid_value_type;
    detail::shared_any_helper<T,close_policy,invalid_value,unique>::reset( t, invalid_value_type() );
}

// destroy designated object and store new resource
template<typename T,class close_policy,class invalid_value,int unique,typename U>
inline void reset( shared_any<T,close_policy,invalid_value,unique> & t, U newT )
{
    detail::shared_any_helper<T,close_policy,invalid_value,unique>::reset( t, newT );
}

// swap the contents of two shared_any objects
template<typename T,class close_policy,class invalid_value,int unique>
inline void swap( shared_any<T,close_policy,invalid_value,unique> & left, 
                  shared_any<T,close_policy,invalid_value,unique> & right )
{
    detail::shared_any_helper<T,close_policy,invalid_value,unique>::swap( left, right );
}

// Define some relational operators on shared_* types so they
// can be used in hashes and maps
template<typename T,class close_policy,class invalid_value,int unique>
inline bool operator==(
    shared_any<T,close_policy,invalid_value,unique> const & left, 
    shared_any<T,close_policy,invalid_value,unique> const & right )
{
    return get( left ) == get( right );
}

template<typename T,class close_policy,class invalid_value,int unique>
inline bool operator!=(
    shared_any<T,close_policy,invalid_value,unique> const & left, 
    shared_any<T,close_policy,invalid_value,unique> const & right )
{
    return get( left ) != get( right );
}

template<typename T,class close_policy,class invalid_value,int unique>
inline bool operator<(
    shared_any<T,close_policy,invalid_value,unique> const & left, 
    shared_any<T,close_policy,invalid_value,unique> const & right )
{
    return std::less<T>( get( left ), get( right ) );
}

#endif // SHARED_ANY

// This causes the shared_* typedefs to be defined
DECLARE_SMART_ANY_TYPEDEFS(shared)
