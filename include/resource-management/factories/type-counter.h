namespace CherylE{
    size_t typeCounter(bool count = false){
        static size_t count = 0;
        if(count){
            return ++count;
        }
        return count;
    }
}