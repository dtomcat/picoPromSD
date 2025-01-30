#ifndef SHA1_HPP
#define SHA1_HPP

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   C++ version by S.H. <dx4m>
   100% Public Domain
 */

extern "C"{
#include "stdint.h"
}

class SHA1{
private:
    static void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);
public:
    
    typedef struct
    {
        uint32_t state[5];
        uint32_t count[2];
        unsigned char buffer[64];
    } SHA1_CTX;
    
    static void Init(SHA1_CTX * context);
    static void Update(SHA1_CTX * context, const unsigned char *data, uint32_t len);
    static void Final(unsigned char digest[20], SHA1_CTX * context);
    static void Hash(char *hash_out, const char *str, int len);
};

#endif /* SHA1_HPP */
