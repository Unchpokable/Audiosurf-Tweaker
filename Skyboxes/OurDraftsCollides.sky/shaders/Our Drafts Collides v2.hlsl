// Our Drafts Collides - two skies in one, split by the horizon.
//
// Audiosurf pitches the camera down hard on any fast descent - up to about sixty degrees under the
// horizon - and swings the track's bearing constantly. A sky designed as one dome is therefore a sky
// the player sees perhaps half of. This one is built the other way round: the upper hemisphere is
// the calm sky (Chroma - cloud, aurora, an iridescent film), the lower is the angry one (Dying Sun -
// a swollen eclipse arc under a red haze), and which one fills the screen is decided by how the song
// is going.
//
// WHAT MAKES IT AFFORDABLE
//
// [branch] on d.y. A wave of pixels entirely above the horizon runs only the upper sky; entirely
// below, only the lower. Both are paid for only in the blend band, which is a thin strip of screen.
// So this costs about what *one* of the two costs, not their sum - which is the whole reason a
// composition this size fits in the budget at all.
//
// The same trick again, one level up: every layer sits behind [branch] on its own intensity, and
// those are uniforms, so the branch is perfectly coherent - every pixel takes the same side. A layer
// turned off costs nothing rather than being multiplied by zero. That makes the layer knobs a real
// performance control alongside the render-scale slider: an old card can switch the film off and buy
// the frame time back.
//
// THE RULE THAT FALLS OUT OF THAT
//
// Screen-space derivatives are undefined inside dynamic flow control. Everything that measures a
// pixel - fwidth for the disc edges, the angular pixel size the thin features clamp to - is
// therefore computed in main(), before the first branch, and passed down as a value. Nothing below
// this line may call fwidth or ddx.
//
// COORDINATES
//
// The direction is the only coordinate, as in Chroma: d.y for height bands, dot(d,k) for angular
// distance, 3D noise on d for isotropic detail, and a bounded plane projection for the two layered
// things - clouds above, haze below. Same projection, mirrored:
//
//     above:  d.xz / (max( d.y, 0) + c)
//     below:  d.xz / (max(-d.y, 0) + c)
//
// No equirectangular anything, and so no pole and no seam in the composition itself. The one atan2
// is inside the arc, measuring position *around* the ring, and it brings two singular places rather
// than one - see the note above lower_arc(). Both are handled there, and the second one was found
// the hard way: it drew itself on screen as a hard-edged wedge.
//
// WHAT WAS DROPPED FROM DYING SUN
//
// The 48-step volumetric ground fog. It was the most expensive thing in that shader by a wide margin
// and it drew the one part of the picture that the track sits in front of. The red haze it also had
// is kept, and stays what it always was - a direct fbm, never a march.
#include "sky_common.hlsli"

/* @sky
name    = Our Drafts Collides v2
version = 1

group = Horizon
param = g_blend.x | Split (deg)     | 0.0  | -40.0 | 40.0
param = g_blend.y | Blend (deg)     | 14.0 | 1.0   | 60.0
param = g_blend.z | Filmic tonemap  | 0.85 | 0.0   | 1.0
param = g_blend.w | Speed           | 0.50 | 0.0   | 2.0

// Tells the plugin where this program keeps its sun, so anything drawn over the sky - the
// geometry layer's clouds - is lit by the same light instead of by its own guess. One line
// rather than a second set of the same knobs to keep in step.
sun = degrees | g_sun.x | g_sun.y
// The twin. Its bearing is authored as an offset from the first, which is why the binding takes
// an optional fourth field instead of only absolute values.
sun = degrees | g_sun.x | g_sun.y | g_sun.z

group = Suns
param = g_sun.x  | Bearing (deg)      | -14.48 | -180.0 | 180.0
param = g_sun.y  | Elevation (deg)    | 29.00  | -20.0  | 70.0
param = g_sun.z  | Twin offset (deg)  | 180.0 | 0.0    | 180.0
param = g_sun.w  | Twin strength      | 1.00   | 0.0    | 1.0

group = Eclipse
param = g_eclipse.x    | Size (deg)         | 5.32  | 0.20 | 20.0
param = g_eclipse.y    | Ring width         | 0.039 | 0.0  | 0.40
param = g_eclipse.z    | Moon offset X      | 0.028 | -0.5 | 0.5
param = g_eclipse.w    | Moon offset Y      | -0.024| -0.5 | 0.5
param = g_eclipse_fx.x | Core brightness    | 0.00  | 0.0  | 8.0
param = g_eclipse_fx.y | Corona             | 3.00  | 0.0  | 3.0
param = g_eclipse_fx.z | Inner bleed        | 4.00  | 0.0  | 4.0
param = g_eclipse_fx.w | Flicker            | 0.247 | 0.0  | 1.0
param = g_glow.x       | Corona reach       | 12.00 | 0.3  | 12.0
param = g_glow.y       | Sky glow           | 0.00  | 0.0  | 2.0
param = g_glow.z       | Sky glow tightness | 600.0 | 4.0  | 600.0
color = g_sun_tint     | Core colour        | 1.0,0.96,0.82
color = g_corona       | Corona tint        | 1.0,0.84,0.52
color = g_moon         | Moon disc          | 0.010,0.012,0.018
param = g_bleed.x      | Bleed depth        | 0.070 | 0.01 | 0.50
param = g_bleed.y      | Bleed sharpness    | 2.50  | 0.3  | 8.0

group = Sun veil
param = g_veil.x   | Amount        | 0.55  | 0.0 | 1.0
param = g_veil.y   | Reach (deg)   | 26.0  | 2.0 | 90.0
param = g_veil.z   | Horizon amount| 0.30  | 0.0 | 1.0
param = g_veil.w   | Brightness    | 1.10  | 0.0 | 4.0
color = g_veil_tint | Tint         | 0.72,0.78,0.92

group = Upper sky
color = g_zenith  | Zenith      | 0.0342,0.1485,0.2857
color = g_horizon | Horizon     | 0.0713,0.0290,0.2780
color = g_warm    | Warm side   | 0.42,0.13,0.12
param = g_warm.w  | Warm amount | 0.30 | 0.0 | 1.0

group = Stars
param = g_star.x    | Density    | 0.09 | 0.0  | 0.60
param = g_star.y    | Brightness | 1.40 | 0.0  | 6.0
param = g_star.z    | Size (deg) | 0.11 | 0.02 | 0.60
param = g_star.w    | Twinkle    | 0.55 | 0.0  | 1.0
color = g_star_tint | Tint       | 0.80,0.90,1.0

group = Wisps
param = g_wisp.x      | Intensity       | 0.16 | 0.0  | 0.8
param = g_wisp.y      | Thickness (deg) | 0.55 | 0.05 | 4.0
param = g_wisp.z      | Wander (deg)    | 4.50 | 0.0  | 20.0
param = g_wisp.w      | Spacing (deg)   | 9.00 | 1.0  | 30.0
param = g_wisp_tint.w | Centre (deg)    | 16.0 | -30.0| 60.0
color = g_wisp_tint   | Tint            | 0.75,0.90,1.0

group = Aurora
param = g_aurora.x  | Intensity       | 0.45 | 0.0  | 2.0
param = g_aurora.y  | Base elev (deg) | 12.0 | -10.0| 60.0
param = g_aurora.z  | Curtain height  | 22.0 | 1.0  | 60.0
param = g_aurora.w  | Ray width (deg) | 0.80 | 0.08 | 6.0
color = g_aurora_lo | Lower colour    | 0.15,0.85,0.55
color = g_aurora_hi | Upper colour    | 0.20,0.65,0.95
param = g_aurora2.x | Cluster size (deg)| 55.0 | 10.0 | 180.0
param = g_aurora2.y | Cluster rarity    | 0.62 | 0.0  | 0.95
param = g_aurora2.z | Cluster drift     | 0.05 | 0.0  | 0.5
param = g_aurora3.w | Diffuse sheet     | 0.30 | 0.0  | 1.0
param = g_aurora3.y | Ray contrast      | 1.60 | 0.6  | 6.0
param = g_aurora3.z | Base edge (deg)   | 2.60 | 0.2  | 12.0
param = g_aurora4.x | Crest size (deg)   | 22.0 | 3.0  | 60.0
param = g_aurora4.y | Ragged base        | 0.35 | 0.0  | 1.0
param = g_aurora4.z | Spike reach        | 0.45 | 0.0  | 1.0

group = Gold film
param = g_film.x    | Opacity      | 0.34 | 0.0  | 1.0
param = g_film.y    | Detail (deg) | 5.50 | 0.4  | 30.0
param = g_film.z    | Warp         | 3.56 | 0.0  | 5.0
param = g_film.w    | Fringes      | 2.00 | 2.0  | 70.0
color = g_gold_base | Base colour  | 0.88,0.68,0.20
color = g_gold_hi   | Highlight    | 1.0,0.94,0.70
param = g_film2.x   | Filament     | 0.75 | 0.0 | 1.0
param = g_film2.y   | Filament edge| 3.00 | 1.0 | 20.0
param = g_film2.z   | Stretch      | 2.60 | 1.0 | 8.0

group = Lower sky
color = g_ground | Under horizon | 0.09,0.03,0.05
color = g_under  | Deep          | 0.015,0.004,0.010
param = g_under.w | Depth        | 2.20 | 0.3 | 8.0

group = Eclipse arc
param = g_arc.x     | Radius (deg)    | 40.66 | 5.0   | 120.0
param = g_arc.y     | Width (deg)     | 0.651 | 0.05  | 12.0
param = g_arc.z     | Span (deg)      | 264.1 | 0.0   | 360.0
param = g_arc.w     | Intensity       | 0.694 | 0.0   | 4.0
param = g_arc_dir.x | Bearing (deg)   | -45.52| -180.0| 180.0
param = g_arc_dir.y | Elevation (deg) | -52.53| -90.0 | 20.0
param = g_arc_dir.z | Flicker         | 0.470 | 0.0   | 1.0
param = g_arc_dir.w | Glow (deg)      | 8.00  | 0.2   | 30.0
color = g_arc_core  | Core colour     | 1.0,0.98,0.92
color = g_arc_glow  | Glow colour     | 1.0,0.72,0.38
param = g_arc2.x    | Twin offset (deg) | 180.0 | 0.0  | 180.0
param = g_arc2.y    | Twin strength     | 1.00  | 0.0  | 1.0
param = g_arc2.z    | Edge taper        | 0.60  | 0.15 | 3.0
param = g_arc2.w    | Core brightness   | 5.00  | 0.0  | 12.0

group = Red haze
param = g_haze.x   | Density    | 0.85 | 0.0 | 3.0
param = g_haze.y   | Coverage   | 0.55 | 0.0 | 1.0
param = g_haze.z   | Detail     | 9.00 | 1.0 | 30.0
param = g_haze.w   | Flicker    | 0.35 | 0.0 | 1.0
color = g_haze_tint | Mid tone  | 0.92,0.12,0.18
color = g_haze_hot  | Hot tone  | 1.0,0.62,0.42
param = g_flow.x    | Flow speed   | 0.35 | 0.0 | 2.0
param = g_flow.y    | Flow stretch | 1.80 | 1.0 | 8.0
param = g_flow.z    | Turn scale   | 2.20 | 0.5 | 8.0
*/

// c6 and up. sky_common.hlsli owns c0-c5; of those this program reads g_zenith, g_horizon, g_ground
// and g_runtime, so those are what the plugin uploads. There is no ceiling to work around any more -
// the plugin uploads exactly the runs the constant table declares, up to the ps_3_0 register file.
float4 g_blend : register(c6);       // x = split (deg), y = blend width (deg), z = tonemap, w = time scale
float4 g_sun : register(c7);         // x = bearing, y = elevation (deg), z = twin offset (deg), w = twin strength
float4 g_eclipse : register(c8);     // x = size (deg), y = ring width, zw = moon offset (radius fractions)
float4 g_eclipse_fx : register(c9);  // x = core, y = corona, z = inner bleed, w = flicker
float4 g_glow : register(c10);       // x = corona reach, y = sky glow, z = glow tightness
float4 g_sun_tint : register(c11);   // rgb
float4 g_corona : register(c12);     // rgb
float4 g_moon : register(c13);       // rgb
float4 g_warm : register(c14);       // rgb, w = amount
float4 g_star : register(c15);       // x = density, y = brightness, z = size (deg), w = twinkle
float4 g_star_tint : register(c16);  // rgb
float4 g_wisp : register(c17);       // x = intensity, yzw = thickness / wander / spacing (deg)
float4 g_wisp_tint : register(c18);  // rgb, w = centre elevation (deg)
float4 g_aurora : register(c19);     // x = intensity, yzw = height / thickness / detail (deg)
float4 g_aurora_lo : register(c20);  // rgb
float4 g_aurora_hi : register(c21);  // rgb
float4 g_film : register(c24);       // x = opacity, y = detail (deg), z = warp, w = fringe count
float4 g_gold_base : register(c25);  // rgb
float4 g_gold_hi : register(c26);    // rgb
float4 g_under : register(c27);      // rgb = deep colour below, w = how fast it gets there
float4 g_arc : register(c28);        // x = radius, y = width, z = span (deg), w = intensity
float4 g_arc_dir : register(c29);    // x = bearing, y = elevation (deg), z = flicker, w = glow reach (deg)
float4 g_arc_core : register(c30);   // rgb
float4 g_arc_glow : register(c31);   // rgb
float4 g_haze : register(c32);       // x = density, y = coverage, z = detail, w = flicker coupling
float4 g_haze_tint : register(c33);  // rgb
float4 g_haze_hot : register(c34);   // rgb
float4 g_arc2 : register(c35);       // x = twin offset (deg), y = twin strength, z = edge taper, w = core brightness
float4 g_bleed : register(c36);      // x = depth into the moon (radius fraction), y = sharpness
float4 g_veil : register(c37);       // x = amount, y = reach (deg), z = horizon amount, w = brightness
float4 g_veil_tint : register(c38);  // rgb
float4 g_aurora2 : register(c39);    // x = cluster size (deg), y = cluster rarity, z = drift
float4 g_flow : register(c40);       // x = speed, y = stretch, z = turn scale (deg)
float4 g_film2 : register(c41);      // x = filament amount, y = filament sharpness, z = stretch
float4 g_aurora3 : register(c44);     // y = ray contrast, z = base edge (deg), w = diffuse sheet
float4 g_aurora4 : register(c45);     // x = crest size (deg), y = ragged base, z = spike strength

static const float k_deg = 0.01745329;
static const float k_pi = 3.14159265;

// One lattice cell of a noise sampled at `d * s` spans 1/s radians, because |d| is one and the
// sample point moves along an arc of radius s. That identity is the whole scale system.
static const float k_cell_deg = 57.29578;

#define CLOUD_OCTAVES 4
#define HAZE_OCTAVES 4
#define WISP_OCTAVES 2
#define AURORA_OCTAVES 3
#define FILM_OCTAVES 3
#define CORONA_OCTAVES 3

float scale_of(float feature_degrees)
{
    return k_cell_deg / max(feature_degrees, 0.02);
}

float3 direction_of(float bearing_deg, float elevation_deg)
{
    const float b = bearing_deg * k_deg;
    const float e = elevation_deg * k_deg;

    return float3(sin(b) * cos(e), sin(e), cos(b) * cos(e));
}

// ---------------------------------------------------------------------------------------------
// Noise
// ---------------------------------------------------------------------------------------------

float hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float hash21(float2 p)
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float noise3(float3 x)
{
    float3 p = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(lerp(hash31(p + float3(0, 0, 0)), hash31(p + float3(1, 0, 0)), f.x),
                    lerp(hash31(p + float3(0, 1, 0)), hash31(p + float3(1, 1, 0)), f.x),
                    f.y),
        lerp(lerp(hash31(p + float3(0, 0, 1)), hash31(p + float3(1, 0, 1)), f.x),
            lerp(hash31(p + float3(0, 1, 1)), hash31(p + float3(1, 1, 1)), f.x),
            f.y),
        f.z);
}

float noise2(float2 x)
{
    float2 p = floor(x);
    float2 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(hash21(p + float2(0, 0)), hash21(p + float2(1, 0)), f.x),
        lerp(hash21(p + float2(0, 1)), hash21(p + float2(1, 1)), f.x),
        f.y);
}

static const float3x3 k_rot3 = float3x3(0.00, 0.80, 0.60, -0.80, 0.36, -0.48, -0.60, -0.48, 0.64);
static const float2x2 k_rot2 = float2x2(0.80, 0.60, -0.60, 0.80);

// Unrolled by default, because frame time is what matters here and compile time is paid once, in the
// background, by a thread nobody is waiting on.
//
// Measured on this file: as real loops, 1.4 s to compile and 2249 instruction slots; unrolled, 12 s
// and 3990. The slot count going up is not a cost - there are 32768 - and what it buys is the loop
// counter and the per-octave branch gone, plus the scheduler free to interleave octaves that were
// previously separated by a backward jump.
//
// Set FBM_UNROLL to 0 while iterating on the shader itself: twelve seconds between saving the file
// and seeing the change is a bad loop to work in, and the visual result is identical either way.
#define FBM_UNROLL 1

#if FBM_UNROLL
#define FBM_FLOW [unroll]
#else
#define FBM_FLOW [loop]
#endif
float fbm3(float3 p, int octaves)
{
    float v = 0.0;
    float a = 0.5;

    FBM_FLOW for(int i = 0; i < octaves; ++i)
    {
        v += a * noise3(p);
        p = mul(p, k_rot3) * 2.02 + 100.0;
        a *= 0.5;
    }

    return v;
}

float fbm2(float2 p, int octaves)
{
    float v = 0.0;
    float a = 0.5;

    FBM_FLOW for(int i = 0; i < octaves; ++i)
    {
        v += a * noise2(p);
        p = mul(p, k_rot2) * 2.03 + 100.0;
        a *= 0.5;
    }

    return v;
}

// The same walk with each octave folded about its midpoint. `abs(2n-1)` turns the smooth field into
// one with creases where it used to cross the middle, and stacking those gives lobed, cauliflower
// edges - the shape a cumulus has and a smoke plume does not.
//
// Renormalised by the same 2^-k series so it lands in the same 0..1 range as fbm2 and the coverage
// threshold means the same thing for both.
float fbm2_billow(float2 p, int octaves)
{
    float v = 0.0;
    float a = 0.5;
    float norm = 0.0;

    FBM_FLOW for(int i = 0; i < octaves; ++i)
    {
        v += a * abs(2.0 * noise2(p) - 1.0);
        norm += a;
        p = mul(p, k_rot2) * 2.03 + 100.0;
        a *= 0.5;
    }

    return v / max(norm, 1e-5);
}

// ---------------------------------------------------------------------------------------------
// The per-pixel frame
// ---------------------------------------------------------------------------------------------

// Built once in main and passed down by value. `px` in particular is a derivative, and derivatives
// are undefined inside dynamic flow control - so it is measured up there, where the flow is still
// straight, and everything below simply reads it.
struct sky_frame {
    float3 d;
    float3 east;
    float3 north;
    float cos_el;
    float px; // angular size of a pixel, radians
    float t;
};

// Signed distance in radians from the horizontal band at sin(elevation) = height. A step in d.y is
// not a step in angle - near the zenith the same dy spans much more - and dividing by cos(elevation)
// is that first-order correction, which is what lets thicknesses be authored in degrees.
float band_offset(sky_frame f, float height)
{
    return (f.d.y - height) / f.cos_el;
}

// ---------------------------------------------------------------------------------------------
// Shared: the eclipse, seen from both halves
// ---------------------------------------------------------------------------------------------

// The suns come in a pair, the second one a fixed bearing away from the first. That is not symmetry
// for its own sake: the track swings its heading constantly, and one sun is in frame maybe half the
// time. Two opposite ones are in frame nearly always.
//
// Only the nearer is evaluated. Whichever it is, the shape is the same, so the cost of the second is
// one dot product and a select rather than a second eclipse.
struct sun_pick {
    float3 dir;
    float weight;
    float cosine;
};

sun_pick pick_sun(float3 d)
{
    const float3 first = direction_of(g_sun.x, g_sun.y);
    const float3 second = direction_of(g_sun.x + g_sun.z, g_sun.y);

    const float ca = dot(d, first);
    const float cb = dot(d, second);

    sun_pick out_pick;

    const bool take_second = cb > ca;
    out_pick.dir = take_second ? second : first;
    out_pick.cosine = take_second ? cb : ca;
    out_pick.weight = take_second ? saturate(g_sun.w) : 1.0;

    return out_pick;
}

// Takes the scene and hands it back composited: the moon has to occlude the sky and the corona, not
// sit on them translucently.
//
// `aa_sun` and `aa_moon` are the two edge widths, measured from the pixel by the caller. A fixed
// width is either a hard step or a smear depending on a render scale the shader is not told; these
// are correct at 33 per cent and at native alike, and correct too when the ring is thinner than a
// pixel, where the smoothstep then yields honest partial coverage instead of a staircase.
float3 eclipse_over(sky_frame f, sun_pick sun, float3 moon_dir, float a_sun, float a_moon, float aa_sun, float aa_moon, float3 scene)
{
    const float r_sun = g_eclipse.x * k_deg;
    const float r_moon = r_sun * (1.0 - saturate(g_eclipse.y));

    float flicker = sin(f.t * 24.0) * cos(f.t * 38.0) * sin(f.t * 11.0);
    flicker = lerp(1.0, 0.88 + 0.22 * flicker, saturate(g_eclipse_fx.w)) * sun.weight;

    // Mie halo: the sun seen through air rather than the sun itself. One pow, and most of what makes
    // a bright disc feel like it is inside the atmosphere instead of pasted onto it.
    scene += g_sun_tint.rgb * (pow(saturate(sun.cosine), g_glow.z) * g_glow.y * sun.weight);

    // Corona. Measured in radii so the shape survives resizing, and streaked by sampling noise on
    // the *tangent* direction - which is constant along a ray out from the centre and varies only
    // around the ring, so the noise reads as radial streamers rather than as blobs.
    const float radii = max(a_sun - r_sun, 0.0) / max(r_sun, 1e-5);
    const float3 tangent = normalize(f.d - sun.dir * sun.cosine + 1e-6);

    const float streak = fbm3(tangent * 5.0 + float3(0.0, f.t * 0.03, 0.0), CORONA_OCTAVES);
    const float corona = exp(-radii * g_glow.x) * (0.30 + 1.40 * streak) * g_eclipse_fx.y;

    scene += g_corona.rgb * (corona * flicker);
    scene += g_sun_tint.rgb * (g_eclipse_fx.x * flicker * smoothstep(r_sun + aa_sun, r_sun - aa_sun, a_sun));

    scene = lerp(g_moon.rgb, scene, smoothstep(r_moon - aa_moon, r_moon + aa_moon, a_moon));

    // Light leaking past the moon's limb, brightest on the side the sun peeks out of. Both
    // directions live in the moon's tangent plane, which is the screen-space comparison the original
    // made, done on the sphere instead.
    //
    // Shallow and steep rather than broad and soft: `depth` is how far into the disc it reaches as a
    // fraction of the radius, and the exponent decides how much of that is spent right at the rim.
    // A linear ramp over a fifth of the disc washed the dark core out from the inside; a sharp one
    // over a fifteenth welds the light to the edge instead, which is what a limb actually looks like.
    const float inner = r_moon - a_moon;
    const float depth = max(g_bleed.x, 1e-4) * r_moon;

    // Faded against the moon's own antialiased edge rather than cut with step(): a hard start
    // exactly where the disc's edge is being smoothed would put a jagged line back on the one
    // boundary the whole eclipse is judged by.
    const float rim = saturate(1.0 - inner / depth);
    const float bleed = pow(rim, max(g_bleed.y, 0.1)) * smoothstep(-aa_moon, aa_moon, inner);

    const float3 tan_d = f.d - moon_dir * dot(f.d, moon_dir);
    const float3 tan_sun = sun.dir - moon_dir * dot(sun.dir, moon_dir);
    const float asym = dot(normalize(tan_d + 1e-6), normalize(tan_sun + 1e-6)) * 0.5 + 0.5;

    // Derived from the corona rather than being a fourth picker: it is the same light seen through
    // the moon's edge, and two knobs that must agree are one knob with a trap in it.
    const float3 bleed_tint = saturate(g_corona.rgb * 0.6 + 0.4);

    return scene + bleed_tint * (bleed * (0.45 + 0.45 * asym) * g_eclipse_fx.z * flicker);
}

// ---------------------------------------------------------------------------------------------
// Upper sky
// ---------------------------------------------------------------------------------------------

float3 upper_gradient(sky_frame f, float3 sun_dir)
{
    float3 col = lerp(g_horizon.rgb, g_zenith.rgb, pow(saturate(f.d.y), 0.45));

    // A warm glow on the side the light is on, seamless by construction because it is built from a
    // dot product rather than from an azimuth. Raised to a power so it reads as a glow around one
    // bearing instead of half the sky being pink, and tied to the horizon, where the air is.
    const float2 bearing = f.d.xz;
    const float len = max(length(bearing), 1e-4);

    const float align = dot(bearing / len, normalize(sun_dir.xz + 1e-5)) * 0.5 + 0.5;
    const float warm = pow(align, 3.0) * g_warm.w * saturate(1.0 - f.d.y * 1.6);

    return lerp(col, g_warm.rgb, saturate(warm));
}

// A lattice in direction space, one hash per cell, the star placed well inside its own cell so no
// neighbourhood search is needed.
//
// The radius has a floor of one pixel, and the brightness is scaled down by however much that floor
// widened it. Without the floor a fine star field is a field of sub-pixel samples that flickers as
// the camera turns; without the compensation, dropping to 33 per cent turns it into headlights.
float3 upper_stars(sky_frame f)
{
    const float cell_deg = max(g_star.z * 6.0, 0.05);
    const float s = scale_of(cell_deg);

    const float3 p = f.d * s;
    const float3 cell = floor(p);
    const float3 local = p - cell;

    const float h = hash31(cell);
    const float present = step(1.0 - g_star.x, h);

    const float3 centre = 0.32 + 0.36 * float3(hash31(cell + 11.0), hash31(cell + 23.0), hash31(cell + 37.0));

    const float authored = g_star.z * k_deg;
    const float radius = max(authored, f.px);
    const float shrink = saturate(authored / max(radius, 1e-6));

    float star = saturate(1.0 - (length(local - centre) / s) / radius);
    star = star * star * star;

    const float twinkle = 1.0 - g_star.w * (0.5 + 0.5 * sin(f.t * 2.7 + h * 43.0));
    const float3 tint = lerp(g_star_tint.rgb, float3(1.0, 0.92, 0.80), hash31(cell + 5.0));

    return tint * (present * star * shrink * twinkle * g_star.y * saturate(f.d.y * 3.0 + 0.15));
}

// Three thin threads at fixed elevations that wander. One fbm per thread rather than two: the second
// one only ever supplied a break-up factor along the thread, and a hash of the same lattice cell
// decorrelates just as well for the price of six instructions.
float3 upper_wisps(sky_frame f)
{
    float3 col = 0.0;

    const float3 bearing = normalize(float3(f.d.x, 0.0, f.d.z) + 1e-5);
    const float thickness = g_wisp.y * k_deg;
    const float wander = g_wisp.z * k_deg;
    const float spacing = g_wisp.w * k_deg;

    const float wander_scale = scale_of(max(g_wisp.z * 4.0, 8.0));
    const float edge = max(thickness, f.px);

    for(int i = 0; i < 3; ++i) {
        const float fi = float(i);
        const float drift = f.t * (0.06 + 0.03 * fi);

        const float3 sample_at = bearing * wander_scale + float3(0.0, fi * 7.0 + drift, 0.0);
        const float n = fbm3(sample_at, WISP_OCTAVES);

        // Two octaves peak at 0.75, so this is what re-centres them on zero rather than on 0.5.
        const float offset = (n - 0.375) * 2.0 * wander;

        const float centre = sin(g_wisp_tint.w * k_deg + (fi - 1.0) * spacing);
        const float dist = abs(band_offset(f, centre) - offset);

        // Break-up along the thread, from the same sample. A sine rather than frac(): frac() of a
        // continuous field is a step function, and using one as a mask puts a hard edge wherever the
        // argument crosses an integer - which drew the threads as rows of dashes. A sine decorrelates
        // just as well for a mask and stays continuous.
        const float fade = smoothstep(0.15, 0.62, 0.5 + 0.5 * sin(n * 21.7));

        col += g_wisp_tint.rgb * (smoothstep(edge, 0.0, dist) * fade * g_wisp.x);
    }

    return col;
}

// Curtains: structure that varies fast horizontally and slowly vertically. Squashing d.y inside the
// lookup is the whole trick - one field, sampled through an anisotropic scale, comes out as vertical
// streaks with no azimuth anywhere in sight.
// Rays, and specifically not fbm.
//
// The ordinary fbm cannot make these, and the reason is its rotation: k_rot3 mixes the axes between
// octaves, so whatever anisotropy the first octave was given is gone by the second. What comes back
// is round fine detail sitting on a stretched base - and round detail on a stretched base reads as
// cloud, which is exactly what this layer drew before.
//
// Here the squash is inside the loop and applies to every octave, and the octaves are decorrelated
// by translation instead. The aspect ratio that results is the point: real curtains are fractions of
// a degree wide and tens of degrees tall, nearer a hundred to one than the ten to one a rotating fbm
// could hold on to.

float3 upper_aurora(sky_frame f)
{
    const float3 bearing = normalize(float3(f.d.x, 0.0, f.d.z) + 1e-5);

    // Where the aurora exists at all. A coarse field over the bearing, thresholded hard, leaves one
    // to three patches around the sky rather than an even belt all the way round - which is how it
    // looks in life, and in the references, and is the difference between a phenomenon and a
    // wallpaper.
    //
    // It is also the cheapest thing in the function and gates the two most expensive, so this reads
    // as a saving rather than a cost: outside the patches the curtain noise is never sampled. The
    // branch is per-pixel rather than uniform, but patches are large and contiguous, so a wave is
    // almost always wholly in or wholly out.
    const float cluster_raw = fbm3(bearing * scale_of(g_aurora2.x) + float3(0.0, f.t * g_aurora2.z, 0.0), 2);
    const float rarity = saturate(g_aurora2.y) * 0.75;
    const float cluster = smoothstep(rarity, rarity + 0.16, cluster_raw);

    [branch] if(cluster <= 0.001)
    {
        return 0.0;
    }

    const float snake = (fbm3(bearing * scale_of(70.0) + float3(0.0, f.t * 0.05, 0.0), 2) - 0.375) * 2.0;

    const float thickness = max(g_aurora.z * k_deg, 1e-3);
    const float offset = band_offset(f, sin(g_aurora.y * k_deg)) - snake * thickness * 1.4;

    // Height above the base, in curtain heights. Everything below is shaped by this one number.
    //
    // The base is not a line. A comb whose teeth all start at the same height and run the same
    // length reads as a barcode, which is what the first dense version drew - the rays were right
    // and the *edge* was wrong. This jitter varies over a few rays at a time, so neighbours agree
    // roughly and the bottom of the curtain comes out ragged rather than ruled.
    const float jitter = noise3(bearing * scale_of(max(g_aurora.w, 0.05) * 5.0) + float3(0.0, f.t * 0.05, 0.0));

    const float h = max(offset - (jitter - 0.5) * thickness * saturate(g_aurora4.y) * 2.0, 0.0) / thickness;

    // THE RAY FIELD IS ONE-DIMENSIONAL, and that is the whole design.
    //
    // Two previous attempts sampled a 3D field squashed vertically, and both drew torn vertical
    // dashes. The reason is not the amount of squash: value noise makes *blobs*, and a squashed blob
    // is still a blob - it has a top and a bottom, and those ends are what read as dashes. A real ray
    // has no ends; it rises from the base and fades.
    //
    // So the field is a function of position *along* the curtain only, sampled on the horizontal
    // bearing circle - seamless, because a circle closes, and pole-free, because nothing is sampled
    // at the poles. There is nothing vertical in it that could break, at any height, ever. The
    // vertical structure comes entirely from the profile it is multiplied by.
    //
    // THE RAY FIELD DEPENDS ON AZIMUTH ALONE. Not on height, not on the view direction's elevation,
    // not on anything that changes as a ray rises. That is the entire guarantee of straightness:
    // there is nothing in the field that *can* move upward, so a ray cannot wander, curve or break,
    // at any height, ever.
    //
    // The previous attempt leaned the rays by rotating the sample circle by an angle proportional to
    // height. It seemed harmless - the same field, shifted along itself - but it put a height term
    // back into the field, and combined with the contour below it produced long curved ribbons that
    // read as hair rather than as light. Real rays are near enough parallel over the visible sky,
    // and they converge toward the zenith on screen only because meridians do.
    const float comb = fbm3(bearing * scale_of(g_aurora.w) + float3(0.0, f.t * 0.03, 0.0), 2);

    // Crests: a slow field along the same circle that gathers rays into long banks, which is what
    // the eye reads as structure rather than as noise.
    const float crest_raw = fbm3(bearing * scale_of(g_aurora4.x) + float3(0.0, f.t * 0.02, 0.0), 2);
    const float crest = 0.25 + 0.75 * smoothstep(0.26, 0.58, crest_raw);

    // Soft bands, not contour lines.
    //
    // A contour is a ribbon: it has two edges with a filled middle, and that is precisely what made
    // the last version look like flowing hair. A wide threshold instead draws a band that simply
    // fades out to either side - no edges of its own - which is what a column of glowing air looks
    // like, and which overlaps additively with its neighbours the way light does.
    const float softness = 0.5 / max(g_aurora3.y, 0.3);
    const float rays = smoothstep(0.5 - softness, 0.5 + softness, comb);

    // Behind the rays, the diffuse sheet they sit in. Photographs of aurora are never bare columns
    // on black: there is always a smooth glow between them, and its absence was a large part of why
    // this read as a pattern rather than as light.
    const float sheet = saturate(g_aurora3.w);

    // The vertical profile: a bright dense foot and a long thin spike above it. Two lobes, because
    // one exponential can be either dense or tall and the description asks for both at once.
    const float base_edge = max(g_aurora3.z * k_deg, 1e-4);
    const float foot = smoothstep(-base_edge, base_edge * 0.35, offset);
    const float dense = exp(-h * 5.0);
    const float spike = exp(-h * 1.05) * saturate(g_aurora4.z);

    // Uneven brightness across the comb, from the same jitter. Rays of identical intensity are the
    // other half of what made it read as printed rather than lit.
    const float uneven = 0.35 + 0.65 * jitter;

    const float glow = foot * (dense + spike) * (sheet + rays * (1.0 - sheet)) * crest * cluster * uneven;

    // The colour climbs through the curtain from its base, not through the sky: ramping on d.y put a
    // curtain sitting at twenty degrees past the top of the ramp everywhere, and the lower colour was
    // never seen at all.
    const float3 tint = lerp(g_aurora_lo.rgb, g_aurora_hi.rgb, saturate(h * 0.75));

    return tint * (glow * g_aurora.x);
}

// Thin-film interference over a warped height field.
//
// Two economies against the naive version, together worth more than half its cost. The warp is a
// single fbm turned into a two-component tangential displacement by a swirl, rather than two fbm for
// two components - the field only has to be plausible, not decorrelated. And the relief taps
// re-evaluate the height alone at two offsets rather than re-running the warp, because the taps
// exist to find the *fine* relief and that comes from the last octaves, not from where the warp put
// the sample.
float film_height(sky_frame f, float3 p, float3 warp)
{
    return fbm3(p + warp + f.t * 0.04, FILM_OCTAVES);
}

float3 upper_film(sky_frame f, float3 sun_dir, out float alpha, out float sharp_spec)
{
    const float s = scale_of(g_film.y);

    // Stretched along the sphere's east axis before anything else touches it. Marbling is not
    // isotropic: ink drawn across water leaves shapes far longer than they are wide, and an
    // isotropic field can only ever produce blobs no matter how it is warped afterwards. One
    // multiply, and it is what separates a paint pattern from a cloud.
    const float3 p = float3(f.d.x, f.d.y * max(g_film2.z, 1.0), f.d.z) * s;

    const float q = fbm3(p + f.t * 0.07, FILM_OCTAVES);

    // One value into a displacement, by walking it round a circle in a FIXED basis.
    //
    // It used to walk round the sphere's own tangent frame (east, north), and that was a pole. There
    // is no smooth non-vanishing tangent field on a sphere - the hairy ball theorem is not a curio
    // here, it is the reason the zenith drew every filament converging on one point. The warp does
    // not need to be tangential: it displaces a sample inside a 3D field, and leaving the sphere is
    // free. Three fixed axes have no singular direction at all.
    const float swirl = q * 6.2832;
    const float3 warp
        = float3(cos(swirl), sin(swirl * 0.73 + 1.7), sin(swirl)) * (g_film.z * s * 0.10);

    const float height = film_height(f, p, warp);

    // The relief, as a true 3D gradient rather than two taps in a tangent frame - same reason. Three
    // taps along fixed axes are well defined everywhere; projecting the result onto the tangent
    // plane afterwards is a subtraction, and unlike a frame it degrades gracefully at the poles
    // because there is nothing to degrade: the projection of a defined vector is always defined.
    const float eps = 0.035;
    const float3 grad = float3(film_height(f, p + float3(eps, 0.0, 0.0), warp) - height,
        film_height(f, p + float3(0.0, eps, 0.0), warp) - height,
        film_height(f, p + float3(0.0, 0.0, eps), warp) - height);

    // The surface is the sphere, so its normal is the view direction; the relief perturbs it. The
    // scale on `d` is how flat the film is - steeper puts the sharp specular in thin lines along
    // every ridge, which reads as scratches on glass rather than as a sheen.
    const float3 tangential = grad - f.d * dot(grad, f.d);
    const float3 normal = normalize(f.d * (0.75 * eps) - tangential);

    // Lit by the sun rather than by an authored constant. That is one fewer arbitrary number, it
    // ties the film to the rest of the composition, and it is stable under every camera move because
    // both vectors live in sky space.
    const float3 half_vec = normalize(sun_dir + f.d);

    const float ndh = max(dot(normal, half_vec), 0.0);
    const float spec = pow(ndh, 28.0);
    sharp_spec = pow(ndh, 110.0);

    // Interference phase. In a real thin film this is the optical path difference, and what it does
    // is shift the hue - which is the whole reason an oil slick is worth looking at. The three
    // channels sample the same cosine a third of a turn apart, so the colour walks the base-to-
    // highlight ramp instead of the film being one flat gold.
    const float phase = height * g_film.w + q * 5.0 - f.t * 1.6;
    const float t_shift = 0.5 + 0.5 * cos(phase);
    
    float3 col = lerp(g_gold_base.rgb, g_gold_hi.rgb, t_shift);
    col += g_gold_hi.rgb * (sharp_spec * 0.5 + spec * 0.30);

    // Two ways of turning the height field into coverage, and the knob crossfades between them.
    //
    // The smooth one is a sheet: everything above a level is painted, so the result is broad areas
    // with soft borders - nacre. The filament one keeps only a narrow band about one level of the
    // field, which is its contour line - and a contour is what ink on water actually reads as:
    // threads, loops and the rims of voids, rather than the regions those rims enclose.
    //
    // The width has to be small and absolute, not a power of a fold. Folding about the midpoint and
    // raising it to a power was the first attempt and it painted the whole sky: three octaves put
    // almost all of the field within a narrow spread of its own mean, so `1 - |2h - 1|` is close to
    // one nearly everywhere and no exponent rescues that. Measuring distance to the level directly
    // does not care how the field is distributed.
    const float sheet = smoothstep(0.26, 0.74, height);

    const float band = 0.16 / max(g_film2.y, 1.0);
    const float filament = 1.0 - smoothstep(0.0, band, abs(height - 0.5));

    const float coverage = lerp(sheet, filament, saturate(g_film2.x));
    const float fringe = 0.35 + 0.65 * (0.5 + 0.5 * sin(phase));

    alpha = saturate(g_film.x) * coverage * fringe;

    return col;
}

// ---------------------------------------------------------------------------------------------
// Lower sky
// ---------------------------------------------------------------------------------------------

float3 lower_gradient(sky_frame f)
{
    return lerp(g_ground.rgb, g_under.rgb, saturate(-f.d.y * g_under.w));
}

// The swollen arc: a ring of angular radius R about a direction, lit over a span centred on the
// ring's own "up".
//
// This is the one place in the file with an atan2 on the view direction, and its coordinate has TWO
// singular places, not one. The obvious one is the wrap at the bottom of the ring, which the span
// mask has already switched off for any span under 360 degrees and which at exactly 360 sits under a
// constant mask - harmless either way.
//
// The other is the ring's own axis, where `tangent` degenerates and every value of `around` meets in
// a point, exactly as azimuths meet at a pole. That one is only harmless if nothing this function
// emits can reach that far, so the glows are measured in degrees and default to a small fraction of
// the radius. They were originally multiples of the line width, and a 0.9-degree line with a 60x
// glow reached 54 degrees - past the 46-degree radius, onto the axis, and drew the pole on screen as
// a hard-edged wedge.
//
// The shape is a lens, not a strip of even width. `taper` runs from one at the crown to zero at the
// tips, so the band itself narrows to a point rather than the span mask simply dimming a bar of
// constant thickness - the difference between a crescent and a highlighted segment.
//
// And the glow is derived from the core rather than living beside it: same taper, same profile, and
// admitted only where the core is not. That way it softens the core's own boundary instead of adding
// a second, independently shaped halo that brightens the middle twice over.
float3 arc_at(sky_frame f, float bearing_deg, float weight, float flicker)
{
    const float3 axis = direction_of(bearing_deg, g_arc_dir.y);

    const float cd = clamp(dot(f.d, axis), -1.0, 1.0);
    const float radius = g_arc.x * k_deg;

    // Distance from the ring line, in radians.
    const float band = abs(acos(cd) - radius);

    // Where around the ring. The tangent frame is built from the axis, so "up" is toward the zenith
    // and the span opens symmetrically about it.
    const float3 ref = abs(axis.y) > 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    const float3 right = normalize(cross(ref, axis));
    const float3 up = cross(axis, right);

    const float3 tangent = normalize(f.d - axis * cd + 1e-6);
    const float around = atan2(dot(tangent, right), dot(tangent, up));

    // Position along the lit span: zero at the crown, one at the tip.
    const float half_span = max(saturate(g_arc.z / 360.0) * k_pi, 1e-4);
    const float along = saturate(abs(around) / half_span);

    // An ellipse profile - flat through the middle and falling vertically at the very end, which is
    // what makes the tip an actual point instead of a chopped-off bar. The exponent shapes how much
    // of the span stays at full width; below one it stays wide longer and sharpens later.
    const float taper = pow(saturate(1.0 - along * along), max(g_arc2.z, 0.05));

    // Authored width against the pixel floor, and dimmed by however much the floor widened it. A tip
    // that narrows below the sample spacing would otherwise end in a ragged pixel-wide line instead
    // of fading out; this is the same trade the star field makes, for the same reason.
    const float authored = g_arc.y * k_deg * taper;
    const float width = max(authored, f.px);
    const float shrink = saturate(authored / max(width, 1e-6));

    const float core = smoothstep(width, 0.0, band) * shrink;

    // In degrees, and clamped to a fraction of the radius: a glow that reaches its own axis draws
    // the pole of `around` as a wedge, which is what the outer one used to do. Scaled by the taper
    // as well, so the halo comes to a point with the core rather than blooming past its tips.
    const float outer = min(max(g_arc_dir.w, 0.05) * k_deg, radius * 0.45) * taper + f.px;
    const float glow = (exp(-band / (outer * 0.25)) * 1.6 + exp(-band / outer) * 0.55) * taper;

    float3 col = g_arc_core.rgb * (core * g_arc2.w);
    col += g_arc_glow.rgb * (glow * (1.0 - core));

    return col * (flicker * g_arc.w * weight);
}

float3 lower_arc(sky_frame f)
{
    // Stepped flicker, as in the original: a value that holds for a frame of its own clock and then
    // jumps, which is what makes it read as electrical rather than as a sine. Computed once and
    // shared, so the twin pulses with its partner rather than beside it.
    const float step_time = floor(f.t * 28.0);
    const float flicker1 = hash11(step_time * 1.13);
    const float flicker2 = hash11(step_time * 2.71);
    const float spike = step(0.9, hash11(step_time * 5.37)) * 0.6;

    const float lively = lerp(0.72, 1.35, flicker1 * 0.7 + flicker2 * 0.3) + spike;
    const float flicker = lerp(1.0, lively, saturate(g_arc_dir.z));

    float3 col = arc_at(f, g_arc_dir.x, 1.0, flicker);

    // The twin, for the same reason the suns have one: the track swings its heading constantly, and
    // one arc is in frame maybe half the time. Added rather than picked-nearer, unlike the suns -
    // an arc's glow reaches far enough that a nearest-of-two switch would put a seam through it,
    // where the eclipse's falls off inside the disc long before the boundary matters.
    [branch] if(g_arc2.y > 0.001)
    {
        col += arc_at(f, g_arc_dir.x + g_arc2.x, saturate(g_arc2.y), flicker);
    }

    return col;
}

// Aerial perspective around the sun, laid over the eclipse and under everything that comes after it.
//
// The complaint it answers is precise: with the disc's core turned off, a black circle inside a
// bright ring on a dark sky reads as a hole cut in the canvas. That is not a lighting problem, it is
// a *contrast* problem - nothing in the picture sits between the ring and the void, so the eye takes
// the void for absence rather than for an object.
//
// Haze fixes it the way air does. Scattered light is in front of everything, so it lifts the black
// toward the sky's own colour, and it is thickest where the light comes from - which puts the most
// lift exactly on the disc that needed it. The ring stays bright because the haze adds to it too.
//
// No noise: this is a smooth function of two angles, and giving it structure would only fight the
// layers drawn after it.
float3 sun_veil(sky_frame f, float cosine_to_sun, float3 scene)
{
    // Reach is authored in degrees and converted to the exponential's scale, so the knob means the
    // angle at which the veil has fallen to about a third rather than an abstract tightness.
    const float reach = max(g_veil.y, 0.5) * k_deg;
    const float around_sun = exp(-acos(clamp(cosine_to_sun, -1.0, 1.0)) / reach);

    // A band of it along the horizon as well, which is where depth of air actually is, and which
    // ties the two hemispheres together across the seam.
    const float along_horizon = exp(-abs(f.d.y) * 3.2) * saturate(g_veil.z);

    const float density = saturate(around_sun * saturate(g_veil.x) + along_horizon);

    return lerp(scene, g_veil_tint.rgb * g_veil.w, density);
}

// The red haze, on the lower plane - the same bounded projection the clouds use, mirrored. Kept as a
// direct fbm, which is what it always was: the thing that was expensive in Dying Sun was the
// separate 48-step ground fog, and that is gone.
//
// Composited rather than added, because the brief for it is that it *covers* the arc.
float4 lower_haze(sky_frame f)
{
    const float down = max(-f.d.y, 0.0);
    const float s = 75.4 / max(g_haze.z, 0.5);

    const float2 plane = f.d.xz / (down + 0.32) * s;

    // Streams rather than drifting blotches.
    //
    // Three things together make something read as flowing. A direction that varies slowly across
    // the sphere, so the streams curve past one another instead of all sliding the same way. Noise
    // stretched *along* that direction, so the shapes are filaments rather than blobs - this is what
    // does most of the work, and it is free, because stretching is a scale applied to the sample
    // coordinate. And advection along the same direction, so they travel down their own length,
    // which is what stops the motion looking like a sliding photograph.
    //
    // The bulk motion is a rigid translation: one direction, the same everywhere, growing with time.
    // Rigid is the operative word. A displacement that varies across space and grows without bound
    // folds the domain onto itself, and a folded domain does not look like movement - it looks like
    // marble, all rings and level curves. That was the second attempt here, and the first was worse
    // still: rotating the sampling basis by a spatially varying angle, which is the same mistake
    // wearing a different hat.
    const float2 flow = float2(-0.020, 0.008) * (1.0 + g_flow.x * 8.0) * f.t;

    // On top of it, a curl that does vary across space - but *bounded*, because it is modulated by a
    // sine rather than by t. Streams stop being perfectly parallel and start breathing past one
    // another, and because the amplitude never grows, the domain is never folded far enough to
    // crease. This is the whole difference between flow and marble, and it is one bound.
    const float turn = fbm2(plane * (1.0 / max(g_flow.z, 0.1)) + f.t * 0.02, 2) * 6.2832;
    const float2 wobble = float2(cos(turn), sin(turn)) * (0.45 * sin(f.t * 0.13 + turn * 0.5));

    // The stretch stays in a fixed frame: an anisotropy that turns with the flow is a spatially
    // varying basis by another name. A prevailing direction reads as wind, which is what haze lying
    // over ground actually has.
    const float stretch = max(g_flow.y, 1.0);
    const float2 carried = plane + flow + wobble;

    float2 warped = float2(carried.x / stretch, carried.y);

    const float n = fbm2(warped + fbm2(warped * 2.3 - f.t * 0.012, HAZE_OCTAVES), HAZE_OCTAVES);

    const float lo = 0.86 - saturate(g_haze.y) * 0.66;
    const float mask = smoothstep(lo, lo + 0.34, n);

    // Out over the last few degrees at the horizon, where the projection's rate of change peaks, and
    // thinning toward the nadir where a real layer is seen edge-on.
    const float fade = smoothstep(0.0, 0.09, down) * (1.0 - smoothstep(0.40, 1.0, down) * 0.55);

    // Coupled to the arc's clock, so the haze pulses with the thing it is lit by rather than living
    // its own life next to it.
    const float step_time = floor(f.t * 28.0);
    const float pulse = lerp(1.0, lerp(0.80, 1.25, hash11(step_time * 1.13)), saturate(g_haze.w));

    const float density = saturate(mask * fade * g_haze.x * pulse);

    // The hot tone appears where the haze is thickest, which is where in life it would be closest to
    // whatever is lighting it.
    const float3 col = lerp(g_haze_tint.rgb, g_haze_hot.rgb, saturate(density * density * 1.4));

    return float4(col * (0.35 + 0.65 * density), density);
}

// ---------------------------------------------------------------------------------------------

float4 main(sky_in input) : COLOR0
{
    const float3 d = normalize(input.dir);

    sky_frame f;
    f.d = d;
    f.t = g_runtime.x * g_blend.w;
    f.east = normalize(float3(d.z, 0.0, -d.x) + 1e-5);
    f.north = cross(d, f.east);
    f.cos_el = sqrt(max(1.0 - d.y * d.y, 1e-4));

    // Every derivative in the shader is taken here, above the first branch. Inside dynamic flow
    // control they are undefined, and both hemispheres and every layer below are behind one.
    f.px = (fwidth(d.x) + fwidth(d.y) + fwidth(d.z)) * 0.35;

    const sun_pick sun = pick_sun(d);

    // The moon's tangent frame and both angles, also above the branch - the eclipse straddles the
    // horizon and its edges want the exact per-quantity derivative rather than the general px.
    const float3 ref = abs(sun.dir.y) > 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    const float3 sun_right = normalize(cross(ref, sun.dir));
    const float3 sun_up = cross(sun.dir, sun_right);

    const float3 moon_dir
        = normalize(sun.dir + (sun_right * g_eclipse.z + sun_up * g_eclipse.w) * (g_eclipse.x * k_deg));

    const float a_sun = acos(clamp(sun.cosine, -1.0, 1.0));
    const float a_moon = acos(clamp(dot(d, moon_dir), -1.0, 1.0));

    const float aa_sun = max(fwidth(a_sun), 1e-6);
    const float aa_moon = max(fwidth(a_moon), 1e-6);

    // How much of each sky this pixel is. The blend is in degrees about a configurable split so the
    // seam can be pushed off the horizon, which matters because the horizon is exactly where the
    // track is.
    const float split = sin(g_blend.x * k_deg);
    const float half_band = max(g_blend.y * k_deg, 1e-3);
    const float mix_upper = smoothstep(-half_band, half_band, band_offset(f, split));

    // The arc is evaluated once, outside both hemispheres, and added to each. It belongs to neither:
    // a ring wide enough to be worth calling swollen necessarily crosses the horizon, and confining
    // it to the lower branch put its lit crown in the half that never draws it. Added rather than
    // composited last so that the haze - which is a lower-sky layer - can still cover it, which is
    // the whole point of the haze.
    //
    // Cheap enough to be unconditional in the geometric sense: one acos, one atan2 and two exps, next
    // to the twenty-odd noise lookups the upper sky spends. The branch here is on the uniform.
    float3 arc = 0.0;

    [branch] if(g_arc.w > 0.001)
    {
        arc = lower_arc(f);
    }

    float3 scene = 0.0;

    // The order below is the whole point of this pass, so it is worth stating plainly:
    //
    //   base sky -> paint -> eclipse -> veil -> cloud, aurora, wisps, stars
    //
    // The paint goes *under* the eclipse because paint is a thing in the sky and the eclipse is
    // behind the sky - a film drawn over the sun made the sun look printed on. The veil goes over
    // the eclipse because haze is in front of everything. And the rest goes over the veil because
    // they are nearer still, which is also why the veil does not wash them out.
    //
    // The cost of that ordering is a second upper branch after the eclipse rather than one before
    // it. The branch itself is free; the work inside is the same work, just split.

    // Two branches rather than an if/else pair, so a wave entirely on one side of the horizon runs
    // exactly one of them. Only the blend band pays for both, and that band is a strip.
    [branch] if(mix_upper > 0.001)
    {
        float3 upper = upper_gradient(f, sun.dir) + arc;

        // Each layer behind its own uniform. A uniform branch is perfectly coherent - every pixel
        // takes the same side - so a layer switched off costs nothing at all rather than being
        // computed and multiplied by zero. That is what makes these knobs a performance control.
        [branch] if(g_film.x > 0.001)
        {
            float alpha = 0.0;
            float sharp_spec = 0.0;
            const float3 gold = upper_film(f, sun.dir, alpha, sharp_spec);

            upper = lerp(upper, gold, alpha);
            upper += g_gold_hi.rgb * (sharp_spec * alpha * 0.35);
        }

        scene += upper * mix_upper;
    }

    [branch] if(mix_upper < 0.999)
    {
        float3 lower = lower_gradient(f) + arc;

        [branch] if(g_haze.x > 0.001)
        {
            const float4 haze = lower_haze(f);
            lower = lerp(lower, haze.rgb, haze.a);
        }

        scene += lower * (1.0 - mix_upper);
    }

    // The eclipse outside both, because it is the one thing that belongs to neither half - it sits
    // near the horizon and is meant to be the hinge the two skies turn about.
    scene = eclipse_over(f, sun, moon_dir, a_sun, a_moon, aa_sun, aa_moon, scene);

    [branch] if(g_veil.x > 0.001 || g_veil.z > 0.001)
    {
        scene = sun_veil(f, sun.cosine, scene);
    }

    // Everything nearer than the air, over the veil. Upper hemisphere only, so this is the second
    // half of the branch that started above - same work, moved after the eclipse so the sun sits
    // behind the weather rather than on top of it.
    [branch] if(mix_upper > 0.001)
    {
        float3 post = 0.0;

        [branch] if(g_aurora.x > 0.001)
        {
            post += upper_aurora(f);
        }

        [branch] if(g_wisp.x > 0.001)
        {
            post += upper_wisps(f);
        }

        [branch] if(g_star.y > 0.001 && g_star.x > 0.0001)
        {
            post += upper_stars(f);
        }

        scene += post * mix_upper;
    }

    // ACES, mixed in rather than switched on, so the two halves are graded by one curve and the
    // eclipse core rolls off instead of clipping flat. Dying Sun always ended this way; Chroma never
    // did, and a hard seam between two different tone responses would have shown at the horizon.
    const float3 tonemapped = (scene * (2.51 * scene + 0.03)) / (scene * (2.43 * scene + 0.59) + 0.14);

    return float4(lerp(scene, tonemapped, saturate(g_blend.z)), 1.0);
}
