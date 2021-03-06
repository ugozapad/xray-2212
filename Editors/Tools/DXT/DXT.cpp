// DXT.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "dxtlib.h"
#include "ETextureParams.h"
#include "dds.h"
#include "d3dx.h"

BOOL APIENTRY DllMain( HANDLE hModule, 
                       u32  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

static HFILE gFileOut;
static HFILE gFileIn;

const u32 fcc_DXT1 = MAKEFOURCC('D','X','T','1');
const u32 fcc_DXT2 = MAKEFOURCC('D','X','T','2');
const u32 fcc_DXT3 = MAKEFOURCC('D','X','T','3');
const u32 fcc_DXT4 = MAKEFOURCC('D','X','T','4');
const u32 fcc_DXT5 = MAKEFOURCC('D','X','T','5');

void __cdecl WriteDTXnFile (DWORD count, void *buffer)
{
	if (count==sizeof(DDS_HEADER)){
	// correct DDS header
		DDS_HEADER* hdr=(DDS_HEADER*)buffer;
		if (hdr->dwSize==count){
			switch (hdr->ddspf.dwFourCC){ 
			case fcc_DXT1:
			case fcc_DXT2:
			case fcc_DXT3:
			case fcc_DXT4:
			case fcc_DXT5:
				hdr->ddspf.dwRGBBitCount = 0;
				break;
			}
		}
	}
    _write(gFileOut, buffer, count);
}


void __cdecl ReadDTXnFile (DWORD count, void *buffer)
{
    _read(gFileIn, buffer, count);
}

HRESULT WriteCompressedData(void* data, int miplevel, u32 size)
{
    _write(gFileOut, data, size);
	FillMemory(data,size,0xff);
	return 0;
}
  
ENGINE_API u32* Build32MipLevel(u32 &_w, u32 &_h, u32 &_p, u32 *pdwPixelSrc, STextureParams* fmt, float blend)
{
	R_ASSERT(pdwPixelSrc);
	R_ASSERT((_w%2)==0	);
	R_ASSERT((_h%2)==0	);
	R_ASSERT((_p%4)==0	);

	u32	dwDestPitch	= (_w/2)*4;
	u32*	pNewData= xr_alloc<u32>( (_h/2)*dwDestPitch );
	u8*		pDest	= (u8 *)pNewData;
	u8*		pSrc	= (u8 *)pdwPixelSrc;

	float	mixed_a = (float) u8(fmt->fade_color >> 24);
	float	mixed_r = (float) u8(fmt->fade_color >> 16);
	float	mixed_g = (float) u8(fmt->fade_color >> 8);
	float	mixed_b = (float) u8(fmt->fade_color >> 0);

	float	inv_blend	= 1.f-blend;
	for (u32 y = 0; y < _h; y += 2){
		u8* pScanline = pSrc + y*_p;
		for (u32 x = 0; x < _w; x += 2){
			u8*	p1	= pScanline + x*4;
			u8*	p2	= p1+4;					if (1==_w)	p2 = p1;
			u8*	p3	= p1+_p;				if (1==_h)  p3 = p1;
			u8*	p4	= p2+_p;				if (1==_h)  p4 = p2;
			float	c_r	= float(u32(p1[0])+u32(p2[0])+u32(p3[0])+u32(p4[0])) / 4.f;
			float	c_g	= float(u32(p1[1])+u32(p2[1])+u32(p3[1])+u32(p4[1])) / 4.f;
			float	c_b	= float(u32(p1[2])+u32(p2[2])+u32(p3[2])+u32(p4[2])) / 4.f;
			float	c_a	= float(u32(p1[3])+u32(p2[3])+u32(p3[3])+u32(p4[3])) / 4.f;
			
			if (fmt->flags.is(STextureParams::flFadeToColor)){
				c_r		= c_r*inv_blend + mixed_r*blend;
				c_g		= c_g*inv_blend + mixed_g*blend;
				c_b		= c_b*inv_blend + mixed_b*blend;
			}
			if (fmt->flags.is(STextureParams::flFadeToAlpha)){
				c_a		= c_a*inv_blend + mixed_a*blend;
			}

			float	A	= (c_a+c_a/8.f); 
			int _r = int(c_r);	clamp(_r,0,255);	*pDest++	= u8(_r);
			int _g = int(c_g);	clamp(_g,0,255);	*pDest++	= u8(_g);
			int _b = int(c_b);	clamp(_b,0,255);	*pDest++	= u8(_b);
			int _a = int(A);	clamp(_a,0,255);	*pDest++	= u8(_a);
		}
	}
	_w/=2; _h/=2; _p=_w*4;
	return pNewData;
}
/*
bool IsThisNumberPowerOfTwo(unsigned int n)
{
        return (((n & (n-1)) == 0) && (n != 0));
}
*/
IC u32 GetPowerOf2Plus1(u32 v)
{
    u32 cnt=0;
    while (v) {v>>=1; cnt++; };
    return cnt;
}

void FillRect(u8* data, u8* new_data, u32 offs, u32 pitch, u32 h, u32 full_pitch){
	for (u32 i=0; i<h; i++) CopyMemory(data+(full_pitch*i+offs),new_data+i*pitch,pitch);
}

int DXTCompressImage	(LPCSTR out_name, u8* raw_data, u32 w, u32 h, u32 pitch, 
						STextureParams* fmt, u32 depth)
{
	R_ASSERT((0!=w)&&(0!=h));

    gFileOut = _open( out_name, _O_WRONLY|_O_BINARY|_O_CREAT|_O_TRUNC,_S_IWRITE);
    if (gFileOut==-1){
        fprintf(stderr, "Can't open output file %s\n", out_name);
        return false;
    }

	HRESULT hr=-1;
// convert to Options
    CompressionOptions		nvOpt;
	ZeroMemory				(&nvOpt,sizeof(nvOpt));

    if (fmt->flags.is(STextureParams::flGenerateMipMaps))	nvOpt.MipMapType=dGenerateMipMaps;
    else													nvOpt.MipMapType=dNoMipMaps;
    nvOpt.bBinaryAlpha	    = !!(fmt->flags.is(STextureParams::flBinaryAlpha));
    nvOpt.bAlphaBorder		= !!(fmt->flags.is(STextureParams::flAlphaBorder));
    nvOpt.bBorder			= !!(fmt->flags.is(STextureParams::flColorBorder));
    nvOpt.BorderColor.u		= fmt->border_color;
    nvOpt.bFadeColor		= !!(fmt->flags.is(STextureParams::flFadeToColor));
    nvOpt.bFadeAlpha		= FALSE;//fmt->flag.bFadeToAlpha;
    nvOpt.FadeToColor.u		= 0;//fmt->fade_color;
    nvOpt.FadeAmount		= 0;//fmt->fade_amount;
    nvOpt.bDitherColor		= !!(fmt->flags.is(STextureParams::flDitherColor));
	nvOpt.bDitherMIP0		= !!(fmt->flags.is(STextureParams::flDitherEachMIPLevel));
    nvOpt.bGreyScale		= !!(fmt->flags.is(STextureParams::flGreyScale));
	nvOpt.TextureType		= (fmt->type==STextureParams::ttCubeMap)?kTextureTypeCube:kTextureType2D;
    switch(fmt->fmt){
    case STextureParams::tfDXT1				: 	nvOpt.TextureFormat = kDXT1	; 	break;
    case STextureParams::tfADXT1			: 	nvOpt.TextureFormat = kDXT1a; 	break;
    case STextureParams::tfDXT3				: 	nvOpt.TextureFormat = kDXT3	; 	break;
    case STextureParams::tfDXT5				: 	nvOpt.TextureFormat = kDXT5	; 	break;
	case STextureParams::tf4444				:
    case STextureParams::tf1555				: 	nvOpt.TextureFormat = k1555	; 	break;
    case STextureParams::tf565				: 	nvOpt.TextureFormat = k565	; 	break;
    case STextureParams::tfRGB				: 	nvOpt.TextureFormat = k888	; 	break;
    case STextureParams::tfRGBA				: 	nvOpt.TextureFormat = k8888	; 	break;
    }
	
	switch(fmt->mip_filter){
	case STextureParams::kMIPFilterAdvanced	:	break;

	case STextureParams::kMIPFilterPoint	:	nvOpt.MIPFilterType = kMIPFilterPoint		;	break;		
	case STextureParams::kMIPFilterBox		:	nvOpt.MIPFilterType = kMIPFilterBox			;	break;
	case STextureParams::kMIPFilterTriangle	:	nvOpt.MIPFilterType = kMIPFilterTriangle	;	break;	
	case STextureParams::kMIPFilterQuadratic:	nvOpt.MIPFilterType = kMIPFilterQuadratic	;	break;	
	case STextureParams::kMIPFilterCubic	:	nvOpt.MIPFilterType = kMIPFilterCubic		;	break;

	case STextureParams::kMIPFilterCatrom	:	nvOpt.MIPFilterType = kMIPFilterCatrom		;	break;
	case STextureParams::kMIPFilterMitchell	:	nvOpt.MIPFilterType = kMIPFilterMitchell	;	break;

	case STextureParams::kMIPFilterGaussian	:	nvOpt.MIPFilterType = kMIPFilterGaussian	;	break;
	case STextureParams::kMIPFilterSinc		:	nvOpt.MIPFilterType = kMIPFilterSinc		;	break;
	case STextureParams::kMIPFilterBessel	:	nvOpt.MIPFilterType = kMIPFilterBessel		;	break;

	case STextureParams::kMIPFilterHanning	:	nvOpt.MIPFilterType = kMIPFilterHanning		;	break;
	case STextureParams::kMIPFilterHamming	:	nvOpt.MIPFilterType = kMIPFilterHamming		;	break;
	case STextureParams::kMIPFilterBlackman	:	nvOpt.MIPFilterType = kMIPFilterBlackman	;	break;
	case STextureParams::kMIPFilterKaiser	:	nvOpt.MIPFilterType = kMIPFilterKaiser		;	break;
	}
//-------------------
 
	if ((fmt->flags.is(STextureParams::flGenerateMipMaps))&&((STextureParams::kMIPFilterAdvanced==fmt->mip_filter)||
		(fmt->flags.is(STextureParams::flFadeToColor))||(fmt->flags.is(STextureParams::flFadeToAlpha))))
	{
		nvOpt.MipMapType	= dUseExistingMipMaps;

		u8* pImagePixels	= 0;
		int numMipmaps		= GetPowerOf2Plus1(__min(w,h));
		u32 line_pitch		= w*2*4;
		pImagePixels		= xr_alloc<u8>(line_pitch*h);
		u32 w_offs			= 0;
		u32 dwW				= w;
		u32 dwH				= h;
		u32 dwP				= pitch;
		u32* pLastMip		= xr_alloc<u32>(w*h*4);
		CopyMemory			(pLastMip,raw_data,w*h*4);
		FillRect			(pImagePixels,(u8*)pLastMip,w_offs,pitch,dwH,line_pitch);
		w_offs				+= dwP;

		float	blend		= 0;
		float	d			= (float(fmt->fade_amount)/100.f)*numMipmaps-1;
		if (d<1.f) d		= 1.f;
		for (int i=1; i<numMipmaps; i++){
			if ((fmt->flags.is(STextureParams::flFadeToColor))||(fmt->flags.is(STextureParams::flFadeToAlpha))){
				blend		= i/d;
				if (blend>1.f) blend=1.f;
			}
			u32* pNewMip	= Build32MipLevel(dwW,dwH,dwP,pLastMip,fmt,blend);
			FillRect		(pImagePixels,(u8*)pNewMip,w_offs,dwP,dwH,line_pitch);
			xr_free			(pLastMip); 
			pLastMip		= pNewMip; 
			pNewMip			= 0;
			w_offs			+= dwP;
		}
		xr_free				(pLastMip);

		hr					= nvDXTcompress(pImagePixels, w*2, h, line_pitch, &nvOpt, depth, 0);
		xr_free				(pImagePixels);
	}else{
		hr					= nvDXTcompress(raw_data, w, h, pitch, &nvOpt, depth, 0);
	}
    _close					(gFileOut);
	if (hr!=DD_OK)			return 0;
	else					return 1;
}

extern int DXTCompressBump(LPCSTR out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch, STextureParams* fmt, u32 depth);

extern "C" __declspec(dllexport) 
int DXTCompress	(LPCSTR out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch, 
					STextureParams* fmt, u32 depth)
{
	switch (fmt->type){
	case STextureParams::ttImage:	
	case STextureParams::ttCubeMap: 
		return DXTCompressImage	(out_name, raw_data, w, h, pitch, fmt, depth);
	break;
	case STextureParams::ttBumpMap: 
		return DXTCompressBump	(out_name, raw_data, normal_map, w, h, pitch, fmt, depth);
	break;
	default: NODEFAULT;
	}
}
