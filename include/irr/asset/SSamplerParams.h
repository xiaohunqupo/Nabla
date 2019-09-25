#ifndef __IRR_S_SAMPLER_PARAMS_H_INCLUDED__
#define __IRR_S_SAMPLER_PARAMS_H_INCLUDED__

#include <cstdint>

namespace irr { namespace asset
{

//! Texture coord clamp mode outside [0.0, 1.0]
enum E_TEXTURE_CLAMP
{
    //! Texture repeats
    ETC_REPEAT = 0,
    //! Texture is clamped to the edge pixel
    ETC_CLAMP_TO_EDGE,
    //! Texture is clamped to the border pixel (if exists)
    ETC_CLAMP_TO_BORDER,
    //! Texture is alternatingly mirrored (0..1..0..1..0..)
    ETC_MIRROR,
    //! Texture is mirrored once and then clamped to edge
    ETC_MIRROR_CLAMP_TO_EDGE,
    //! Texture is mirrored once and then clamped to border
    ETC_MIRROR_CLAMP_TO_BORDER
};

enum E_TEXTURE_BORDER_COLOR
{
    ETBC_FLOAT_TRANSPARENT_BLACK = 0,
    ETBC_INT_TRANSPARENT_BLACK,
    ETBC_FLOAT_OPAQUE_BLACK,
    ETBC_INT_OPAQUE_BLACK,
    ETBC_FLOAT_OPAQUE_WHITE,
    ETBC_INT_OPAQUE_WHITE
};

enum E_TEXTURE_FILTER
{
    ETF_NEAREST = 0,
    ETF_LINEAR
};

enum E_SAMPLER_MIPMAP_MODE
{
    ESMM_NEAREST = 0,
    ESMM_LINEAR
};

enum E_COMPARE_OP
{
    ECO_NEVER = 0,
    ECO_LESS,
    ECO_EQUAL,
    ECO_LESS_OR_EQUAL,
    ECO_GREATER,
    ECO_NOT_EQUAL,
    ECO_GREATER_OR_EQUAL,
    ECO_ALWAYS
};

struct SSamplerParams
{
    struct {
        uint32_t TextureWrapU : 3;
        uint32_t TextureWrapV : 3;
        uint32_t TextureWrapW : 3;
        uint32_t BorderColor : 3;
        uint32_t MinFilter : 1;
        uint32_t MaxFilter : 1;
        uint32_t MipmapMode : 1;
        //! Encoded as power of two (so that if you need 16, Anisotropy should be 4); max value is 5
        uint32_t AnisotropicFilter : 3;
        uint32_t CompareEnable : 1;
        uint32_t CompareFunc : 3;
    };
    float LodBias;
    float MinLod;
    float MaxLod;
};

}}

#endif