
#pragma once

#include "afxdialogex.h"

// CInputConfigDlg dialog


#define MAX_INPATHLIST	20
struct INPUT_CONFIG
{
	int RadioCsvFile;
	
	char PictureFolder[MAX_PATH];
	char CsvFile[MAX_INPATHLIST][MAX_PATH];
	char CsvFolder[MAX_INPATHLIST][MAX_PATH];
	char BlanksPictureFolder[MAX_PATH];

	int NumCsvFile, IdxCsvFile, NumCsvFolder, IdxCsvFolder;

	BOOL MergeFiles, SkipErrorChecks;
	BOOL FolderExcludeClassified, FolderExcludeModel;
	BOOL AnnotationProcessing, DebugAnnotationProcessing;
	BOOL DoColourTestForBlanks;
};

// CInputConfigDlg
class CInputConfigDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CInputConfigDlg)

private:
	INPUT_CONFIG m_IC;
	void AddPathToList(CComboBox *pCB, char *pNewPath, char pPath[][MAX_PATH], int SingleEntrySize, int &Num, int &Index);
	void InitComboBox(CComboBox *pCB, char pPath[][MAX_PATH], int &Num, int &Index);
	void EnableControlsAccrordingly();

public:
	CInputConfigDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CInputConfigDlg();
	void SetInputPathsData(INPUT_CONFIG INP);
	INPUT_CONFIG GetInputPathsData();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_INPUT_PATHS };
#endif

protected:

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_ComboBoxFile, m_ComboBoxFolder;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnCbnSelchangeComboCsvFile();
	afx_msg void OnCbnSelchangeComboCsvFolder();
	afx_msg void OnBnClickedBtnPicFolder();
	afx_msg void OnBnClickedButtonCsvFile();
	afx_msg void OnBnClickedButtonCsvFolder();
	afx_msg void OnBnClickedRadioCsvFile();
	afx_msg void OnBnClickedRadioCsvFolder();
	afx_msg void OnBnClickedButtonCsvFileRem();
	afx_msg void OnBnClickedButtonCsvFolderRem();
	afx_msg void OnBnClickedBtnBlankPicFolder();
	afx_msg void OnBnClickedRadioCsvBlank();
};
