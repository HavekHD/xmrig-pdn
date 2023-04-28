#include "pufferfish2.hpp"

size_t pf_encode(char *dst, void *src, size_t size)
{
    uint8_t *dptr = (uint8_t *) dst;
    uint8_t *sptr = (uint8_t *) src;
    uint8_t *end  = (uint8_t *) sptr + size;
    uint8_t c1, c2;

    do
    {
        c1 = *sptr++;
        *dptr++ = itoa64[shr(c1, 2)];
        c1 = shl((c1 & 0x03), 4);

        if (sptr >= end)
        {
            *dptr++ = itoa64[c1];
            break;
        }

        c2 = *sptr++;
        c1 |= shr(c2, 4) & 0x0f;
        *dptr++ = itoa64[c1];
        c1 = shl((c2 & 0x0f), 2);

        if (sptr >= end)
        {
            *dptr++ = itoa64[c1];
            break;
        }

        c2 = *sptr++;
        c1 |= shr(c2, 6) & 0x03;
        *dptr++ = itoa64[c1];
        *dptr++ = itoa64[c2 & 0x3f];
    }
    while (sptr < end);

    return ((char *)dptr - dst);
}

size_t pf_decode(void *dst, char *src, size_t size)
{
    uint8_t *sptr = (uint8_t *) src;
    uint8_t *dptr = (uint8_t *) dst;
    uint8_t *end = (uint8_t *) dst + size;
    uint8_t c1, c2, c3, c4;

    do
    {
        c1 = chr64(*sptr);
        c2 = chr64(*(sptr + 1));

        if (c1 == 255 || c2 == 255)
            break;

        *dptr++ = shl(c1, 2) | shr((c2 & 0x30), 4);
        if (dptr >= end)
            break;

        c3 = chr64(*(sptr + 2));
        if (c3 == 255)
            break;

        *dptr++ = shl((c2 & 0x0f), 4) | shr((c3 & 0x3c), 2);
        if (dptr >= end)
            break;

        c4 = chr64(*(sptr + 3));
        if (c4 == 255)
            break;

        *dptr++ = shl((c3 & 0x03), 6) | c4;
        sptr += 4;
    }
    while (dptr < end);

    return (dptr - (uint8_t *) dst);
}

void pf_hashpass(const void *key_r, const size_t key_sz, uint8_t *out)
{
    unsigned char key[PF_DIGEST_LENGTH]  = { 0 };
    // static salt used
    unsigned char salt[PF_DIGEST_LENGTH] = { 101, 232, 121, 212, 125, 241, 222, 240, 175, 55, 141, 50, 233, 244, 254, 58, 130, 79, 181, 30, 33, 67, 192, 51, 34, 222, 242, 41, 54, 26, 243, 177, 122, 114, 74, 61, 101, 61, 5, 203, 159, 65, 244, 185, 13, 9, 232, 226, 136, 106, 120, 218, 72, 83, 125, 28, 250, 98, 151, 122, 130, 231, 55, 78 };

    uint64_t *salt_u64, *key_u64;
    uint64_t *S[PF_SBOX_N], P[18];
    uint64_t L  = 0, R =  0;
    uint64_t LL = 0, RR = 0;
    uint64_t count = 2, sbox_sz = 8192;
    uint8_t log2_sbox_sz = 13;

    int i, j, k;

    key_u64  = (uint64_t *) &key;
    salt_u64 = (uint64_t *) &salt;

    PF_HMAC(salt, PF_DIGEST_LENGTH, (unsigned char*)key_r, key_sz, key);

    for (i = 0; i < PF_SBOX_N; i++)
    {
        S[i] = (uint64_t *) calloc(sbox_sz, sizeof(uint64_t));

        for (j = 0; j < sbox_sz; j += (PF_DIGEST_LENGTH / sizeof(uint64_t)))
        {
            PF_HMAC(key, PF_DIGEST_LENGTH, salt, PF_DIGEST_LENGTH, key);

            for (k = 0; k < (PF_DIGEST_LENGTH / sizeof(uint64_t)); k++)
                S[i][j + k] = key_u64[k];
        }
    }

    HASH_SBOX(key);

    P[ 0] = 0x243f6a8885a308d3ULL ^ key_u64[0];
    P[ 1] = 0x13198a2e03707344ULL ^ key_u64[1];
    P[ 2] = 0xa4093822299f31d0ULL ^ key_u64[2];
    P[ 3] = 0x082efa98ec4e6c89ULL ^ key_u64[3];
    P[ 4] = 0x452821e638d01377ULL ^ key_u64[4];
    P[ 5] = 0xbe5466cf34e90c6cULL ^ key_u64[5];
    P[ 6] = 0xc0ac29b7c97c50ddULL ^ key_u64[6];
    P[ 7] = 0x3f84d5b5b5470917ULL ^ key_u64[7];
    P[ 8] = 0x9216d5d98979fb1bULL ^ key_u64[0];
    P[ 9] = 0xd1310ba698dfb5acULL ^ key_u64[1];
    P[10] = 0x2ffd72dbd01adfb7ULL ^ key_u64[2];
    P[11] = 0xb8e1afed6a267e96ULL ^ key_u64[3];
    P[12] = 0xba7c9045f12c7f99ULL ^ key_u64[4];
    P[13] = 0x24a19947b3916cf7ULL ^ key_u64[5];
    P[14] = 0x0801f2e2858efc16ULL ^ key_u64[6];
    P[15] = 0x636920d871574e69ULL ^ key_u64[7];
    P[16] = 0xa458fea3f4933d7eULL ^ key_u64[0];
    P[17] = 0x0d95748f728eb658ULL ^ key_u64[1];

    ENCRYPT_P;
    ENCRYPT_S;

    do
    {
        L = R = 0;
        HASH_SBOX(key);
        REKEY(key);
    }
    while (--count);

    //HASH_SBOX(key);
    //memcpy(out, key, PF_DIGEST_LENGTH);
    HASH_SBOX_O(key, out);

    for (i = 0; i < PF_SBOX_N; i++)
        free(S[i]);
}

int pf_crypt(const void *pass, const size_t pass_sz, char *hash)
{
    uint8_t buf[PF_DIGEST_LENGTH] = { 0 };

    pf_hashpass(pass, pass_sz, buf);
    pf_encode(hash + PF_SALTSPACE - 1, buf, PF_DIGEST_LENGTH);

    return 0;
}

int pf_newhash(const char *pass, const size_t pass_sz, char *hash)
{
    return pf_crypt(pass, pass_sz, hash);
}