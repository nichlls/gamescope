/*
Copyright (c) 2023, 2025, Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

SPDX-License-Identifier: BSD-3-Clause
*/

// Adapted from Qualcomm SGSR1, d926f074bcb9d714e179f1ce0fcb9ee2eeb5074e.
// sgsr/v1/include/glsl/sgsr1_shader_mobile.frag, with the departures noted below.

mediump float sgsrFastLanczos2(mediump float x)
{
	// The polynomial turns positive again past 4, the narrower kernel can reach it.
	x = min(x, 4.0);
	mediump float wA = x-4.0;
	mediump float wB = x*wA-wA;
	wA *= wA;
	return wB*wA;
}
// Above the reference 0.55 to narrow the kernel, so far taps cannot outvote a lone dark texel.
const float sgsrKernelWidth = 0.65;
const mediump float sgsrMaxCorrection = 23.0 / 255.0;

// Outlier rejection fades in with tap distance, so the texel covering the pixel always votes.
mediump float sgsrWeight(mediump float dx, mediump float dy, mediump float g, mediump float std, mediump vec2 direction)
{
	mediump float d2 = (dx*dx)+(dy*dy);
	mediump float outlier = clamp(abs(g)*std, 0.0, 1.0) * min(d2 * 0.5, 1.0);
	mediump float across = dx * direction.x + dy * direction.y;
	mediump float directedD2 = d2 + 0.55 * across * across;
	return sgsrFastLanczos2(directedD2 * sgsrKernelWidth + outlier);
}
// Clamped to its own nearest 2x2, so shared weights cannot pull a channel out of range.
mediump float sgsrChannel(mediump float w[12], mediump vec4 ud, mediump vec4 l, mediump vec4 r, mediump float invSum)
{
	mediump float acc = dot(vec4(w[0], w[1], w[2], w[3]), ud) + dot(vec4(w[4], w[5], w[6], w[7]), l) + dot(vec4(w[8], w[9], w[10], w[11]), r);
	mediump float lo = min(min(l.y, l.z), min(r.x, r.w));
	mediump float hi = max(max(l.y, l.z), max(r.x, r.w));
	return clamp(acc * invSum, lo, hi);
}
mediump float sgsrMean(mediump vec4 l, mediump vec4 r)
{
	return (l.y+l.z+r.x+r.w)*0.25;
}

vec4 sampleSgsr1(sampler2D tex, vec2 uv)
{
	const float edgeThreshold = 8.0 / 255.0;
	mediump vec4 color = textureLod(tex, uv, 0.0);
	mediump float rcasProtection = 0.0;

	vec2 size = vec2(textureSize(tex, 0));
	vec4 viewport = vec4(1.0 / size, size);
	{
		highp vec2 imgCoord = ((uv*viewport.zw)+vec2(-0.5,0.5));
		highp vec2 imgCoordPixel = floor(imgCoord);
		highp vec2 coord = (imgCoordPixel*viewport.xy);
		mediump vec2 pl = (imgCoord+(-imgCoordPixel));
		mediump vec4 leftG = textureGather(tex,coord, 1);
		mediump vec4 leftR = textureGather(tex,coord,0);
		mediump vec4 leftB = textureGather(tex,coord,2);
		mediump vec4 leftY = 0.2126*leftR + 0.7152*leftG + 0.0722*leftB;
		mediump float colorY = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
		mediump float edgeVote = abs(leftY.z - leftY.y) + abs(colorY - leftY.y) + abs(colorY - leftY.z);
		if(edgeVote > edgeThreshold)
		{
			rcasProtection = smoothstep(edgeThreshold * 1.2, edgeThreshold * 3.0, edgeVote);
			coord.x += viewport.x;
			highp vec2 rightCoord = coord + vec2(viewport.x, 0.0);
			highp vec2 upCoord = coord + vec2(0.0, -viewport.y);
			highp vec2 downCoord = coord + vec2(0.0, viewport.y);
			mediump vec4 rightG = textureGather(tex,rightCoord, 1);
			mediump vec4 upDownG;
			upDownG.xy = textureGather(tex,upCoord, 1).wz;
			upDownG.zw = textureGather(tex,downCoord, 1).yx;
			mediump vec4 rightR = textureGather(tex,rightCoord,0), rightB = textureGather(tex,rightCoord,2);
			mediump vec4 upDownR, upDownB;
			upDownR.xy = textureGather(tex,upCoord,0).wz;
			upDownR.zw = textureGather(tex,downCoord,0).yx;
			upDownB.xy = textureGather(tex,upCoord,2).wz;
			upDownB.zw = textureGather(tex,downCoord,2).yx;

			mediump vec4 rightY = 0.2126*rightR + 0.7152*rightG + 0.0722*rightB;
			mediump vec4 upDownY = 0.2126*upDownR + 0.7152*upDownG + 0.0722*upDownB;
			mediump float meanY = sgsrMean(leftY, rightY);
			leftY -= vec4(meanY); rightY -= vec4(meanY); upDownY -= vec4(meanY);
			mediump vec2 grad = vec2((rightY.x-leftY.z)+(rightY.w-leftY.y), (rightY.x-leftY.z)-(rightY.w-leftY.y));
			mediump float gradLen = length(grad);
			mediump vec2 direction = gradLen > 1e-5 ? grad / gradLen : vec2(0.0);
			mediump float sum = dot(abs(leftY)+abs(rightY)+abs(upDownY),vec4(1.0));
			mediump float std = 2.181818/max(sum, 1e-5);

			mediump float w[12];
			w[0]  = sgsrWeight(pl.x,     pl.y+1.0, upDownY.x, std, direction);
			w[1]  = sgsrWeight(pl.x-1.0, pl.y+1.0, upDownY.y, std, direction);
			w[2]  = sgsrWeight(pl.x-1.0, pl.y-2.0, upDownY.z, std, direction);
			w[3]  = sgsrWeight(pl.x,     pl.y-2.0, upDownY.w, std, direction);
			w[4]  = sgsrWeight(pl.x+1.0, pl.y-1.0, leftY.x, std, direction);
			w[5]  = sgsrWeight(pl.x,     pl.y-1.0, leftY.y, std, direction);
			w[6]  = sgsrWeight(pl.x,     pl.y,     leftY.z, std, direction);
			w[7]  = sgsrWeight(pl.x+1.0, pl.y,     leftY.w, std, direction);
			w[8]  = sgsrWeight(pl.x-1.0, pl.y-1.0, rightY.x, std, direction);
			w[9]  = sgsrWeight(pl.x-2.0, pl.y-1.0, rightY.y, std, direction);
			w[10] = sgsrWeight(pl.x-2.0, pl.y,     rightY.z, std, direction);
			w[11] = sgsrWeight(pl.x-1.0, pl.y,     rightY.w, std, direction);
			mediump float invSum = 1.0 / max((((w[0]+w[1])+(w[2]+w[3]))+((w[4]+w[5])+(w[6]+w[7]))+((w[8]+w[9])+(w[10]+w[11]))), 1e-5);

			color.r = clamp(sgsrChannel(w, upDownR, leftR, rightR, invSum), color.r-sgsrMaxCorrection, color.r+sgsrMaxCorrection);
			color.g = clamp(sgsrChannel(w, upDownG, leftG, rightG, invSum), color.g-sgsrMaxCorrection, color.g+sgsrMaxCorrection);
			color.b = clamp(sgsrChannel(w, upDownB, leftB, rightB, invSum), color.b-sgsrMaxCorrection, color.b+sgsrMaxCorrection);
		}
	}

	color.a = rcasProtection;
	return color;
}
