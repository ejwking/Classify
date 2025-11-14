
#pragma once

#include "afxdialogex.h"


// CAIAssistDlg dialog

class CAIAssistDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CAIAssistDlg)

public:
	CAIAssistDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CAIAssistDlg();

	void SetParams(char *pBitmapPath);

//	CEdit m_EditQuery;
	CString m_StrQuery, m_StrResponse;
    char *m_pBitmapPath;
	CComboBox m_ComboModel;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_AI_ASSIST };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedButtonQuery();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonAiInfo();
};
