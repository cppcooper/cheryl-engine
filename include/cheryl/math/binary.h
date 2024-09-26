#ifndef DATA_STRUCTURES
#define DATA_STRUCTURES

#include <ostream>
#include <cinttypes>


typedef uint8_t ubyte;

/*This class is the return type of an operator[] overload
it is used only when u8bits has been accessed as an array

This class allows u8bits to be used as a boolean after being indexed*/
class u8bits_access {
public:
    u8bits_access(int Bit, ubyte* Byte) {
        B = Byte;
        b = Bit;
        bitmask = 1<<b;
    }

    bool operator==(bool a) {
        if (a)
            return ((*B&bitmask) != 0);
        else
            return ((*B&bitmask) == 0);
    }

    u8bits_access& operator=(bool a) {
        if (a)
            *B |= bitmask;
        else
            *B &= ~bitmask;
        return *this;
    }

    bool operator()() {
        return ((*B&bitmask) != 0);
    }

private:
    ubyte* B;                    //Points to Byte's address
    ubyte b;                    //Specifies the bit position
    ubyte bitmask;                //Bitmask based on bit position
};
//This struct allows the access of individual bits in a byte as an array
struct u8bits {
    ubyte B;

    friend std::ostream& operator<<(std::ostream& out, const u8bits& obj) {
        for (int i = 8; i > 0; --i) {
            int bitmask = (1<<i);
            if (obj.B&bitmask)
                out<<"1";
            else
                out<<"0";
        }
        return out;
    }

    u8bits_access operator[](int i) {
        //Since we can't overload operators inside of an operator overload
        //we therefore return an object whose operators are overloaded
        u8bits_access R = u8bits_access(i, &B);
        return R;
    }
};																	//This union allows us to access a byte and its individual bits

union u8byte {
    ubyte Byte;
    u8bits Bit;
};

#endif
