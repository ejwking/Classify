
#pragma once

//#include "afxdialogex.h"


// CMyMsgBox dialog

class CMyMsgBox : public CDialogEx
{
	DECLARE_DYNAMIC(CMyMsgBox)

public:
	char *m_pText;
	int m_NewlineCount, m_FontPoint, m_ShowingInstructions;

	CMyMsgBox(CWnd* pParent = nullptr);

	void SetText(char *pText);
	void SetInstructionsText(int Open);

	virtual ~CMyMsgBox();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_MSG_BOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
};
