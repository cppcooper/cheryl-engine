#pragma once

#include <functional>

class InputBinding
{
protected:
    unsigned int m_Priority = -1;
public:
    virtual void poll() = 0;
};


class InputStateDigital;
class InputBindingDigital : public InputBinding
{
private:
    //todo: std::function<void(...)> m_ActionCallback;
    std::function<void()> m_ActionCallback;
    InputStateDigital* m_State = nullptr;

public:
    InputBindingDigital( InputStateDigital* state, std::function<void()> action, unsigned int priority );
    void poll() override;
};


class InputStateAnalog;
class InputBindingAnalog : public InputBinding
{
private:
    //todo: std::function<void(double, double, ...)> m_RangeCallback;
    std::function<void(double, double)> m_RangeCallback;
    InputStateAnalog* m_State = nullptr;

public:
    InputBindingAnalog( InputStateAnalog* state, std::function<void( double, double )> action, unsigned int priority );
    void poll() override;
};