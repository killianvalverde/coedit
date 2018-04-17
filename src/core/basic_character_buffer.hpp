//
// Created by Killian on 31/01/18.
//

#ifndef COEDIT_CORE_BASIC_CHARACTER_BUFFER_HPP
#define COEDIT_CORE_BASIC_CHARACTER_BUFFER_HPP

#include <experimental/filesystem>
#include <fstream>
#include <memory>

#include "basic_character_buffer_cache.hpp"
#include "core_exception.hpp"
#include "fundamental_types.hpp"


namespace coedit {
namespace core {


namespace stdfs = std::experimental::filesystem;


template<
        typename TpChar,
        std::size_t CHARACTER_BUFFER_SIZE,
        std::size_t CHARACTER_BUFFER_CACHE_SIZE,
        std::size_t CHARACTER_BUFFER_ID_BUFFER_SIZE,
        std::size_t CHARACTER_BUFFER_ID_BUFFER_CACHE_SIZE
>
class basic_character_buffer_cache;


template<
        typename TpChar,
        std::size_t CHARACTER_BUFFER_SIZE,
        std::size_t CHARACTER_BUFFER_CACHE_SIZE,
        std::size_t CHARACTER_BUFFER_ID_BUFFER_SIZE,
        std::size_t CHARACTER_BUFFER_ID_BUFFER_CACHE_SIZE
>
class basic_character_buffer
{
public:
    using char_type = TpChar;
    
    using character_buffer_type = basic_character_buffer<
            TpChar,
            CHARACTER_BUFFER_SIZE,
            CHARACTER_BUFFER_CACHE_SIZE,
            CHARACTER_BUFFER_ID_BUFFER_SIZE,
            CHARACTER_BUFFER_ID_BUFFER_CACHE_SIZE
    >;
    
    using character_buffer_cache_type = basic_character_buffer_cache<
            TpChar,
            CHARACTER_BUFFER_SIZE,
            CHARACTER_BUFFER_CACHE_SIZE,
            CHARACTER_BUFFER_ID_BUFFER_SIZE,
            CHARACTER_BUFFER_ID_BUFFER_CACHE_SIZE
    >;
    
    enum class operation_types : uint8_t
    {
        NIL = 0x0,
        CHARACTER_INSERTED = 0x1,
        CHARACTER_ERASED = 0x2,
        CHARACTER_MOVED = 0x4,
        CHARACTER_BUFFER_INSERTED = 0x8,
        CHARACTER_BUFFER_ERASED = 0x10,
        ALL = 0x1F
    };
    
    struct operation_done
    {
        cbid_t op_cb;
        cboffset_t op_offset;
        kcontain::flags<operation_types> op_types;
    };
    
    basic_character_buffer()
            : buf_()
            , cbid_(EMPTY)
            , prev_(EMPTY)
            , nxt_(EMPTY)
            , size_(0)
            , cb_cache_(nullptr)
    {
    }
    
    basic_character_buffer(
            cbid_t cbid,
            character_buffer_cache_type* chars_buf_cache
    )
            : buf_()
            , cbid_(cbid)
            , prev_(EMPTY)
            , nxt_(EMPTY)
            , size_(0)
            , cb_cache_(chars_buf_cache)
    {
    }
    
    basic_character_buffer(
            cbid_t cbid,
            cbid_t prev,
            character_buffer_cache_type* chars_buf_cache
    )
            : buf_()
            , cbid_(cbid)
            , prev_(prev)
            , nxt_(EMPTY)
            , size_(0)
            , cb_cache_(chars_buf_cache)
    {
        character_buffer_type& prev_cb = cb_cache_->get_character_buffer(prev_);
        
        prev_cb.link_character_buffer_after_current(*this);
        prev_cb.move_characters_to(*this);
    }
    
    basic_character_buffer(
            const stdfs::path& cb_path,
            character_buffer_cache_type* chars_buf_cache
    )
            : cb_cache_(chars_buf_cache)
    {
        std::ifstream ifs;
    
        ifs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        ifs.open(cb_path, std::ios::in | std::ios::binary);
    
        ifs.read((char*)&cbid_, sizeof(cbid_));
        ifs.read((char*)&prev_, sizeof(prev_));
        ifs.read((char*)&nxt_, sizeof(nxt_));
        ifs.read((char*)&size_, sizeof(size_));
        ifs.read((char*)buf_, sizeof(char_type) * size_);
    
        ifs.close();
    }
    
    basic_character_buffer& operator =(const basic_character_buffer& rhs)
    {
        if (this != &rhs)
        {
            memcpy(buf_, rhs.buf_, rhs.size_ * sizeof(char_type));
            cbid_ = rhs.cbid_;
            prev_ = rhs.prev_;
            nxt_ = rhs.nxt_;
            size_ = rhs.size_;
            cb_cache_ = rhs.cb_cache_;
        }
        
        return *this;
    }
    
    basic_character_buffer& operator =(basic_character_buffer&& rhs) noexcept
    {
        if (this != &rhs)
        {
            memcpy(buf_, rhs.buf_, rhs.size_ * sizeof(char_type));
            std::swap(cbid_, rhs.cbid_);
            std::swap(prev_, rhs.prev_);
            std::swap(nxt_, rhs.nxt_);
            std::swap(size_, rhs.size_);
            std::swap(cb_cache_, rhs.cb_cache_);
        }
        
        return *this;
    }
    
    // todo : return the operation done.
    operation_done insert_character(char_type ch, cboffset_t cboffset)
    {
        operation_done op_done;
        character_buffer_type* cur_cb = &get_character_buffer_for_insertion(&cboffset);
        
        if (cur_cb->size_ == CHARACTER_BUFFER_SIZE)
        {
            cur_cb->cb_cache_->insert_character_buffer_after(cur_cb->cbid_);
            op_done.op_types.set(operation_types::CHARACTER_BUFFER_INSERTED);
        }
        
        if (cboffset > cur_cb->size_)
        {
            cur_cb = &cur_cb->get_character_buffer_for_insertion(&cboffset);
        }
        op_done.op_cb = cur_cb->get_cbid();
        
        if (cboffset < cur_cb->size_)
        {
            memcpy((cur_cb->buf_ + cboffset + 1),
                   (cur_cb->buf_ + cboffset),
                   (cur_cb->size_ - cboffset) * sizeof(char_type));
            op_done.op_types.set(operation_types::CHARACTER_MOVED);
        }
    
        cur_cb->buf_[cboffset] = ch;
        ++cur_cb->size_;
        op_done.op_types.set(operation_types::CHARACTER_INSERTED);
        op_done.op_offset = cboffset;
        
        return op_done;
    }
    
    // todo : implement an intern defragment policy.
    // todo : retunr the operation done correctly.
    operation_done erase_character(cboffset_t cboffset)
    {
        operation_done op_done;
        character_buffer_type& cur_cb = get_character_buffer(&cboffset);
        
        if (cboffset + 1 < cur_cb.size_)
        {
            memcpy((cur_cb.buf_ + cboffset),
                   (cur_cb.buf_ + cboffset + 1),
                   (cur_cb.size_ - cboffset - 1) * sizeof(char_type));
        }
        
        --cur_cb.size_;
    
        op_done.op_type = operation_types::CHARACTER_ERASED;
        op_done.op_offset = cboffset;
        
        return op_done;
    }
    
    cboffset_t get_line_length(cboffset_t cboffset)
    {
        character_buffer_type* cur_cb = this;
        cboffset_t line_len = 0;
        
        do
        {
            cur_cb = &cur_cb->get_character_buffer(&cboffset);
            
            while (cboffset < cur_cb->size_ &&
                   cur_cb->buf_[cboffset] != LF &&
                   cur_cb->buf_[cboffset] != CR)
            {
                ++line_len;
                ++cboffset;
            }
            
            if (cboffset < cur_cb->size_)
            {
                ++line_len;
                if (cur_cb->buf_[cboffset] == CR &&
                    cboffset + 1 < CHARACTER_BUFFER_SIZE &&
                    cur_cb->buf_[cboffset + 1] == LF)
                {
                    ++line_len;
                }
            }
            
        } while (cboffset == cur_cb->size_ && cur_cb->nxt_ != EMPTY);
    
        return line_len;
    }
    
    void store(const stdfs::path& cb_path) const
    {
        std::ofstream ofs;
        
        ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        ofs.open(cb_path, std::ios::out | std::ios::binary);
    
        ofs.write((const char*)&cbid_, sizeof(cbid_));
        ofs.write((const char*)&prev_, sizeof(prev_));
        ofs.write((const char*)&nxt_, sizeof(nxt_));
        ofs.write((const char*)&size_, sizeof(size_));
        ofs.write((const char*)buf_, sizeof(char_type) * size_);
        
        ofs.close();
    }
    
    char_type& operator [](cboffset_t i)
    {
        auto& chars_buf = get_character_buffer(&i);
        
        return chars_buf.buf_[i];
    }
    
    cbid_t get_cbid() const
    {
        return cbid_;
    }
    
    cbid_t get_prev() const
    {
        return prev_;
    }
    
    cbid_t get_nxt() const
    {
        return nxt_;
    }
    
    cboffset_t get_size() const
    {
        return size_;
    }

private:
    character_buffer_type& get_character_buffer(cboffset_t* cboffset)
    {
        character_buffer_type* cur_cb = this;
    
        while (*cboffset >= cur_cb->size_)
        {
            if (cur_cb->nxt_ == EMPTY)
            {
                throw characte_buffer_overflow_exception();
            }
    
            *cboffset -= cur_cb->size_;
            cur_cb = &cb_cache_->get_character_buffer(cur_cb->nxt_);
        }
    
        return *cur_cb;
    }
    
    character_buffer_type& get_character_buffer_for_insertion(cboffset_t* cboffset)
    {
        character_buffer_type* cur_cb = this;
        
        while (*cboffset > cur_cb->size_)
        {
            if (cur_cb->nxt_ == EMPTY)
            {
                throw characte_buffer_overflow_exception();
            }
    
            *cboffset -= cur_cb->size_;
            cur_cb = &cb_cache_->get_character_buffer(cur_cb->nxt_);
        }
        
        return *cur_cb;
    }
    
    void link_character_buffer_after_current(character_buffer_type& cb_to_link)
    {
        if (nxt_ != EMPTY)
        {
            character_buffer_type& nxt_cb = cb_cache_->get_character_buffer(nxt_);
            nxt_cb.prev_ = cb_to_link.cbid_;
        }
        
        nxt_ = cb_to_link.cbid_;
        cb_to_link.prev_ = cbid_;
    }
    
    void move_characters_to(character_buffer_type& cb_dest)
    {
        constexpr cboffset_t half_size = CHARACTER_BUFFER_SIZE / 2;
        
        memcpy(cb_dest.buf_, buf_ + half_size, half_size * sizeof(char_type));
        size_ -= half_size;
        cb_dest.size_ = half_size;
    }

private:
    char_type buf_[CHARACTER_BUFFER_SIZE];
    
    cbid_t cbid_;
    
    cbid_t prev_;
    
    cbid_t nxt_;
    
    cboffset_t size_;
    
    character_buffer_cache_type* cb_cache_;
};


}
}


#endif
