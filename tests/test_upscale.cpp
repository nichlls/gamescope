#include <catch2/catch_test_macros.hpp>

#include "main.hpp"

TEST_CASE("SGSR keeps the wire value Steam writes", "[upscale]") {
	REQUIRE( uint32_t( GamescopeUpscaleFilter::SGSR ) == 5 );
	REQUIRE( uint32_t( GamescopeUpscaleFilter::CLEARSCALE ) == 6 );
}

TEST_CASE("SGSR only takes SDR RGB input", "[upscale]") {
	REQUIRE( SgsrSupportsInput( GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false ) );
	REQUIRE( SgsrSupportsInput( GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, false ) );
	REQUIRE_FALSE( SgsrSupportsInput( GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, true ) );
	for ( auto colorspace : { GAMESCOPE_APP_TEXTURE_COLORSPACE_SCRGB, GAMESCOPE_APP_TEXTURE_COLORSPACE_HDR10_PQ, GAMESCOPE_APP_TEXTURE_COLORSPACE_PASSTHRU } )
		REQUIRE_FALSE( SgsrSupportsInput( colorspace, false ) );
}

TEST_CASE("SGSR falls back on input it cannot read", "[upscale]") {
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::SGSR, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, false ) == GamescopeUpscaleFilter::SGSR );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::SGSR, GAMESCOPE_APP_TEXTURE_COLORSPACE_HDR10_PQ, false ) == GamescopeUpscaleFilter::FSR );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::SGSR, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, true ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::NIS, GAMESCOPE_APP_TEXTURE_COLORSPACE_HDR10_PQ, false ) == GamescopeUpscaleFilter::NIS );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::CLEARSCALE, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, false ) == GamescopeUpscaleFilter::CLEARSCALE );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::CLEARSCALE, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, false, true ) == GamescopeUpscaleFilter::FSR );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::CLEARSCALE, GAMESCOPE_APP_TEXTURE_COLORSPACE_LINEAR, false ) == GamescopeUpscaleFilter::FSR );
	REQUIRE( ResolveUpscaleFilter( GamescopeUpscaleFilter::CLEARSCALE, GAMESCOPE_APP_TEXTURE_COLORSPACE_SRGB, true ) == GamescopeUpscaleFilter::LINEAR );
}

TEST_CASE("ClearScale only accepts qualified per-axis scale ratios", "[upscale]") {
	REQUIRE( ClearScaleSupportsScale( 640, 400, 1280, 800 ) );
	REQUIRE( ClearScaleSupportsScale( 853, 533, 1280, 800 ) );
	REQUIRE_FALSE( ClearScaleSupportsScale( 1280, 800, 1280, 800 ) );
	REQUIRE_FALSE( ClearScaleSupportsScale( 640, 400, 1280, 439 ) );
	REQUIRE_FALSE( ClearScaleSupportsScale( 320, 200, 1280, 800 ) );
}

TEST_CASE("ParseUpscaleFilter names every filter", "[upscale]") {
	REQUIRE( ParseUpscaleFilter( "linear" ) == GamescopeUpscaleFilter::LINEAR );
	REQUIRE( ParseUpscaleFilter( "nearest" ) == GamescopeUpscaleFilter::NEAREST );
	REQUIRE( ParseUpscaleFilter( "fsr" ) == GamescopeUpscaleFilter::FSR );
	REQUIRE( ParseUpscaleFilter( "nis" ) == GamescopeUpscaleFilter::NIS );
	REQUIRE( ParseUpscaleFilter( "pixel" ) == GamescopeUpscaleFilter::PIXEL );
	REQUIRE( ParseUpscaleFilter( "sgsr" ) == GamescopeUpscaleFilter::SGSR );
	REQUIRE( ParseUpscaleFilter( "clearscale" ) == GamescopeUpscaleFilter::CLEARSCALE );
	REQUIRE_FALSE( ParseUpscaleFilter( "sharp" ).has_value() );
}

TEST_CASE("UpscaleFilterUsesSharpness", "[upscale]") {
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::SGSR ) );
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::FSR ) );
	REQUIRE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::NIS ) );
	REQUIRE_FALSE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::LINEAR ) );
	REQUIRE_FALSE( UpscaleFilterUsesSharpness( GamescopeUpscaleFilter::PIXEL ) );
}

TEST_CASE("GetUpscaleSettings", "[upscale]") {
	SECTION("a Steam focus window forces linear and fit") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			true, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER, 7 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::LINEAR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::FIT );
		REQUIRE( settings.nSharpness == 7 );
	}

	SECTION("a non-Steam focus window keeps the wanted settings") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::FSR, GamescopeUpscaleScaler::INTEGER, 7 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::FSR );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::INTEGER );
		REQUIRE( settings.nSharpness == 7 );
	}

	SECTION("passes through a different wanted filter and scaler pair") {
		const UpscaleSettings_t settings = GetUpscaleSettings(
			false, GamescopeUpscaleFilter::NEAREST, GamescopeUpscaleScaler::AUTO, 0 );

		REQUIRE( settings.eFilter == GamescopeUpscaleFilter::NEAREST );
		REQUIRE( settings.eScaler == GamescopeUpscaleScaler::AUTO );
	}
}
