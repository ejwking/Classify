
#include "pch.h"
#include "Annotations.h"
#include "FixAndCropPictures.h"
#include "CsvFile.h"
#include "YoloExportDlg.h"

// ########################################################
// #################### for global use ####################
int BitmapCoordinatesToBbox_Inc_Exc(int L_inclusive, int R_exclusive, int T_inclusive, int B_exclusive, int Bitmap_Width, int Bitmap_Height, BBOX *pBox)
{
	// A YOLO bounding box which includes the entire bitmap would be left=0, top=0, right=width, bottom=height.
	// The bounding box is, left/top - inclusive, right/bottom - exclusive. 
	// 
	// in terms of drawing the boxes, it wouldnt make sense to use inclusive (left/top) and exclusive (right/bottom) positions, because visually we want to see the same
	// effect on all sides of the box, and not have the left/top sides obscuring a line of pixels that belong to the bounded object.
	// 
	// So it makes sense to draw a bounding box with all exclusive coordinates, so the user can contain the object, this also means that a bounding box
	// line may be drawn one pixel outside the bitmap on all sides.
	if(Bitmap_Width>0 && Bitmap_Height>0){
		pBox->x1 = (float)L_inclusive / (float)Bitmap_Width;
		pBox->y1 = (float)T_inclusive / (float)Bitmap_Height;
		pBox->x2 = (float)R_exclusive / (float)Bitmap_Width;
		pBox->y2 = (float)B_exclusive / (float)Bitmap_Height;
		return 1;
	}
	else MsgBox2("BitmapCoordinatesToBbox_Inc_Exc, error - zero width or height");
	return 0;
}

void BboxToPixelWdHt_Inc_Exc(int &PixelWd, int &PixelHt, int Bitmap_Width, int Bitmap_Height, BBOX *pBox, int Rounding/*=0*/)
{
	float w = (pBox->x2 - pBox->x1) * (float)Bitmap_Width;
	float h = (pBox->y2 - pBox->y1) * (float)Bitmap_Height;
	if(Rounding){
		PixelWd = (int)round(w);
		PixelHt = (int)round(h);
	}
	else{
		PixelWd = (int)w;
		PixelHt = (int)h;
	}
}

void BboxToBitmapCoordinates_Inc_Exc(int &L_inclusive, int &R_exclusive, int &T_inclusive, int &B_exclusive, int Bitmap_Width, int Bitmap_Height, BBOX *pBox, int Rounding/*=0*/)
{
	if(Rounding){
		// When a new bounding box is defined in the UI we have its pixel coordinates, then we convert to the BBOX floats for storage, then when displaying this box again
		// we convert from BBOX floats to pixels (integers), and as expected there are sometimes rounding errors that mean the bounding box displayed is a pixel different 
		// to the bounding box originally defined in the UI. hence rounding - round to nearest pixel when displaying bbox (but not for any other purpose).
		L_inclusive = (int)round(pBox->x1 * (float)Bitmap_Width);
		T_inclusive = (int)round(pBox->y1 * (float)Bitmap_Height);
		R_exclusive = (int)round(pBox->x2 * (float)Bitmap_Width);
		B_exclusive = (int)round(pBox->y2 * (float)Bitmap_Height);
	}
	else{
		L_inclusive = (int)(pBox->x1 * (float)Bitmap_Width);
		T_inclusive = (int)(pBox->y1 * (float)Bitmap_Height);
		R_exclusive = (int)(pBox->x2 * (float)Bitmap_Width);
		B_exclusive = (int)(pBox->y2 * (float)Bitmap_Height);
	}
}

void rect_BboxToBitmapCoordinates_Inc_Exc(RECT &Rect, int Bitmap_Width, int Bitmap_Height, BBOX *pBox, int Rounding/*=0*/)
{
	// REPLACE RECT EVERYWHERE WITH AN INT VERSION , call it iRECT, or myRECT
	int L = (int)Rect.left; // because RECT is 'long' not int.
	int R = (int)Rect.right;
	int T = (int)Rect.top;
	int B = (int)Rect.bottom;
	BboxToBitmapCoordinates_Inc_Exc(L, R, T, B, Bitmap_Width, Bitmap_Height, pBox, Rounding);
	Rect.left = L;
	Rect.right = R;
	Rect.top = T;
	Rect.bottom = B;
}

int BboxError(BBOX *pBox)
{
	if( pBox->x1>=0.0f && pBox->x1<=1.0f && 
		pBox->x2>=0.0f && pBox->x2<=1.0f && 
		pBox->y1>=0.0f && pBox->y1<=1.0f && 
		pBox->y2>=0.0f && pBox->y2<=1.0f )
		return 0;
	return 1;
}
// ########################################################
// ########################################################

CAnnotations::CAnnotations()
{
	m_AllGroups.Initialise();
	m_GroupsFltr.Initialise();
	m_SrcLines.Initialise();
	m_ClsfdLines.Initialise();

	// If average filename length is 50 chars, therefore 400,000/50 = 8000 unique csv line filenames per memory block.
	g_MemPool.Initialise(400*1000);

	InitJobData(0);	// not necessary
}

CAnnotations::~CAnnotations()
{
	m_AllGroups.FreeMem();
	m_GroupsFltr.FreeMem();
	m_SrcLines.FreeMem();
	m_ClsfdLines.FreeMem();
}

void CAnnotations::InitJobData(int CsvList)
{
	m_pCWnd       = ::AfxGetApp()->m_pMainWnd;
	m_pPicFilters = nullptr;
	m_CsvList     = CsvList;
	m_NumCsvList  = 0;
	m_DoingBackup = 0;
	m_NumColumns  = 0;
	memset(&m_Path,   0, sizeof(m_Path));
	memset(&m_Backup, 0, sizeof(m_Backup));

	g_MemPool.Reset();
	m_SrcLines.Reset();

	m_AllGroups.Reset();
	m_GroupsFltr.Reset();
	m_ClsfdLines.Reset();
}

int CAnnotations::AllocateMemCsvLine()
{
	if(m_SrcLines.Num >= m_SrcLines.Max){
		int bfsz = m_SrcLines.Max;
		m_SrcLines.EstimateNum = 0; // disable estimate, its not worth the complication.

		if(m_SrcLines.Max == 0){
			// start off with space for 24000 csv lines, assuming an average of 3 annotations per pic this would do 8000 pictures.
			m_SrcLines.Max = max(24000, m_SrcLines.EstimateNum);
		}
		else{
			// SOURCELINE is 48 bytes (it has now increased since writing this).
			// keep doubling the allocation until we get to 200,000, then increase by 200,000 every time (sensible ?).
			// 
			//  24,000 -  1,152,000 bytes, roughly 8000 pictures
			//  48,000 -  2,304,000 bytes, roughly 16,000 pictures
			//  96,000 -  4,608,000 bytes, roughly 32,000 pictures
			// 192,000 -  9,216,000 bytes, roughly 64,000 pictures
			// 384,000 - 18,432,000 bytes, roughly 128,000 pictures
			// capped increase, no more doubling.
			// 584,000 - 28,032,000 bytes, roughly 194,666 pictures
			// 784,000 - 37,632,000 bytes, roughly 261,333 pictures
			// 
			// realistically we're unlikely to do more than 250,000 pictures (rougly 750,000 csv lines).
			//  (also there is allocating required for the classified lines, and filename text).
			int theAddition = m_SrcLines.Max;
			theAddition     = min(200000, theAddition);
			m_SrcLines.Max += theAddition;
			m_SrcLines.Max  = max(m_SrcLines.Max, m_SrcLines.EstimateNum);	// EstimateNum is cumulative for csv files.
		}
		CString Str;
		float membytes = (float)(sizeof(SOURCELINE) * m_SrcLines.Max) / 1000000.0f;
		Str.Format("sourceline [%d->%d] %.2fMB\r\n", bfsz, m_SrcLines.Max, membytes);
		m_MemoryReport += Str;

		OutputDbgStr("\n AllocateMemCsvLine - m_SrcLines.Max %d", m_SrcLines.Max);
		SOURCELINE *pOldBuf = m_SrcLines.pCsvLns;
		if((m_SrcLines.pCsvLns = (SOURCELINE*)realloc((void*)m_SrcLines.pCsvLns, m_SrcLines.Max*sizeof(SOURCELINE))) == 0){
			// realloc failed.
			if(pOldBuf)
				free(pOldBuf);
			MsgBox2("error - AllocateMemCsvLine realloc failed, m_SrcLines.Max %d", m_SrcLines.Max);
			m_SrcLines.Initialise();
			return 0;
		}
	}
	return 1;
}

int CAnnotations::LoadCsvColumnHeaders(char **ppFields, int NumFields)
{
	OutputDbgStr("\n LoadCsvColumnHeaders - ");
	int i;
	for(i=0; i<NumFields; i++){
		strcpy_s(m_ColumnHeaders[i], num_entries(m_ColumnHeaders[i]), ppFields[i]);
		OutputDbgStr("%d)%s, ", i, m_ColumnHeaders[i]);
	}
	m_NumColumns = i;
	OutputDbgStr("\n m_NumColumns %d\n", m_NumColumns);

	if(m_SrcLines.Format == CSVFORMAT::CSV_NOT_SET){
		// determin what type/format the csv is by reading the headers in the first line.
		if(m_NumColumns > 0){
			if(strcmp(m_ColumnHeaders[0], VERSION_STR) == 0){
				m_SrcLines.Format = CSVFORMAT::CSV_CLASSIFIED;	// FYI - see how column headers are written in function WriteCsvLine.
				if(!m_CsvList){
					// Now we know the format of this csv, its a classified csv written by this program, we can set the classified csv path.
					strcpy_s(m_Path.ClassifiedCsv, num_entries(m_Path.ClassifiedCsv), m_Path.ModelCsv);
				}
			}
			else if(strcmp(m_ColumnHeaders[0], "ModelAnnotated_A") == 0){
				m_SrcLines.Format = CSVFORMAT::CSV_MODEL_A;
			}
			else if(strcmp(m_ColumnHeaders[0], "ModelAnnotated_B") == 0){
				m_SrcLines.Format = CSVFORMAT::CSV_MODEL_B;
			}
			else if(strcmp(m_ColumnHeaders[0], "prelabelled_1") == 0){
				m_SrcLines.Format = CSVFORMAT::CSV_PRELABELLED_1;
			}
		}
		if(m_SrcLines.Format != CSVFORMAT::CSV_NOT_SET)
			return 1;
	}
	CsvLineToMsgBox("error - csv column headers unrecognised", ppFields, NumFields);
	return 0;
}

int CAnnotations::CsvProcessLine(char **ppFields, int NumFields, int RecurseCsvOffset)
{
	int Ok = 0;
	if(AllocateMemCsvLine()){
		if(m_SrcLines.Num < m_SrcLines.Max){
			SOURCELINE *pDat = &m_SrcLines.pCsvLns[m_SrcLines.Num];
			memset(pDat, 0, sizeof(SOURCELINE));

			if(RecurseCsvOffset > 0)
				*pDat = m_SrcLines.pCsvLns[m_SrcLines.Num-1];

				 if(m_SrcLines.Format == CSVFORMAT::CSV_MODEL_A) Ok = LoadDataStructure_ModelAnnotated_A(ppFields, NumFields, pDat);
			else if(m_SrcLines.Format == CSVFORMAT::CSV_MODEL_B) Ok = LoadDataStructure_ModelAnnotated_B(ppFields, NumFields, pDat); // one i use for yolo annotated pictures.
			else if(m_SrcLines.Format == CSVFORMAT::CSV_CLASSIFIED) Ok = LoadDataStructure_Classified(ppFields, NumFields, pDat);
			else if(m_SrcLines.Format == CSVFORMAT::CSV_PRELABELLED_1) Ok = LoadDataStructure_PreLabelled_1(ppFields, NumFields, pDat, RecurseCsvOffset);
			else MsgBox2("error - csv format not recognised");

			if(Ok && RecurseCsvOffset!=-1){
				if(pDat->ClassId < 0)
					memset(&pDat->Box, 0, sizeof(BBOX));// its a 'negative' which means there are no bounding boxes, zero the box fields.
				m_SrcLines.Num++;

				if(RecurseCsvOffset > 0){
					// recursive call used for a variable number of class/bboxes specified in a single csv line, as can happen when .json is converted into .csv format.
					// RecurseCsvOffset ==  0, first call save the SOURCELINE and do not recurse.
					// RecurseCsvOffset >   0, save the SOURCELINE (might be the first or successive SOURCELINE) and then recurse.
					// RecurseCsvOffset == -1, stop recurse.
					CsvProcessLine(ppFields, NumFields, RecurseCsvOffset);
				}
			}
		}
		else MsgBox2("error - m_SrcLines.Max %d", m_SrcLines.Max);
	}
	return Ok;
}

#ifdef sslrdisabled
int CAnnotations::SetSourceLineRepeats(SOURCELINE *pDat, SOURCELINEREPEATS *pRepeats)
{
	MsgBox2("SetSourceLineRepeats not doing this anymore, causes complications when merging");
	// Since SOURCELINEREPEATS is often identical for many pictures (it is always identical for the annotations in a single picture) check
	// the previous (pictures) annotations to see if we can reuse an existing SOURCELINEREPEATS.
	int MaxTries = 0;
	int i = m_SrcLines.Num - 1;
	while((i>=0) && (MaxTries<30)){
		// (MaxTries<24) ideally this would be 24 unique entries, ..so we could improve this by not incrementing MaxTries unless the 
		// entries change compared to the last index. (have an absolute max tries as well).
		SOURCELINEREPEATS *pPrev = m_SrcLines.pCsvLns[i].pReP;
		if(pPrev){ // cant be null anyway.
			if( pPrev->Width == pRepeats->Width && todo check pfile_name
				pPrev->Height == pRepeats->Height &&
				pPrev->Pic_classified == pRepeats->Pic_classified && 
				pPrev->PictureCode == pRepeats->PictureCode &&
				pPrev->ColourCode == pRepeats->ColourCode )
				TIMESTAMP
			{
				// we already have an identical SOURCELINEREPEATS.
				pDat->pReP = pPrev;
				return 1;
			}
		}
		i--;
		MaxTries++;
	}
	pDat->pReP = (SOURCELINEREPEATS*)g_MemPool.GetBuffer(sizeof(SOURCELINEREPEATS));
	if(pDat->pReP){
		*pDat->pReP = *pRepeats;
		m_SrcLines.NumSLRepeats++;	// just for debugging.
		return 1;
	}
	MsgBox2("error - SetSourceLineRepeats sz %d\r\n %s", sizeof(SOURCELINEREPEATS), pDat->Repeats.pfile_name);
	return 0;
}
#endif

int CAnnotations::SetSourceLineFilename(SOURCELINE *pDat, char *pFilename)
{
	int Len = (int)strlen(pFilename);

	if(Len>2 && pFilename[0]=='"' && pFilename[Len-1]=='"'){
		pFilename[Len-1] = 0;	// remove the quotation marks if filename string is within them.
		pFilename++;			// (having the filename within quotation marks allows the csv parser to ignor commas in the filename).
		Len -= 2;
	}

	if(m_SrcLines.pPreviousFilename){
		if(strcmp(m_SrcLines.pPreviousFilename, pFilename) == 0){
			// Filename hasnt changed since the previous line, so point to same buffer as the previous line.
			pDat->Repeats.pfile_name = m_SrcLines.pPreviousFilename;
			return 1;
		}
	}

	Len += 1;	// +1 so Len includes the null.
	if(Len>5 && Len<=MAX_PATH){	// 5 chars would be the shortest filename, eg, a.bmp
		int numbuf_1, numbuf_2, maxelem;
		g_MemPool.GetUsageStatistics(numbuf_1, maxelem);
		pDat->Repeats.pfile_name = g_MemPool.GetBuffer(Len);
		g_MemPool.GetUsageStatistics(numbuf_2, maxelem);
		if(numbuf_1 != numbuf_2){
			CString Str;
			float Sz = (float)(maxelem*numbuf_2) / 1000000.0f;
			Str.Format(" memorypool - %.2fMB across %d blocks\r\n", Sz, numbuf_2);
			m_MemoryReport += Str;
		}
#ifdef FUNCDBG
		static int o=0, l=0;
		l += Len;
		OutputDbgStr("\n %d. g_MemPool.GetBuffer, bytes %d", o++, l);
#endif
		if(pDat->Repeats.pfile_name){
			if(strcpy_s(pDat->Repeats.pfile_name, Len, pFilename) == 0){
				m_SrcLines.pPreviousFilename = pDat->Repeats.pfile_name;
				return 1;
			}
		}
	}
	MsgBox2("error - SetSourceLineFilename \r\n %d %s \r\n %s", Len, pDat->Repeats.pfile_name, pFilename);
	return 0;
}

void CAnnotations::CsvLineToMsgBox(char *pComment, char **ppFields, int Num, SOURCELINE *pDat/*=nullptr*/)
{
	CString str, addstr;
	str.Format("%s\r\n", pComment);
	for(int s=0; s<Num; s++){
		addstr.Format("\r\n %d : %s ", s, ppFields[s]);
		str += addstr;
	}
	if(pDat){
		addstr.Format("\r\n\r\nSOURCELINE\r\n ClassId %d", pDat->ClassId);
		str += addstr;
		addstr.Format("\r\n Confidence %d", pDat->Confidence);
		str += addstr;
		addstr.Format("\r\n Box: x1 %f, x2 %f, y1 %f, y2 %f", pDat->Box.x1, pDat->Box.x2, pDat->Box.y1, pDat->Box.y2);
		str += addstr;
	}
	MsgBox2_buf(str.GetBuffer(str.GetLength()));
	str.ReleaseBuffer();	// must surely be released as FolderList goes out of scope, but just in case.
}

int CAnnotations::PlaceholderTypeConv(int src, int &dst)
{
	// replacement for the 2 functions below
	dst = src;
	return 1;
}
int CAnnotations::IntegerToShort(int Val, short &short_val)
{
	if(Val>SHRT_MIN && Val<SHRT_MAX){ // can be >= and <=
		short_val = (short)Val;
		return 1;
	}
	return 0;
}
int CAnnotations::IntegerTo_Char(int Val, char &char_val)
{
	if(Val>SCHAR_MIN && Val<SCHAR_MAX){ // can be >= and <=
		char_val = (char)Val;
		return 1;
	}
	return 0;
}

int CAnnotations::StrToNumericClassId(char *pStr, int *pclass_id)
{
	int cid=0;
	int ok = g_CIdM.GetClassId(pStr, &cid);
	*pclass_id = cid;
	return ok;	// no need for g_CIdM.ClassIdIsValid(cid);
}

int CAnnotations::SourceLineErrorCode(SOURCELINE *pLne)
{
	// (class id has already been verified in StrToNumericClassId).
	int Error = 0;
	if(pLne->Repeats.Width<1 || pLne->Repeats.Height<1)
		Error = 1;
	if(BboxError(&pLne->Box))
		Error += 10;
	if(pLne->Confidence<0 || pLne->Confidence>100)
		Error += 100;
	if(pLne->model_code < 0)
		Error += 1000;
	if(pLne->Repeats.ColourCode<0 || pLne->Repeats.ColourCode>=COLOURCODE::COLOUR_NUM_ENTRIES)
		Error += 10000;
	if(pLne->Repeats.PictureCode<0 || pLne->Repeats.PictureCode>=PICTURECODE::PIC_NUM_ENTRIES)
		Error += 100000;
	if(pLne->Repeats.Pic_classified<0 || pLne->Repeats.Pic_classified>=CLASSIFIED_STATUS::CLASSIFIED_NUM_ENTRIES)
		Error += 1000000;
	if(pLne->model_code<0 || pLne->model_code>=MODELCODE::MODEL_NUM_ENTRIES)
		Error += 10000000;
	if(pLne->Repeats.Time_Stamp<0 || pLne->Repeats.Time_Stamp>INT_MAX) // 8 digits
		Error += 100000000;
	if(Error != 0)
		MsgBox2("error - SourceLineErrorCode\r\n Error %d", Error);
	return Error;
}

int CAnnotations::LoadDataStructure_Classified(char **ppFields, int Num, SOURCELINE *pDat)
{
	// Another possible column would be 'partial' object to specify if object is truncated, (column count can be flexible, so could add this in future).
	if(Num==14 || Num==15){
		// classified csv annotation file - also see where its written, WriteCsvLine.
		//______________________________________________________________________________________________________________________________________________
		// 0              1          2            3           4     5      6        7      8      9      10     11       12         13         14
		// filename_count,classified,picture_code,colour_code,width,height,filename,box_x1,box_y1,box_x2,box_y2,class_id,model_code,debug_code,timestamp
		int Ok = 1;
		Ok &= PlaceholderTypeConv(atoi(ppFields[1]), pDat->Repeats.Pic_classified);
		Ok &= PlaceholderTypeConv(atoi(ppFields[2]), pDat->Repeats.PictureCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[3]), pDat->Repeats.ColourCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[4]), pDat->Repeats.Width);
		Ok &= PlaceholderTypeConv(atoi(ppFields[5]), pDat->Repeats.Height);
		//Ok &= SetSourceLineRepeats(pDat, &SLR);
		Ok &= SetSourceLineFilename(pDat, ppFields[6]);
		pDat->Box.x1 = (float)atof(ppFields[7]);
		pDat->Box.y1 = (float)atof(ppFields[8]);
		pDat->Box.x2 = (float)atof(ppFields[9]);
		pDat->Box.y2 = (float)atof(ppFields[10]);
		Ok &= StrToNumericClassId(ppFields[11], &pDat->ClassId);
		Ok &= PlaceholderTypeConv(atoi(ppFields[12]), pDat->model_code);
		pDat->DebugCode = atoi(ppFields[13]);
		if(Num == 15)
			pDat->Repeats.Time_Stamp = atoi(ppFields[14]);

		if(Ok)
			if(SourceLineErrorCode(pDat) == 0)
				return 1;
		CsvLineToMsgBox("LoadDataStructure_Classified - unacceptable values in line", ppFields, Num, pDat);
	}
	else CsvLineToMsgBox("LoadDataStructure_Classified - invalid number of fields on line", ppFields, Num);
	return 0;
}

int CAnnotations::LoadDataStructure_ModelAnnotated_B(char **ppFields, int Num, SOURCELINE *pDat)
{
	if(Num == 13){
		//_________________________________________________________________________________________________________________________
		// 0              1            2           3     4      5        6      7      8      9      10       11         12
		// filename_count,picture_code,colour_code,width,height,filename,box_x1,box_y1,box_x2,box_y2,class_id,confidence,model_code
		int Ok = 1;
		Ok &= PlaceholderTypeConv(atoi(ppFields[1]), pDat->Repeats.PictureCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[2]), pDat->Repeats.ColourCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[3]), pDat->Repeats.Width);
		Ok &= PlaceholderTypeConv(atoi(ppFields[4]), pDat->Repeats.Height);
		//Ok &= SetSourceLineRepeats(pDat, &SLR);
		Ok &= SetSourceLineFilename(pDat, ppFields[5]);
		pDat->Box.x1 = (float)atof(ppFields[6]);
		pDat->Box.y1 = (float)atof(ppFields[7]);
		pDat->Box.x2 = (float)atof(ppFields[8]);
		pDat->Box.y2 = (float)atof(ppFields[9]);
		Ok &= StrToNumericClassId(ppFields[10], &pDat->ClassId);
		Ok &= PlaceholderTypeConv(atoi(ppFields[11]), pDat->Confidence);
		Ok &= PlaceholderTypeConv(atoi(ppFields[12]), pDat->model_code);
		/*pDat->model_code = MODELCODE::MODEL_YOLOV7;*/
		if(Ok)
			if(SourceLineErrorCode(pDat) == 0)
				return 1;
		CsvLineToMsgBox("LoadDataStructure_ModelAnnotated_B - unacceptable values in line", ppFields, Num, pDat);
	}
	else CsvLineToMsgBox("LoadDataStructure_ModelAnnotated_B - invalid number of fields on line", ppFields, Num);
	return 0;
}

int CAnnotations::LoadDataStructure_ModelAnnotated_A(char **ppFields, int Num, SOURCELINE *pDat)
{
	if(Num == 11){
		//_____________________________________________________________________________________________________________
		// 0              1            2           3     4      5        6        7         8       9          10      
		// filename_count,picture_code,colour_code,width,height,filename,box_left,box_right,box_top,box_bottom,class_id
		int Ok = 1;
		pDat->model_code = MODEL_GRD_DINO; // etc
		Ok &= PlaceholderTypeConv(atoi(ppFields[1]), pDat->Repeats.PictureCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[2]), pDat->Repeats.ColourCode);
		Ok &= PlaceholderTypeConv(atoi(ppFields[3]), pDat->Repeats.Width);
		Ok &= PlaceholderTypeConv(atoi(ppFields[4]), pDat->Repeats.Height);
		//Ok &= SetSourceLineRepeats(pDat, &SLR);
		Ok &= SetSourceLineFilename(pDat, ppFields[5]);
		int lf = atoi(ppFields[6]);  // inclusive
		int rh = atoi(ppFields[7]);  // exclusive
		int tp = atoi(ppFields[8]);  // inclusive
		int bt = atoi(ppFields[9]);  // exclusive
		Ok &= StrToNumericClassId(ppFields[10], &pDat->ClassId);
		if(lf<0 || rh>pDat->Repeats.Width || tp<0 || bt>pDat->Repeats.Height)
			Ok = 0;
		Ok &= BitmapCoordinatesToBbox_Inc_Exc(lf, rh, tp, bt, pDat->Repeats.Width, pDat->Repeats.Height, &pDat->Box);
		if(Ok)
			if(SourceLineErrorCode(pDat) == 0)
				return 1;
		CsvLineToMsgBox("LoadDataStructure_ModelAnnotated_A - unacceptable values in line", ppFields, Num, pDat);
	}
	else CsvLineToMsgBox("LoadDataStructure_ModelAnnotated_A - invalid number of fields on line", ppFields, Num);
	return 0;
}

int CAnnotations::LoadClassAndBbox_PreLabelled_1(char **ppFields, int Num, SOURCELINE *pDat, int *pLoaded)
{
	int Ok = 1;
	*pLoaded = 0;
	if(Num>=6 && strlen(ppFields[0])>0){
		if(strlen(ppFields[0]) != 7) Ok = 0;
		else if(strcmp(ppFields[0], "vehicle") != 0) Ok = 0;
		Ok &= StrToNumericClassId(ppFields[1], &pDat->ClassId);
		pDat->Box.x1 = (float)atof(ppFields[2]);	// x min
		pDat->Box.y1 = (float)atof(ppFields[3]);	// y min
		pDat->Box.x2 = (float)atof(ppFields[4]) + pDat->Box.x1;	// width
		pDat->Box.y2 = (float)atof(ppFields[5]) + pDat->Box.y1;	// height
		*pLoaded = 1;
	}
	return Ok;
}

int CAnnotations::LoadDataStructure_PreLabelled_1(char **ppFields, int Num, SOURCELINE *pDat, int &RecurseCsvOffset)
{
	// example of single line (i've put it on 4 lines below):
	// 
	// /home/bvizcaino/projectes/vehicle_class_data/fira_bcn_images_training2/dataset_training_2/truck/E7649KX.jpg,
	// 1957,1468,
	// vehicle,truck,0.21614716402657128,0.0034059945504087193,0.7802759325498212,0.9931880108991825,
	// vehicle,truck,0.00027,0.41398,0.1011900007724762,0.33629998564720154,,,,,,,,,,,,,,,,,,
	if(Num > 0){
		int Ok = 1;
		if(RecurseCsvOffset == 0){
			pDat->model_code = MODEL_PRELABELLED;
			char *pDir1 = "/home/bvizcaino/projectes/vehicle_class_data/fira_bcn_images_training2/";
			char *pDir2 = "/home/bvizcaino/projectes/vehicle_class_data/";
			char *pDir3 = "/home/bvizcaino/projectes/";
			char *pMatch, TmpPath[MAX_PATH]={0}, *pPath = ppFields[0];

			if( (pMatch = strstr(pPath, pDir1)) )
				strcpy_s(TmpPath, num_entries(TmpPath), &pPath[strlen(pDir1)]);
			else if( (pMatch = strstr(pPath, pDir2)) )
				strcpy_s(TmpPath, num_entries(TmpPath), &pPath[strlen(pDir2)]);
			else if( (pMatch = strstr(pPath, pDir3)) )
				strcpy_s(TmpPath, num_entries(TmpPath), &pPath[strlen(pDir3)]);
			else Ok = 0;
			if(Ok){
				char FinishedPath[MAX_PATH]={0};
				int Len = (int)strlen(TmpPath);
				for(int i=0; i<Len; i++)
					if(TmpPath[i] == '/')
						TmpPath[i] = '\\';
				sprintf_s(FinishedPath, num_entries(FinishedPath), "prelabelled_1\\%s", TmpPath);
				Ok &= SetSourceLineFilename(pDat, FinishedPath);
				Ok &= PlaceholderTypeConv(atoi(ppFields[1]), pDat->Repeats.Width);
				Ok &= PlaceholderTypeConv(atoi(ppFields[2]), pDat->Repeats.Height);
				int Colour;
				FolderAndFilenameToPath(TmpPath, num_entries(TmpPath), m_Path.PictureFolder, FinishedPath);
				Ok &= g_CBmpFile.GetBitmapSpecifics(TmpPath, nullptr, nullptr, &Colour);
					 if(Colour ==  0) pDat->Repeats.ColourCode = COLOURCODE::COLOUR_NO;
				else if(Colour ==  1) pDat->Repeats.ColourCode = COLOURCODE::COLOUR_YES;
				else if(Colour == -1) Ok = 0;
			}
		}
		int CBOffset = 3;
		if(RecurseCsvOffset==0 && strlen(ppFields[CBOffset])==0)
			pDat->ClassId = -1;
		else{
			int Loaded = 0;
			Ok &= LoadClassAndBbox_PreLabelled_1(&ppFields[RecurseCsvOffset+CBOffset], Num-(RecurseCsvOffset+CBOffset), pDat, &Loaded);
			if(Ok){
				if(Loaded)
					RecurseCsvOffset += 6;	// must recurse to see if theres more bboxes in the csv line.
				else{
					// we have recursed but not found another bbox, calling function must *not* save this as another SOURCELINE.
					RecurseCsvOffset = -1;	// -1 is the signal to calling function not to save this as another SOURCELINE.
					return 1;
				}
			}
		}

		if(Ok)
			if(SourceLineErrorCode(pDat) == 0)
				return 1;
		CsvLineToMsgBox("LoadDataStructure_PreLabelled_1 - unacceptable values in line", ppFields, Num, pDat);
	}
	else MsgBox2("error - LoadDataStructure_PreLabelled_1, num fields %d", Num);
	return 0;
}

int CAnnotations::GroupRepeatValuesOk(SOURCELINE *pL1, SOURCELINE *pL2)
{
	// several fields must have the values repeated for the entire filename group. check this is indeed the case.
	int Ok = 1;
	if(pL1->Repeats.Time_Stamp     != pL2->Repeats.Time_Stamp) Ok = 0;
	if(pL1->Repeats.pfile_name     != pL2->Repeats.pfile_name) Ok = 0;
	if(pL1->Repeats.Width          != pL2->Repeats.Width) Ok = 0;
	if(pL1->Repeats.Height         != pL2->Repeats.Height) Ok = 0;
	if(pL1->Repeats.Pic_classified != pL2->Repeats.Pic_classified) Ok = 0;
	if(pL1->Repeats.PictureCode    != pL2->Repeats.PictureCode) Ok = 0;
	if(pL1->Repeats.ColourCode     != pL2->Repeats.ColourCode) Ok = 0;
	if(!Ok)
		MsgBox2("GroupRepeatValuesOk - failed, csv line error \r\n%s", pL1->Repeats.pfile_name);
	return Ok;
}

//#define REFUSE_MISSING_PICS
int CAnnotations::SourceCsvFormattingOk()
{
	int  Ok=1, ValidatePictureFolder=0;
	char PicPath[MAX_PATH];
	FILENAMEGROUP *pFnG = m_AllGroups.pFNG;
	for(int i=0; i<m_AllGroups.Num && Ok; i++, pFnG++){
		int NegativeCount=0, PositiveCount=0;
		SOURCELINE *pLine = pFnG->pSourceLines;
		for(int b=0; b<pFnG->NumSourceLines && Ok; b++, pLine++){

			if(pLine->ClassId < 0) NegativeCount++;
			else PositiveCount++;

			if(b > 0)
				Ok &= GroupRepeatValuesOk((pLine-1), pLine);		
		}
		if(NegativeCount > 0){
			if(NegativeCount>1 || PositiveCount>0){
				MsgBox2("error - SourceCsvFormattingOk\r\n 'negative' picture must comprise one csv line with class id '-1'\r\n actual lines - NegativeCount %d, PositiveCount %d\r\n csv line filename - %s", NegativeCount, PositiveCount, pFnG->pPicInfo->pfile_name);
				Ok = 0;
			}
		}
#ifdef REFUSE_MISSING_PICS
		sprintf_s(PicPath, num_entries(PicPath), "%s\\%s", m_Path.PictureFolder, pFnG->pPicInfo->pfile_name);
		if(FileExists(PicPath) == 0)
			pFnG->Classified = CLASSIFIED_REFUSE;
#else
		if(ValidatePictureFolder < 10){
			// Test the first 10 unnique filenames.
			// These filenames (for the pictures) on each line of the csv should exist in the user specified picture folder, if they dont we cannot proceed.
			ValidatePictureFolder++;
			sprintf_s(PicPath, num_entries(PicPath), "%s\\%s", m_Path.PictureFolder, pFnG->pPicInfo->pfile_name);
			if(FileExists(PicPath) == 0){
				MsgBox2("error - SourceCsvFormattingOk\r\n%s\r\nFilename - %s \r\n  does not exist in the specified picture folder - \r\n    %s", PicPath, pFnG->pPicInfo->pfile_name, m_Path.PictureFolder);
				//Ok = 0;
			}
		}
#endif
	}
	OutputDbgStr("\n SourceCsvFormattingOk - m_AllGroups.Num %d\n", m_AllGroups.Num);
	return Ok;
}

int CAnnotations::AssignFilenameCount(int SkipErrorChecking)
{
	m_SrcLines.NumFilenames = 0;
	m_SrcLines.LargestFnGrp = 0;	// this is calculated below, but not ever used anywhere at the moment, why did i add it?
	if(m_SrcLines.Num > 0){
		int i = 0;
		SOURCELINE *pScr = m_SrcLines.pCsvLns;
		while(i < m_SrcLines.Num){
			SOURCELINE *p1stLine = &pScr[i];	// first csv line in the group with matching filenames.
			p1stLine->FilenameCount = ++m_SrcLines.NumFilenames;
			int shft = i + 1;
			while(shft < m_SrcLines.Num){
				// keep incrementing array while pFilename matches the filename in the 
				// first line, and assign every matching filename with the filename count.
				if(strcmp(p1stLine->Repeats.pfile_name, pScr[shft].Repeats.pfile_name) == 0){
					// Q: why not just check the filename pointers are the same, because all filenames for the same picture (i.e. the same filename group) will be pointing to the same buffer?
					// A: ..no, when merging that will not be the case, the filename entries for different csv files are loaded at different point in time, hence different buffers.
					pScr[shft].FilenameCount = p1stLine->FilenameCount;
					shft++;
				}
				else break;
			}
			m_SrcLines.LargestFnGrp = max(m_SrcLines.LargestFnGrp, shft-i);
			i = shft;	// move to the start of the next filename group.

			if((i%10000)==0 || i==m_SrcLines.Num-1)
				ProgressUpdate_SetWindowText(m_pCWnd, "filename group count", i, m_SrcLines.Num);
		}

		if(SkipErrorChecking)
			return 1;

		// error checking, the filename of a filename group must not be repeated elsewhere in the list outside the group.
		int SecondFilenameCount = 1;
		for(pScr=&m_SrcLines.pCsvLns[i=0]; i<m_SrcLines.Num-1; i++, pScr++){
			
			// below if: could test FilenameCount here instead of pFilename, but using pFilename acts as a double check for the code above (thats un-necessary).
			/*if(strcmp(pScr->Repeats.pfile_name, (pScr+1)->Repeats.pfile_name) != 0){*/
			// ok, so as per the comment above I'm using FilenameCount (in order to help speed this code up a bit).
			if(pScr->FilenameCount != (pScr+1)->FilenameCount){

				// new filename group at (pScr+1) ..so we know (pScr+1) has a different filename compared with pSrc, 
				// so next test (pScr + 2) and onwards to check pScr->Repeats.pfile_name does not exist elsewhere in the list.
				int k = i + 2;
				while(k < m_SrcLines.Num){
					if(strcmp(pScr->Repeats.pfile_name, m_SrcLines.pCsvLns[k].Repeats.pfile_name) == 0){
						MsgBox2("error - AssignFilenameCount\r\n..matching filenames are not grouped together in csv\r\n%s - index %d\r\n%s - index %d", pScr->Repeats.pfile_name, i, m_SrcLines.pCsvLns[k].Repeats.pfile_name, k);
						return 0;
					}
					// ###(1)###, attempt to speed things up a bit by only checking the filename once per filename group.
					int PrvFnC = m_SrcLines.pCsvLns[k].FilenameCount;
					k++;
					// ###(2)###, read above, skip over matching FilenameCount's.
					while(k<m_SrcLines.Num && PrvFnC==m_SrcLines.pCsvLns[k].FilenameCount) k++;
				}
				SecondFilenameCount++;
			}
			// this loop is slow, (also, after a merge, doing this is maybe a little pointless, but it does check the merge has worked properly).
			if((i%1000)==0 || i==m_SrcLines.Num-1)
				ProgressUpdate_SetWindowText(m_pCWnd, "filename group check", i, m_SrcLines.Num);
		}

		if(SecondFilenameCount == m_SrcLines.NumFilenames)
			return 1;
		MsgBox2("error - AssignFilenameCount\r\n SecondFilenameCount %d(%d)", SecondFilenameCount, m_SrcLines.NumFilenames);
	}
	else MsgBox2("error - zero csv lines");
	return 0;
}

int CAnnotations::AddSpaceForCroppedAnnotations()
{
	if(m_SrcLines.Num > 0){
		int Previous_Num = m_SrcLines.Num;
		m_SrcLines.Num += ((m_SrcLines.Num * 50) / 100);
		if(AllocateMemCsvLine()){
			m_SrcLines.Num = m_SrcLines.Max - 1;	// what we got from AllocateMemCsvLine, it will be no less than the extra added above but probably more.
			int i = Previous_Num;
			if(m_SrcLines.pCsvLns[i-1].FilenameCount == m_SrcLines.NumFilenames){
				m_SrcLines.NumFilenames++;
				while(i < m_SrcLines.Num){
					memset(&m_SrcLines.pCsvLns[i], 0, sizeof(SOURCELINE));
					m_SrcLines.pCsvLns[i].FilenameCount = m_SrcLines.NumFilenames;
					i++;
				}
				return 1;
			}
		}
	}
	return 0;
}

/*static int CompareFilename(const void *arg1, const void *arg2)
{
	SOURCELINE *pS1 = (SOURCELINE*)arg1;
	SOURCELINE *pS2 = (SOURCELINE*)arg2;
//	return strcmp(pS1->Repeats.pfile_name, pS2->Repeats.pfile_name);			'A' before 'B' - no, we dont need to put in alphabetical order (this works)
	return (strcmp(pS1->Repeats.pfile_name, pS2->Repeats.pfile_name) == 0); // we just want to group together matching filenames - (this not working, what is wrong? never mind did my own sort).
}*/
int CAnnotations::SourceLineFilenameMerge()
{
	if(m_SrcLines.Num > 1){
		// loop to merge/group-together matching filenames, (does not put alphabetical order).
		int i = 0;
		char *pf = m_SrcLines.pCsvLns[i++].Repeats.pfile_name;
		while(i < m_SrcLines.Num){
			if(strcmp(pf, m_SrcLines.pCsvLns[i].Repeats.pfile_name) != 0){
				int k = i + 1;
				while(k < m_SrcLines.Num){
					if(strcmp(pf, m_SrcLines.pCsvLns[k].Repeats.pfile_name) == 0){
						SOURCELINE TmpSL      = m_SrcLines.pCsvLns[i];
						m_SrcLines.pCsvLns[i] = m_SrcLines.pCsvLns[k];
						m_SrcLines.pCsvLns[k] = TmpSL;
						i++;
					}
					k++;
				}
				pf = m_SrcLines.pCsvLns[i].Repeats.pfile_name;
			}
			i++;
			// this merge/sort is surprisingly slow.
			if((i%1000)==0 || i==m_SrcLines.Num-1)
				ProgressUpdate_SetWindowText(m_pCWnd, "filename merge", i-1, m_SrcLines.Num);
		}
		// or use qsort(m_SrcLines.pCsvLns, m_SrcLines.Num, sizeof(SOURCELINE), CompareFilename);
		return 1;
	}
	return 0;
}

int CAnnotations::LoadCsv_File(int FileNum, char *pPath, int ExcludeModel, int ExcludeClassified, int &Excluded, int &AvgChrsPerLine)
{
	Excluded = 0;
	AvgChrsPerLine = 0;
	CCsvFile csv;
	int Filesize, NumFields, LineNumber, Ok = 0;
	if((Filesize = csv.Open(pPath, FileNum)) > 0){
		m_SrcLines.Format = CSVFORMAT::CSV_NOT_SET;
		m_SrcLines.EstimateNum += (Filesize / 90);	// roughly 90 chars per line on average.
		OutputDbgStr("\n LoadCsv_File - m_SrcLines.EstimateNum %d, %s", m_SrcLines.EstimateNum, pPath);
		do{
			if((Ok = csv.GetNextLine(m_ppFields, MAX_COLUMNS, &NumFields, &LineNumber)) != 0){
				if(NumFields > 0){
					// NumFields==0, we can skip missing lines, thats ok, eg if a gap had been put between 2 lines.
					if(LineNumber == 0){
						Ok = LoadCsvColumnHeaders(m_ppFields, NumFields);
						int clsf = (m_SrcLines.Format == CSVFORMAT::CSV_CLASSIFIED);
						if( (!clsf && ExcludeModel)    || // dont want model files, break.
							(clsf && ExcludeClassified) ) // dont want classified files, break.
						{ Excluded = 1; break; }
					}
					else{
						int RecurseCsvOffset = 0;
						Ok = CsvProcessLine(m_ppFields, NumFields, RecurseCsvOffset);
					}
				}
			}
		}
		while(Ok && NumFields>0);
		csv.Close();
		if(LineNumber > 0)
			AvgChrsPerLine = Filesize / (LineNumber+1);
	}
	return Ok;
}

int CAnnotations::LoadNegativeSourceline(char *pBitmapPath, int DoColourTest)
{
	// function loads the SOURCELINE array, it not creating/loading any csv's, that only happen when the csv file is saved to disk.
	if(AllocateMemCsvLine()){
		if(m_SrcLines.Num < m_SrcLines.Max){
			SOURCELINE *pSL = &m_SrcLines.pCsvLns[m_SrcLines.Num];
			memset(pSL, 0, sizeof(SOURCELINE));
			SOURCELINEREPEATS *pR = &pSL->Repeats;
			int Ok=1, Wd=0, Ht=0, Colour=0;
			int Len = (int)strlen(m_Path.PictureFolder) + 1;
			Ok &= SetSourceLineFilename(pSL, &pBitmapPath[Len]); // relative path
			if(DoColourTest){
				Ok &= g_CBmpFile.GetBitmapSpecifics(pBitmapPath, &Wd, &Ht, &Colour);
					 if(Colour ==  0) pR->ColourCode = COLOURCODE::COLOUR_NO;
				else if(Colour ==  1) pR->ColourCode = COLOURCODE::COLOUR_YES;
				else if(Colour == -1) Ok = 0;
			}
			else
				Ok &= g_CBmpFile.GetBitmapSpecifics(pBitmapPath, &Wd, &Ht, nullptr);
			pR->Height = Ht;
			pR->Width  = Wd;
			pSL->ClassId = -1; // this entry is a negative, i.e the bitmap has one sourceline entry with class id -1.
			if(Ok){
				m_SrcLines.Num++;
				return 1;
			}
		}
		else MsgBox2("error - LoadNegativeSourceline m_SrcLines.Max %d", m_SrcLines.Max);
	}
	return 0;
}

static int g_DirListInterrupt=0;
struct MAKEBLANKSLIST
{
	CAnnotations *pCAnn;
	INPUT_CONFIG *pInputConfig;
	HWND hDlg;
	BOOL JustCountFiles;
	int Ok, Count, PreCount;
	char tempstr[MAX_PATH];
};
int CAnnotations::Static_MakeDirList_CreateBlankCsv(int IsFile, int Mode, char *pDir, char *pName, void *pData)
{
	// misleading function name maybe, this here is part of the process of creating a csv file with blank entries (meaning one negative class-id entry for each bitmap), 
	// but creating the actual csv file isnt done until the user presses the save button, here this function loads the SOURCELINE array with one entry for each bitmap.
	Mode = Mode; // UNUSED PARAM, this relates to QuickLoad, see MakeDirList.
	if(IsFile){
		if(FileHasBitmapExtension(pName)){
			MAKEBLANKSLIST *pMbl = (MAKEBLANKSLIST*)pData;
			if(pMbl->JustCountFiles){
				pMbl->PreCount++;
				if((pMbl->PreCount % 1000) == 0)
					sprintf_s(pMbl->tempstr, num_entries(pMbl->tempstr), TEXT("%d picture count"), pMbl->PreCount);
			}
			else{
				sprintf_s(pMbl->tempstr, num_entries(pMbl->tempstr), TEXT("%s\\%s"), pDir, pName);
				pMbl->Ok &= pMbl->pCAnn->LoadNegativeSourceline(pMbl->tempstr, pMbl->pInputConfig->DoColourTestForBlanks);
				if((pMbl->Count % 10) == 0){
					if(pMbl->PreCount != 0)
						sprintf_s(pMbl->tempstr, num_entries(pMbl->tempstr), TEXT("%d / %d compiling entries - %s"), pMbl->Count, pMbl->PreCount, pName);
					else
						sprintf_s(pMbl->tempstr, num_entries(pMbl->tempstr), TEXT("%d compiling entries - %s"), pMbl->Count, pName);
					SetWindowText(pMbl->pCAnn->m_pCWnd->m_hWnd, pMbl->tempstr);
				}
				pMbl->Count++;
				if(!pMbl->Ok)
					g_DirListInterrupt = 1;
			}
		}
	}
	return 1;
}

struct MAKECSVLIST
{
	CAnnotations *pCAnn;
	INPUT_CONFIG *pInputConfig;
	CString ReportText;
	HWND hDlg;
	int Ok, NumModel, NumClsfd, NumCsvFiles, NumAllFiles, LoadCsv, PrevNumLines;
	void Init(){
		Ok=1; NumModel=0; NumClsfd=0; NumCsvFiles=0; NumAllFiles=0; LoadCsv=0; PrevNumLines=0;
	}
};
int CAnnotations::Static_MakeDirList_LoadCsvFiles(int IsFile, int Mode, char *pDir, char *pName, void *pData)
{
	Mode = Mode; // UNUSED PARAM, this relates to QuickLoad, see MakeDirList.
	if(IsFile){
		MAKECSVLIST *pMcl = (MAKECSVLIST*)pData;
		if(FileHasCsvExtension(pName)){
			CString Tmp;
			char csvPath[MAX_PATH];
			sprintf_s(csvPath, num_entries(csvPath), TEXT("%s\\%s"), pDir, pName);
			if(pMcl->LoadCsv){
				// load each and every csv file.
				CAnnotations *pCA = pMcl->pCAnn;
				int Excluded=0, AvgChrsPerLine=0;
				pMcl->Ok &= pCA->LoadCsv_File(pMcl->NumCsvFiles+1, csvPath, pMcl->pInputConfig->FolderExcludeModel, pMcl->pInputConfig->FolderExcludeClassified, Excluded, AvgChrsPerLine);
				if(!Excluded){
					Tmp.Format("[%d] Ok %d, %s, lines %d/%d, chrs/line %d, %s\r\n", pMcl->NumCsvFiles++, pMcl->Ok, (pCA->m_SrcLines.Format==CSVFORMAT::CSV_CLASSIFIED)?"CLASSIFIED":"MODEL", pCA->m_SrcLines.Num-pMcl->PrevNumLines, pCA->m_SrcLines.Num, AvgChrsPerLine, csvPath);
					pMcl->PrevNumLines = pCA->m_SrcLines.Num;
					pMcl->ReportText += Tmp;
					if(pCA->m_SrcLines.Format == CSVFORMAT::CSV_CLASSIFIED)
						pMcl->NumClsfd++;
					else
						pMcl->NumModel++;
				}
				if(!pMcl->Ok)
					g_DirListInterrupt = 1;
			}
			else{
				// just a make list of all the csv files.
				Tmp.Format("[%d] %s\r\n", pMcl->NumCsvFiles++, csvPath);
				pMcl->ReportText += Tmp;
			}
		}
		pMcl->NumAllFiles++;
	}
	return 1;
}

int CAnnotations::Static_MakeDirList_Interrupt(HWND hDlg)
{
	hDlg = hDlg; // stop warning.
/*	if(g_DirListInterrupt){
		MessageBox( hDlg, "make dir has been interrupted"), "warning"), MB_OK);
		g_DirListInterrupt = INTERRUPT_USER_QUIT;
		return FALSE;
	}*/
	return (g_DirListInterrupt == 0);
}

int CAnnotations::LoadCsv_Folder(char *pFolder, INPUT_CONFIG *pInputConfig, int *pNumClassified, int *pNumModel)
{
	MAKECSVLIST mcl;
	int QuickLoad=1, IncludeSubDirs=1;
	CString Tmp;
	g_DirListInterrupt = 0;
	mcl.Init();
	// ========================================================
	// Load csv's (1), or just do a run to count csv files (0)
	mcl.LoadCsv = 1; // PUT IN UI 
	// ========================================================

	mcl.hDlg = AfxGetMainWnd()->m_hWnd;	// not used at the moment.
	mcl.pCAnn = this;
	mcl.pInputConfig = pInputConfig;
	mcl.ReportText.Format("csv folder: %s\r\n\r\ncsv file list:\r\n", pFolder);
	MakeDirList(QuickLoad, TRUE, mcl.hDlg, IncludeSubDirs, 0, pFolder, "*.*", Static_MakeDirList_LoadCsvFiles, Static_MakeDirList_Interrupt, &mcl);
	m_NumCsvList = mcl.NumCsvFiles;
	if(mcl.LoadCsv){
		Tmp.Format("\r\n classified csv files: %d\r\n model csv files: %d\r\n total csv lines: %d", mcl.NumClsfd, mcl.NumModel, m_SrcLines.Num);
		mcl.ReportText += Tmp;
	}
	else{
		Tmp.Format("\r\n total files: %d", mcl.NumAllFiles);
		mcl.ReportText += Tmp;
	}
	MsgBox2_buf(mcl.ReportText.GetBuffer(mcl.ReportText.GetLength()));
	mcl.ReportText.ReleaseBuffer();	// must surely be released as ReportText goes out of scope, but just in case.

	*pNumClassified = mcl.NumClsfd;
	*pNumModel = mcl.NumModel;
	return (mcl.Ok && m_NumCsvList>0);
}

FILENAMEGROUP *CAnnotations::CreateBlankCsvFile(char *pPictureBaseFolder, char *pPictureSubFolder, MOREOPTIONS *pMO, INPUT_CONFIG *pIC)
{
	if(FolderExists(pPictureBaseFolder) && FolderExists(pPictureSubFolder)){
		size_t len = strlen(pPictureBaseFolder); // length excluding the null terminator.
		if(len < strlen(pPictureSubFolder)){
			int Match = 1;
			for(int i=0; i<len && Match; i++) // pPictureSubFolder must be a sub folder of pPictureBaseFolder.
				if(pPictureBaseFolder[i] != pPictureSubFolder[i])
					Match = 0;
			if(Match){
				InitJobData(0);
				strcpy_s(m_Path.PictureFolder, num_entries(m_Path.PictureFolder), pPictureBaseFolder);
				MAKEBLANKSLIST mbl={0};
				mbl.Ok = 1;
				mbl.pInputConfig = pIC;
				mbl.hDlg = AfxGetMainWnd()->m_hWnd;	// not used at the moment.
				mbl.pCAnn = this;
				int QuickLoad=1, IncludeSubDirs=1;
				g_DirListInterrupt = 0;

				mbl.JustCountFiles = 1;
				MakeDirList(QuickLoad, TRUE, mbl.hDlg, IncludeSubDirs, 0, pPictureSubFolder, "*.*", Static_MakeDirList_CreateBlankCsv, Static_MakeDirList_Interrupt, &mbl);
				mbl.JustCountFiles = 0;
				MakeDirList(QuickLoad, TRUE, mbl.hDlg, IncludeSubDirs, 0, pPictureSubFolder, "*.*", Static_MakeDirList_CreateBlankCsv, Static_MakeDirList_Interrupt, &mbl);
				if(mbl.Ok)
					return GetFirstFilenameGroup(pIC->SkipErrorChecks, pMO, pIC);
			}
			else{
				MsgBox2("error - folder containing the pictures must be a sub folder (at any level) of the base picture folder\r\n\r\n%s\r\n%s", pPictureBaseFolder, pPictureSubFolder);
			}
		}			
	}
	else MsgBox2("error - base picture folder doesnt exist \r\n\r\n%s", pPictureBaseFolder);
	return nullptr;
}

#define ADDSTR_MEMREPORT() \
	CString Str; Str.Format("## csv file(s) loaded - num csv lines %d\r\n", m_SrcLines.Num); \
	m_MemoryReport += Str;

FILENAMEGROUP *CAnnotations::LoadCsvFileList(char *pCsvFolder, char *pPictureFolder, MOREOPTIONS *pMO, INPUT_CONFIG *pIC)
{
	if(FolderExists(pCsvFolder)){
		InitJobData(1);
		strcpy_s(m_Path.PictureFolder, num_entries(m_Path.PictureFolder), pPictureFolder);
		int NumClassified, NumModel;
		if(LoadCsv_Folder(pCsvFolder, pIC, &NumClassified, &NumModel)){
			ADDSTR_MEMREPORT()
			if(pIC->MergeFiles){
				// merge model csv files, i.e, we have the same pictures annotated by different models, each csv file is generated from one model.
				if(NumClassified > 0)
					MsgBox2("error - merge files\r\n\r\n num model csv files: %d\r\n num classified csv files: %d\r\n\r\n classified csv files should not be merged, disable merge in options dialog\r\n BULLSHIT", NumModel, NumClassified);
				else if(SourceLineFilenameMerge())
					return GetFirstFilenameGroup(1, pMO, pIC);	// (1) - merge has already sorted list by filename so AssignFilenameCount can skip error checks.
			}
			else{
				// do not merge - used for classified csv files (and model csv files that do not need merging, i.e, if *not* using multiple models on the same pictures).
				return GetFirstFilenameGroup(0, pMO, pIC); // dont skip the filenamegroup error checking here because user might have wrongly included more than one csv file for the same picture folder (a merge should be used).
			}
		}
	}
	else MsgBox2("error - folder doesnt exist \r\n\r\n%s", pCsvFolder);
	return nullptr;
}

FILENAMEGROUP *CAnnotations::LoadCsvFile(char *pCsvFile, char *pPictureFolder, MOREOPTIONS *pMO, INPUT_CONFIG *pIC)
{
	if(FileExists(pCsvFile)){
		InitJobData(0);
		strcpy_s(m_Path.ModelCsv, num_entries(m_Path.ModelCsv), pCsvFile);
		strcpy_s(m_Path.PictureFolder, num_entries(m_Path.PictureFolder), pPictureFolder);
		int unused, AvgChrsPerLine;
		if(LoadCsv_File(1, pCsvFile, 0, 0, unused, AvgChrsPerLine)){
			ADDSTR_MEMREPORT()
			OutputDbgStr("\n LoadCsv_File SUCCESS - %s, lines %d, avg chrs per line %d, %s\r\n", (m_SrcLines.Format==CSVFORMAT::CSV_CLASSIFIED)?"CLASSIFIED":"MODEL", m_SrcLines.Num, AvgChrsPerLine, pCsvFile);
			return GetFirstFilenameGroup(pIC->SkipErrorChecks, pMO, pIC);
		}
	}
	else MsgBox2("error - path doesnt exist \r\n\r\n%s", pCsvFile);
	return nullptr;
}

void CAnnotations::ClearJob()
{
	InitJobData(0);
}

FILENAMEGROUP *CAnnotations::GetFirstFilenameGroup(int SkipErrorChecking, MOREOPTIONS *pMO, INPUT_CONFIG *pIC)
{
	if(AssignFilenameCount(SkipErrorChecking)){
		int Ok=1, SLMerge2=0;
		if(pIC->AnnotationProcessing){
			Ok = 0;
			if(AddSpaceForCroppedAnnotations())
				if(InitFilenameGroup_All(0)) // make groupings without adding classified lines, no need for classified lines at this stage.
					if(AnnotationProcessing(&m_AllGroups, &m_SrcLines, m_Path.PictureFolder, pMO, SLMerge2, pIC->DebugAnnotationProcessing))
						if(SLMerge2 ? SourceLineFilenameMerge() : 1)
							Ok = AssignFilenameCount(0); // filename error checking not required.
		}

		if(Ok){
			FILENAMEGROUP *pFnG = InitFilenameGroup_All(1);
			if(pFnG)
				if(SourceCsvFormattingOk())
					return pFnG;
		}
	}
	return nullptr;
}

/*char *RenameRelativePath(char *pPath)
{
	static char NewPath[MAX_PATH];
	int len = (int)strlen(pPath);
	if(len == 22){
		char *pB = &pPath[5];
		sprintf_s(NewPath, num_entries(NewPath), "anpr\\euro\\uk\\%s", pB);// anpr\BuckOV\000014.bmp
		return NewPath;
	}
	return pPath;
}*/

#define CSV_OUTPUT_FORMAT	0	// should always be zero, i.e. outputting as a classified csv file, the other options are for when i've wanted to update the format of existing model/platefinder csv files.
static int g_FnCount = 0, g_PrevFnGIndex = 0, g_AnnotationCount = 0;
int CAnnotations::WriteCsvLine(int FnGIndex, FILE *pFile, SOURCELINE *pLine, int Classified, int TimeStamp, int Header)
{
#if CSV_OUTPUT_FORMAT!=0
	static int warn=0; if(warn==0){ MsgBox("WARNING - WriteCsvLine doing csv conversion"); warn++; }
#endif
	// This is the classified csv format..
	static char Str[MAX_PATH+100] = {0};
	int Size;
	if(Header){
		g_PrevFnGIndex    = 0;
		g_FnCount         = 0;
		g_AnnotationCount = 0;
#if CSV_OUTPUT_FORMAT==0		// output as classified csv file
		Size = sprintf_s(Str, num_entries(Str), "%s,filename_count,classified,picture_code,colour_code,width,height,filename,box_x1,box_y1,box_x2,box_y2,class,model_code,debug_code,timestamp\n", VERSION_STR);
#elif CSV_OUTPUT_FORMAT==1		// output as platefinder model csv file
		Size = sprintf_s(Str, num_entries(Str), "ModelAnnotated_A,filename_count,picture_code,colour_code,width,height,filename,box_left,box_right,box_top,box_bottom,class\n");
#elif CSV_OUTPUT_FORMAT==2		// output as yolo(etc) model csv file
		Size = sprintf_s(Str, num_entries(Str), "ModelAnnotated_B,filename_count,picture_code,colour_code,width,height,filename,box_x1,box_y1,box_x2,box_y2,class,confidence,model_code\n");
#endif
		return (fwrite(Str, sizeof(char), strlen(Str), pFile) == Size);
	}
	else{
		if(FnGIndex!=g_PrevFnGIndex || FnGIndex==0){
			g_PrevFnGIndex = FnGIndex;		// filename group index on the classified data, remember some entries will be refused so count must be redone.
			g_FnCount++;
		}
		BBOX Bx = pLine->Box;
		if(BboxError(&Bx) == 0){
			g_AnnotationCount++;
#if CSV_OUTPUT_FORMAT==0		// output as classified csv file
			int ExcFilenameCt = 0;
			Size = sprintf_s(Str, num_entries(Str), "%d,%d,%d,%d,%d,%d,\"%s\",%f,%f,%f,%f,%s,%d,%d,%d\n", ExcFilenameCt?0:g_FnCount, Classified, pLine->Repeats.PictureCode, pLine->Repeats.ColourCode, pLine->Repeats.Width, pLine->Repeats.Height, pLine->Repeats.pfile_name, Bx.x1, Bx.y1, Bx.x2, Bx.y2, g_CIdM.GetLabel(pLine->ClassId), pLine->model_code, pLine->DebugCode, TimeStamp);
#elif CSV_OUTPUT_FORMAT==1		// output as platefinder model csv file
			if(!g_CIdM.Id_inCategory(pLine->ClassId, CATEGORY_PLATE))// this means we can convert a classified csv into platefinder csv, and also eliminate negatives, it all depends what you want to do.
				return 1;
			int lf,rh,tp,bt;
			BboxToBitmapCoordinates_Inc_Exc(lf, rh, tp, bt, pLine->Repeats.Width, pLine->Repeats.Height, &Bx, 1);
			Size = sprintf_s(Str, num_entries(Str), "%d,%d,%d,%d,%d,\"%s\",%d,%d,%d,%d,%s\n", g_FnCount, pLine->Repeats.PictureCode, pLine->Repeats.ColourCode, pLine->Repeats.Width, pLine->Repeats.Height, pLine->Repeats.pfile_name, lf, rh, tp, bt, g_CIdM.GetLabel(pLine->ClassId));
#elif CSV_OUTPUT_FORMAT==2		// output as yolo model csv file
			Size = sprintf_s(Str, num_entries(Str), "%d,%d,%d,%d,%d,\"%s\",%f,%f,%f,%f,%s,%d,%d\n", g_FnCount, pLine->Repeats.PictureCode, pLine->Repeats.ColourCode, pLine->Repeats.Width, pLine->Repeats.Height, pLine->Repeats.pfile_name, Bx.x1, Bx.y1, Bx.x2, Bx.y2, g_CIdM.GetLabel(pLine->ClassId), pLine->Confidence, pLine->model_code);
#endif
			return (fwrite(Str, sizeof(char), strlen(Str), pFile) == Size);
		}
		else MsgBox2("WriteCsvLine - BboxError \r\n %f, %f, %f, %f", Bx.x1, Bx.y1, Bx.x2, Bx.y2);
	}
	return 0;
}

void CAnnotations::DeleteOrMove(FILENAMEGROUP *pFnG, int Delete)
{
	char Path[MAX_PATH];
	GetPicturePath(pFnG, Path, MAX_PATH);
	if(Delete){
		DeleteFile(Path);
	}
	else{
		// move
		char Folder[MAX_PATH], Filename[MAX_PATH], newPath[MAX_PATH];
		FullPathToFolderAndFilename(Path, Folder, MAX_PATH, Filename, MAX_PATH);
		sprintf_s(newPath, num_entries(newPath), "%srejects", Folder);
		int Ok = 0;
			 if(FolderExists(newPath)) Ok = 1;
		else if(CreateDirectory(newPath, NULL)) Ok = 1;
		if(Ok){
			sprintf_s(newPath, num_entries(newPath), "%s\\%s", newPath, Filename);
			MoveFile(Path, newPath);
		}
	}
}

#define WRITE_ORIGINAL_SRC_LINES	\
	/* write to file the original source data */	\
	for(int b=0; b<pFnG->NumSourceLines && Ok; b++)	\
		Ok &= WriteCsvLine(i, pFile, &pFnG->pSourceLines[b], pFnG->Classified, pFnG->Timestamp, 0);

#define EXCLUDED_MSG(val) val?"excluded":"included"
int CAnnotations::SaveClassifiedCsvFile(int SaveAsDlg, int Exclude_NotClassified, int Exclude_Refused, int DeleteRefusedPictures)
{
	if(m_AllGroups.Num > 0){
		char *pPath = GetClassifiedCsvPath(SaveAsDlg);
		if(pPath){
			int   Ok = 0;
			FILE *pFile = nullptr;
			fopen_s(&pFile, pPath, "wt");
			if(pFile){
				Ok = WriteCsvLine(0, pFile, nullptr, 0, 0, 1);
				FILENAMEGROUP *pFnG = m_AllGroups.pFNG;
				for(int i=0; i<m_AllGroups.Num && Ok; i++, pFnG++){
					if(pFnG->Classified == 0){
						if(Exclude_NotClassified == 0)
							WRITE_ORIGINAL_SRC_LINES
					}
					else if(pFnG->Classified == CLASSIFIED_REFUSE){
						if(Exclude_Refused){
							if(DeleteRefusedPictures)
								DeleteOrMove(pFnG, 0); // delete the pics.
						}
						else
							WRITE_ORIGINAL_SRC_LINES
					}
					else if(pFnG->Classified == CLASSIFIED_ACCEPT ||
						    pFnG->Classified == CLASSIFIED_SPECIAL )
					{
						SOURCELINE Copy={0};
						// from SOURCELINE get the data that is the same for every annotation in this filename group (also this data doesnt exist in CLASSIFIEDLINE)...
						Copy.Repeats = pFnG->pSourceLines[0].Repeats;
						for(int b=0; b<pFnG->NumClsfdLines && Ok; b++){
							// ...and now add the classified data to it.
							SourceClassifiedDataExchange(0, &Copy, &pFnG->pClsfdLines[b]);
							Ok &= WriteCsvLine(i, pFile, &Copy, pFnG->Classified, pFnG->Timestamp, 0);
						}
					}
					else{
						Ok = 0;
						MsgBox2("error - SaveClassifiedCsvFile - classified status (%d)", pFnG->Classified);
					}
				}
				fclose(pFile);

				if(Ok){
					if(!m_DoingBackup)
						MsgBox2("Classified csv successfully saved to..\r\n\r\n%s\r\n\r\n'not classified' entries are - [%s]\r\n'classified refuse' entries are - [%s]\r\n\r\ncsv line count : %d\r\nfilename count : %d", 
							pPath, EXCLUDED_MSG(Exclude_NotClassified), EXCLUDED_MSG(Exclude_Refused), g_AnnotationCount, g_FnCount);
					return 1;	// Save successful.
				}
			}
			if(!Ok)
				MsgBox2("SaveClassifiedCsvFile - FAILED..\r\n\r\n%s\r\n code : bkup %d, save_as %d, num %d, ptr %d", pPath, m_DoingBackup, SaveAsDlg, m_AllGroups.Num, pFile);
		}
	}
	return 0;	// No save done. Either user cancelled the 'save as' dialog, or an error occured and msg box popped up.
}

char *CAnnotations::GetClassifiedCsvPath()
{
	// see comments in GetClassifiedCsvPath for when this path is set.
	if(m_Path.ClassifiedCsv[0] == 0)
		return nullptr;
	return m_Path.ClassifiedCsv;
}

char *CAnnotations::GetDefaultFilename(char *pFilename, int NumEntries)
{
	// make default filename if we can, but we cant always - when user loads multiple csv's both ClassifiedCsv and ModelCsv will be null.
	memset(pFilename, 0, NumEntries);
	if(m_Path.ClassifiedCsv[0] != 0) strcpy_s(pFilename, NumEntries, PointerToFilename(m_Path.ClassifiedCsv));
	else if(m_Path.ModelCsv[0] != 0) strcpy_s(pFilename, NumEntries, PointerToFilename(m_Path.ModelCsv));
	size_t Len = strlen(pFilename);
	if(Len > 0){
		if(Len>4 && pFilename[Len-4]=='.'){
			pFilename[Len-4] = 0;	// put null terminator on the '.'
			if(m_Path.ClassifiedCsv[0] == 0)
				sprintf_s(pFilename, NumEntries, "%s_classified", pFilename);
		}
		else{
			MsgBox2("error - GetDefaultFilename unexpected file extension\r\n%s", pFilename);
			memset(pFilename, 0, NumEntries);
		}
	}
	return pFilename;
}

#define NUM_BACKUPS	10
char *CAnnotations::GetClassifiedCsvPath(int SaveAsDlg)
{
	// function makes the path to save the classified csv to.
	static char Path[MAX_PATH];
	if(m_DoingBackup){
		if(m_Backup.Folder[0] != 0){
			int Ok = 0;
				 if(FolderExists(m_Backup.Folder)) Ok = 1;
			else if(CreateDirectory(m_Backup.Folder, NULL)) Ok = 1;
			if(Ok){
				static int Count = 0;
				int BackupIndex = (Count % NUM_BACKUPS);
				// There will be a maximum of NUM_BACKUPS backups stored in the backup folder.
				// When the backup folder is full new backups will overwrite old backups.
				// Therefore to find the last backup you must look at the 'date modified' file attribute.

				// We could add model name to the filename, so there would be NUM_BACKUPS backups per model name, but I think thats unnecessary.
				sprintf_s(Path, num_entries(Path), "%s\\%d.csv", m_Backup.Folder, BackupIndex);
				OutputDbgStr("\n Doing backup to - %s", Path);
				Count++;
				return Path;
			}
			else MsgBox2("error - couldnt create backup folder");
		}
		else MsgBox2("error - missing backup folder path");
	}
	else{
		if(SaveAsDlg){
			// Create a suggested/default filename then open the save as dialog window.
			char defFilename[MAX_PATH];
			CFileDialog FileDlg(FALSE, "csv", GetDefaultFilename(defFilename, num_entries(defFilename)), OFN_OVERWRITEPROMPT, CSV_FILE_FILTERS);
			if(FileDlg.DoModal() == IDOK){
				sprintf_s(Path, num_entries(Path), "%s\\%s", (const CHAR*)FileDlg.GetFolderPath(), (const CHAR*)FileDlg.GetFileName());
				if(m_Path.ClassifiedCsv[0] == 0)
					// If user didnt open a classified csv, and if user hasnt yet saved, then we dont yet have a classified csv path. Here set the classified
					// csv path only when its first brought into existance ...because a 'save as' shouldnt change the original 'save' destination.
					strcpy_s(m_Path.ClassifiedCsv, num_entries(m_Path.ClassifiedCsv), Path);
				return Path;
			}
		}
		else{
			if(m_Path.ClassifiedCsv[0] != 0)
				// The classified csv already exists, and user has chosen to overwrite this file.
				return m_Path.ClassifiedCsv;
			MsgBox2("error - 'save' is not possible as the classified csv file doesnt yet exist, use 'save as' instead.");
		}
	}
	return nullptr;
}

void CAnnotations::ExportForYOLO(MOREOPTIONS *pMO)
{
	CYoloExportDlg m_Dlg;
	m_Dlg.SetParams(m_Path.PictureFolder, m_AllGroups.Num, &m_GroupsFltr, (float)pMO->General.TargetWidth, (float)pMO->General.TargetHeight);
	m_Dlg.DoModal();
}

void CAnnotations::StatisticsPopup(MOREOPTIONS *pMO)
{
	// make MOREOPTIONS global and give it a sensible name, this passing it is bit silly.
	CYoloExportDlg m_Dlg;
	m_Dlg.SetParams("", m_AllGroups.Num, &m_GroupsFltr, (float)pMO->General.TargetWidth, (float)pMO->General.TargetHeight);
	m_Dlg.StatisticsPopup(0);
	// not actually using the CYoloExportDlg dialog here.
}

int CAnnotations::GetFNGIndex()
{
	return m_GroupsFltr.CurrentIdx;
}
int CAnnotations::GetFNGCount(int *pNum)
{
	*pNum = m_AllGroups.Num;
	return m_GroupsFltr.Num;
}

int CAnnotations::AllocateFilenameGroups(int Number, int WithClassifiedLine)
{
	// allocate - FILENAMEGROUP
	if(Number > m_AllGroups.Max){
		OutputDbgStr("\n AllocateFilenameGroups - FILENAMEGROUP\n Number %d, m_AllGroups.Max %d\n", Number, m_AllGroups.Max);
		m_AllGroups.Max = Number;
		FILENAMEGROUP *pOldBuf = m_AllGroups.pFNG;
		if((m_AllGroups.pFNG = (FILENAMEGROUP*)realloc((void*)m_AllGroups.pFNG, sizeof(FILENAMEGROUP) * m_AllGroups.Max)) == 0){
			// realloc failed.
			if(pOldBuf)
				free(pOldBuf);
			m_AllGroups.Initialise();
			MsgBox2("error - AllocateFilenameGroups - m_AllGroups.Max realloc (%d)", m_AllGroups.Max);
			return 0;
		}

		m_GroupsFltr.Max = m_AllGroups.Max;
		FILENAMEGROUP **ppOldBuf = m_GroupsFltr.ppFNG;
		if((m_GroupsFltr.ppFNG = (FILENAMEGROUP**)realloc((void*)m_GroupsFltr.ppFNG, sizeof(FILENAMEGROUP*) * m_GroupsFltr.Max)) == 0){
			// realloc failed.
			if(ppOldBuf)
				free(ppOldBuf);
			m_GroupsFltr.Initialise();
			MsgBox2("error - AllocateFilenameGroups - m_GroupsFltr.Max realloc (%d)", m_GroupsFltr.Max);
			return 0;
		}
	}

	if(WithClassifiedLine){
		// allocate - CLASSIFIEDLINE
		Number = m_SrcLines.Num + 1 + (Number * m_ClsfdLines.NumSpare);	// NumSpare - if we're classifying this is space for new annotations added in the GUI (per filename group).
		if(Number > m_ClsfdLines.Max){
			OutputDbgStr("\n AllocateFilenameGroups - CLASSIFIEDLINE\n Number %d, m_ClsfdLines.Max %d, m_SrcLines.Num %d\n", Number, m_ClsfdLines.Max, m_SrcLines.Num);
			m_ClsfdLines.Max = Number;
			CLASSIFIEDLINE *pOldBuf = m_ClsfdLines.pLines;
			if((m_ClsfdLines.pLines = (CLASSIFIEDLINE*)realloc((void*)m_ClsfdLines.pLines, sizeof(CLASSIFIEDLINE) * m_ClsfdLines.Max)) == 0){
				// realloc failed.
				if(pOldBuf)
					free(pOldBuf);
				m_ClsfdLines.Initialise();
				MsgBox2("error - AllocateFilenameGroups - m_ClsfdLines.Max realloc (%d)", m_ClsfdLines.Max);
				return 0;
			}
		}
	}

	return ( m_AllGroups.pFNG!=nullptr    && 
			 m_GroupsFltr.ppFNG!=nullptr  && 
			(m_ClsfdLines.pLines!=nullptr || !WithClassifiedLine));
}

int CAnnotations::AddClassifiedLine(FILENAMEGROUP *pFnG, BBOX *pbb)
{
	if(pFnG->NumClsfdLines < pFnG->MaxClsfdLines){
		CLASSIFIEDLINE *pCL = &pFnG->pClsfdLines[pFnG->NumClsfdLines++];
		memset(pCL, 0, sizeof(CLASSIFIEDLINE));
		pCL->ClassId = g_CIdM.GetPreferredId();
		pCL->Model_Code = MODEL_BYHAND;
		if(pbb){
			// box position is already defined.
			pCL->box = *pbb;
		}
		else{
			// box position not defined, make something up.
			float inc = 0.02f, lmt = 0.4f;
			float shf = inc * (float)(pFnG->NumClsfdLines-1);
			// put newly added box in a slightly different position each time.
			if(shf <= lmt){
				pCL->box.x1 = 0.05f + shf;
				pCL->box.y1 = 0.05f + shf;
				pCL->box.x2 = 0.55f + shf;
				pCL->box.y2 = 0.55f + shf;
			}
			else{
				shf -= lmt + inc;
				if(shf <= lmt){
					pCL->box.x1 = 0.05f + shf;
					pCL->box.y1 = 0.45f - shf;
					pCL->box.x2 = 0.55f + shf;
					pCL->box.y2 = 0.95f - shf;
				}
			}

			if(pCL->box.x1-pCL->box.x2 == 0.0f){
				// above failed.
				pCL->box.x1  = 0.30f;
				pCL->box.y1  = 0.30f;
				pCL->box.x2  = 0.70f;
				pCL->box.y2  = 0.70f;
			}
		}
		return 1;
	}
	MsgBox2("error - AddClassifiedLine \r\n MaxClsfdLines %d\r\n m_ClsfdLines.NumSpare %d\r\n\r\n buffer full, see comments in code", pFnG->MaxClsfdLines, m_ClsfdLines.NumSpare);
	// this error - classifiedline array code needs redesign, this error is unacceptable, i would fix this if i wanted to spend any more time on time program.
	// ..but for now you can just increase value of NUM_SPARE_ANNOTATIONS.
	return 0;
}

void CAnnotations::SourceClassifiedDataExchange(int source_to_classified, SOURCELINE *pSrc, CLASSIFIEDLINE *pClsf)
{
	if(source_to_classified){
		pClsf->ClassId    = pSrc->ClassId;
		pClsf->DebugCode  = pSrc->DebugCode;// debug code is not actually classified data as such, but I want it included in the classified csv file so it must be copied into CLASSIFIEDLINE.
		pClsf->Model_Code = pSrc->model_code;
		pClsf->box        = pSrc->Box;
	}
	else{
		// classified to source
		pSrc->ClassId    = pClsf->ClassId;
		pSrc->DebugCode  = pClsf->DebugCode;
		pSrc->model_code = pClsf->Model_Code;
		pSrc->Box        = pClsf->box;
	}
}

void CAnnotations::ResetClassifiedLines(FILENAMEGROUP *pFnG)
{
	// copy the original csv lines into the classified lines.
	for(int i=0; i<pFnG->NumSourceLines; i++){
		memset(&pFnG->pClsfdLines[i], 0, sizeof(CLASSIFIEDLINE));
		SourceClassifiedDataExchange(1, &pFnG->pSourceLines[i], &pFnG->pClsfdLines[i]);
	}
	pFnG->NumClsfdLines = pFnG->NumSourceLines;
	pFnG->Classified = pFnG->pPicInfo->Pic_classified;
	pFnG->Timestamp = pFnG->pPicInfo->Time_Stamp;
}

int CAnnotations::InitClassifiedLines(FILENAMEGROUP *pFnG)
{
	// classified lines are initially a copy of the original csv lines. Plus space for expansion, i.e. m_ClsfdLines.NumSpare.
	int NumLines = pFnG->NumSourceLines + m_ClsfdLines.NumSpare;
	if(m_ClsfdLines.Num+NumLines < m_ClsfdLines.Max){
		// getting the memory for the classified lines.
		pFnG->MaxClsfdLines = NumLines;
		pFnG->pClsfdLines   = &m_ClsfdLines.pLines[m_ClsfdLines.Num];
		m_ClsfdLines.Num   += NumLines;
		// initialise the classified lines.
		ResetClassifiedLines(pFnG);
		return 1;
	}
	MsgBox2("error - InitClassifiedLines\r\n m_ClsfdLines.Max %d, m_ClsfdLines.Num %d + %d", m_ClsfdLines.Max, m_ClsfdLines.Num, NumLines);
	return 0;
}

void CAnnotations::RandomiseFilenameGroups()
{
	int   Seed = 237645;
	float dNumFNG = (float)m_GroupsFltr.Num;
	float MaxRandom = (float)RAND_MAX;
	srand(Seed);
	for(int i=0; i<m_GroupsFltr.Num; i++){
		float randomNormalised = (float)rand() / MaxRandom;
		int   randomIndex      = (int)(dNumFNG * randomNormalised);
		if(randomIndex < 0) randomIndex = 0;
		if(randomIndex >= m_GroupsFltr.Num) randomIndex = m_GroupsFltr.Num - 1;
		if(randomIndex != i){
			// swap [i] with [randomIndex].
			FILENAMEGROUP *pFNG = m_GroupsFltr.ppFNG[i];
			m_GroupsFltr.ppFNG[i] = m_GroupsFltr.ppFNG[randomIndex];
			m_GroupsFltr.ppFNG[randomIndex] = pFNG;
		}
    }
}

static int g_SortClass=0, g_AreaHeightWidth=0;
int IncludeInSort(int BoxClassId)
{
	if(g_SortClass >= 0){
		if(BoxClassId == g_SortClass)
			return 1; // sorting based on g_SortClass only.
	}
	else if(BoxClassId >= 0)
		return 1; // sorting based on any class.
	return 0;
}

int GetBoxArea_ProportionOfPic(FILENAMEGROUP *pfng, int Min_else_Max)
{
	float Extreme = Min_else_Max ? 1.0f : 0.0f;	// negative pictures will end up with this value, and sorting will put them at the end of the list.
	float Wd, Ht, Val=0.0f;
	for(int i=0; i<pfng->NumClsfdLines; i++){
		if(IncludeInSort(pfng->pClsfdLines[i].ClassId)){
			BBOX *pBox = &pfng->pClsfdLines[i].box;
			Wd = pBox->x2-pBox->x1;
			Ht = pBox->y2-pBox->y1;
				 if(g_AreaHeightWidth == 0) Val = Ht * Wd;
			else if(g_AreaHeightWidth == 1) Val = Ht;
			else if(g_AreaHeightWidth == 2) Val = Wd;
			if(Min_else_Max){
				if(Val < Extreme) Extreme = Val;
			}
			else{
				if(Val > Extreme) Extreme = Val;
			}
		}
	}
	return (int)(Extreme * 100000.0f); // scale up cos we're converting to int.
}

int GetBoxArea_PixelsOnOriginalPic(FILENAMEGROUP *pfng, int Min_else_Max)
{
	int Extreme = Min_else_Max ? (10000*10000) : 0;	// negative pictures will end up with this value, and sorting will put them at the end of the list.
	int Wd, Ht, Val=0;
	for(int i=0; i<pfng->NumClsfdLines; i++){
		if(IncludeInSort(pfng->pClsfdLines[i].ClassId)){
			BboxToPixelWdHt_Inc_Exc(Wd, Ht, pfng->pPicInfo->Width, pfng->pPicInfo->Height, &pfng->pClsfdLines[i].box);
				 if(g_AreaHeightWidth == 0) Val = Ht * Wd;
			else if(g_AreaHeightWidth == 1) Val = Ht;
			else if(g_AreaHeightWidth == 2) Val = Wd;
			if(Min_else_Max){
				if(Val < Extreme) Extreme = Val;
			}
			else{
				if(Val > Extreme) Extreme = Val;
			}
		}
	}
	return Extreme;
}

static int CompareBoxArea_ProportionOfPic_smallest(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (GetBoxArea_ProportionOfPic(pfg1, 1) - GetBoxArea_ProportionOfPic(pfg2, 1));
}
static int CompareBoxArea_PixelsOnOriginal_smallest(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (GetBoxArea_PixelsOnOriginalPic(pfg1, 1) - GetBoxArea_PixelsOnOriginalPic(pfg2, 1));
}

static int CompareBoxArea_ProportionOfPic_largest(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (GetBoxArea_ProportionOfPic(pfg2, 0) - GetBoxArea_ProportionOfPic(pfg1, 0));
}
static int CompareBoxArea_PixelsOnOriginal_largest(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (GetBoxArea_PixelsOnOriginalPic(pfg2, 0) - GetBoxArea_PixelsOnOriginalPic(pfg1, 0));
}

static int CompareTimestamp(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (int)(pfg2->Timestamp - pfg1->Timestamp);
}
static int ComparePictureArea(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	return (int)((pfg1->pPicInfo->Width*pfg1->pPicInfo->Height) - (pfg2->pPicInfo->Width*pfg2->pPicInfo->Height));
}
static int CompareBBoxCount(const void *arg1, const void *arg2)
{
	FILENAMEGROUP *pfg1 = *(FILENAMEGROUP**)arg1;	// pointer syntax! it makes sense but i had to look it up on the internet.
	FILENAMEGROUP *pfg2 = *(FILENAMEGROUP**)arg2;
	int Num_1 = (pfg1->NumClsfdLines==1 && pfg1->pClsfdLines[0].ClassId<0) ? 0 : pfg1->NumClsfdLines;
	int Num_2 = (pfg2->NumClsfdLines==1 && pfg2->pClsfdLines[0].ClassId<0) ? 0 : pfg2->NumClsfdLines;
	return (Num_1 - Num_2);
}

static void box_sort_warning(int warning)
{
	if(warning == 1)
		MsgBox2("Only boxes from class [ %s ] are used for the sort\r\n\r\n g_AreaHeightWidth : %d", g_CIdM.GetLabel(g_SortClass), g_AreaHeightWidth);
	else 
		MsgBox2("No class id specified, sort will include all class ids. To sort based on a single class do the following...\
\r\n\r\nIn the picture filter section, enable class ids filter, and set radio box to 'all' classes.\r\nThe first class in the csv list will be used for the sort.\
\r\nAlso it would be illogical not to apply the filter before doing the sort, although you dont have to.\r\n\r\n g_AreaHeightWidth : %d", g_AreaHeightWidth);
}

FILENAMEGROUP *CAnnotations::ApplySort(int PicSortCode, int AreaHeightWidth, PICTUREFILTERS *pPicFilters)
{
	if(m_GroupsFltr.Num > 0){
		int warning = 0;
		g_AreaHeightWidth = AreaHeightWidth;
		g_SortClass = -1;
		if(pPicFilters->ClassIDs.Enable && pPicFilters->ClassIDs.AnyValues==0 && pPicFilters->ClassIDs.NumValues>0){
			g_SortClass = pPicFilters->ClassIDs.Values[0];
			warning = 1;
		}

		if(PicSortCode == PICSORT::PICSORT_RANDOMISE)
			RandomiseFilenameGroups();
		else if(PicSortCode == PICSORT::PICSORT_BBOX_COUNT){
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareBBoxCount);
		}
		else if(PicSortCode == PICSORT::PICSORT_TIMESTAMP){
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareTimestamp);
		}
		else if(PicSortCode == PICSORT::PICSORT_PICTURE_AREA){
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), ComparePictureArea);
		}
		else if(PicSortCode == PICSORT::PICSORT_BBOX_PROPORTION_SML){
			box_sort_warning(warning);
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareBoxArea_ProportionOfPic_smallest);
		}
		else if(PicSortCode == PICSORT::PICSORT_BBOX_PROPORTION_LRG){
			box_sort_warning(warning);
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareBoxArea_ProportionOfPic_largest);
		}
		else if(PicSortCode == PICSORT::PICSORT_BBOX_PIXEL_SML){
			box_sort_warning(warning);
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareBoxArea_PixelsOnOriginal_smallest);
		}
		else if(PicSortCode == PICSORT::PICSORT_BBOX_PIXEL_LRG){
			box_sort_warning(warning);
			qsort(m_GroupsFltr.ppFNG, m_GroupsFltr.Num, sizeof(FILENAMEGROUP*), CompareBoxArea_PixelsOnOriginal_largest);
		}
	}
	return GetAtIndex(0);
	// how do we un-sort the list? ..press the filters refresh button to remake the list.
}

FILENAMEGROUP *CAnnotations::ApplyFilters(PICTUREFILTERS *pPicFilters)
{
	m_pPicFilters = pPicFilters;	// will be null if we've not using filters.
	return InitFilenameGroup_Filtered();
}

int CAnnotations::Keep_ClassifiedStatus(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	if(pPF->Remove.NotClassified)
		if(pFNG->Classified == 0)
			return 0;
	if(pPF->Remove.ClassifiedAccept)
		if(pFNG->Classified == CLASSIFIED_ACCEPT)
			return 0;
	if(pPF->Remove.ClassifiedRefuse)
		if(pFNG->Classified == CLASSIFIED_REFUSE)
			return 0;
	if(pPF->Remove.ClassifiedSpecial)
		if(pFNG->Classified == CLASSIFIED_SPECIAL)
			return 0;
	return 1;
}

int CAnnotations::Keep_PicAspectRatio(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	// picture aspect ratio filter
	if(pPF->Ratio.Enable){
		float ratio, y = (float)pFNG->pPicInfo->Height, x = (float)pFNG->pPicInfo->Width;
		if(x > 0.0f){	// aspect ratio from - y100/X to Y/x100
			ratio = y / x;
			if(pPF->Ratio.X > 0){
				float maxRatio = 100.0f / (float)pPF->Ratio.X;
				if(ratio > maxRatio)
					return 0;
			}
			if(pPF->Ratio.Y > 0){
				float minRatio = (float)pPF->Ratio.Y / 100.0f;
				if(ratio < minRatio)
					return 0;
			}
		}
	}
	return 1;
}

int CAnnotations::Keep_ClassIDs(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	// class id filter
	if(pPF->ClassIDs.Enable){
		// keep a picture if it includes any of the class ids in the CSVENTRY list.
		int i, x, NumMatch=0;
		CSVENTRY *pCsvEnt = &pPF->ClassIDs;
		for(x=0; x<pCsvEnt->NumValues; x++){
			int ID = pCsvEnt->Values[x];
			for(i=0; i<pFNG->NumClsfdLines; i++){
				if(pFNG->pClsfdLines[i].ClassId == ID){
					NumMatch++;
					break; // must break otherwise the same 'ID' could get matched by different annotations (and increment NumMatch), and thats not the idea.
				}
			}
		}
		if(pPF->ClassIDs.AnyValues){
			// requirement - match any pCsvEnt->Values, i.e, pictures with any of the CSVENTRY ClassIDs.
			if(NumMatch == 0)
				return 0;
		}
		else{
			// requirement - match all pCsvEnt->Values, i.e, pictures with all the CSVENTRY ClassIDs.
			if(NumMatch != pCsvEnt->NumValues)
				return 0;
		}
	}
	return 1;
}

int CAnnotations::Keep_DebugCode(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	// debug code - reject pic if it does not include the debug code.
	if(pPF->Debug_Codes.Enable){
		// COULD USE THE sourceline or classifiedline. Seeing as this is only for debugging FixAndCropPicture code i'll use sourceline.
		int i, x, NumMatch=0;
		CSVENTRY *pCsvEnt = &pPF->Debug_Codes;
		for(x=0; x<pCsvEnt->NumValues; x++){
			int Code = (1<<pCsvEnt->Values[x]);
		/*	for(i=0; i<pFNG->NumSourceLines; i++){ // source line ?
				if(pFNG->pSourceLines[i].DebugCode & Code){	*/
			for(i=0; i<pFNG->NumClsfdLines; i++){ // classified line
				if(pFNG->pClsfdLines[i].DebugCode & Code){
					NumMatch++;
					break; // must break otherwise the same 'Code' could get matched by different annotations (and increment NumMatch), whereas we must match all 'Code's.
				}
			}
		}
		if(NumMatch != pCsvEnt->NumValues)
			return 0;
	}
	return 1;
}

static int BBoxIdentical(BBOX *pA, BBOX *pB)
{
	float tol = 0.001f; // identical to 3 decimal points.
	return ( fabs(pA->x1-pB->x1)<tol && fabs(pA->x2-pB->x2)<tol &&
			 fabs(pA->y1-pB->y1)<tol && fabs(pA->y2-pB->y2)<tol );
}

int CAnnotations::Keep_MultiProbabilities(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	if(pPF->Remove.SingleProbability){
		for(int i=0; i<pFNG->NumClsfdLines; i++)
			for(int k=0; k<pFNG->NumClsfdLines; k++)
				if(i != k)
					if(BBoxIdentical(&pFNG->pClsfdLines[i].box, &pFNG->pClsfdLines[k].box))
						return 1;
		return 0; // remove, picture has only single probability objects.
	}
	return 1;
}

int CAnnotations::Keep_ColourStatus(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	if(pPF->Remove.ColourUnknown)
		if(pFNG->pPicInfo->ColourCode == COLOURCODE::COLOUR_UNDEFINED)
			return 0;
	if(pPF->Remove.ColourYes)
		if(pFNG->pPicInfo->ColourCode == COLOURCODE::COLOUR_YES)
			return 0;
	if(pPF->Remove.ColourNo)
		if(pFNG->pPicInfo->ColourCode == COLOURCODE::COLOUR_NO)
			return 0;
	return 1;
}

int CAnnotations::Keep_NegativePositive(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	if(pPF->Remove.Negatives)
		for(int i=0; i<pFNG->NumClsfdLines; i++)
			if(pFNG->pClsfdLines[i].ClassId < 0)
				return 0;
	if(pPF->Remove.Positives)
		for(int i=0; i<pFNG->NumClsfdLines; i++)
			if(pFNG->pClsfdLines[i].ClassId >= 0)
				return 0;
	return 1;
}

int CAnnotations::Keep_PartialFilename(FILENAMEGROUP *pFNG, PICTUREFILTERS *pPF)
{
	if(pPF->PartialFileName.Enable){
		char *pMatch;
		pMatch = strstr(pFNG->pPicInfo->pfile_name, pPF->PartialFileName.Str);
		if(!pMatch)
			return 0;
	}
	return 1;
}

int CAnnotations::KeepFilenameGroup(FILENAMEGROUP *pFNG)
{
	if(m_pPicFilters){
		// REJECT PICTURES according to the filters that are enabled.
		PICTUREFILTERS *pPF = m_pPicFilters;
		//-----------------------------------
		// reject the picture if the min/max bbox count fails. todo - exclude negatives.
		if(pPF->BBoxCount.Enable)
			if(pFNG->NumClsfdLines<pPF->BBoxCount.Min || pFNG->NumClsfdLines>pPF->BBoxCount.Max)
				return 0;
		//-----------------------------------
		// min picture size filter
		if(pPF->MinDims.Enable)
			if(pFNG->pPicInfo->Width<pPF->MinDims.Wd || pFNG->pPicInfo->Height<pPF->MinDims.Ht)
				return 0;
		//-----------------------------------
		if(!Keep_NegativePositive(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_ClassIDs(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_DebugCode(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_ColourStatus(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_ClassifiedStatus(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_PicAspectRatio(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_PartialFilename(pFNG, m_pPicFilters))
			return 0;
		if(!Keep_MultiProbabilities(pFNG, m_pPicFilters))
			return 0;
	}
	return 1;	// no filtering, keep it.
}

FILENAMEGROUP *CAnnotations::InitFilenameGroup_Filtered()
{
	int Inverse = (m_pPicFilters && m_pPicFilters->Inverse) ? 1 : 0;

	// load m_GroupsFltr with the wanted pictures.
	m_GroupsFltr.Num = 0;
	m_GroupsFltr.CurrentIdx = 0;
	if(m_GroupsFltr.Max >= m_AllGroups.Max)
		for(int i=0; i<m_AllGroups.Num; i++)
			// go through the list and copy entries into the filtered list if they pass the filter tests in KeepFilenameGroup
			if(KeepFilenameGroup(&m_AllGroups.pFNG[i]) != Inverse)
				m_GroupsFltr.ppFNG[m_GroupsFltr.Num++] = &m_AllGroups.pFNG[i];

	m_GroupsFltr.SkipIncrements = max(m_GroupsFltr.Num/100, 1);
	if(m_GroupsFltr.Num > 0)
		return m_GroupsFltr.ppFNG[m_GroupsFltr.CurrentIdx];
	return nullptr;
}

FILENAMEGROUP *CAnnotations::InitFilenameGroup_All(int WithClassifiedLine)
{
	m_AllGroups.Reset();
	m_GroupsFltr.Reset();
	m_ClsfdLines.Reset();
	if(m_SrcLines.NumFilenames > 0){
		if(AllocateFilenameGroups(m_SrcLines.NumFilenames, WithClassifiedLine)){
			// One bounding box (or call it an annotation) per line in the csv, and a filename(i.e. bitmap) can comprise multiple 
			// bboxes. So here we create a list of these 'filename groups'.
			int i = 0, Ok = 1;
			while(i<m_SrcLines.Num && Ok){
				if(m_AllGroups.Num >= m_AllGroups.Max){
					MsgBox2("error - InitFilenameGroup_All\r\n m_AllGroups.Max %d", m_AllGroups.Max);
					Ok = 0;
				}
				else{
					FILENAMEGROUP *pG = &m_AllGroups.pFNG[m_AllGroups.Num];
					memset(pG, 0, sizeof(FILENAMEGROUP));
					pG->pSourceLines   = &m_SrcLines.pCsvLns[i];
					pG->pPicInfo       = &pG->pSourceLines->Repeats;
					pG->Classified     = pG->pPicInfo->Pic_classified;
					pG->Timestamp      = pG->pPicInfo->Time_Stamp;
					pG->NumSourceLines = 1;
					i++;
					while(i < m_SrcLines.Num){
						if(m_SrcLines.pCsvLns[i].FilenameCount != m_SrcLines.pCsvLns[i-1].FilenameCount)
							break;
						pG->NumSourceLines++;
						i++;
					}
					if(WithClassifiedLine)
						Ok &= InitClassifiedLines(pG);
					m_AllGroups.Num++;
				}
			}
			if(Ok)
				return InitFilenameGroup_Filtered();
		}
		else MsgBox2("error - InitFilenameGroup_All\r\n AllocateFilenameGroups");
	}
	return nullptr;
}

FILENAMEGROUP *CAnnotations::GetAtIndex(int Index)
{
	if(Index>=0 && Index<m_GroupsFltr.Num){
		m_GroupsFltr.CurrentIdx = Index;
		return m_GroupsFltr.ppFNG[m_GroupsFltr.CurrentIdx];
	}
	return nullptr;	
}
FILENAMEGROUP *CAnnotations::GetAtFilename(char *pFilename)
{
	if(strlen(pFilename) > 0){
		for(int i=0; i<m_GroupsFltr.Num; i++){
			if(strcmp(m_GroupsFltr.ppFNG[i]->pPicInfo->pfile_name, pFilename) == 0){
				m_GroupsFltr.CurrentIdx = i;
				return m_GroupsFltr.ppFNG[i];
			}
		}
	}
	return nullptr;
}
FILENAMEGROUP *CAnnotations::GetAtFirstOfType(int Classified, int Forward)
{
	if(Forward){
		for(int i=0; i<m_GroupsFltr.Num; i++) if(Classified ? m_GroupsFltr.ppFNG[i]->Classified : !m_GroupsFltr.ppFNG[i]->Classified){
			m_GroupsFltr.CurrentIdx = i;
			return m_GroupsFltr.ppFNG[i];
		}
	}
	else{
		for(int i=m_GroupsFltr.Num-1; i>=0; i--) if(Classified ? m_GroupsFltr.ppFNG[i]->Classified : !m_GroupsFltr.ppFNG[i]->Classified){
			m_GroupsFltr.CurrentIdx = i;
			return m_GroupsFltr.ppFNG[i];
		}
	}
	return nullptr;
}
FILENAMEGROUP *CAnnotations::GetLastLatestTimestamp()
{
	int TS, maxTS = 0, lastIdx = 0;
	for(int i=0; i<m_GroupsFltr.Num; i++){
		TS = m_GroupsFltr.ppFNG[i]->Timestamp;
		if(TS!=0 && TS>=maxTS){
			maxTS = TS;
			lastIdx = i;
		}
	}
	if(maxTS != 0){
		m_GroupsFltr.CurrentIdx = lastIdx;
		return m_GroupsFltr.ppFNG[lastIdx];
	}
	return nullptr;
}
FILENAMEGROUP *CAnnotations::GetEnd(int Last)
{
	if(m_GroupsFltr.Num > 0){
		m_GroupsFltr.CurrentIdx = Last ? (m_GroupsFltr.Num-1) : 0;
		return m_GroupsFltr.ppFNG[m_GroupsFltr.CurrentIdx];
	}
	return nullptr;
}
FILENAMEGROUP *CAnnotations::GetPrevious(int Coarse/*=0*/)
{
	if(m_GroupsFltr.Num > 0){
		int Increment = Coarse ? m_GroupsFltr.SkipIncrements : 1;
		int idx = m_GroupsFltr.CurrentIdx - Increment;
		m_GroupsFltr.CurrentIdx = (idx > 0) ? idx : 0;
		return m_GroupsFltr.ppFNG[m_GroupsFltr.CurrentIdx];
	}
	return nullptr;
}
FILENAMEGROUP *CAnnotations::GetNext(int BackupIntervalMins, int Coarse/*=0*/)
{
	if(m_GroupsFltr.Num > 0){
		DoBackup(BackupIntervalMins);	// call this here for the current index (which is classified).
		int Increment = Coarse ? m_GroupsFltr.SkipIncrements : 1;
		int idx = m_GroupsFltr.CurrentIdx + Increment;
		m_GroupsFltr.CurrentIdx = (idx < m_GroupsFltr.Num) ? idx : m_GroupsFltr.Num-1;
		return m_GroupsFltr.ppFNG[m_GroupsFltr.CurrentIdx];
	}
	return nullptr;
}

/*int CAnnotations::EndOfList()
{
	if(m_GroupsFltr.CurrentIdx == 0)
		return 1;
	if(m_GroupsFltr.CurrentIdx == m_GroupsFltr.Num-1)
		return 2;
	return 0;
}*/

void CAnnotations::GetPicturePath(FILENAMEGROUP *pGrp, char *pPath, int PathCount)
{
	FolderAndFilenameToPath(pPath, PathCount, m_Path.PictureFolder, pGrp->pPicInfo->pfile_name);
}

void CAnnotations::DoBackup(int BackupIntervalMins)
{
	if(BackupIntervalMins > 0){
		// the only reason to use this auto-backup is for the security of saving the file to different file paths, 
		// otherwise its a fairly pointless feature when all you need to do is press the save button yourself.
		if(m_Backup.TickCount == 0){
			if(BackupIntervalMins < 10)
				MsgBox2("BackupIntervalMins %d", BackupIntervalMins);
			// backups not initialised, initialise now.
			if(GetCurrentDirectory(num_entries(m_Backup.Folder), m_Backup.Folder)){
				sprintf_s(m_Backup.Folder, num_entries(m_Backup.Folder), "%s\\csv backups", m_Backup.Folder);
				m_Backup.TickCount = GetTickCount64() + 1;
			}
			else{
				m_Backup.TickCount = 0;
				MsgBox2("error - init DoBackup");
			}
		}
		else{
			// backups already initialised.
			ULONGLONG Ticks = GetTickCount64();
			ULONGLONG Interval_ms = (BackupIntervalMins * 60) * 1000;
			if(Ticks-m_Backup.TickCount > Interval_ms){
				// do backup
				m_DoingBackup = 1;	// hummmm not nice ?
				SaveClassifiedCsvFile(0, 0, 0, 0);
				m_DoingBackup = 0;
				m_Backup.TickCount = Ticks;
			}
		}
	}
}
