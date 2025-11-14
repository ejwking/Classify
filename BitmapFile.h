
#pragma once

#include "Useful.h"

struct BMP_SPECIAL_FX
{
	RECT *pBoxes;
	int NumBoxes;
};

class CBitmapFile
{
public:
	CBitmapFile();
	~CBitmapFile();
	HBITMAP GetBitmapHandle(char *pPath);
	int GetBitmapSpecifics(char *pPath, int *pWidth, int *pHeight, int *pColour);
	int BitmapToFile(HBITMAP hBitmap, HDC hDC, char *pPath);
	int SaveCroppedBitmap(char *pSourceBmpPath, RECT CropZone, char *pDestinationBmpPath, int DestBmpWd, int DestBmpHt, int HorizontalFlip, BMP_SPECIAL_FX *pSE=nullptr);

private:
	BYTE *m_pReadFileBuf;
	DWORD m_FileSize;

	int  BitmapToFile_bmp(HBITMAP hBitmap, HDC hDC, char *pPath);
	void SpecialEffects(CDC *pDC, int Wd, int Ht, BMP_SPECIAL_FX *pSE);
	void AdditionRGB(COLORREF Clr, int &R, int &G, int &B, int &Samples);
	void AverageRGB(int &R, int &G, int &B, int Samples);

	HBITMAP DecodeData(IStream *pstm);
	HBITMAP LoadEncodedImage(LPCTSTR szFile);
};

extern CBitmapFile g_CBmpFile;