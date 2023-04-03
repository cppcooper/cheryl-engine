#pragma once
#ifndef CE_MACROS_H
#define CE_MACROS_H
#include <string>
#include <cstring>
#include <mutex>
#include <type_traits>

#define isclass(type) std::is_class_v<type>
#define isderived(base,type) std::is_base_of_v<base, type>
#define ismethodptr(method) std::is_member_function_pointer_v<decltype(method)>
#define hasmethod(type,method) std::is_member_function_pointer_v<decltype(&type::method)>
#define hasdefconstructor(type) std::is_trivially_default_constructible_v<type>
//todo work for static methods #define hasmethod_any(type,method) std::enable_if<std::is_member_function_pointer_v<decltype(&type::method)>>



#ifdef _MSC_VER
    #define __CEFUNCTION__ __FUNCTION__
    #define CE_STRCPY strcpy_s
    inline std::string ExtractClassName(const char* func){
        std::string raw(func);
        std::string classname;
        if(raw.find("lambda") != std::string::npos){
            classname = raw.substr(0,raw.rfind("::TypeName"));
        } else {
            classname = raw.substr(0,raw.rfind("::"));
        }
        size_t pos = classname.find("class ");
        if( pos != std::string::npos ){
            while( pos != std::string::npos ){
                classname.erase(pos,6);
                pos = classname.find("class ");
            }
            pos = classname.find(" ");
            while( pos != std::string::npos ){
                classname.erase(pos,1);
                pos = classname.find(" ");
            }
        }
        return classname;
    }
#else
    #define __CEFUNCTION__ __PRETTY_FUNCTION__
    #define CE_STRCPY strcpy
    inline std::string ExtractClassName(const char* func){
        std::string raw(func);
        std::string classname;
        if(raw.find("lambda") != std::string::npos){
            classname = raw.substr(0,raw.rfind("::", raw.find("(")));
        } else {
            classname = raw.substr(raw.find(" ",raw.find(" ")+1)+1,raw.rfind("::",raw.find("(")));
        }
        if(classname.find("<") != std::string::npos){
            std::string templatetypeinfo;
            size_t p1 = raw.find("=")+2;
            size_t p2 = raw.find("]");
            templatetypeinfo = raw.substr(p1,p2-p1);
            classname = classname.substr(0,classname.find("<")) + "<" + templatetypeinfo + ">";
        } else {
            classname = classname.substr(0,classname.rfind("::"));
        }
        return classname;
    }
#endif

#define _CE_HERE __CEFUNCTION__, __LINE__

// todo: universalize, this will currently only give unique ids with respect to similarly derived classes
#define TYPEID static uint32_t TypeID_impl() {              \
                    static uint32_t id = id_counter++;      \
                    return id;                              \
               }                                            \
               uint32_t TypeID() const override {           \
                    return TypeID_impl();                   \
               }

#define TYPENAMEAVAILABLE_VIRTUAL \
public: \
virtual const char* TypeName() { \
    static char name[256] = {}; \
    static std::once_flag flag; \
    std::call_once(flag, [&](){ \
        CE_STRCPY(name,ExtractClassName(__CEFUNCTION__).c_str()); \
    });\
    return name; \
}                                 \
private:

#define TYPENAMEAVAILABLE_STATIC \
public: \
static const char* TypeName_static() { \
    static char name[256] = {}; \
    static std::once_flag flag; \
    std::call_once(flag, [&](){ \
        CE_STRCPY(name,ExtractClassName(__CEFUNCTION__).c_str()); \
    });\
    return name; \
} \
private: 

#endif
