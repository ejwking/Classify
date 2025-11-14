
#include "pch.h"
#include "BitmapFile.h"
#include "useful.h"


CBitmapFile g_CBmpFile;


CBitmapFile::CBitmapFile()
{
	static int OneObject = 0;

	if(OneObject != 0)
		MsgBox2("CBitmapFile usage error, more than one object created");

	m_FileSize = 0;
	m_pReadFileBuf = nullptr;

	OneObject++;
}

CBitmapFile::~CBitmapFile()
{
	free(m_pReadFileBuf);
}

HBITMAP CBitmapFile::DecodeData(IStream *pstm)
{
	// various options (IPicture, gdiplus, OpenCV) to decode the image data (jpg/png/etc), I'm using CImage which is the easiest choice because i dont need to include any dependencies.
	// https://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer
	// 
	// ..todo switch to openCV.

	_COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));
    IStreamPtr sp_stream { pstm, false };
    CImage img {};
    _com_util::CheckError(img.Load(sp_stream));
    HBITMAP hBitmap = img.Detach();
	// This implementation either throws a _com_error, or returns an HBITMAP that refers to the image constructed from the in-memory data.
	// When the function returns, the memory buffer can be safely freed. The returned HBITMAP is owned by the caller, and needs to be released with a call to DeleteObject.
	return hBitmap;
}

#ifdef globallockversion
HBITMAP CBitmapFile::LoadEncodedImage_GL(LPCTSTR szFile)
{
	// GlobalAlloc VERSION. 
	// This version works, and maybe the global lock is necessary in this case, although I cant see why, however I did another version of this function using the heap functions.
	// 
	// From msdn : The global functions have greater overhead and provide fewer features than other memory management functions. New applications should 
	// use the heap functions unless documentation states that a global function should be used. For more information, see Global and Local Functions.

	HANDLE hFile = CreateFile(szFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	_ASSERTE(INVALID_HANDLE_VALUE != hFile);
	DWORD dwFileSize = GetFileSize(hFile, NULL);
	_ASSERTE(-1 != dwFileSize);

	LPVOID pvData = NULL;
	// alloc memory based on file size
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwFileSize);
	_ASSERTE(NULL != hGlobal);

	pvData = GlobalLock(hGlobal);
	_ASSERTE(NULL != pvData);
    TRACE("\nGlobalAlloc allocated %d bytes\n", GlobalSize(hGlobal));

	DWORD dwBytesRead = 0;
	// read file and store in global memory
	BOOL bRead = ReadFile(hFile, pvData, dwFileSize, &dwBytesRead, NULL);
	_ASSERTE(FALSE != bRead);
	GlobalUnlock(hGlobal);
	CloseHandle(hFile);

	LPSTREAM pstm = NULL;	// IStream*
	// create IStream* from global memory
	HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pstm);
	_ASSERTE(SUCCEEDED(hr) && pstm);
	
	HBITMAP hBitmap = DecodeData(pstm);
	pstm->Release();
	GlobalFree(hGlobal);
	return hBitmap;
}
#endif

HBITMAP CBitmapFile::LoadEncodedImage(LPCTSTR szFile)
{
	HANDLE hFile = CreateFile(szFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	_ASSERTE(INVALID_HANDLE_VALUE != hFile);

	DWORD Sz = GetFileSize(hFile, NULL);
	if(Sz > m_FileSize){
		BYTE *pOldBuf = m_pReadFileBuf;
		m_FileSize = Sz;
		if((m_pReadFileBuf = static_cast<BYTE*>(realloc(static_cast<void*>(m_pReadFileBuf), m_FileSize))) == 0){
			if(pOldBuf)
				free(pOldBuf);
			m_pReadFileBuf = nullptr;
		}
	}

	if(m_pReadFileBuf){
		DWORD dwBytesRead = 0;
		// read file and store in memory
		BOOL bRead = ReadFile(hFile, static_cast<LPVOID>(m_pReadFileBuf), m_FileSize, &dwBytesRead, NULL);
		_ASSERTE(FALSE != bRead);
		CloseHandle(hFile);

		// create IStream* from memory
		IStream *pstm = SHCreateMemStream(m_pReadFileBuf, static_cast<UINT>(m_FileSize));
		_ASSERTE(pstm);
		HBITMAP hBitmap = DecodeData(pstm);
		return hBitmap;
	}
	return 0;
}

int CBitmapFile::BitmapToFile_bmp(HBITMAP hBitmap, HDC hDC, char *pPath)
{
	int    Ok = 1;
	BITMAP bmp;
	DWORD  dwBytesWritten = 0, dwSizeofDIB = 0, dwBmpSize = 0;
	HANDLE hFile = NULL, hDIB = NULL;
	char  *lpbitmap = NULL;

	// Get the BITMAP from the HBITMAP.
	GetObject(hBitmap, sizeof(BITMAP), &bmp);
	BITMAPFILEHEADER bmfHeader;
	BITMAPINFOHEADER bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bmp.bmWidth;
	bi.biHeight = bmp.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 24;//32;
	bi.biCompression = BI_RGB; // BI_JPEG / BI_PNG ..they dont work?
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	dwBmpSize = (((bmp.bmWidth * bi.biBitCount) + (bi.biBitCount-1)) / bi.biBitCount) * 4 * bmp.bmHeight;

	// Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that call HeapAlloc using a handle to 
	// the process's default heap. Therefore, GlobalAlloc and LocalAlloc have greater overhead than HeapAlloc.
	hDIB = GlobalAlloc(GHND, dwBmpSize);
	lpbitmap = (char*)GlobalLock(hDIB);

	// Gets the "bits" from the bitmap, and copies them into a buffer that's pointed to by lpbitmap.
	GetDIBits(hDC, hBitmap, 0, (UINT)bmp.bmHeight, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	// A file is created, this is where we will save the bitmap.
	hFile = CreateFile(pPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// Add the size of the headers to the size of the bitmap to get the total file size.
	dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	// Offset to where the actual bitmap bits start.
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);

	// Size of the file.
	bmfHeader.bfSize = dwSizeofDIB;

	// bfType must always be BM for Bitmaps.
	bmfHeader.bfType = 0x4D42; // BM.

	Ok &= WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
	Ok &= WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
	Ok &= WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

	// Unlock and Free the DIB from the heap.
	GlobalUnlock(hDIB);
	GlobalFree(hDIB);

	// Close the handle for the file that was created.
	CloseHandle(hFile);
	return Ok;
}

int CBitmapFile::BitmapToFile(HBITMAP hBitmap, HDC hDC, char *pPath)
{
	hDC = hDC; // stop warning.
//	if(use_cimage){
		// msdn "... the file name's file extension will be used to determine the image format.
		// If no extension is provided, the image will be saved in BMP format."
		CImage image;
		image.Attach(hBitmap);
		HRESULT Result = image.Save(pPath);
		return (Result == S_OK);
/*	}
	else{
		// no need for this really, but I will never been saving as bmp anyway, always jpg/png.
		return BitmapToFile_bmp(hBitmap, hDC, pPath);
	}*/
	return 0;
}

HBITMAP CBitmapFile::GetBitmapHandle(char *pPath)
{
	if(FileExists(pPath) == 0){
		MsgBox2("error - LoadPictureFile, file does not exist\r\n%s", pPath);
	}
	else{
	/*	size_t Len = strlen(pPath);
		if(Len > 4){
			if(         pPath[Len-4] =='.' && 
				tolower(pPath[Len-3])=='b' && 
				tolower(pPath[Len-2])=='m' && 
				tolower(pPath[Len-1])=='p' )
			{
				return (HBITMAP)::LoadImage(NULL, pPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE|LR_CREATEDIBSECTION);	// DWORD Err = GetLastError();
			}
		}*/
		// jpg, png, tiff. ACTUALLY this does .bmp too, no need for the above LoadImage.
		return LoadEncodedImage(pPath);
	}
	return nullptr;
}

void CBitmapFile::AverageRGB(int &R, int &G, int &B, int Samples)
{
	if(Samples > 1){
		R /= Samples;
		G /= Samples;
		B /= Samples;
	}
}

void CBitmapFile::AdditionRGB(COLORREF Clr, int &R, int &G, int &B, int &Samples)
{
	R += GetRValue(Clr);
	G += GetGValue(Clr);
	B += GetBValue(Clr);
	Samples++;
}

void CBitmapFile::SpecialEffects(CDC *pDC, int Wd, int Ht, BMP_SPECIAL_FX *pSE)
{
	// used for blurring sections of the bitmap.
	for(int i=0; i<pSE->NumBoxes; i++){
		RECT fullRt = pSE->pBoxes[i];

		if(fullRt.left < 0) fullRt.left = 0;
		if(fullRt.top  < 0) fullRt.top  = 0; // remember, left/top inclusive, right/bottom exclusive. Ditto for CDC::Rectangle, see below.
		if(fullRt.right  > Wd) fullRt.right  = Wd;
		if(fullRt.bottom > Ht) fullRt.bottom = Ht;

		int W = fullRt.right-fullRt.left, H = fullRt.bottom-fullRt.top;
		if(W>0 && H>0){
			int X_res = min(W/5, 6), Y_res = min(H/5, 6);
			// W/5 and H/5 is saying divide the rect up at 5 pixel intervals, but with larger objects that can result in too 
			// much resolution, resolution must be capped at 6 otherwise the object can become visible (dont go higher than 6).
			//
			// CLASSIFYING NOTES, if i annotated a single vehicle by placing several blur boxes over it this would effectively increase the resolution and make the bluring less effective, 
			// so use one blur box if possible... SO IDEALLY at the above resolution the blur box should cover a large section of the vehicle, if it covers just a small section of it the 
			// resolution could be too high, (but sometimes only a small section of the vehicle is visible). 
			// 
			// ALSO be sure to cover the whole vehicle, seen some examples where i dont think the vehicle is covered well enough.

			// keep the blur pixels fairly square, so long and thin blur rects dont have long and thin blur pixels. THIS MIGHT ALLOW SMALLER VEHICLE SECTIONS (WHICH ARE USUALLY LONG AND THIN) to be be blurred with higher resolution.
				 if(W < H) X_res /= (H/W);
			else if(H < W) Y_res /= (W/H);

			X_res = max(X_res, 1);
			Y_res = max(Y_res, 1);
			for(int x=0; x<X_res; x++){
				RECT Cell;
				Cell.left  = fullRt.left + ((W*x)/X_res);
				Cell.right = (x==X_res-1) ? fullRt.right : (fullRt.left + ((W*(x+1))/X_res));
				for(int y=0; y<Y_res; y++){
					Cell.top    = fullRt.top + ((H*y)/Y_res);
					Cell.bottom = (y==Y_res-1) ? fullRt.bottom : (fullRt.top + ((H*(y+1))/Y_res));

					int Samples=0, R=0, G=0, B=0;
					int pix_Y = (Cell.top + Cell.bottom) / 2;
					int pix_X = (Cell.left + Cell.right) / 2;

					AdditionRGB(pDC->GetPixel(pix_X, pix_Y), R, G, B, Samples);
					if(pix_X-1 >= 0) AdditionRGB(pDC->GetPixel(pix_X-1, pix_Y), R, G, B, Samples);
					if(pix_X+1 < Wd) AdditionRGB(pDC->GetPixel(pix_X+1, pix_Y), R, G, B, Samples);
					if(pix_Y-1 >= 0) AdditionRGB(pDC->GetPixel(pix_X, pix_Y-1), R, G, B, Samples);
					if(pix_Y+1 < Ht) AdditionRGB(pDC->GetPixel(pix_X, pix_Y+1), R, G, B, Samples);
					AverageRGB(R, G, B, Samples);

					COLORREF CR = RGB(R,G,B);
					CBrush Brh(CR);
					pDC->SelectObject(&Brh);
					CPen Pen(PS_SOLID, 1, CR);
					pDC->SelectObject(&Pen);
					pDC->Rectangle(&Cell);
					// CDC::Rectangle
					// The rectangle extends up to, but doesn't include, the right and bottom coordinates. This means that the height of the rectangle is y2 - y1 and the width of the rectangle is x2 - x1. 
				}
			}
		}
	}
}

int CBitmapFile::SaveCroppedBitmap(char *pSourceBmpPath, RECT CropZone, char *pDestinationBmpPath, int DestBmpWd, int DestBmpHt, int HorizontalFlip, BMP_SPECIAL_FX *pSE/*=nullptr*/)
{
	int Ok = 0;
	HBITMAP hBitmap = GetBitmapHandle(pSourceBmpPath);
	if(hBitmap){
		DIBSECTION DibS;
		GetObject(hBitmap, sizeof(DibS), &DibS);
		if(CropZone.left<0 || CropZone.top<0 || CropZone.left>=CropZone.right || CropZone.top>=CropZone.bottom){
			MsgBox2("error - SaveCroppedBitmap crop zone, l%d r%d t%d b%d", CropZone.left, CropZone.right, CropZone.top, CropZone.bottom);
			return 0;
		}
		if(CropZone.right>DibS.dsBm.bmWidth || CropZone.bottom>DibS.dsBm.bmHeight){
			MsgBox2("error - SaveCroppedBitmap crop zone, w%d h%d", DibS.dsBm.bmWidth, DibS.dsBm.bmHeight);
			return 0;
		}			
		CDC dc;
		if(dc.CreateCompatibleDC(NULL)){
			dc.SelectObject(hBitmap);
			HBITMAP hBitmapDest; 
			hBitmapDest = CreateCompatibleBitmap(dc.m_hDC, DestBmpWd, DestBmpHt);
			if(hBitmapDest){
				CDC dcDest;
				if(dcDest.CreateCompatibleDC(&dc)){
					dcDest.SelectObject(hBitmapDest);
					int Src_Wd = CropZone.right - CropZone.left;
					int Src_Ht = CropZone.bottom - CropZone.top;

					if( HorizontalFlip    || 
						DestBmpWd!=Src_Wd || // requested width/height does not match crop zone dimensions on the source bitmap, so we are stretching.
						DestBmpHt!=Src_Ht )
					{
						dcDest.SetStretchBltMode(HALFTONE);
						if(HorizontalFlip) dcDest.StretchBlt(0, 0, DestBmpWd, DestBmpHt, &dc, CropZone.right-1, CropZone.top, -Src_Wd, Src_Ht, SRCCOPY);
						else               dcDest.StretchBlt(0, 0, DestBmpWd, DestBmpHt, &dc, CropZone.left,    CropZone.top,  Src_Wd, Src_Ht, SRCCOPY);
					}
					else{
						// when we are not flipping/stretching BitBlt should be used, its quicker, and StretchBlt may interpolate.
						dcDest.BitBlt(0, 0, DestBmpWd, DestBmpHt, &dc, CropZone.left, CropZone.top, SRCCOPY);
					}

					if(pSE)
						SpecialEffects(&dcDest, DestBmpWd, DestBmpHt, pSE);
					if(BitmapToFile(hBitmapDest, dcDest.m_hDC, pDestinationBmpPath))
						Ok = 1;
					dcDest.DeleteDC();
				}
				DeleteObject(hBitmapDest);
			}
			dc.DeleteDC();
		}
		DeleteObject(hBitmap);
	}
	return Ok;
}

struct DIBITMAP
{
	int BOTTOM_UP;
	int W,H,BPP;				/* Dimensions of active part of buffer          */
	int Span;					/* Width of buffer in bytes including alignment */
	int Planes;					/* Bytes per pixel, usually 1 or 3              */
	unsigned char *lpData;		/* Pointer to the bitmap data (are bottom up)   */
};
static DIBITMAP g_DIB;

#define myLDibXY(LDib, bias, span, _x, _y) LDib.lpData[(LDib.H-(_y)-1) * span + (_x) * (LDib.Planes) + (bias)]
#define myLDibXY_BOTTOM_DOWN(LDib, bias, span, _x, _y) LDib.lpData[ (_y * -span) + (_x * LDib.Planes) + (bias)]
static int PictureIsColour()
{
	int MIN_DIF = 5;
	if(g_DIB.Planes == 1)
		return 0;
	if(g_DIB.Planes == 3){
		int c, i, x, y, Ht, Wd;
		unsigned char Pix1, Pix2, Pix3, *pData;
		RECT Rt;
		i         = g_DIB.W / 6;
		Rt.left   = i;
		Rt.right  = g_DIB.W - i;
		i         = g_DIB.H / 6;
		Rt.top    = i;
		Rt.bottom = g_DIB.H - i;

		Ht = Rt.bottom - Rt.top;
		Wd = Rt.right - Rt.left;
		for(c=0; c<2; c++){
			for(i=1; i<=8; i++){
				// Test pixels to see if image is colour ...because even if image is 24bit it might still be greyscale, this is the case for many of our test images.
				// Test 8 pixels in diagonal line (making a cross) and see if values differ across the pixel channels.
				x = (Wd * i * 10) / 100;
				y = (Ht * i * 10) / 100;
				if(c==1)
					x = Wd - x;
				x += Rt.left;
				y += Rt.top;

				if(g_DIB.BOTTOM_UP)
					pData = &myLDibXY(g_DIB, 0, g_DIB.Span, x, y);
				else
					pData = &myLDibXY_BOTTOM_DOWN(g_DIB, 0, g_DIB.Span, x, y);

				Pix1 = *pData;		// RED   ######### planes == 3
				Pix2 = *(pData + 1);// GREEN ######### planes == 3
				Pix3 = *(pData + 2);// BLUE  ######### planes == 3

			//	if (Pix1!=Pix2 || Pix1!=Pix3 || Pix2!=Pix3 ){
				if (abs(Pix1-Pix2)>MIN_DIF || abs(Pix1-Pix3)>MIN_DIF || abs(Pix2-Pix3)>MIN_DIF ){	// Strange stuff, we have 24bit images, greyscale in appearance, but values in each channel can differ by one. Why?
					return 1;
				}
			}
		}
		return 0;
	}
	MsgBox2("error - PictureIsColour");
	return -1;
}

int CBitmapFile::GetBitmapSpecifics(char *pPath, int *pWidth, int *pHeight, int *pColour)
{
	// this is a bit slow for getting height and width, but i can wait a couple of minutes for it to go through the bitmaps so I shall leave it.
	// copilot/gpt-5 gave me some quicker but far more complicated options.
	int Ok = 0;
	HBITMAP hBitmap = GetBitmapHandle(pPath);
	if(hBitmap){
		Ok = 1;
		DIBSECTION DibS;
		GetObject(hBitmap, sizeof(DibS), &DibS);
		g_DIB.lpData = (BYTE*)DibS.dsBm.bmBits;
		g_DIB.BPP    = DibS.dsBm.bmBitsPixel;
		g_DIB.Planes = ((DibS.dsBm.bmBitsPixel==32) || (DibS.dsBm.bmBitsPixel==24)) ? 3 : 1;
		g_DIB.H      = DibS.dsBm.bmHeight;
		g_DIB.Span   = DibS.dsBm.bmWidthBytes;
		g_DIB.W      = DibS.dsBm.bmWidth;
		g_DIB.BOTTOM_UP = 1;
		if(pWidth)
			*pWidth = DibS.dsBm.bmWidth;
		if(pHeight)
			*pHeight = DibS.dsBm.bmHeight;
		if(pColour)
			*pColour = PictureIsColour();
		DeleteObject(hBitmap);
	}
	return Ok;
}
