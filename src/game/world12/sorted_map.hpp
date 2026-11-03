// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace bmh::world12 {
// Small saved dictionaries need allocation-free moves even when an optional
// value first becomes engaged. Some tree-map implementations allocate their
// sentinel in a move constructor. Vector storage avoids that allocation.
// Insertion invalidates iterators/references; callers retain keys across it.
template<class Key,class Value> class SortedMap {
public:
    using key_type=Key;
    using mapped_type=Value;
    using value_type=std::pair<const Key,Value>;
    using storage_type=std::vector<value_type>;
    using iterator=typename storage_type::iterator;
    using const_iterator=typename storage_type::const_iterator;
    using reverse_iterator=typename storage_type::reverse_iterator;
    using const_reverse_iterator=typename storage_type::const_reverse_iterator;
    using size_type=typename storage_type::size_type;

    SortedMap()=default;
    SortedMap(const SortedMap&)=default;
    SortedMap(SortedMap&&)=default;
    SortedMap& operator=(SortedMap&&)=default;
    SortedMap& operator=(const SortedMap& other) {
        if(this!=&other){SortedMap staged(other);swap(staged);}return *this;
    }
    SortedMap(std::initializer_list<value_type> rows) {
        for(const auto& row:rows)emplace(row.first,row.second);
    }
    SortedMap(const std::map<Key,Value>& rows):rows_(rows.begin(),rows.end()){}
    SortedMap& operator=(const std::map<Key,Value>& rows) {
        SortedMap staged(rows);swap(staged);return *this;
    }
    SortedMap& operator=(std::initializer_list<value_type> rows) {
        SortedMap staged(rows);swap(staged);return *this;
    }

    iterator begin()noexcept{return rows_.begin();}
    const_iterator begin()const noexcept{return rows_.begin();}
    const_iterator cbegin()const noexcept{return rows_.cbegin();}
    iterator end()noexcept{return rows_.end();}
    const_iterator end()const noexcept{return rows_.end();}
    const_iterator cend()const noexcept{return rows_.cend();}
    reverse_iterator rbegin()noexcept{return rows_.rbegin();}
    const_reverse_iterator rbegin()const noexcept{return rows_.rbegin();}
    const_reverse_iterator crbegin()const noexcept{return rows_.crbegin();}
    reverse_iterator rend()noexcept{return rows_.rend();}
    const_reverse_iterator rend()const noexcept{return rows_.rend();}
    const_reverse_iterator crend()const noexcept{return rows_.crend();}
    bool empty()const noexcept{return rows_.empty();}
    size_type size()const noexcept{return rows_.size();}
    void clear()noexcept{rows_.clear();}
    void swap(SortedMap& other)noexcept(noexcept(rows_.swap(other.rows_))){rows_.swap(other.rows_);}

    iterator lower_bound(const Key& key) {
        return std::lower_bound(begin(),end(),key,[](const auto& row,const auto& k){return row.first<k;});
    }
    const_iterator lower_bound(const Key& key)const {
        return std::lower_bound(begin(),end(),key,[](const auto& row,const auto& k){return row.first<k;});
    }
    iterator find(const Key& key) {
        const auto at=lower_bound(key);return at!=end()&&!(key<at->first)?at:end();
    }
    const_iterator find(const Key& key)const {
        const auto at=lower_bound(key);return at!=end()&&!(key<at->first)?at:end();
    }
    bool contains(const Key& key)const{return find(key)!=end();}
    size_type count(const Key& key)const{return contains(key)?1:0;}
    Value& at(const Key& key) {
        const auto found=find(key);if(found==end())throw std::out_of_range("sorted map: unknown key");return found->second;
    }
    const Value& at(const Key& key)const {
        const auto found=find(key);if(found==end())throw std::out_of_range("sorted map: unknown key");return found->second;
    }
    Value& operator[](const Key& key){return emplace(key).first->second;}

    template<class... Args> std::pair<iterator,bool> emplace(const Key& key,Args&&... args) {
        const auto at=lower_bound(key);
        if(at!=end()&&!(key<at->first))return {at,false};
        const auto index=size_type(at-begin());
        if(at==end()) {
            rows_.emplace_back(std::piecewise_construct,std::forward_as_tuple(key),
                               std::forward_as_tuple(std::forward<Args>(args)...));
        }else {
            // Immutable keys cannot be shifted by assignment. Stage insertion
            // before swapping storage, retaining the original on allocation or
            // value-copy failure.
            storage_type staged;staged.reserve(size()+1);
            for(size_type i=0;i<index;++i)staged.emplace_back(rows_[i]);
            staged.emplace_back(std::piecewise_construct,std::forward_as_tuple(key),
                                std::forward_as_tuple(std::forward<Args>(args)...));
            for(size_type i=index;i<size();++i)staged.emplace_back(rows_[i]);
            rows_.swap(staged);
        }
        return {begin()+static_cast<typename storage_type::difference_type>(index),true};
    }
    std::pair<iterator,bool> insert(const value_type& row){return emplace(row.first,row.second);}
    std::pair<iterator,bool> insert(value_type&& row){return emplace(row.first,std::move(row.second));}
    size_type erase(const Key& key) {
        const auto at=find(key);if(at==end())return 0;
        storage_type staged;staged.reserve(size()-1);
        for(auto i=begin();i!=end();++i)if(i!=at)staged.emplace_back(*i);
        rows_.swap(staged);return 1;
    }
    bool operator==(const SortedMap&)const=default;
    friend bool operator==(const SortedMap& left,const std::map<Key,Value>& right) {
        return left.size()==right.size()&&std::equal(left.begin(),left.end(),right.begin());
    }
    friend bool operator==(const std::map<Key,Value>& left,const SortedMap& right){return right==left;}
private:
    storage_type rows_;
};
}
