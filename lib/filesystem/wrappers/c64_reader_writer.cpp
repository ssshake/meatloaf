#include "c64_reader_writer.h"

// from https://style64.org/petscii/

const char16_t U8Char::utf8map[] = {
//  ---0,  ---1,  ---2,  ---3,  ---4,  ---5,  ---6,  ---7,  ---8,  ---9,  --10,  --11,  --12,  --13,  --14,  --15    
       0,     0,     0,     3,     0,     0,     0,     0,     0,     0,     0,     0,     0,    10,     0,     0,
       0,     0,     0,     0,   0x8,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,  0x28,  0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f, // punct
    0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d,  0x3e,  0x3f, // numbers

    0x40,  0x61,  0x62,  0x63,  0x64,  0x65,  0x66,  0x67,  0x68,  0x69,  0x6a,  0x6b,  0x6c,  0x6d,  0x6e,  0x6f, // a-z
    0x70,  0x71,  0x72,  0x73,  0x74,  0x75,  0x76,  0x77,  0x78,  0x79,  0x7a,  0x5b,  0xa3,  0x5d,0x2191,0x2190,

  0x2500,  0x41,  0x42,  0x43,  0x44,  0x45,  0x46,  0x47,  0x48,  0x49,  0x4a,  0x4b,  0x4c,  0x4d,  0x4e,  0x4f, // A-Z
    0x50,  0x51,  0x52,  0x53,  0x54,  0x55,  0x56,  0x57,  0x58,  0x59,  0x5a,0x253c,  0x00,0x2502,  0x00,  0x00,

    0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,0x2028,  0x00,  0x00, // control codes
    0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,

    0xa0,0x258c,0x2584,0x2594,0x2581,0x258e,0x2592,  0x00,  0x00,  0x00,  0x00,0x251c,0x2597,0x2514,0x2510,0x2582, // tables etc.
  0x250c,0x2534,0x252c,0x2524,0x258e,0x258d,  0x00,  0x00,  0x00,0x2583,0x2713,0x2596,0x259d,0x2518,0x2598,0x259a,
  0x2500,  0x41,  0x42,  0x43,  0x44,  0x45,  0x46,  0x47,  0x48,  0x49,  0x4a,  0x4b,  0x4c,  0x4d,  0x4e,  0x4f, // A-Z
    0x50,  0x51,  0x52,  0x53,  0x54,  0x55,  0x56,  0x57,  0x58,  0x59,  0x5a,0x253c,  0x00,0x2502,  0x00,  0x00, 
    0xa0,0x258c,0x2584,0x2594,0x2581,0x258e,0x2592,  0x00,  0x00,  0x00,  0x00,0x251c,0x2597,0x2514,0x2510,0x2582, // tables etc.
  0x250c,0x2534,0x252c,0x2524,0x258e,0x258d,  0x00,  0x00,  0x00,0x2583,0x2713,0x2596,0x259d,0x2518,0x2598,  0x00

};
