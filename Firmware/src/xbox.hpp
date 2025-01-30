#ifndef XBOX_HPP
#define XBOX_HPP

#include "sha1.hpp"

enum{
    VERSION_10 = 0,
    VERSION_11 = 1,
    VERSION_16 = 2,
    VERSION_DBG = 3,
};

class XboxCrypto{
private:
    int HMAC1Reset(void);
    int HMAC2Reset(void);
    static void swap_byte(unsigned char *a, unsigned char *b);
protected:
    int ver = -1;
    SHA1::SHA1_CTX ctx;
public:
    
    typedef struct
    {
        unsigned char state[256];
        unsigned char x;
        unsigned char y;
    } RC4KEY;

    typedef struct
    {
        unsigned char HMAC_SHA1_Hash[20];
        unsigned char Confounder[8];
        unsigned char HDDKey[16];
        unsigned char XBERegion[4];
    } eepromdata;

    XboxCrypto();
    XboxCrypto(int version);
    //~XboxCrypto();
    void HMAC(unsigned char *result, ... );
    static void InitRC4(unsigned char *pRC4KeyData, int KeyLen, RC4KEY *pRC4Key);
    static void CryptRC4(unsigned char *pData, int DataLen, RC4KEY *pRC4Key);
    int setVersion(int version);
    int getVersion(void);
    int decrypt(unsigned char *eeprom);
    int encrypt(unsigned char *eeprom);
};

#endif
