
#pragma once

#include "Annotations.h"

struct MEMORYDC
{
	HBITMAP    hBitmap;
	DIBSECTION DibS;
	CDC        DC;	// or HDC? ..CDC better cos I can use GDI functions using class '->' syntax.
	int        Wd, Ht;
	int        InitBitmap, InitDC;
};

struct ANCHORPOINT
{
	int x, y;
	int TL, TR, BL, BR;
	int L, R, T, B;
};

struct MY_RECT
{
	int Lf, Rh, Tp, Bt;
};

#define MAXTILES	48
struct SINGLETILE
{
	MY_RECT DSP;			// the display tile inclusive/exclusive rect.
	MY_RECT InclusiveLimits;// inclusive limits for bounding box UI editing/positioning (limit are, -1/width, -1/height)
};

struct DISPLAY_TILES
{
	int NumTiles;
	int NumRows, NumColumns;
	int Tile_Wd, Tile_Ht;
	SINGLETILE Tiles[MAXTILES];
	void Reset()
	{
		NumTiles = NumRows = NumColumns = 0;
		Tile_Wd = Tile_Ht = 0;
	}
};

struct MYGDIOBJS
{
	CFont Font_16, Font_24;
	CPen  PenCrossHairs, PenAnchors, PenAnchorsThick, PenHidden;
	CPen  PenBox, PenSelectedBox, PenRed, PenOrange;
};

class CDisplay
{
private:
	int       m_PaintCount;
	MEMORYDC  m_Pic, m_DspBkg, m_DspAll;
	CFont     m_Font_16, m_Font_24;
	MYGDIOBJS m_GdiObjs;
	int       m_LineThickness, m_SelectedBBoxIdx, m_FixedSizeError, m_TileGap, m_HideCrossHairs;
	RECT      m_UIDefinedBox;
	POINT     m_CrossHairs, m_PrevCrossHairs;
	char      m_TempStr[128];

	DISPLAY_TILES   m_DT;
	ANCHORPOINT     m_Anchors[9];
	int             m_MovementAnchorIdx, m_AnchorsThickness, m_MovingAnchor;
//	DISPLAYOPTIONS *m_pDspOps;

	RECT GetBBoxScreenCoordinates(int Offset_X, int Offset_Y, int Picture_Wd, int Picture_Ht, BBOX *pBox);
	CPen *MySelectPen(CDC *pDC, CPen *pPen, int setBKMode);
	void DrawLineEx(CDC *pDC, int x1, int y1, int x2, int y2, int Thickness, COLORREF C);
	void DrawLine(CDC *pDC, int x1, int y1, int x2, int y2);
	void DrawRectEx(CDC *pDC, int Top, int Bot, int Left, int Right, int Thickness, COLORREF C);
	void DrawRect(CDC *pDC, RECT Rt);
	void DrawRect(CDC *pDC, int Top, int Bot, int Left, int Right);
	int  SetupDisplayTiling(BOOL Tiling, int NumTiles, int DisplayWd, int DisplayHt, DISPLAYOPTIONS *pOptions);
	void CalculateTileSize(int NumTiles, float DisplayWd, float GapWd, float DisplayHt, float GapHt, float BmpWd, float BmpHt);
	void DrawAnnotationsOnTile(CDC *pDC, FILENAMEGROUP *pGrp, int Tiling, int TileIndex, DISPLAYOPTIONS *pOptions);
	void DrawLabel(CDC *pDC, int InfoLevel, RECT Rt, SINGLETILE *pST, CLASSIFIEDLINE *pCL, SOURCELINE *pSL_Debug);
	void DrawAnchorPoint(CDC *pDC);
	void DrawBboxAndLabel(CDC *pDC, int TileIndex, CLASSIFIEDLINE *pCL, DISPLAYOPTIONS *pOptions, int Selected, SOURCELINE *pSL_Debug);
	SOURCELINE *GetSourceLine(FILENAMEGROUP *pGrp, int Index);
	void UpdateBboxScreenCoordinates(FILENAMEGROUP *pGrp, DISPLAYOPTIONS *pOptions);
	void TextOnDisplay(CDC *pDC, char *pText, COLORREF CR, int x, int y, int Size);
	int  DrawUIDefinedBox(CDC *pDC);
	char *GetLabelText(int class_id);
	int  InitDisplayDC(CDC *pDC, MEMORYDC *pMemDC);
	void CreateReusableObjects();
	void DestroyReusableObjects();
	void FreeBitmapObjects(MEMORYDC *pMDC);
	int  ScreenToBBox(int Offset_X, int Offset_Y, int Picture_Wd, int Picture_Ht, RECT Rt_all_exclusive, BBOX *pBbox);
	void SetAnchorPoints(RECT Rt);
	int  FixedPictureDimensions(int Tiling, int DisplayWd, int DisplayHt, int Requested_Wd, int Requested_Ht);
	int  LoadTileCoordinates(int DisplayWd, int DisplayHt, int GapWd, int GapHt);
	void DrawCrossHairs(CDC *pDC);
	int  EditingPointWithinLimits(int x, int y, SINGLETILE *pST);

public:
	CDisplay();
	~CDisplay();

	int  LoadPicture(char *pPath, CDC *pDC);
	int  DrawPictureAndAnnotations(CDC *pDC, int DisplayWd, int DisplayHt, int Tiling, FILENAMEGROUP *pGrp, DISPLAYOPTIONS *pOptions, int RefreshBackground);
	void DeletePictureObjects();
	void ClearDisplay(CDC *pDC, int DisplayWd, int DisplayHt);
	int  GetTileIdxFromScreenCoords(int x, int y);
	int  GetBboxIdxFromScreenCoords(FILENAMEGROUP *pGrp, int x, int y);
	int  SetSelectedBox(FILENAMEGROUP *pGrp, int SelectionType, int Idx);
	int  GetSelectedBox(FILENAMEGROUP *pGrp, int SelectionType);
	void ClearBoxSelections(FILENAMEGROUP *pGrp);
	int  SetMovementAnchor(int x, int y);
	void UpdateAnchorPosition(FILENAMEGROUP *pGrp, int x, int y, int Tiling);
	void GetPictureDims(int &Wd, int &Ht, float &Scale, int &display_Wd, int &display_Ht);
	int  StartUIDefinedBox(int x, int y);
	void MoveUIDefinedBox(int x, int y);
	int  GetUIDefinedBox(BBOX &Bbox, int MinPixelSize);
	int  SetCrossHairsPosition(int x, int y);
	void HideCrossHairs();
};

