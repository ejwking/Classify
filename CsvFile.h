
#pragma once


#define LINEBUFMAX 1200

class CCsvFile
{
public:
	CCsvFile();
	~CCsvFile();

	int  Open(char *pPath, int Display_FileNum);
	int  GetCommaSeparatedFields(char *pBuff, char **ppFields, int MaxFields, int StringInQuotationMarks=0);
	int  GetNextLine(char **ppFields, int MaxFields, int *pNumFields, int *pLineNumber);
	void Close();
	int  BufToIntegerList(char *pBuf, int *pList, int Count);
private:
	char  m_MsgStr[128];
	FILE *m_pFile;
	CWnd *m_pCWnd;
	int   m_Filesize, m_Ok, m_LineCount, m_FileNum;
	char  m_LineReadBuf[LINEBUFMAX];
};

