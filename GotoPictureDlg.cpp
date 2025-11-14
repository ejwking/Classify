// FindPictureDlg.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "GotoPictureDlg.h"
#include "Useful.h"


// CGotoPictureDlg dialog

IMPLEMENT_DYNAMIC(CGotoPictureDlg, CDialogEx)

CGotoPictureDlg::CGotoPictureDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_GOTO_PICTURE, pParent)
{
	memset(&m_GotoPic, 0, sizeof(m_GotoPic));
}

CGotoPictureDlg::~CGotoPictureDlg()
{
}

void CGotoPictureDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Radio(pDX, IDC_RADIO_GOTO_INDEX, m_GotoPic.RadioIndex);	// first radio in group (remember it must be marked with - WS_GROUP)
	//DDX_Radio(pDX, IDC_RADIO_GOTO_FILENAME, m_GotoPic.RadioIndex); 
	//DDX_Radio(pDX, IDC_RADIO_GOTO_NON_CLASSIFIED, m_GotoPic.RadioIndex); 

	DDX_Text(pDX, IDC_EDIT_GOTO_INDEX, m_GotoPic.PicIndex);
	DDX_Text(pDX, IDC_EDIT_GOTO_FILENAME, m_GotoPic.Filename, num_entries(m_GotoPic.Filename));
}


BEGIN_MESSAGE_MAP(CGotoPictureDlg, CDialogEx)
END_MESSAGE_MAP()


// CGotoPictureDlg message handlers
