namespace CherylE
{
    template<class T>
    class Singleton{
    private:
        Singleton(){}
        //todo: remove, add to get(...)?
        Singleton(...){}
        Singleton(const Singleton&) = delete;
    public:
        static T& getInstance(){
            static T instance;
            return instance;
        }
    };
}