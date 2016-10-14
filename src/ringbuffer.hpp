// ringbuffer.h
// Author: Markus Redeker

#pragma warning(disable:4786)

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

// Use:
// ringbuffer r;
// r.push(); r.back() = new_element;
// oldest_element = r.front(); r.pop();

template<typename ET, int S, typename ST=int>
class ringbuffer
{
public:
    typedef ET value_type;
    typedef ST size_type;

    ringbuffer()
    {
        clear();
    }

    ~ringbuffer() {}

    size_type size()     const { return m_size; }
    size_type max_size() const { return S; }

    bool empty() const	{ return m_size == 0; }
    bool full()  const	{ return m_size == S; }

    value_type& front() { return m_buffer[m_front]; }
    value_type& back() { return m_buffer[m_back]; }

    value_type get(size_type i) {
        return m_buffer[(m_back + i + 1) % m_size];
    }

    void clear() {
        m_size = 0;
        m_front = 0;
        m_back  = S - 1;
    }

    void push()	{
        m_back = (m_back + 1) % S;
        if( size() == S )
            m_front = (m_front + 1) % S;
        else
            m_size++;
    }

    void push(const value_type& x) {
        push();
        back() = x;
    }

    void pop() {
        if( m_size > 0  ) {
            m_size--;
            m_front = (m_front + 1) % S;
        }
    }

    void back_erase(const size_type n) {
        if( n >= m_size )
            clear();
        else {
            m_size -= n;
            m_back = (m_front + m_size - 1) % S;
        }
    }

    void front_erase(const size_type n) {
        if( n >= m_size )
            clear();
        else {
            m_size -= n;
            m_front = (m_front + n) % S;
        }
    }
    value_type m_buffer[S];
    size_type m_size;

    size_type m_front;
    size_type m_back;

protected:

};


#endif // __RINGBUFFER_H__
