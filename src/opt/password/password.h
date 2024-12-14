/* password.h
 * Helper for encoding game state into opaque tokens for persistence.
 * Tokens are G0 excluding space, quote, and backslash: They can be pasted directly into JSON string tokens.
 * A 1-bit change anywhere in the plaintext is guaranteed to produce very different output, in multiple places.
 * It's not exactly high-end cryptography, but the tokens are definitely obscure enough that no one would understand them by accident.
 *
 * Suppose you have all of the encodable state in a struct:
 *   struct my_state {
 *     uint16_t hp;
 *     uint16_t gold;
 *     uint8_t flags[32];
 *     // or whatever
 *   } my_state;
 *
 * To save it:
 *   PASSWORD_ENCODE(my_state)
 *   if (passwordc<0) return -1;
 *   if (egg_store_set("save",4,password,passwordc)<0) return -1;
 *
 * To load it:
 *   char tmp[1024]; // <-- Exact size necessary is knowable from sizeof(my_state), if you care.
 *   int tmpc=egg_store_get(tmp,sizeof(tmp),"save",4);
 *   if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
 *   PASSWORD_DECODE(my_state,tmp,tmpc)
 *   if (!decode_ok) return -1;
 *   // Then do your own consistency checks if desired.
 */
 
#ifndef PASSWORD_H
#define PASSWORD_H

/* XOR each byte against the previous *filtered* byte.
 * (or if you want to XOR against *unfiltered* bytes, just do them in the wrong order).
 */
void password_filter(void *v,int c);
void password_unfilter(void *v,int c);

/* Simple position-sensitive checksum.
 * Importantly, it has a nonzero initial state, so if your payload is straight zeroes it will still come out noisy.
 */
unsigned int password_checksum(const void *v,int c);

/* Add or subtract the index of each byte to itself, to obfuscate patterns.
 */
void password_add_index(void *v,int c);
void password_sub_index(void *v,int c);

/* Base64 using an unusual custom alphabet.
 * Binary length must be a multiple of 3, and text 4.
 * It only contains G0, and no quote or backslash, so the encoded text can paste right into a JSON string token.
 */
int password_b64_encode(char *dst,int dsta,const void *src,int srcc);
int password_b64_decode(void *dst,int dsta,const char *src,int srcc);

/* Recommended sequence for encoding:
 *  - Assemble your plaintext data with a checksum in front. Size must be a multiple of 3.
 *  - password_add_index()
 *  - password_filter()
 *  - password_b64_encode()
 * Recommended sequence for decoding:
 *  - password_b64_decode()
 *  - password_unfilter()
 *  - password_sub_index()
 *  - Validate checksum.
 * Or if you want to operate directly against a struct:
 *  - Beware that multi-byte fields will fail for native builds on big-endian hosts. Wasm builds are fine, wasm is always little-endian.
 *  - Include a uint32_t checksum at the start, and uint16_t pad at the end.
 *  - srcc=sizeof(my_struct)-sizeof(my_struct)%3; // Force a multiple of three by eliminating some of pad.
 *  - Beyond that, follow the steps above.
 * Or better yet, use the macros below.
 */
 
#define PASSWORD_ENCODE(mystruct) \
  struct { unsigned int checksum; typeof(mystruct) payload; int pad; } _pe_tmp={0,mystruct,0}; \
  char password[(sizeof(_pe_tmp)/3)*4]; \
  int passwordc=-1; \
  { \
    int srcc=sizeof(_pe_tmp)-sizeof(_pe_tmp)%3; \
    _pe_tmp.checksum=password_checksum(&_pe_tmp.payload,srcc-4); \
    password_add_index(&_pe_tmp,srcc); \
    password_filter(&_pe_tmp,srcc); \
    passwordc=password_b64_encode(password,sizeof(password),&_pe_tmp,srcc); \
    if (passwordc>sizeof(password)) passwordc=-1; \
  }
  
#define PASSWORD_DECODE(mystruct,password,passwordc) \
  int decode_ok=0; \
  { \
    struct { unsigned int checksum; typeof(mystruct) payload; int pad; } tmp={0}; \
    int tmpc=password_b64_decode(&tmp,sizeof(tmp),password,passwordc); \
    if ((tmpc>=sizeof(tmp)-2)&&(tmpc<=sizeof(tmp))) { \
      password_unfilter(&tmp,tmpc); \
      password_sub_index(&tmp,tmpc); \
      if (tmp.checksum==password_checksum(&tmp.payload,tmpc-4)) { \
        mystruct=tmp.payload; \
        decode_ok=1; \
      } \
    } \
  }

#endif
