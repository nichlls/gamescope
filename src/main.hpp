#pragma once

#include <getopt.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <utility>

#include "gamescope_shared.h"

extern const char *gamescope_optstring;
extern const struct option *gamescope_options;

extern std::atomic< bool > g_bRun;

extern int g_nNestedWidth;
extern int g_nNestedHeight;
extern int g_nNestedRefresh; // mHz
extern int g_nNestedUnfocusedRefresh; // mHz
extern int g_nNestedDisplayIndex;

extern uint32_t g_nOutputWidth;
extern uint32_t g_nOutputHeight;
extern bool g_bForceRelativeMouse;
extern int g_nOutputRefresh; // mHz
extern bool g_bOutputHDREnabled;
extern bool g_bForceInternal;

extern bool g_bForceCompositionRotation;
extern uint32_t g_uOutputRotation;

extern bool g_bFullscreen;

extern bool g_bGrabbed;

extern float g_mouseSensitivity;
extern const char *g_sOutputName;

enum class GamescopeUpscaleFilter : uint32_t
{
    LINEAR = 0,
    NEAREST,
    FSR,
    NIS,
    PIXEL,
    SGSR,
    CLEARSCALE,

    FROM_VIEW = 0xF, // internal
};

static constexpr bool UpscaleFilterUsesSharpness( GamescopeUpscaleFilter eFilter )
{
    return eFilter == GamescopeUpscaleFilter::FSR ||
           eFilter == GamescopeUpscaleFilter::NIS ||
           eFilter == GamescopeUpscaleFilter::SGSR;
}

// v0.1 ClearScale consumes encoded, opaque SDR RGB. Linear/scRGB, PQ and
// YCbCr inputs are kept on the established Gamescope filter path.
static constexpr bool ClearScaleSupportsInput( GamescopeAppTextureColorspace eColorspace, bool bYcbcr, bool bHasAlpha = false )
{
	return !bYcbcr && !bHasAlpha && eColorspace == GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB;
}

static constexpr bool ClearScaleSupportsScale( uint32_t inputWidth, uint32_t inputHeight, uint32_t outputWidth, uint32_t outputHeight )
{
    if ( !inputWidth || !inputHeight || !outputWidth || !outputHeight )
        return false;
    const float scaleX = float(outputWidth) / float(inputWidth);
    const float scaleY = float(outputHeight) / float(inputHeight);
    return scaleX >= 1.1f && scaleX <= 2.0f && scaleY >= 1.1f && scaleY <= 2.0f;
}

// cs_sgsr reads the plain sampler slot, not the YCbCr one, and thresholds in 8-bit SDR units.
static constexpr bool SgsrSupportsInput( GamescopeAppTextureColorspace eColorspace, bool bYcbcr )
{
    return !bYcbcr && ( eColorspace == GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR || eColorspace == GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB );
}

// Sharp ran FSR before SGSR existed, so HDR keeps that rather than losing the sharpening. Neither pre-pass reads the YCbCr slot.
static constexpr GamescopeUpscaleFilter ResolveUpscaleFilter( GamescopeUpscaleFilter eFilter, GamescopeAppTextureColorspace eColorspace, bool bYcbcr, bool bHasAlpha = false )
{
	if ( eFilter == GamescopeUpscaleFilter::CLEARSCALE && !ClearScaleSupportsInput( eColorspace, bYcbcr, bHasAlpha ) )
        return bYcbcr ? GamescopeUpscaleFilter::LINEAR : GamescopeUpscaleFilter::FSR;
    if ( eFilter != GamescopeUpscaleFilter::SGSR || SgsrSupportsInput( eColorspace, bYcbcr ) )
        return eFilter;
    return bYcbcr ? GamescopeUpscaleFilter::LINEAR : GamescopeUpscaleFilter::FSR;
}

static constexpr bool DoesHardwareSupportUpscaleFilter( GamescopeUpscaleFilter eFilter )
{
    // Could do nearest someday... AMDGPU DC supports custom tap placement to an extent.

    return eFilter == GamescopeUpscaleFilter::LINEAR;
}

enum class GamescopeUpscaleScaler : uint32_t
{
    AUTO,
    INTEGER,
    FIT,
    FILL,
    STRETCH,
};

struct UpscaleSettings_t
{
    GamescopeUpscaleFilter eFilter{};
    GamescopeUpscaleScaler eScaler{};
    int nSharpness{};
};

// XXX(misyl): This is bad! We shouldnt change the upscaler like this at all!!!
// We should move this to business logic in paint_window or something!
static constexpr UpscaleSettings_t GetUpscaleSettings(
    bool bFocusIsSteam,
    GamescopeUpscaleFilter eWantedFilter,
    GamescopeUpscaleScaler eWantedScaler,
    int nWantedSharpness )
{
    if ( bFocusIsSteam )
        return UpscaleSettings_t{ GamescopeUpscaleFilter::LINEAR, GamescopeUpscaleScaler::FIT, nWantedSharpness };

    return UpscaleSettings_t{ eWantedFilter, eWantedScaler, nWantedSharpness };
}

// One name table for the --filter option and the scaling_filter command, so the two cannot drift.
inline std::optional<GamescopeUpscaleFilter> ParseUpscaleFilter( std::string_view svName )
{
    static constexpr std::pair<std::string_view, GamescopeUpscaleFilter> k_Filters[] =
    {
        { "linear",  GamescopeUpscaleFilter::LINEAR },
        { "nearest", GamescopeUpscaleFilter::NEAREST },
        { "fsr",     GamescopeUpscaleFilter::FSR },
        { "nis",     GamescopeUpscaleFilter::NIS },
        { "pixel",   GamescopeUpscaleFilter::PIXEL },
        { "sgsr",    GamescopeUpscaleFilter::SGSR },
        { "clearscale", GamescopeUpscaleFilter::CLEARSCALE },
    };
    for ( const auto &[svFilterName, eFilter] : k_Filters )
    {
        if ( svFilterName == svName )
            return eFilter;
    }
    return std::nullopt;
}

extern GamescopeUpscaleFilter g_wantedUpscaleFilter;
extern GamescopeUpscaleScaler g_wantedUpscaleScaler;
extern int g_upscaleFilterSharpness;

extern bool g_bBorderlessOutputWindow;

extern bool g_bExposeWayland;

extern bool g_bRt;

extern int g_nXWaylandCount;
extern bool g_bNoTouchPointerEmulation;

extern uint32_t g_preferVendorID;
extern uint32_t g_preferDeviceID;

