#pragma once
#ifndef CEFACTORY_H
#define CEFACTORY_H

namespace CherylE{
    template<class typeinterface, template<typename> class child>
    class iFactory{
    public:
        virtual ~iFactory(){}
        virtual typeinterface* create() = 0;
        virtual void destroy(typeinterface* p) = 0;
        template<typename... Args>
        void construct(typeinterface* p, Args... args){
            static_cast<child<typeinterface>*>(this)->construct_impl(p,std::forward<Args>(args)...);
        }
    };
}

#endif