
#include "pch.h"
#include "Display.h"

enum COLR
{
	Black   = RGB(0,0,0),
 	White   = RGB(255,255,255),
 	Red     = RGB(255,0,0),
 	Green   = RGB(0,255,0),
 	Blue    = RGB(0,0,255),
 	Yellow  = RGB(255,255,0),
	Orange  = RGB(255,135,0),
 	Cyan    = RGB(0,255,255),
 	Magenta = RGB(255,0,255),
 	Gray    = RGB(128,128,128),
 	Purple  = RGB(128,0,128),
 	Teal    = RGB(0,128,128)
};
#define OFF_DISPLAY_POSITION -1

CDisplay::CDisplay()
{
	m_PaintCount = 0;
	m_LineThickness = 2;
	m_TileGap = 10;
	m_MovementAnchorIdx = -1;
	m_MovingAnchor = 0;
	m_AnchorsThickness = 0;
	m_FixedSizeError = 0;
	m_HideCrossHairs = 0;
	m_PrevCrossHairs.x = m_PrevCrossHairs.y = m_CrossHairs.x = m_CrossHairs.y = OFF_DISPLAY_POSITION;
	memset(&m_UIDefinedBox, 0, sizeof(m_UIDefinedBox));
	//
	m_Pic.InitDC        = 0;
	m_Pic.InitBitmap    = 0;
	m_DspBkg.InitDC     = 0;
	m_DspBkg.InitBitmap = 0;
	m_DspAll.InitDC     = 0;
	m_DspAll.InitBitmap = 0;

	CreateReusableObjects();
}

CDisplay::~CDisplay()
{
	FreeBitmapObjects(&m_Pic);
	FreeBitmapObjects(&m_DspBkg);
	FreeBitmapObjects(&m_DspAll);
	DestroyReusableObjects();
}

void CDisplay::DrawLine(CDC *pDC, int x1, int y1, int x2, int y2)
{
	pDC->MoveTo(x1, y1);
	pDC->LineTo(x2, y2);
}

void CDisplay::DrawLineEx(CDC *pDC, int x1, int y1, int x2, int y2, int Thickness, COLORREF C)
{
	int Style = (Thickness>1) ? PS_SOLID : PS_DOT;
	if(Style != PS_SOLID){
		pDC->SetBkMode(OPAQUE);
		pDC->SetBkColor(RGB(0,0,0));
	}
	CPen *oldpen, pen(Style, Thickness, C);
	oldpen = pDC->SelectObject(&pen);
	pDC->SetROP2(R2_COPYPEN);
	pDC->MoveTo(x1, y1);
	pDC->LineTo(x2, y2);
	pDC->SelectObject(oldpen);
}

void CDisplay::DrawRect(CDC *pDC, int Top, int Bot, int Left, int Right)
{
	pDC->MoveTo(Left,  Top); pDC->LineTo(Left,  Bot);	// not continuous, like this because the PS_DOT's need to be aligned.
	pDC->MoveTo(Left,  Top); pDC->LineTo(Right, Top);
	pDC->MoveTo(Right, Top); pDC->LineTo(Right, Bot);
	pDC->MoveTo(Left,  Bot); pDC->LineTo(Right, Bot);
	if(m_LineThickness > 1){
		// make lines 2 thick by drawing another line, have to draw them individually so the PS_DOT's match up.
		// its the inner line (drawn above) that defines the position of the box, so here we are drawing more lines around the outside.
		pDC->MoveTo(Left-1,  Top); pDC->LineTo(Left-1,  Bot);
		pDC->MoveTo(Left,  Top-1); pDC->LineTo(Right, Top-1);
		pDC->MoveTo(Right+1, Top); pDC->LineTo(Right+1, Bot);
		pDC->MoveTo(Left,  Bot+1); pDC->LineTo(Right, Bot+1);
	}
	if(m_LineThickness > 2){
		pDC->MoveTo(Left-2,  Top); pDC->LineTo(Left-2,  Bot);
		pDC->MoveTo(Left,  Top-2); pDC->LineTo(Right, Top-2);
		pDC->MoveTo(Right+2, Top); pDC->LineTo(Right+2, Bot);
		pDC->MoveTo(Left,  Bot+2); pDC->LineTo(Right, Bot+2);
	}
}

void CDisplay::DrawRect(CDC *pDC, RECT Rt)
{
	DrawRect(pDC, Rt.top, Rt.bottom, Rt.left, Rt.right);
}

void CDisplay::DrawRectEx(CDC *pDC, int Top, int Bot, int Left, int Right, int Thickness, COLORREF C)
{
	int Style = (Thickness>1) ? PS_SOLID : PS_DOT;//PS_DASH
	if(Style != PS_SOLID){
		pDC->SetBkMode(OPAQUE);
		pDC->SetBkColor(RGB(0,0,0));
	}
	CPen *oldpen, pen(Style, Thickness, C);
	oldpen = pDC->SelectObject(&pen);
	pDC->SetROP2(R2_COPYPEN);
	pDC->MoveTo(Left , Top);
	pDC->LineTo(Left , Bot);
	pDC->LineTo(Right, Bot);
	pDC->LineTo(Right, Top);
	pDC->LineTo(Left , Top);
	pDC->SelectObject(oldpen);
}

void CDisplay::DestroyReusableObjects()
{
	m_GdiObjs.Font_16.DeleteObject();
	m_GdiObjs.Font_24.DeleteObject();
	m_GdiObjs.PenCrossHairs.DeleteObject();
	m_GdiObjs.PenAnchors.DeleteObject();
	m_GdiObjs.PenAnchorsThick.DeleteObject();
	m_GdiObjs.PenHidden.DeleteObject();
	m_GdiObjs.PenBox.DeleteObject();
	m_GdiObjs.PenSelectedBox.DeleteObject();
	m_GdiObjs.PenRed.DeleteObject();
	m_GdiObjs.PenOrange.DeleteObject();
}

void CDisplay::CreateReusableObjects()
{
	m_GdiObjs.Font_16.CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Ariel");
	m_GdiObjs.Font_24.CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Ariel");
	m_GdiObjs.PenCrossHairs.CreatePen(PS_DOT, 1, COLR::White);
	m_GdiObjs.PenAnchors.CreatePen(PS_SOLID, 3, COLR::Magenta);
	m_GdiObjs.PenAnchorsThick.CreatePen(PS_SOLID, 5, COLR::Magenta);
	m_GdiObjs.PenHidden.CreatePen(PS_SOLID, 4, COLR::Magenta);
	m_GdiObjs.PenBox.CreatePen(PS_DOT, 1, COLR::Green);
	m_GdiObjs.PenSelectedBox.CreatePen(PS_DOT, 1, COLR::Yellow);
	m_GdiObjs.PenRed.CreatePen(PS_DOT, 1, COLR::Red);
	m_GdiObjs.PenOrange.CreatePen(PS_DOT, 1, COLR::Orange);
}

char *CDisplay::GetLabelText(int class_id)
{
	if(class_id < 0)
		return " ** N E G A T I V E ** ";
	char *pLabel = g_CIdM.GetLabel(class_id);
	if(pLabel)
		return pLabel;
	// we do not have an accepted class-id/label so just return the number. 
	// This should *not* happen as class ids are checked as the csv is read in, so pop up a warning.
	static int warn = 0;
	if(warn == 0){
		MsgBox2("error - CDisplay - invalid class id (%d)", class_id);
		warn++;
	}
	static char str[12];
	_itoa_s(class_id, str, num_entries(str), 10);
	return str;
}

void CDisplay::DrawLabel(CDC *pDC, int InfoLevel, RECT Rt, SINGLETILE *pST, CLASSIFIEDLINE *pCL, SOURCELINE *pSL_Debug)
{
	pDC->SetBkMode(OPAQUE);
	CFont   *oldobj   = pDC->SelectObject((Rt.bottom-Rt.top>300) ? &m_GdiObjs.Font_24 : &m_GdiObjs.Font_16);
	COLORREF oldBkCol = pDC->SetBkColor(RGB(0,0,0));
	COLORREF oldTxCol = pDC->SetTextColor(COLR::Green);
	pSL_Debug = pSL_Debug; // stop warning
/*	char conf[6]={0};
	char debugcode[16]={0};
	if(pCL->DebugCode != 0)
		sprintf_s(debugcode, num_entries(debugcode), " [%d]", pCL->DebugCode);
	if(pSL_Debug && pSL_Debug->Confidence!=0 && pCL->ClassId>=0)
		// extra info from source line, this data will be 'dirty' after the annotations have been editted, its just for debugging.
		sprintf_s(conf, num_entries(conf), " %d%%", pSL_Debug->Confidence);*/

	if(pCL->ClassId < 0)
		sprintf_s(m_TempStr, num_entries(m_TempStr), "%s", GetLabelText(pCL->ClassId));
	else{
		int Wd, Ht;
		BboxToPixelWdHt_Inc_Exc(Wd, Ht, m_Pic.Wd, m_Pic.Ht, &pCL->box);
		if(pCL->Anno_UI.auiFlags&AUI_HIDDEN_BY_OVERLAP)
			sprintf_s(m_TempStr, num_entries(m_TempStr), "%d.%s ", pCL->Anno_UI.AllProbsIndex+1, GetLabelText(pCL->ClassId));
		else
			sprintf_s(m_TempStr, num_entries(m_TempStr), " %s ", GetLabelText(pCL->ClassId));

		//sprintf_s(m_TempStr, num_entries(m_TempStr), "%s %dx%d %s", m_TempStr, Wd, Ht, conf);
		if(InfoLevel == 2)
			sprintf_s(m_TempStr, num_entries(m_TempStr), "%s%dx%d ", m_TempStr, Wd, Ht);
		if(InfoLevel == 3)
			sprintf_s(m_TempStr, num_entries(m_TempStr), "%s[%.4f,%.4f] [%.4f,%.4f] ", m_TempStr, pCL->box.x1, pCL->box.x2, pCL->box.y1, pCL->box.y2);
	}

//	sprintf_s(m_TempStr, num_entries(m_TempStr), " %d-%s ", ClassId, GetLabelText(ClassId));
	CSize CS = pDC->GetTextExtent(m_TempStr);
	Rt.left++; Rt.top++;
	RECT RtExt = Rt;
	RtExt.left -= pST->DSP.Lf; RtExt.right  -= pST->DSP.Lf;
	RtExt.top  -= pST->DSP.Tp; RtExt.bottom -= pST->DSP.Tp;

	int LabelAtTop = 1;
	int cyf = CS.cy + m_LineThickness;
	int y = Rt.top; // y - inside the box at the top.
	if(RtExt.top > cyf) // y - on top of the box.
		y = Rt.top - cyf;
	else if(RtExt.bottom < m_DT.Tile_Ht-cyf){ // y - below the box.
		y = Rt.bottom + m_LineThickness + 1;
		LabelAtTop = 0;
	}

	// stack the labels when we have multiple probabilities (multiple identical rects for the same object).
	if((pCL->Anno_UI.auiFlags&AUI_HIDDEN_BY_OVERLAP) && pCL->Anno_UI.AllProbsIndex>0){
		if(LabelAtTop) y += CS.cy * pCL->Anno_UI.AllProbsIndex;
		else           y -= CS.cy * pCL->Anno_UI.AllProbsIndex;
	}
	int x = RtExt.left;
	if(x+CS.cx > m_DT.Tile_Wd)
		x = m_DT.Tile_Wd - CS.cx;
	x += pST->DSP.Lf;
	pDC->TextOut(x, y, m_TempStr);
	pDC->SelectObject(oldobj);
	pDC->SetBkColor(oldBkCol);
	pDC->SetTextColor(oldTxCol);
}

void CDisplay::TextOnDisplay(CDC *pDC, char *pText, COLORREF CR, int x, int y, int Size)
{
	pDC->SetBkMode(OPAQUE);//TRANSPARENT
	COLORREF oldBkCol = pDC->SetBkColor(RGB(0,0,0));
	CFont   *oldobj   = pDC->SelectObject((Size==0) ? &m_GdiObjs.Font_16 : &m_GdiObjs.Font_24);
	COLORREF oldTxCol = pDC->SetTextColor(CR);
	pDC->TextOut(x, y, pText);
	pDC->SetBkColor(oldBkCol);
	pDC->SelectObject(oldobj);
	pDC->SetTextColor(oldTxCol);
}

#define ALL_PROBS_OVERLAP_TOLERANCE	10	// possibly should be set in the GUI, and be less than 10.

void CDisplay::UpdateBboxScreenCoordinates(FILENAMEGROUP *pGrp, DISPLAYOPTIONS *pOptions)
{
	if(!m_MovingAnchor){
		// above, why not when moving anchor..
		// ..because Screen position is calculated in UpdateAnchorPosition from the screen x,y coordinates as user edits the position, and not from the bbox as done below.

		if(m_DT.NumTiles == 1){
			// all classified lines drawn to one tile, i.e, index zero.
			CLASSIFIEDLINE *pA = pGrp->pClsfdLines;
			for(int i=0; i<pGrp->NumClsfdLines; i++, pA++){
				if(pA->ClassId >= 0){
					pA->Anno_UI.Screen = GetBBoxScreenCoordinates(m_DT.Tiles[0].DSP.Lf, m_DT.Tiles[0].DSP.Tp, m_DT.Tile_Wd, m_DT.Tile_Ht, &pA->box);
					pA->Anno_UI.auiFlags = 0; // reset UI flags and set them below.
					pA->Anno_UI.AllProbsIndex = 0;
					int Wd, Ht;
					BboxToPixelWdHt_Inc_Exc(Wd, Ht, pGrp->pPicInfo->Width, pGrp->pPicInfo->Height, &pA->box);
					if(Wd<=pOptions->HighlightHeight || Ht<=pOptions->HighlightHeight)
						pA->Anno_UI.auiFlags |= AUI_MIN_SIZE_THRESHOLD;
					if(g_CIdM.Id_inCategory(pA->ClassId, CATEGORY_DETECTIONS) == 0)
						pA->Anno_UI.auiFlags |= AUI_SPECIAL_CATEGORY;
				}
			}
			// sometimes we have 2 or more boxes with virtually exactly the same position, and one hides the other, in this scenario we will 
			// draw a diagonal line through the box to alert user there are hidden box(es) - also know as 'all probabilities' for a single object.
			pA = pGrp->pClsfdLines;
			for(int a=0; a<pGrp->NumClsfdLines-1; a++, pA++){
				if((pA->Anno_UI.auiFlags&AUI_HIDDEN_BY_OVERLAP) == 0){
					int NumOverlap = 0;
					for(int b=a+1; b<pGrp->NumClsfdLines; b++){
						CLASSIFIEDLINE *p_B = &pGrp->pClsfdLines[b];
						if( abs(pA->Anno_UI.Screen.left  -p_B->Anno_UI.Screen.left)  < ALL_PROBS_OVERLAP_TOLERANCE && 
							abs(pA->Anno_UI.Screen.top   -p_B->Anno_UI.Screen.top)   < ALL_PROBS_OVERLAP_TOLERANCE &&
							abs(pA->Anno_UI.Screen.right -p_B->Anno_UI.Screen.right) < ALL_PROBS_OVERLAP_TOLERANCE &&
							abs(pA->Anno_UI.Screen.bottom-p_B->Anno_UI.Screen.bottom)< ALL_PROBS_OVERLAP_TOLERANCE )
						{
							NumOverlap++;
							p_B->Anno_UI.auiFlags |= AUI_HIDDEN_BY_OVERLAP;
							p_B->Anno_UI.AllProbsIndex = NumOverlap;
						}
					}
					if(NumOverlap > 0)
						pA->Anno_UI.auiFlags |= AUI_HIDDEN_BY_OVERLAP;
				}
			}
		}
		else if(pGrp->NumClsfdLines == m_DT.NumTiles){
			// one classified line drawn on each tile.
			for(int i=0; i<pGrp->NumClsfdLines; i++){
				if(pGrp->pClsfdLines[i].ClassId >= 0){
					pGrp->pClsfdLines[i].Anno_UI.Screen = GetBBoxScreenCoordinates(m_DT.Tiles[i].DSP.Lf, m_DT.Tiles[i].DSP.Tp, m_DT.Tile_Wd, m_DT.Tile_Ht, &pGrp->pClsfdLines[i].box);
					pGrp->pClsfdLines[i].Anno_UI.auiFlags = 0;
				}
			}
		}
		else MsgBox2("error - UpdateBboxScreenCoordinates");
	}
}

CPen *CDisplay::MySelectPen(CDC *pDC, CPen *pPen, int setBKMode)
{
	if(setBKMode){
		pDC->SetBkMode(OPAQUE);
		pDC->SetBkColor(RGB(0,0,0));
	}
	static CPen *oldpen;
	oldpen = pDC->SelectObject(pPen);
	pDC->SetROP2(R2_COPYPEN);
	return oldpen;
}

void CDisplay::DrawBboxAndLabel(CDC *pDC, int TileIndex, CLASSIFIEDLINE *pCL, DISPLAYOPTIONS *pOptions, int Selected, SOURCELINE *pSL_Debug)
{
	SINGLETILE *pST = &m_DT.Tiles[TileIndex];
	RECT Rt = pCL->Anno_UI.Screen;
	if(pCL->ClassId >= 0){ // draw bbox rect if this is not a 'negative'.

		CPen *nonselectedbox;
		if(pCL->Anno_UI.auiFlags&AUI_SPECIAL_CATEGORY)
			nonselectedbox = &m_GdiObjs.PenRed;
		else
			nonselectedbox = (pCL->Anno_UI.auiFlags&AUI_MIN_SIZE_THRESHOLD) ? &m_GdiObjs.PenOrange : &m_GdiObjs.PenBox;

		CPen *oldpen = MySelectPen(pDC, Selected ? &m_GdiObjs.PenSelectedBox : nonselectedbox, 1);
		DrawRect(pDC, Rt);
		pDC->SelectObject(oldpen);

		// draw diagonal line across box to alert user that this object has multiple probabilities, (but only if labels are not hidden).
		if(pOptions->ShowLabels && (pCL->Anno_UI.auiFlags&AUI_HIDDEN_BY_OVERLAP) && pCL->Anno_UI.AllProbsIndex>0){
			if(m_MovementAnchorIdx<0 && !Selected){	// also dont draw diagonal if we're editing or highlighting an individual box (i.e. selected).
				oldpen = MySelectPen(pDC, &m_GdiObjs.PenHidden, 0);
				int w=0, h=0, gap=10;
				if(pCL->Anno_UI.AllProbsIndex > 1){
					w = ((Rt.right - Rt.left) / gap) * (pCL->Anno_UI.AllProbsIndex-1);
					h = ((Rt.bottom - Rt.top) / gap) * (pCL->Anno_UI.AllProbsIndex-1);
				}
				if(pCL->Anno_UI.AllProbsIndex < gap){
					// (improve this?) hard coded max 'gap' probabilities - but in reality there should never be more than say 3.
					DrawLine(pDC, Rt.left+w, Rt.bottom, Rt.right, Rt.top+h);
					pDC->SelectObject(oldpen);
				}
			}
		}
	}
	else{
		// this to put the 'negative' label text into the center of the image.
		Rt.left   = (pST->DSP.Lf + pST->DSP.Rh) / 10;
		Rt.right  = pST->DSP.Rh;
		Rt.top    = (pST->DSP.Tp + pST->DSP.Bt) / 5;
		Rt.bottom = pST->DSP.Bt;
	}

	if(pOptions->ShowLabels)
		DrawLabel(pDC, pOptions->ShowLabels, Rt, pST, pCL, pSL_Debug);
}

void CDisplay::DrawAnchorPoint(CDC *pDC)
{
	if(m_MovementAnchorIdx >= 0){
		ANCHORPOINT *pA = &m_Anchors[m_MovementAnchorIdx];
		CPen *oldpen = MySelectPen(pDC, (m_AnchorsThickness==0) ? &m_GdiObjs.PenAnchors : &m_GdiObjs.PenAnchorsThick, 0);
		int Len = 12 + (m_AnchorsThickness*8);
		if(pA->L || pA->R)
			DrawLine(pDC, pA->x-Len, pA->y, pA->x+Len, pA->y);
		else if(pA->T || pA->B)
			DrawLine(pDC, pA->x, pA->y-Len, pA->x, pA->y+Len);
		else if(pA->TL || pA->BR)
			DrawLine(pDC, pA->x-Len, pA->y-Len, pA->x+Len, pA->y+Len);
		else if(pA->TR || pA->BL)
			DrawLine(pDC, pA->x-Len, pA->y+Len, pA->x+Len, pA->y-Len);
		else{
			DrawLine(pDC, pA->x-Len, pA->y, pA->x+Len, pA->y);
			DrawLine(pDC, pA->x, pA->y-Len, pA->x, pA->y+Len);
		}
		pDC->SelectObject(oldpen);
	}
}

void CDisplay::DrawCrossHairs(CDC *pDC)
{
	if(m_DT.NumTiles==1 && !m_HideCrossHairs){

		m_PrevCrossHairs = m_CrossHairs;

		if(m_CrossHairs.x>=0 && m_CrossHairs.y>=0){
			int Vertical=1, Horizontal=1;
			if(m_MovementAnchorIdx >= 0){
				ANCHORPOINT *pA = &m_Anchors[m_MovementAnchorIdx];
				if(pA->L || pA->R)
					Vertical = 0;
				else if(pA->T || pA->B)
					Horizontal = 0;
				else if(pA->TL || pA->TR || pA->BL || pA->BR){}
				else{
					Vertical = Horizontal = 0;
				}
			}
			SINGLETILE *pST = &m_DT.Tiles[0];
			CPen *oldpen = MySelectPen(pDC, &m_GdiObjs.PenCrossHairs, 1);
			if(Horizontal) DrawLine(pDC, m_CrossHairs.x, pST->DSP.Tp, m_CrossHairs.x, pST->DSP.Bt-1);
			if(Vertical  ) DrawLine(pDC, pST->DSP.Lf, m_CrossHairs.y, pST->DSP.Rh-1, m_CrossHairs.y);
			pDC->SelectObject(oldpen);
		}
	}
}

SOURCELINE *CDisplay::GetSourceLine(FILENAMEGROUP *pGrp, int Index)
{
	// sourceline pointer, this is for retrieving the DebugCode and Confidence from the source line (its not stored in the classified line).
	// warning - the source line index will only match up with the classified line index if the user has not added or deleted bboxes.
	return (pGrp->NumSourceLines == pGrp->NumClsfdLines) ? &pGrp->pSourceLines[Index] : nullptr;
}

void CDisplay::DrawAnnotationsOnTile(CDC *pDC, FILENAMEGROUP *pGrp, int Tiling, int TileIndex, DISPLAYOPTIONS *pOptions)
{
	if(Tiling){
		// one annotation per picture/tile.
		CLASSIFIEDLINE *pCL = &pGrp->pClsfdLines[TileIndex];
		DrawBboxAndLabel(pDC, TileIndex, pCL, pOptions, pCL->Anno_UI.Selected, GetSourceLine(pGrp, TileIndex));
	}
	else{
		// no tiling, all bboxes drawn on a single image.
		int SelectedIdx = -1, SelectionType = 0;
		CLASSIFIEDLINE *pCL = pGrp->pClsfdLines;
		for(int i=0; i<pGrp->NumClsfdLines; i++, pCL++){
			if(pCL->Anno_UI.Selected){
				SelectedIdx = i;
				SelectionType = pCL->Anno_UI.Selected;
			}
		}

		if(SelectedIdx == -1){
			// no selection draw all boxes as they come in the list.
			pCL = pGrp->pClsfdLines;
			for(int i=0; i<pGrp->NumClsfdLines; i++, pCL++)
				DrawBboxAndLabel(pDC, TileIndex, pCL, pOptions, 0, GetSourceLine(pGrp, i));
		}
		else{
			// we have a selection, draw selected box last so its on top of all the others.
			// ...not any more, changed it so when editing all boxes are hidden except the box we are editing, it focuses ones attention better.
		/*	int PT = m_LineThickness;
			m_LineThickness = 1;
			pCL = pGrp->pClsfdLines;
			for(int i=0; i<pGrp->NumClsfdLines; i++, pCL++)
				if(i != SelectedIdx)
					DrawBboxAndLabel(pDC, TileIndex, pCL, 0, 0, GetSourceLine(pGrp, i));
		
			m_LineThickness = PT;*/
			DrawBboxAndLabel(pDC, TileIndex, &pGrp->pClsfdLines[SelectedIdx], pOptions, 1, GetSourceLine(pGrp, SelectedIdx));
		}
	}
	DrawAnchorPoint(pDC);
}

int CDisplay::DrawUIDefinedBox(CDC *pDC)
{
	if(m_DT.NumTiles == 1){
		if(m_UIDefinedBox.right!=m_UIDefinedBox.left || m_UIDefinedBox.bottom!=m_UIDefinedBox.top){
			RECT uibx = m_UIDefinedBox;
			// user is defining a rect with the mouse, the updated cursor position always goes in right/bottom, so before painting reverse 
			// coordinates if box is currently backwards, this is necessary because line thickness is added to the outside edge.
			if(m_UIDefinedBox.right < m_UIDefinedBox.left){
				uibx.left  = m_UIDefinedBox.right;
				uibx.right = m_UIDefinedBox.left;
			}
			if(m_UIDefinedBox.bottom < m_UIDefinedBox.top){
				uibx.top    = m_UIDefinedBox.bottom;
				uibx.bottom = m_UIDefinedBox.top;
			}
			CPen *oldpen = MySelectPen(pDC, &m_GdiObjs.PenSelectedBox, 1);
			DrawRect(pDC, uibx);
			pDC->SelectObject(oldpen);
			return 1;
		}
	}
	return 0;
}

int CDisplay::LoadTileCoordinates(int DisplayWd, int DisplayHt, int GapWd, int GapHt)
{
	int Ok = 0;
	SINGLETILE *pTile;
	if(m_DT.NumTiles == 1){
		// center picture when we have only one tile.
		pTile = &m_DT.Tiles[0];
		pTile->DSP.Lf = (DisplayWd / 2) - (m_DT.Tile_Wd / 2);
		pTile->DSP.Tp = (DisplayHt / 2) - (m_DT.Tile_Ht / 2);
		pTile->DSP.Rh = pTile->DSP.Lf + m_DT.Tile_Wd;
		pTile->DSP.Bt = pTile->DSP.Tp + m_DT.Tile_Ht;
		pTile->InclusiveLimits = pTile->DSP;
		pTile->InclusiveLimits.Lf--;
		pTile->InclusiveLimits.Tp--;
		Ok = 1;
	}
	else{
		int i = 0;
		for(int Row=0; Row<m_DT.NumRows; Row++){
			for(int Column=0; Column<m_DT.NumColumns; Column++){
				if(i < m_DT.NumTiles){
					pTile = &m_DT.Tiles[i++];
					memset(pTile, 0, sizeof(SINGLETILE));
					pTile->DSP.Lf = (m_DT.Tile_Wd*Column) + (GapWd*Column) + GapWd;
					pTile->DSP.Tp = (m_DT.Tile_Ht*Row)    + (GapHt*Row)    + GapHt;
					pTile->DSP.Rh = pTile->DSP.Lf + m_DT.Tile_Wd;
					pTile->DSP.Bt = pTile->DSP.Tp + m_DT.Tile_Ht;
				}
			}
		}
		if(i == m_DT.NumTiles) Ok = 1;
	}
	if(!Ok) MsgBox2("error - display LoadTileCoordinates (%d)", m_DT.NumTiles);
	return Ok;
}

int CDisplay::FixedPictureDimensions(int Tiling, int DisplayWd, int DisplayHt, int Requested_Wd, int Requested_Ht)
{
	// Fixed picture dimensions - user specified width and height for the picture (useful to specify YOLO input size).
	if(Tiling){
		MsgBox2("error - tiling must be switched off to use fixed picture dimensions");
		return 0;
	}
	else{
		if(Requested_Wd>0 && Requested_Ht>0){
			m_DT.Tile_Wd    = Requested_Wd;
			m_DT.Tile_Ht    = Requested_Ht;
			m_DT.NumTiles   = 1;
			m_DT.NumRows    = 1;
			m_DT.NumColumns = 1;
			if(m_DT.Tile_Wd+(m_TileGap*2)<DisplayWd && m_DT.Tile_Ht+(m_TileGap*2)<DisplayHt){
				m_FixedSizeError = 0;
				return LoadTileCoordinates(DisplayWd, DisplayHt, m_TileGap, m_TileGap);
			}
		}
		m_DT.Reset();
		if(!m_FixedSizeError){
			MsgBox2("requested picture dimensions : w %d, h %d\r\n are invalid or do not fit in the display area : w %d, h %d", Requested_Wd, Requested_Ht, DisplayWd, DisplayHt);
			m_FixedSizeError = 1;	// can only show this show this message once (per picture) because it will block resizing of the window.
		}
	}
	return 0;
}

int CDisplay::DrawPictureAndAnnotations(CDC *pDC, int DisplayWd, int DisplayHt, int Tiling, FILENAMEGROUP *pGrp, DISPLAYOPTIONS *pOptions, int RefreshBackground)
{
	m_LineThickness = pOptions->IncreaseLineThickness ? 3 : 2;

	if(RefreshBackground){
		m_DT.Reset();
		static FILENAMEGROUP *pPrvGrp = nullptr;
		if(m_FixedSizeError && ((pGrp!=pPrvGrp) || (pOptions->PictureSizing==0)))
			m_FixedSizeError = 0;
		pPrvGrp = pGrp;
	}

	if(m_Pic.InitDC && m_DspBkg.InitDC && m_DspAll.InitDC && pGrp->NumSourceLines>0){
		if(DisplayWd>m_DspBkg.Wd || DisplayHt>m_DspBkg.Ht){
			MsgBox2("error - DrawPictureAndAnnotations, DisplayWd %d(%d) DisplayHt %d(%d)", DisplayWd, m_DspBkg.Wd, DisplayHt, m_DspBkg.Ht);
			return 0;
		}
		if(m_Pic.Wd!=pGrp->pPicInfo->Width || m_Pic.Ht!=pGrp->pPicInfo->Height){
			MsgBox2("error - DrawPictureAndAnnotations, m_Pic.Wd!=CsvWd || m_Pic.Ht!=CsvHt");
			return 0;
		}
		// Tiling (0)-single picture with all annotations. (1)-picture tiling with one annotation per tile.
		int NumTiles = Tiling ? pGrp->NumClsfdLines : 1;

		if(NumTiles>0 && NumTiles<MAXTILES){

			if(RefreshBackground){
				// setup display/tiling positioning
				// SetupDisplayTiling - can ignor return error, because an error would have popped up a msg box, and even if m_DT.NumTiles is zero everything below should still be executed.
				SetupDisplayTiling(Tiling, NumTiles, DisplayWd, DisplayHt, pOptions);
				COLORREF CR = RGB(30, 30, 55);
					 if(pGrp->Classified == CLASSIFIED_ACCEPT ) CR = RGB(0, 140, 0);
				else if(pGrp->Classified == CLASSIFIED_REFUSE ) CR = RGB(200, 0, 0);
				else if(pGrp->Classified == CLASSIFIED_SPECIAL) CR = RGB(0, 0, 200);
				CBrush Brh(CR);
				m_DspBkg.DC.SelectObject(&Brh);
				m_DspBkg.DC.Rectangle(0, 0, DisplayWd, DisplayHt);
				m_DspBkg.DC.SetStretchBltMode(HALFTONE);
				// drawing the background - i.e. picture to each tile
				for(int i=0; i<m_DT.NumTiles; i++)
					m_DspBkg.DC.StretchBlt(m_DT.Tiles[i].DSP.Lf, m_DT.Tiles[i].DSP.Tp, m_DT.Tile_Wd, m_DT.Tile_Ht, &m_Pic.DC, 0, 0, m_Pic.Wd, m_Pic.Ht, SRCCOPY);
				OutputDbgStr("\n[%d] display - refresh background ", m_PaintCount++);
			}
			//else OutputDbgStr("\n[%d] display - refresh FOREGROUND ", m_PaintCount++);

			m_DspAll.DC.BitBlt(0, 0, DisplayWd, DisplayHt, &m_DspBkg.DC, 0, 0, SRCCOPY);
			
			if(!DrawUIDefinedBox(&m_DspAll.DC)){
				// a 'ui box' in creation doesnt yet have an annotation (classifiedline), that is why 'ui box' has its own separate drawing function 'DrawUIDefinedBox' and skips all the  
				// drawing code below. Whereas for boxes that already exist (ie have a classifiedline), they can be drawn by the code below whether they are currently being editted or not.
				DrawCrossHairs(&m_DspAll.DC);
				UpdateBboxScreenCoordinates(pGrp, pOptions);
				if(pOptions->HideAnnotations)
					TextOnDisplay(&m_DspAll.DC, "annotations hidden", COLR::Yellow, 50, 50, 0);
				else{
					if(pGrp->Classified==CLASSIFIED_REFUSE && m_DT.NumTiles==1)
						TextOnDisplay(&m_DspAll.DC, "** R E F U S E **", COLR::Red, m_DT.Tiles[0].DSP.Lf+50, m_DT.Tiles[0].DSP.Tp+50, 1);
					for(int i=0; i<m_DT.NumTiles; i++)
						DrawAnnotationsOnTile(&m_DspAll.DC, pGrp, Tiling, i, pOptions);
					if(pOptions->ShowLabels == 0)
						TextOnDisplay(&m_DspAll.DC, "labels hidden", COLR::Yellow, 50, 50, 0);
				}
			}

			// All drawing is now done, lastly do a BitBlt to the screen dc.
			pDC->BitBlt(0, 0, DisplayWd, DisplayHt, &m_DspAll.DC, 0, 0, SRCCOPY);
		}
		else MsgBox2("error - DrawPictureAndAnnotations, NumTiles %d(max %d)\r\n too many tiles? ..turn off tiling", NumTiles, MAXTILES);
	}
	return m_FixedSizeError;
}

RECT CDisplay::GetBBoxScreenCoordinates(int Offset_X, int Offset_Y, int Picture_Wd, int Picture_Ht, BBOX *pBox)
{
	// read comments in - BitmapCoordinatesToBbox_Inc_Exc
	// convert to all exclusive positions, best for viewing and editing in the UI.
	RECT Rt;
	int L, R, T, B;
	BboxToBitmapCoordinates_Inc_Exc(L, R, T, B, Picture_Wd, Picture_Ht, pBox, 1);
	//OutputDbgStr("\n BBox to screen rect (inc/exc and no display offset), l %d r %d t %d b %d (w %d h %d), x1 %f x2 %f y1 %f y2 %f", L, R, T, B, Picture_Wd, Picture_Ht, pBox->x1, pBox->x2, pBox->y1, pBox->y2);
	Rt.left   = (L + Offset_X) - 1;	// left/top make exclusive.
	Rt.top    = (T + Offset_Y) - 1;
	Rt.right  = R + Offset_X;		// right/bottom already exclusive.
	Rt.bottom = B + Offset_Y;
	return Rt;
}

int CDisplay::ScreenToBBox(int Offset_X, int Offset_Y, int Picture_Wd, int Picture_Ht, RECT Rt_all_exclusive, BBOX *pBbox)
{
	// read comments in - BitmapCoordinatesToBbox_Inc_Exc
	// convert back to inclusive/exclusive values.
	int L = (Rt_all_exclusive.left - Offset_X) + 1;	// left and top exclusive positions have 1 added to convet back to inclusive.
	int T = (Rt_all_exclusive.top  - Offset_Y) + 1;
	int R = Rt_all_exclusive.right  - Offset_X;		// right and bottom are exclusive already so thats ok.
	int B = Rt_all_exclusive.bottom - Offset_Y;
	BitmapCoordinatesToBbox_Inc_Exc(L, R, T, B, Picture_Wd, Picture_Ht, pBbox);
	//OutputDbgStr("\n SCREEN RECT (inc/exc and no display offset) to BBox, l %d r %d t %d b %d (w %d h %d), x1 %f x2 %f y1 %f y2 %f", L, R, T, B, Picture_Wd, Picture_Ht, pBbox->x1, pBbox->x2, pBbox->y1, pBbox->y2);
	if(BboxError(pBbox) == 0)
		return 1;
	MsgBox2("CDisplay::ScreenToBBox - BboxError");
	return 0;
}

void CDisplay::UpdateAnchorPosition(FILENAMEGROUP *pGrp, int x, int y, int Tiling)
{
	if(m_MovementAnchorIdx>=0 && m_SelectedBBoxIdx>=0){
		ANCHORPOINT    *pA  = &m_Anchors[m_MovementAnchorIdx];
		CLASSIFIEDLINE *pCL = &pGrp->pClsfdLines[m_SelectedBBoxIdx];
		SINGLETILE     *pST = Tiling ? &m_DT.Tiles[m_SelectedBBoxIdx] : &m_DT.Tiles[0];
		RECT          *pScr = &pCL->Anno_UI.Screen;	// all exclusive.

		int incLf = pST->InclusiveLimits.Lf; // tile LIMITS (which are inclusive).
		int incRh = pST->InclusiveLimits.Rh;
		int incTp = pST->InclusiveLimits.Tp;
		int incBt = pST->InclusiveLimits.Bt;

		int MinSize = 10;
		if(pA->L || pA->R || pA->T || pA->B || pA->TL || pA->TR || pA->BL || pA->BR){
			// move a corner or a side.
			int ML = (pA->L | pA->TL | pA->BL);
			int MR = (pA->R | pA->TR | pA->BR);
			int MT = (pA->T | pA->TL | pA->TR);
			int MB = (pA->B | pA->BL | pA->BR);
			if(ML) pScr->left   = min(pScr->right -MinSize, max(x, incLf));
			if(MR) pScr->right  = max(pScr->left  +MinSize, min(x, incRh));
			if(MT) pScr->top    = min(pScr->bottom-MinSize, max(y, incTp));
			if(MB) pScr->bottom = max(pScr->top   +MinSize, min(y, incBt));
		}
		else{
			// move box as a whole, m_Anchors[zero] has the central position.
			int MoveX = x-pA->x, MoveY = y-pA->y;
			pScr->left   += MoveX;
			pScr->right  += MoveX;
			pScr->top    += MoveY;
			pScr->bottom += MoveY;
			pScr->left   = max(pScr->left,   incLf);
			pScr->left   = min(pScr->left,   incRh-MinSize);
			pScr->top    = max(pScr->top,    incTp);
			pScr->top    = min(pScr->top,    incBt-MinSize);
			pScr->right  = min(pScr->right,  incRh);
			pScr->right  = max(pScr->right,  incLf+MinSize);
			pScr->bottom = min(pScr->bottom, incBt);
			pScr->bottom = max(pScr->bottom, incTp+MinSize);
		}
		m_MovingAnchor = 1;
		SetAnchorPoints(pCL->Anno_UI.Screen);
		ScreenToBBox(pST->DSP.Lf, pST->DSP.Tp, m_DT.Tile_Wd, m_DT.Tile_Ht, pCL->Anno_UI.Screen, &pCL->box);
		SetCrossHairsPosition(x, y);
	}
	else MsgBox2("error - UpdateAnchorPosition");
}

int CDisplay::SetMovementAnchor(int x, int y) // return true if repaint needed.
{
	int PreviousAnchorIdx = m_MovementAnchorIdx;
	double MinDist = 10000000.0;
	ANCHORPOINT *pSB = m_Anchors;
	for(int i=0; i<num_entries(m_Anchors); i++, pSB++){
		double Dist = GetDistanceBetweenPoints((double)x, (double)y, (double)pSB->x, (double)pSB->y);
		if(Dist < MinDist){
			MinDist = Dist;
			m_MovementAnchorIdx = i;
		}
	}
	if(SetCrossHairsPosition(x, y)) // regardless of whether anchor has changed cross hairs are moving within the display area, so a repaint is needed.
		return 1;
	return (PreviousAnchorIdx != m_MovementAnchorIdx); // if anchor index has changed display should be repainted.
}

void CDisplay::ClearBoxSelections(FILENAMEGROUP *pGrp)
{
	m_SelectedBBoxIdx   = -1;
	m_MovementAnchorIdx = -1;
	m_MovingAnchor      = 0;
	if(pGrp)
		for(int i=0; i<pGrp->NumClsfdLines; i++)
			pGrp->pClsfdLines[i].Anno_UI.Selected = 0;
}

int CDisplay::GetSelectedBox(FILENAMEGROUP *pGrp, int SelectionType)
{
	if(pGrp){
		for(int i=0; i<pGrp->NumClsfdLines; i++){
			// last selected box might have been deleted, so always search for it rather than just returning m_SelectedBBoxIdx.
			if( (SelectionType==ANY_SELECTION_TYPE) ? pGrp->pClsfdLines[i].Anno_UI.Selected : (pGrp->pClsfdLines[i].Anno_UI.Selected==SelectionType) ){
				m_SelectedBBoxIdx = i;
				return i;
			}
		}
	}
	return -1;
}

int CDisplay::SetSelectedBox(FILENAMEGROUP *pGrp, int SelectionType, int Idx)
{
	int BoxSelected = 0;
	if(pGrp && Idx>=0 && Idx<pGrp->NumClsfdLines){
		m_MovementAnchorIdx = -1;
		m_SelectedBBoxIdx   = Idx;
		for(int i=0; i<pGrp->NumClsfdLines; i++){
			pGrp->pClsfdLines[i].Anno_UI.Selected = 0;
			if(pGrp->pClsfdLines[i].ClassId >= 0){	// cannot select a negative class id annotation. 
				if(i == Idx){
					pGrp->pClsfdLines[i].Anno_UI.Selected = SelectionType;	// mark user selection.
					BoxSelected = 1;
				}
			}
		}
		if(BoxSelected)
			// setup movement anchor points for the selected box.
			if(SelectionType == SELECTION_EDITING)
				SetAnchorPoints(pGrp->pClsfdLines[Idx].Anno_UI.Screen);
	}
	return BoxSelected;
}

void CDisplay::SetAnchorPoints(RECT Rt)
{
	memset(m_Anchors, 0, num_entries(m_Anchors)*sizeof(ANCHORPOINT));
	ANCHORPOINT *p = m_Anchors;
	p->x = (Rt.left+Rt.right) / 2;	// Box center.
	p->y = (Rt.top+Rt.bottom) / 2;
	p++;
	p->x =  Rt.left;
	p->y = (Rt.top+Rt.bottom) / 2;
	p->L = 1;
	p++;
	p->x =  Rt.right;
	p->y = (Rt.top+Rt.bottom) / 2;
	p->R = 1;
	p++;
	p->x = (Rt.left+Rt.right) / 2;
	p->y =  Rt.top;
	p->T = 1;
	p++;
	p->x = (Rt.left+Rt.right) / 2;
	p->y =  Rt.bottom;
	p->B = 1;
	p++;
	p->x  = Rt.left;
	p->y  = Rt.top;
	p->TL = 1;
	p++;
	p->x  = Rt.left;
	p->y  = Rt.bottom;
	p->BL = 1;
	p++;
	p->x  = Rt.right;
	p->y  = Rt.top;
	p->TR = 1;
	p++;
	p->x  = Rt.right;
	p->y  = Rt.bottom;
	p->BR = 1;
	p++;
	int Area = (Rt.right-Rt.left) * (Rt.bottom-Rt.top);
	m_AnchorsThickness = 0;
		 if(Area > (300*300)) m_AnchorsThickness = 2;
	else if(Area > (100*100)) m_AnchorsThickness = 1; // size to draw the anchor.
}

void CDisplay::HideCrossHairs()
{
	// hide cross hairs, 
	// cross hairs will automatically become unhidden as soon as theres mouse movement within the display area.
	m_HideCrossHairs = 1;
}

int CDisplay::SetCrossHairsPosition(int x, int y)
{
	// return true for repaint - repaint needed because cross hairs are within the display area - however if m_HideCrossHairs is true only return true after the mouse has been moved.
	// return false for no repaint - the cross hairs are off the display area, or hidden.
	if(m_DT.NumTiles == 1){
		if(EditingPointWithinLimits(x, y, &m_DT.Tiles[0])){
			// cross hairs within the display.
			m_CrossHairs.x = x;
			m_CrossHairs.y = y;
		}
		else{
			// cross hairs outside the display.
			m_CrossHairs.x = m_CrossHairs.y = OFF_DISPLAY_POSITION;
			return (m_CrossHairs.x!=m_PrevCrossHairs.x || m_CrossHairs.y!=m_PrevCrossHairs.y); // no repaint needed except for the first time mouse goes off the display area, ie a repaint to remove cross hairs.
		}
	}

	if(m_HideCrossHairs){
		if(m_CrossHairs.x!=m_PrevCrossHairs.x || m_CrossHairs.y!=m_PrevCrossHairs.y){
			m_HideCrossHairs = 0;
			return 1;
		}
		return 0;
	}
	return 1;
}

int CDisplay::StartUIDefinedBox(int x, int y)// calling this a UI box because it only exists in the UI until its completed, not in the annotation data.
{
	if(m_DT.NumTiles == 1){	// tiling not allowed when editing UI boxes.
		if(EditingPointWithinLimits(x, y, &m_DT.Tiles[0])){
			m_UIDefinedBox.left = m_UIDefinedBox.right = x;
			m_UIDefinedBox.top = m_UIDefinedBox.bottom = y;
			return 1;
		}
	}
	return 0;
}

void CDisplay::MoveUIDefinedBox(int x, int y)
{
	SINGLETILE *pST = &m_DT.Tiles[0];
	x = max(x, pST->InclusiveLimits.Lf);
	x = min(x, pST->InclusiveLimits.Rh);
	y = max(y, pST->InclusiveLimits.Tp);
	y = min(y, pST->InclusiveLimits.Bt);
	m_UIDefinedBox.right  = x;
	m_UIDefinedBox.bottom = y;
}

int CDisplay::EditingPointWithinLimits(int x, int y, SINGLETILE *pST)
{
	return ( x>=pST->InclusiveLimits.Lf && x<=pST->InclusiveLimits.Rh && 
			 y>=pST->InclusiveLimits.Tp && y<=pST->InclusiveLimits.Bt );
}

int CDisplay::GetUIDefinedBox(BBOX &Bbox, int MinPixelSize)
{
	if(m_DT.NumTiles == 1){
		// user has finished defining box position.
		RECT uibx = m_UIDefinedBox;
		// reverse coordinates if box is backwards.
		if(m_UIDefinedBox.right < m_UIDefinedBox.left){
			uibx.left  = m_UIDefinedBox.right;
			uibx.right = m_UIDefinedBox.left;
		}
		if(m_UIDefinedBox.bottom < m_UIDefinedBox.top){
			uibx.top    = m_UIDefinedBox.bottom;
			uibx.bottom = m_UIDefinedBox.top;
		}
		// now we've finished we must zero m_UIDefinedBox so this box isnt drawn in DrawUIDefinedBox anymore.
		memset(&m_UIDefinedBox, 0, sizeof(m_UIDefinedBox));

		if((uibx.right-uibx.left)>MinPixelSize && (uibx.bottom-uibx.top)>MinPixelSize){
			// dont allow tiny boxes, remember this is screen pixels, depending on picture-to-screen scaling the real bbox size will be different.
			SINGLETILE *pST = &m_DT.Tiles[0];
			ScreenToBBox(pST->DSP.Lf, pST->DSP.Tp, m_DT.Tile_Wd, m_DT.Tile_Ht, uibx, &Bbox);
			return 1;
		}
	}
	return 0;
}

int CDisplay::GetBboxIdxFromScreenCoords(FILENAMEGROUP *pGrp, int x, int y)
{
	int MinArea = 100000;
	int MinAreaIdx = -1;
	for(int i=0; i<pGrp->NumClsfdLines; i++){
		RECT Scr = pGrp->pClsfdLines[i].Anno_UI.Screen;
		if(x>Scr.left && x<Scr.right && y>Scr.top && y<Scr.bottom){
			int A = (Scr.right - Scr.left) + (Scr.bottom - Scr.top);
			if(A < MinArea){
				// we return the index of the smallest box when boxes overlap, otherwise we might never be able to select a small box within a big box.
				// however, when several boxes overlap it could still be impossible to select a particular box, so there must be other box selection methods.
				MinArea = A;
				MinAreaIdx = i;
			}
		}
	}
	return MinAreaIdx;	// returns -1 if (x, y) is not in any bounding box.
}

int CDisplay::GetTileIdxFromScreenCoords(int x, int y)
{
	// return which tile is clicked on.
	SINGLETILE *pTile = m_DT.Tiles;
	for(int i=0; i<m_DT.NumTiles; i++, pTile++)
		if(x>=pTile->DSP.Lf && x<pTile->DSP.Rh && y>=pTile->DSP.Tp && y<pTile->DSP.Bt)
			return i;
	return -1;
}

void CDisplay::ClearDisplay(CDC *pDC, int DisplayWd, int DisplayHt)
{
	CGdiObject *pOld = pDC->SelectStockObject(BLACK_BRUSH);
	pDC->Rectangle(0, 0, DisplayWd, DisplayHt);
	pDC->SelectObject(pOld);
}

int CDisplay::SetupDisplayTiling(BOOL Tiling, int NumTiles, int DisplayWd, int DisplayHt, DISPLAYOPTIONS *pOptions)
{
	if(pOptions->PictureSizing != 0){
		// only one 'tile' allowed, with fixed size.
		
		// see CClassifyDlg::IteratePictureSizingScheme
		int PicWd=0, PicHt=0;
		if(pOptions->PictureSizing == 1){
			PicWd = m_Pic.Wd;
			PicHt = m_Pic.Ht;
		}
		else if(pOptions->PictureSizing == 2){
			if(pOptions->FixedDims.Enable){
				PicWd = pOptions->FixedDims.Wd;
				PicHt = pOptions->FixedDims.Ht;
			}
		}
		return FixedPictureDimensions(Tiling, DisplayWd, DisplayHt, PicWd, PicHt);
	}
	else{
		// one or more tiles, calculated to fit the display area.

		int GapWd = m_TileGap;	// Size of gap between tiles and border around the outside of all tiles together.
		int GapHt = m_TileGap;	// ..one value will do? ..well might want to put text under each tile, in which case we'd have a greater Ht than Wd.

		// (GapWd*2) below is the gap(border) around the outside of all the tiles together, i.e. not inbetween tiles.
		// ...this gap doesnt have to be the same as the gap between tiles, but it is the same at the moment.
		CalculateTileSize(NumTiles, (float)(DisplayWd-(GapWd*2)), (float)GapWd, (float)(DisplayHt-(GapHt*2)), (float)GapHt, (float)m_Pic.Wd, (float)m_Pic.Ht);

		// now we have tile size, rows and columns, calculate the coordinates for each tile.
		if(m_DT.NumTiles>0 && m_DT.NumColumns>0 && m_DT.NumRows>0)
			return LoadTileCoordinates(DisplayWd, DisplayHt, GapWd, GapHt);
	}
	return 0;
}

void CDisplay::CalculateTileSize(int NumTiles, float DisplayWd, float GapWd, float DisplayHt, float GapHt, float BmpWd, float BmpHt)
{
	// Function will calculate tile width/height, and number of rows and columns.
	// Gap is taken into account between tiles, but not around the outside edge of all tiles.
	m_DT.NumTiles = 0;
	if(BmpWd>0.0f && DisplayWd>0.0f && DisplayHt>0.0f){
		float bmpRatio = BmpHt / BmpWd;
		if(NumTiles == 1){
			float displayRatio = DisplayHt / DisplayWd;
			m_DT.NumColumns = 1;
			m_DT.NumRows    = 1;
			if(bmpRatio <= displayRatio){
				m_DT.Tile_Wd = (int)DisplayWd;
				m_DT.Tile_Ht = (int)(DisplayWd * bmpRatio);
			}
			else{
				m_DT.Tile_Ht = (int)DisplayHt;
				m_DT.Tile_Wd = (int)(DisplayHt / bmpRatio);
			}
			m_DT.NumTiles = NumTiles;
		}
		else if(NumTiles > 1){
			float MinWastedSpace = DisplayHt * DisplayWd;
			for(int Columns=1; Columns<=NumTiles; Columns++){
				// Works on basis of tile width filling display width, and then adjusting tile height accordingly, so there will 
				// likely be a gap below the tiles as the tile height doesnt divide perfectly into display height.
				int Rows      = NumTiles / Columns;
				int Remainder = (NumTiles % Columns);
				if(Remainder > 0)
					Rows++;
				float TileWd  = (DisplayWd - (GapWd*(Columns-1))) / (float)Columns;
				float TileHt  = TileWd * bmpRatio;
				float TotalHt = TileHt * (float)Rows;
				if(TotalHt <= (DisplayHt - (GapHt*(Rows-1)))){
					float TotalWastedSpace = (DisplayHt - TotalHt) * DisplayWd;				// Wasted space below the tiles.
					if(Remainder > 0)
						TotalWastedSpace += TileWd * TileHt * (float)(Columns - Remainder);	// Wasted space - Blank tiles in last row.
					if(TotalWastedSpace < MinWastedSpace){
						m_DT.NumColumns = Columns;
						m_DT.NumRows    = Rows;
						m_DT.Tile_Wd    = (int)TileWd;
						m_DT.Tile_Ht    = (int)TileHt;
						m_DT.NumTiles   = NumTiles;
						MinWastedSpace  = TotalWastedSpace;
					}
				}
			}
			for(int Rows=1; Rows<=NumTiles; Rows++){										// Opposite of the above loop.
				int Columns   = NumTiles / Rows;
				int Remainder = (NumTiles % Rows);
				if(Remainder > 0)
					Columns++;
				float TileHt  = (DisplayHt - (GapHt*(Rows-1))) / (float)Rows;
				float TileWd  = TileHt / bmpRatio;
				float TotalWd = TileWd * (float)Columns;
				if(TotalWd <= (DisplayWd - (GapWd*(Columns-1)))){
					float TotalWastedSpace = (DisplayWd - TotalWd) * DisplayHt;				// Wasted space below the tiles.
					if(Remainder > 0)						
						TotalWastedSpace += TileWd * TileHt * (float)(Rows - Remainder);	// Wasted space - Blank tiles in last column.
					if(TotalWastedSpace < MinWastedSpace){
						m_DT.NumColumns = Columns;
						m_DT.NumRows    = Rows;
						m_DT.Tile_Wd    = (int)TileWd;
						m_DT.Tile_Ht    = (int)TileHt;
						m_DT.NumTiles   = NumTiles;
						MinWastedSpace  = TotalWastedSpace;
					}
				}
			}
		}
	}
}

void CDisplay::DeletePictureObjects()
{
	// this is for public use, (used to stop the display working when classification is finished).
	FreeBitmapObjects(&m_Pic);
}

void CDisplay::FreeBitmapObjects(MEMORYDC *pMDC)
{
	if(pMDC->InitDC)
		pMDC->DC.DeleteDC();
	pMDC->InitDC = 0;
	if(pMDC->InitBitmap)
		DeleteObject(pMDC->hBitmap);
	pMDC->InitBitmap = 0;
}

int CDisplay::InitDisplayDC(CDC *pDC, MEMORYDC *pMemDC)
{
	if(pMemDC->InitDC==0 || pMemDC->InitBitmap==0){
		FreeBitmapObjects(pMemDC);

		// SM_CMONITORS - number of.
		// SM_CYFULLSCREEN - The height of the client area for a full-screen window on the primary display monitor, in pixels.
		// SM_CYVIRTUALSCREEN - The height of the virtual screen, in pixels. The virtual screen is the bounding rectangle of all display monitors.

		// Window can be resized, so this memory dc must be created for the max screen size. (actually could be slightly smaller cos the display size is not quite the full screen).
		int MaxDisplayWd = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		int MaxDisplayHt = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		pMemDC->hBitmap = CreateCompatibleBitmap(pDC->m_hDC, MaxDisplayWd, MaxDisplayHt);	// not m_Pic.DC
		if(pMemDC->hBitmap){
			pMemDC->InitBitmap = 1;
			GetObject(pMemDC->hBitmap, sizeof(pMemDC->DibS), &pMemDC->DibS);
			pMemDC->Wd = pMemDC->DibS.dsBm.bmWidth;
			pMemDC->Ht = pMemDC->DibS.dsBm.bmHeight;
			if(pMemDC->DC.CreateCompatibleDC(pDC)){
				pMemDC->DC.SelectObject(pMemDC->hBitmap);
				pMemDC->InitDC = 1;
			}
		}
	}
	if(pMemDC->InitDC && pMemDC->InitBitmap)
		return 1;
	MsgBox2("InitDisplayDC - FAILED, Btm %d dc %d", pMemDC->InitBitmap, pMemDC->InitDC);
	return 0;
}

int CDisplay::LoadPicture(char *pPath, CDC *pDC)
{
	int Ok = 0;
	FreeBitmapObjects(&m_Pic);
	if(pDC){
		if(m_Pic.InitBitmap==0 && m_Pic.InitDC==0){
			HBITMAP hBitmap = g_CBmpFile.GetBitmapHandle(pPath);
			if(hBitmap != 0){
				m_Pic.hBitmap = hBitmap;
				m_Pic.InitBitmap = 1;
				GetObject(m_Pic.hBitmap, sizeof(m_Pic.DibS), &m_Pic.DibS);
				m_Pic.Wd = m_Pic.DibS.dsBm.bmWidth;
				m_Pic.Ht = m_Pic.DibS.dsBm.bmHeight;
				m_Pic.DC.CreateCompatibleDC(pDC);
				m_Pic.DC.SelectObject(m_Pic.hBitmap);
				m_Pic.InitDC = 1;
				Ok = 1;
			}
		}
		Ok &= InitDisplayDC(pDC, &m_DspBkg);
		Ok &= InitDisplayDC(pDC, &m_DspAll);
	}
	return Ok;
}

void CDisplay::GetPictureDims(int &Wd, int &Ht, float &Scale, int &display_Wd, int &display_Ht)
{
	Wd = m_Pic.Wd;
	Ht = m_Pic.Ht;
	if(Wd > 0.0f)
		Scale = m_DT.Tile_Wd / (float)Wd;
	display_Wd = m_DT.Tile_Wd;
	display_Ht = m_DT.Tile_Ht;
}
