
#pragma once

#include "afxdialogex.h"

struct GOTOPICTURE
{
	int RadioIndex;
	int PicIndex;
	char Filename[MAX_PATH];
};

// CGotoPictureDlg dialog

class CGotoPictureDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CGotoPictureDlg)

public:

	GOTOPICTURE m_GotoPic;

	CGotoPictureDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CGotoPictureDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FIND_PICTURE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
