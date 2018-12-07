#pragma once
#ifndef CEPOOL_H
#define CEPOOL_H

namespace CherylE{
    class iPool : public PoolAbstract{
        protected:
            virtual alloc allocate(size_t &N) = 0;
        public:
            /*returns an array of size N*/
            virtual void* get(size_t N, fitType fit) = 0;
            /*returns size of array*/
            virtual size_t size(void* p) = 0;
            /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
            virtual resizeResult resize(void* &p, size_t N, bool allow_realloc/* = false*/) = 0;
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
            virtual resizeResult resize(typeinterface* &p, size_t N, bool allow_realloc/* = false*/) = 0;
            /*returns all the elements from p to the end of the array p belongs to*/
            virtual void put(typeinterface* p) = 0;
            /*returns all the elements from p to p+N of the array p belongs to*/
            virtual void put(typeinterface* p, size_t N) = 0;
    };
}

#endif