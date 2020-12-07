#pragma once
#include <cstddef>
#include <cstdint>

template<size_t N>
class RingBuffer
{
private:
    volatile uint8_t  m_buf[N];  // Buffer items are stored into
    volatile uint32_t m_head;    // Index of the next item to remove
    volatile uint32_t m_tail;    // Index where the next item will get inserted
    volatile uint32_t m_size;    // Buffer capacity minus one

public:

    RingBuffer()
    {
        m_head = 0;
        m_tail = 0;
        m_size = N;
    }

    /**
    * @brief Return the max number of elements that can be stored in the ring buffer.
    */
    inline uint32_t capacity()
    {
        return m_size;
    }

    /**
    * @brief Return the number of elements stored in the ring buffer.
    */
    inline uint32_t occupancy()
    {
        return m_tail - m_head;
    }

    /**
    * @brief Return the number of free spaces in the ring buffer.
    */
    inline uint32_t free()
    {
        return m_size - occupancy();
    }

    /**
    * @brief Returns true if and only if the ring buffer is full.
    */
    inline bool is_full()
    {
        return occupancy() == m_size;
    }

    /**
    * @brief Returns true if and only if the ring buffer is empty.
    */
    inline bool is_empty()
    {
        return m_head == m_tail;
    }

    /**
    * Append element onto the end of a ring buffer.
    * @param element Value to append.
    */
    inline void insert(uint8_t element)
    {
        m_buf[m_tail % m_size] = element;
        m_tail++;
    }

    /**
    * @brief Remove and return the first item from a ring buffer.
    */
    inline uint8_t remove()
    {
        uint8_t ch = m_buf[m_head % m_size];
        m_head++;
        return ch;
    }

    /*
    * @brief Return the first item from a ring buffer, without removing it
    */
    inline uint8_t peek()
    {
        return m_buf[m_head % m_size];
    }

    /*
    * @brief Return the nth element from a ring buffer, without removing it
    */
    inline uint8_t peek_at(uint32_t element)
    {
        return m_buf[(m_head + element) % m_size];
    }

    /**
    * @brief Attempt to remove the first item from a ring buffer.
    *
    * If the ring buffer is nonempty, removes and returns its first item.
    * If it is empty, does nothing and returns a negative value.
    *
    * @param rb Buffer to attempt to remove from.
    */
    inline int32_t safe_remove()
    {
        return is_empty() ? -1 : remove();
    }

    /**
    * @brief Attempt to insert an element into a ring buffer.
    *
    * @param element Value to insert into rb.
    * @sideeffect If rb is not full, appends element onto buffer.
    * @return If element was appended, then true; otherwise, false. */
    inline bool safe_insert(uint8_t element)
    {
        if (is_full())
            return false;

        insert(element);
        return true;
    }

    /**
    * @brief Append an item onto the end of a non-full ring buffer.
    *
    * If the buffer is full, removes its first item, then inserts the new
    * element at the end.
    *
    * @param element Value to insert into ring buffer.
    * @return returns true if element was removed else false
    */
    inline bool push_insert(uint8_t element)
    {
        bool ret = false;
        if (is_full())
        {
            remove();
            ret = true;
        }
        insert(element);
        return ret;
    }

    /**
    * @brief Discard all items from a ring buffer.
    */
    inline void reset()
    {
        m_tail = m_head;
    }

};