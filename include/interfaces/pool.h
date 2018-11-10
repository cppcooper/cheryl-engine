#pragma once
#ifndef CEPOOL_H
#define CEPOOL_H

namespace CherylE{
    class PoolAbstract{
        protected:
            using ptr = void*;
            using masterptr = ptr;
            using open_iter = std::multimap<size_t,alloc>::iterator;
            using closed_iter = std::multimap<ptr,alloc>::iterator;
            struct alloc{
                ptr master;
                ptr head;
                size_t master_size;
                size_t head_size;
            };
            enum fitType{
                bestFit, /*get will return the nearest amount of available memory which fits the query*/
                worstFit /*get will return the largest amount of available memory which fits the query*/
            };
        protected:
            size_t m_free = 0;
            size_t m_used = 0;
            size_t m_total = 0;
            //tracks allocations to prevent memory leaks
            std::unordered_set<masterptr> MasterRecord;
            //lookup table for available allocations
            std::map<ptr,alloc> OpenList;
            //lookup table for available allocations
            std::multimap<size_t,open_iter> LookupTable;
            //lookup table for sub-allocations
            std::multimap<ptr,alloc> ClosedList;
        public:
            /*frees all memory*/
            virtual ~PoolAbstract(){
                purge();
            }
            /*frees all memory*/
            virtual void purge() = 0;
            /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
            virtual void pre_allocate(size_t N, size_t blocks) = 0;
            /*returns how many bytes/objects are available*/
            size_t free()const{ return m_free; };
            /*returns how many bytes/objects are not available*/
            size_t used()const{ return m_used; };
            /*returns how many bytes/objects are allocated*/
            size_t total()const{ return m_total; };
    };

    class iPool : public PoolAbstract{
        protected:
            virtual alloc allocate(size_t &bytes) = 0;
            virtual void moveto_open(closed_iter iter, size_t N) = 0;
            virtual closed_iter find_closed(void* p) = 0;
        public:
            /*returns an array of size N*/
            virtual void* get(size_t N, fitType fit) = 0;
            /*returns size of array*/
            virtual size_t size(void* p) = 0;
            /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
            virtual bool resize(void* &p, size_t N, bool allow_realloc/* = false*/) = 0;
            /*returns all the elements from p to the end of the array p belongs to*/
            virtual void put(void* p) = 0;
            /*returns all the elements from p to p+N of the array p belongs to*/
            virtual void put(void* p, size_t N) = 0;
    };

    template<class typeinterface, template<typename> class child> //todo: child appears to be redundant
    class iPoolT : public PoolAbstract{ //remove iPool, duplicate interface with T*
        private:
            static_assert(isclass(typeinterface),"In iPoolT<T>, T must be a class.");
            static_assert(hasmethod(T,TypeName),"In iPoolT<T>, T must implement the method TypeName");
            static_assert(hasmethod(T,get_typeID),"In iPoolT<T>, T must implement the method typeID");
        protected:
            virtual alloc allocate(size_t N) = 0;
            virtual void moveto_open(closed_iter iter, size_t N) = 0;
            virtual closed_iter find_closed(typeinterface* p) = 0;
        public:
            /*returns an array of size N*/
            virtual typeinterface* get(size_t N, fitType fit/* = fitType::worstFit*/) = 0;
            /*returns size of array from p onward*/
            virtual size_t size(typeinterface* p) = 0;
            /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
            virtual bool resize(typeinterface* &p, size_t N, bool allow_realloc/* = false*/) = 0;
            /*returns all the elements from p to the end of the array p belongs to*/
            virtual void put(typeinterface* p) = 0;
            /*returns all the elements from p to p+N of the array p belongs to*/
            virtual void put(typeinterface* p, size_t N) = 0;
    };
}

#endif