
#include "pch.h"
#include "CsvFile.h"
#include "Useful.h"

CCsvFile::CCsvFile()
{
	m_pFile = nullptr;
	m_Filesize = 0;
	m_Ok = 0;
}

CCsvFile::~CCsvFile()
{
	Close();
}

int CCsvFile::Open(char *pPath, int Display_FileNum)
{
	m_Ok = 0;
	m_FileNum = Display_FileNum;
	m_Filesize = 0;
	m_LineCount = 0;
	m_pCWnd = ::AfxGetApp()->m_pMainWnd;
	fopen_s(&m_pFile, pPath, "r");
	if(m_pFile){
		m_Filesize = myGetFileSize(m_pFile) - 1;
		//OutputDbgStr("\n\n CCsvFile::Open - %s\n m_Filesize %d\n\n", pPath, m_Filesize);
		m_Ok = 1;
	}
	else MsgBox2("error - CCsvFile::Open failed\r\npath - %s", pPath);
	return m_Filesize;
}

void CCsvFile::Close()
{
	if(m_pFile)
		fclose(m_pFile);
	m_pFile = nullptr;
}

int CCsvFile::BufToIntegerList(char *pBuf, int *pList, int Count)
{
	// returns -1 if theres an error, otherwise returns the number of integers in the csv buffer.
	int i, Ok = 1, Len = (int)strlen(pBuf);
	for(i=0; i<Len; i++){
		if( (pBuf[i]==',' || (pBuf[i]>='0' && pBuf[i]<='9')) == 0 )
			Ok = 0;
		if(i > 0)
			if(pBuf[i-1]==',' && pBuf[i]==',')
				Ok = 0;
	}
	if(Ok){
		char *ppFields[64];
		int   NumFields = GetCommaSeparatedFields(pBuf, ppFields, num_entries(ppFields));
		if(NumFields > 0){
			for(i=0; i<NumFields; i++){
				if(i < Count)
					pList[i] = atoi(ppFields[i]);
				else
					MsgBox2("error - BufToIntegerList\r\n integer list overflow (%d)", Count);
			}
			return i;
		}
		return 0;
	}
	MsgBox2("invalid csv\r\n\r\n character string can only contain numbers and commas, and no empty fields (eg ,,) \r\n\r\n %s", pBuf);
	return -1;
}

int CCsvFile::GetCommaSeparatedFields(char *pBuff, char **ppFields, int MaxFields, int StringInQuotationMarks/*=0*/)
{
	int QuotationOpen=0;
	int FieldCount=0, s=0, NewField=1;
	while(pBuff[s] != 0){
		if(NewField){
			if(FieldCount < MaxFields)
				ppFields[FieldCount++] = &pBuff[s];
			else{
				MsgBox2("error - GetCommaSeparatedFields - MaxFields %d", MaxFields);
				m_Ok = 0;
				return 0;
			}
			NewField = 0;
		}

		if(StringInQuotationMarks)	// string can contain commas if its put within quotation marks. if we're not using this code string doesnt need quotation marks but cannot contain commas.
			if(pBuff[s] == '"')
				QuotationOpen = !QuotationOpen;

		if(!QuotationOpen){
			if(pBuff[s] == ','){
				pBuff[s] = 0;	// null terminate each field by replacing the ',' with null.
				NewField = 1;
			}
		}
		s++;
	}
//	for(int s=0; s<FieldCount; s++) OutputDbgStr("\n %d(%d) : %s   ", s, FieldCount, ppFields[s]); MsgBox("pFields");
	return FieldCount;
}

int CCsvFile::GetNextLine(char **ppFields, int MaxFields, int *pNumFields, int *pLineNumber)
{
	int i = 0;
	*pNumFields  = 0;
	*pLineNumber = m_LineCount;
	int iVal = fgetc(m_pFile);
//	while(feof(m_pFile)==0 && m_Ok){
	while(iVal!=EOF && m_Ok){
		char Chr = (char)iVal;
		if(i < LINEBUFMAX){
			m_LineReadBuf[i] = Chr;
			if(Chr == '\n'){
				// Text for line is now contained in m_LineReadBuf.
				m_LineCount++;
				break;
			}
			else i++;
		}
		else{
			MsgBox2("error - CCsvFile::GetNextLine LINEBUFMAX");
			m_Ok = 0;
		}
		iVal = fgetc(m_pFile);
		m_Filesize--;
	}

	if((m_LineCount % 1000)==0 || m_Filesize<=0){
		//OutputDbgStr("\n CCsvFile::GetNextLine - m_Filesize Countdown %8d, m_LineCount %6d", m_Filesize, m_LineCount);
		int fs = m_Filesize / 1000;
		sprintf_s(m_MsgStr, num_entries(m_MsgStr), "loading csv file (%d) - line count %d, file size countdown %dKB", m_FileNum, m_LineCount, (fs>500)?fs:0);// fs.. because m_Filesize never actually reaches zero ...its not important.
		SetWindowText(m_pCWnd->m_hWnd, m_MsgStr);
	}

	if(i>0 && m_Ok){
		// Line read is now in m_LineReadBuf.
		// Remove comma at end of line and null terminate the string before calling GetCommaSeparatedFields.
		// Possibilities we must cope with - 
		// a) with or without comma at end of line.
		// b) with or without '\n' after the last line in the file.

		if(m_LineReadBuf[i] == '\n'){

			if(m_LineReadBuf[i-1] == ',') m_LineReadBuf[i-1] = 0;	// comma at end of line.
			else                          m_LineReadBuf[i  ] = 0;	// no comma at end of line.
			
			*pNumFields = GetCommaSeparatedFields(m_LineReadBuf, ppFields, MaxFields, 1);
		}
		else if(i < LINEBUFMAX-1){
			// End of the file and the last line doesnt finish with a \n. 

			if(m_LineReadBuf[i] == ',') m_LineReadBuf[i  ] = 0;		// comma at end of line.
			else                        m_LineReadBuf[i+1] = 0;		// no comma at end of line.

			*pNumFields = GetCommaSeparatedFields(m_LineReadBuf, ppFields, MaxFields, 1);
		}
		else m_Ok = 0;			
	}
	return m_Ok;
}
