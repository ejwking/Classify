// MyMsgBox.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "MyMsgBox.h"
#include "Useful.h"


// CMyMsgBox dialog

IMPLEMENT_DYNAMIC(CMyMsgBox, CDialogEx)

CMyMsgBox::CMyMsgBox(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_MSG_BOX, pParent)
{
	m_pText = nullptr;
	m_FontPoint = 0;
	m_NewlineCount = 0;
	m_ShowingInstructions = 0;
}

CMyMsgBox::~CMyMsgBox()
{

}

void CMyMsgBox::SetText(char *pText)
{
	m_pText = pText;
}

void CMyMsgBox::SetInstructionsText(int Open)
{
	if(Open){
		m_ShowingInstructions = 1;
		m_pText = 
   "\
GENERAL\
\r\n key : [C] - control bar - show/hide\
\r\n key : [T] - picture tiling - on/off (with the exception of deleting boxes, editing is disabled when picture tiling is on)\
\r\n key : [O] - cycle picture sizing - fit display area, actual size, user specified 'fixed picture dimensions'\
\r\n key : [P] - open current picture in default bitmap viewer\
\r\n key : [I] - open Ollama local LLM window, if no box is selected query LLM on entire picture, otherwise on the selected box\
\r\n\
\r\nPICTURE NAVIGATION\
\r\n key : [left][right] - step through picture list\
\r\n key : [page up][page down] - big step through picture list\
\r\n key : [home] - first picture\
\r\n key : [end] - last picture\
\r\n\
\r\nPICTURE CLASSIFICATION\
\r\n key : [enter] - save changes, mark picture as 'classified accept' (green background), move to the next picture\
\r\n key : [#] - discard changes, mark picture as 'classified refuse' (red background), move to the next picture\
\r\n key : [/] - save changes, mark picture as 'classified special' (blue background), move to the next picture (*)\
\r\n\
\r\nANNOTATION EDITING AND NAVIGATION\
\r\n key : [up][down] - cycle bounding boxes and highlight\
\r\n key : [control] - un-highlight or un-select bounding box, if there are no highlighted/selected boxes show/hide annotations\
\r\n key : [L] - cycle level of label info displayed\
\r\n\
\r\n key : [S] - enable editing of currently highlighted box\
\r\n key : [D] - delete currently highlighted box\
\r\n key : [V] - duplicate currently highlighted/selected box (for the purpose of giving an object multiple probabilities)\
\r\n key : [1-9] - set class id for last or currently selected/highlighted bounding box (also see, mouse : wheel)\
\r\n key : [K] - undo changes (revert to source csv file values for the current picture)\
\r\n\
\r\n mouse : middle button - delete bounding box\
\r\n mouse : left button - select bounding box\
\r\n mouse : hover - select movement anchor\
\r\n mouse : right button (and hold) - position/drag movement anchor, or, create new bounding box\
\r\n mouse : wheel - set class id for last or currently selected/highlighted bounding box (also see, key : [1-9])\
\r\n\
\r\n SHIFT and mouse : popular class assignment\
\r\n__________________________________________________________________________________\
\r\nTo save changes to the annotations for the current picture you must press the 'classified accept' or\
\r\n'classified special' key.\
\r\n(*) 'classified special' is a 3rd classification category, it can be useful for pictures you want to keep separate from\
\r\naccept and refuse pictures.\
\r\n__________________________________________________________________________________\
\r\nPURPLE LINE drawn diagonally across bounding box, ..this is to alert you that two or more boxes have identical positions.\
\r\nTILING - tiling is useful when there are many overlapping boxes, and you want to see them all at once and individually.\
\r\n__________________________________________________________________________________\
\r\nAll the 4 lines of a bounding box on screen use 'exclusive' positions. This means you should position the\
\r\nbox to touch the edge of the object on all 4 sides. The thickness of the line is drawn outwards from the\
\r\nbox, so theres no reason not to use thick lines for best visibility.\
\r\n\
\r\n\
";
	}
	else{
		m_pText = 
   "starting instructions\
\r\n\
\r\npress 'LOAD' button, dialog will pop up and input paths/files must be selected\
\r\ncome back here for further instructions once csv(s) are loaded\
\r\n\
";
	}
}

void CMyMsgBox::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMyMsgBox, CDialogEx)
	ON_WM_SHOWWINDOW()
	ON_WM_WINDOWPOSCHANGED()
END_MESSAGE_MAP()

// CMyMsgBox message handlers

BOOL CMyMsgBox::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// TODO:  Add extra initialization here
	if(m_pText){
		// change the text size.
		// https://learn.microsoft.com/en-us/answers/questions/1307887/how-to-set-font-for-single-control-in-dialog

		CWnd *pcw = GetDlgItem(IDC_EDIT_TEXT_DSP);
		m_FontPoint = LoadEditboxFont(pcw);
		pcw->SetWindowText(m_pText);

	//	CSize CS = pEditbox->GetDC()->GetTextExtent(m_pText, strlen(m_pText)); 
	//  doesnt consider /newline, so it pretty useless, didnt have any more luck with DrawText(DT_CALCRECT)
	//	i know the height of a single line (font point), and i can count newlines.

		int Len = (int)strlen(m_pText);
		for(int i=0; i<Len; i++)
			if(m_pText[i] == '\n')
				m_NewlineCount++;

		m_NewlineCount -= 11;
		// edit box is already the height of 10 lines, so deduct this cos we want to know how many lines to make box bigger by.
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CMyMsgBox::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);
	// TODO: Add your message handler code here
	if(m_NewlineCount>0 && m_FontPoint>0){
		int Extra = m_FontPoint * m_NewlineCount;
		CRect WndRt;
		GetWindowRect(&WndRt);

		int H = (WndRt.bottom - WndRt.top) + Extra;
		int W = (WndRt.right - WndRt.left);
		WndRt.top -= Extra/2;
		
		// get screen size, and limit size accordingly
		int ScreenCY = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		WndRt.top = max(0, WndRt.top);
		H = min(H, ScreenCY);

		if(m_ShowingInstructions) W = max(W, 900);
		SetWindowPos(NULL, WndRt.left, WndRt.top, W, H, SWP_SHOWWINDOW);
	}
}

void CMyMsgBox::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
	CDialogEx::OnWindowPosChanged(lpwndpos);
	// TODO: Add your message handler code here
}
