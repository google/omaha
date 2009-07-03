//+---------------------------------------------------------------------------
//
//  Copyright ( C ) Microsoft, 2002.
//
//  File:       shared_any.cpp
//
//  Contents:   pool allocator for reference counts
//
//  Classes:    ref_count_allocator and helpers
//
//  Functions:
//
//  Author:     Eric Niebler ( ericne@microsoft.com )
//
//----------------------------------------------------------------------------

#ifdef _MT
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0403
# endif
# include <windows.h>
# include <unknwn.h>
#endif

#include <cassert>
#include <functional>  // for std::less
#include <algorithm>   // for std::swap

#pragma warning(push)
// C4640: construction of local static object is not thread-safe
#pragma warning(disable : 4640)
#include "shared_any.h"
#include "scoped_any.h"
#pragma warning(pop)

namespace detail
{
    struct critsec
    {
#ifdef _MT
        CRITICAL_SECTION m_cs;

        critsec()
        {
            InitializeCriticalSectionAndSpinCount( &m_cs, 4000 );
        }
        ~critsec()
        {
            DeleteCriticalSection( &m_cs );
        }
        void enter()
        {
            EnterCriticalSection( &m_cs );
        }
        void leave()
        {
            LeaveCriticalSection( &m_cs );
        }
#endif
    };

    namespace
    {
        critsec g_critsec;
    }

    struct lock
    {
#ifdef _MT
        critsec & m_cs;

        explicit lock( critsec & cs )
            : m_cs(cs)
        {
            m_cs.enter();
        }
        ~lock()
        {
            m_cs.leave();
        }
#else
        explicit lock( critsec & )
        {
        }
#endif
    private:
        lock( lock const & );
        lock & operator=( lock const & );
    };

    struct ref_count_block
    {
        static long const s_sizeBlock = 256;

        short m_free_list; // offset to start of freelist
        short m_available; // count of refcounts in this block that are available
        long  m_refcounts[ s_sizeBlock ];

        ref_count_block()
          : m_free_list(0), m_available(s_sizeBlock)
        {
            for( long l=0; l<s_sizeBlock; ++l )
                m_refcounts[l] = l+1;
        }

        bool empty() const // throw()
        {
            return s_sizeBlock == m_available;
        }

        bool full() const // throw()
        {
            return 0 == m_available;
        }

        long volatile *alloc( lock & )
        {
            assert( 0 != m_available );
            long *refcount = m_refcounts + m_free_list;
            m_free_list = static_cast<short>( *refcount );
            --m_available;
            return refcount;
        }

        void free( long volatile *refcount, lock & ) // throw()
        {
            assert( owns( refcount ) );
            *refcount = m_free_list;
            m_free_list = static_cast<short>( refcount - m_refcounts );
            ++m_available;
        }

        bool owns( long volatile *refcount ) const // throw()
        {
            return ! std::less<void*>()( const_cast<long*>( refcount ), const_cast<long*>( m_refcounts ) ) &&
                    std::less<void*>()( const_cast<long*>( refcount ), const_cast<long*>( m_refcounts ) + s_sizeBlock );
        }
    };


    struct ref_count_allocator::node
    {
        node           *m_next;
        node           *m_prev;
        ref_count_block m_block;
        explicit node( node *next=0, node *prev=0 )
        : m_next(next), m_prev(prev), m_block()
        {
            if( m_next )
                m_next->m_prev = this;
            if( m_prev )
                m_prev->m_next = this;
        }
    };


    ref_count_allocator::ref_count_allocator()
      : m_list_blocks(0), m_last_alloc(0), m_last_free(0)
    {
    }

    ref_count_allocator::~ref_count_allocator()
    {
        // Just leak the blocks. It's ok, really.
        // If you need to clean up the blocks and
        // you are certain that no refcounts are
        // outstanding, you can use the finalize()
        // method to force deallocation
    }

    void ref_count_allocator::finalize()
    {
        lock l( g_critsec );
        for( node *next; m_list_blocks; m_list_blocks=next )
        {
            next = m_list_blocks->m_next;
            delete m_list_blocks;
        }
        m_last_alloc = 0;
        m_last_free = 0;
    }

    long volatile *ref_count_allocator::alloc()
    {
        lock l( g_critsec );
        if( ! m_last_alloc || m_last_alloc->m_block.full() )
        {
            for( m_last_alloc = m_list_blocks;
                m_last_alloc && m_last_alloc->m_block.full();
                m_last_alloc = m_last_alloc->m_next );
            if( ! m_last_alloc )
            {
                m_last_alloc = new( std::nothrow ) node( m_list_blocks );
                if( ! m_last_alloc )
                    return 0;
                m_list_blocks = m_last_alloc;
            }
        }
        return m_last_alloc->m_block.alloc( l );
    }

    long volatile *ref_count_allocator::alloc( long val )
    {
        long volatile *refcount = alloc();
        *refcount = val;
        return refcount;
    }

    void ref_count_allocator::free( long volatile *refcount ) // throw()
    {
        // don't rearrange the order of these locals!
        scoped_any<node*,close_delete> scoped_last_free;
        lock l( g_critsec );

        if( ! m_last_free || ! m_last_free->m_block.owns( refcount ) )
        {
            for( m_last_free = m_list_blocks;
                m_last_free && ! m_last_free->m_block.owns( refcount );
                m_last_free = m_last_free->m_next );
        }

        assert( m_last_free && m_last_free->m_block.owns( refcount ) );
        m_last_free->m_block.free( refcount, l );

        if( m_last_free != m_list_blocks && m_last_free->m_block.empty() )
        {
            if( 0 != ( m_last_free->m_prev->m_next = m_last_free->m_next ) )
                m_last_free->m_next->m_prev = m_last_free->m_prev;

            if( ! m_list_blocks->m_block.empty() )
            {
                m_last_free->m_next = m_list_blocks;
                m_last_free->m_prev = 0;
                m_list_blocks->m_prev = m_last_free;
                m_list_blocks = m_last_free;
            }
            else
                reset( scoped_last_free, m_last_free ); // deleted after critsec is released

            m_last_free = 0;
        }
    }

    // Here is the global reference count allocator.
    ref_count_allocator ref_count_allocator::instance;
}
