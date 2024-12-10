extern "C"{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
}

#include "xbox.hpp"
#include "sha1.hpp"


XboxCrypto::XboxCrypto(){
    
}

XboxCrypto::XboxCrypto(int version){
    this->setVersion(version);
}

/* values taken from https://svn.exotica.org.uk:8443/xbmc4xbox/tags/3.5-rc2/xbmc/xbox/XKSHA1.cpp */
int XboxCrypto::HMAC1Reset(void){
    switch (ver)
    {
        case VERSION_DBG:
            ctx.state[0] = 0x85F9E51A;
            ctx.state[1] = 0xE04613D2;
            ctx.state[2] = 0x6D86A50C;
            ctx.state[3] = 0x77C32E3C;
            ctx.state[4] = 0x4BD717A4;
            break;
        case VERSION_10:
            ctx.state[0] = 0x72127625;
            ctx.state[1] = 0x336472B9;
            ctx.state[2] = 0xBE609BEA;
            ctx.state[3] = 0xF55E226B;
            ctx.state[4] = 0x99958DAC;
            break;
        case VERSION_11:
            ctx.state[0] = 0x39B06E79;
            ctx.state[1] = 0xC9BD25E8;
            ctx.state[2] = 0xDBC6B498;
            ctx.state[3] = 0x40B4389D;
            ctx.state[4] = 0x86BBD7ED;
            break;
        case VERSION_16:
            ctx.state[0] = 0x8058763a;
            ctx.state[1] = 0xf97d4e0e;
            ctx.state[2] = 0x865a9762;
            ctx.state[3] = 0x8a3d920d;
            ctx.state[4] = 0x08995b2c;
            break;
    }
    ctx.count[0] = 512;
    return 0;
}

int XboxCrypto::HMAC2Reset(){
    switch (ver)
    {
        case VERSION_DBG:
            ctx.state[0] = 0x5D7A9C6B;
            ctx.state[1] = 0xE1922BEB;
            ctx.state[2] = 0xB82CCDBC;
            ctx.state[3] = 0x3137AB34;
            ctx.state[4] = 0x486B52B3;
            break;
        case VERSION_10:
            ctx.state[0] = 0x76441D41;
            ctx.state[1] = 0x4DE82659;
            ctx.state[2] = 0x2E8EF85E;
            ctx.state[3] = 0xB256FACA;
            ctx.state[4] = 0xC4FE2DE8;
            break;
        case VERSION_11:
            ctx.state[0] = 0x9B49BED3;
            ctx.state[1] = 0x84B430FC;
            ctx.state[2] = 0x6B8749CD;
            ctx.state[3] = 0xEBFE5FE5;
            ctx.state[4] = 0xD96E7393;
            break;
        case VERSION_16:
            ctx.state[0] = 0x01075307;
            ctx.state[1] = 0xa2f1e037;
            ctx.state[2] = 0x1186eeea;
            ctx.state[3] = 0x88da9992;
            ctx.state[4] = 0x168a5609;
            break;
    }
    ctx.count[0] = 512;
    return 0;
}

void XboxCrypto::swap_byte(unsigned char *a, unsigned char *b){
    unsigned char swapByte = *a;
    *a = *b;
    *b = swapByte;
}

void XboxCrypto::HMAC(unsigned char *result, ... ){
    va_list args;
    
    va_start(args, result);
    
    unsigned char res[0x14];
    
    SHA1::Init(&ctx);
    this->HMAC1Reset();
    
    while(1){
        unsigned char *buffer = va_arg(args, unsigned char *);
        int len;
        if(buffer == NULL) break;
        len = va_arg(args, int);
        SHA1::Update(&ctx, buffer, len);
    }
    va_end(args);
    SHA1::Final(res, &ctx);
    
    SHA1::Init(&ctx);
    this->HMAC2Reset();
    
    SHA1::Update(&ctx, res, 0x14);
    SHA1::Final(result, &ctx);
}

void XboxCrypto::InitRC4(unsigned char *pRC4KeyData, int KeyLen, RC4KEY *pRC4Key){
    unsigned char index1;
    unsigned char index2;
    unsigned char *state;
    short counter;
    
    state = &pRC4Key->state[0];
    
    for(counter = 0; counter < 256; counter++){
        state[counter] = (unsigned char)counter;
    }
    
    pRC4Key->x = 0;
    pRC4Key->y = 0;
    index1 = 0;
    index2 = 0;
    
    for(counter = 0; counter < 256; counter++){
        index2 = (pRC4KeyData[index1] + state[counter] + index2) % 256;
        swap_byte(&state[counter], &state[index2]);
        index1 = (index1 + 1) % KeyLen;
    }
}
void XboxCrypto::CryptRC4(unsigned char *pData, int DataLen, RC4KEY *pRC4key){
    unsigned char x, y, xorIndex;
    unsigned char *state;
    long counter;
    
    x = pRC4key->x;
    y = pRC4key->y;
    state = &pRC4key->state[0];
    
    for(counter = 0; counter < DataLen; counter++){
        x = (x + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        pData[counter] ^= state[xorIndex];
    }
    
    pRC4key->x = x;
    pRC4key->y = y;
}

int XboxCrypto::setVersion(int version){
  int cur = ver;
  ver = version;
  return cur;
}

int XboxCrypto::getVersion(void){
  return ver;
}

int XboxCrypto::decrypt(unsigned char *eeprom){

  eepromdata data;
  unsigned char key_hash[20];
  unsigned char hash_confirm[20];
  int ret = -1;

  int verloop = VERSION_10;
  while(verloop <= VERSION_DBG){
    memcpy(&data, eeprom, sizeof(eepromdata));
    this->setVersion(verloop);
    this->HMAC(key_hash, &data.HMAC_SHA1_Hash, 20, NULL);
    XboxCrypto::RC4KEY pRC4;
    XboxCrypto::InitRC4(key_hash, 20, &pRC4);
    XboxCrypto::CryptRC4((unsigned char*)&data+20, 28, &pRC4);

    this->HMAC(hash_confirm, &data.Confounder, 8, &data.HDDKey, 16, &data.XBERegion, 4, NULL);
    if(memcmp(hash_confirm, &data.HMAC_SHA1_Hash, 20) == 0){
      ret = verloop;
      memcpy(eeprom, &data, sizeof(eepromdata));
      break;
    }
    verloop++;
  }
  if(ret == -1) this->setVersion(-1);
  
  return ret;
}
int XboxCrypto::encrypt(unsigned char *eeprom){
  if(this->getVersion() == -1){
    return -1;
  }
  eepromdata data;

  unsigned char key_hash[20];
  memcpy(&data, eeprom, sizeof(eepromdata));

  this->HMAC(key_hash, &data.Confounder, 8, &data.HDDKey, 16, &data.XBERegion, 4, NULL);
  memcpy(data.HMAC_SHA1_Hash, key_hash, 0x14);
  this->HMAC(key_hash, &data.HMAC_SHA1_Hash, 20, NULL);
  
  XboxCrypto::RC4KEY pRC4;
  XboxCrypto::InitRC4(key_hash, 20, &pRC4);
  XboxCrypto::CryptRC4((unsigned char *)&data+20, 28, &pRC4);

  memcpy(eeprom, &data, sizeof(eepromdata));

  return 0;
}
