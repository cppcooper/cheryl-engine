#pragma once
#ifndef CEFACTORY_H
#define CEFACTORY_H

namespace CherylE{
    template<class typeinterface, template<typename> class child>
    class iFactory{
        static_assert(isclass(typeinterface), "In iFactory<T,childFactory>, T must be a class.");
        static_assert(hasmethod(T,TypeName),"In iFactory<T,childFactory>, T must implement the method TypeName");
        static_assert(hasmethod(T,get_typeID),"In iFactory<T,childFactory>, T must implement the method typeID");
    protected:
        size_t typeID = 0;
    public:
        virtual ~iFactory(){}
        virtual typeinterface* create() = 0;
        virtual void destroy(typeinterface* p) = 0;
        template<typename... Args>
        void constructT(typeinterface* p, Args... args){
            static_cast<child<typeinterface>*>(this)->construct_impl(p,std::forward<Args>(args)...);
        }
        virtual const char* TypeName() = 0;
        size_t get_typeID()const{ return typeID; }
    };
}

#endif