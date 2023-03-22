#pragma once
#ifndef CHERYL_ENGINE_FACTORY_H
#define CHERYL_ENGINE_FACTORY_H

#include <string>
#include <cinttypes>

class ManagedObject;

class AbstractFactory
{
protected:
    AbstractFactory() = default;

public:
    virtual ManagedObject* Create( uint32_t N = 1 ) = 0;
    //TODO: virtual bool Destroy( ManagedObject* p ) = 0;
    virtual uint32_t Get_TypeID() = 0;
    virtual std::string& TypeExtensions() = 0;
    virtual std::string& RecordExtension() = 0;
    virtual bool IsFactoryType( ManagedObject* p ) = 0;
};

#endif //CHERYL_ENGINE_FACTORY_H
