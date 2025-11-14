
#include "pch.h"
#include "FixAndCropPictures.h"
#include "Useful.h"

//==========================
// Flags kept in 'DebugCode'
#define DC_ALL_PROBABILITIES		(1<<0)
#define DC_GOOD_ON_TARGET			(1<<1)
#define DC_GOOD_ON_ORIGINAL			(1<<2)
#define DC_NO_EXPORT_POSSIBLE		(1<<3) // IMPORTANT, look at these pics to see where export failed (and could be improved).
#define DC_PIC_ASPECT_FAILED		(1<<4)
#define DC_PIC_SIZE_FAILED			(1<<5) // remember some of these flags go on the original annotations only, not new annotations.

#define DC_NEW_ANNOTATION			(1<<6)
#define DC_EXPORTED_ANNOTATION		(1<<7)
#define DC_EXPORTED_ANNOTATION_FLIP	(1<<8)
#define DC_EXPORT_GD_ON_ORI_FAILED	(1<<9) // good on original that failed to export.
#define DC_BLUR_AREA_SINGLE			(1<<10)
#define DC_BLUR_AREA_COMBINED		(1<<11)
#define DC_CROPZONE_DESIRABLE_MAX	(1<<12)
#define DC_CROPZONE_NONPREF_ASPECT	(1<<13)

#define DC_GOOD_ON_TARGET_DESI_MIN	(1<<14) // ### NOT IN USE ANYMORE ### - But might be useful date so leave it for now.
#define DC_ACCEPT_CROP_ALL			(1<<15) // picture is accepted (i.e, good), but it'll also do some cropping and exporting.
#define DC_ACCEPT_CROP_SOME			(1<<16) // ditto

//=========================================================
// temporary cropzone export flags, kept in 'FilenameCount'
#define CZ_SEED_BOX_BIGGEST			(1<<0)
#define CZ_ALL_GROUPS_GOOD			(1<<1)
#define CZ_CURRENT_GRP_GOOD			(1<<2)
#define CZ_CURRENT_GRP_BLUR			(1<<3)

struct WIDTHHEIGHT
{
	int Wd, Ht;
};
struct MINMAX
{
	float Min, Max;
};

struct CURRENT_CZ_STATS
{
	float MinBoxHt_InCZ, MinBoxWd_InCZ;
	float MinBoxHt, MinBoxWd;
	// put min size cz in here.
	int NumGoodBoxes, NumGoodBoxes_ExcAllProbs, Num_BlurBoxes;
};

#define MAX_CZLIST		10
#define MAX_BOX_IN_CZ	30
struct CROPZONE_RECORD
{
	BBOX cropzone;
	int AllowBlurBoxesInCZ, UsingGoodOnOriginal;
	int debugCZid;
	int NumGood, NumBlur;
	int CurrentGroupFlags[MAX_BOX_IN_CZ];
};

struct EXPORT_RECORD
{
	int NumCropZoneExports;
	int NumBoxesExported; // this could be more than the number of objects if an objects is exported multiple times by different crop zones.
	CROPZONE_RECORD CZlist[MAX_CZLIST]; // NumCropZoneExports
};

struct ACCEPT_PIC_LIMITS
{
	float MaxCZ;
	float MaxBoxSize;
};

struct BOX_PIXEL_LIMITS
{
	// minimums...
	float SmallVehicle;	// real pixel size on the picture.
	float LargeVehicle;
	//
	float Desirable_Min;	// decided (for now) that different values for height and width are not necessary or a good idea for min.
	int   UseDesirableMin;
	float DesirableMaxFactor, DesirableMaxLimit;
};

/*struct SETUP_FIXNCROP
{
};*/

struct LIMITS_FIXNCROP // limits and constants ? reorganise this
{
	// box limits
	BOX_PIXEL_LIMITS BoxPix;

	ACCEPT_PIC_LIMITS Acc;
	
	// export and cropping
	int AllCropZonesPreferredAspect;
	int Pixel_MinPicHeight;
	float PictureAspectRatio_Min, PictureAspectRatio_Min_Preferred;
	float PictureAspectRatio_Max, PictureAspectRatio_Max_Preferred;

	float CenteringVariability; // this isnt a 'limit' its more a 'setup'/constant.
	int AugmentationFreq; // these are flags not limits
	float FullPicGOTmaxDelta;

	// BlurConvertedBox
	int   BlurConvertedBox_ABSMinPixelDist;
	float BlurConvertedBox_MinDistance;
	float BlurConvertedBox_MaxArea;
	// BlurBox
	int   BlurBox_DoAreaCalculation;
	float BlurBox_MaxArea;
	float dbg_MaxAreaSingle, dbg_MaxAreaCombined;
};

struct VEHICLECLASSES
{
	int car, pickup, van, moto, bus, truck;
};

enum PICEXPORTOPTIONS
{
	PIC_EXPORT_NO=0,
	PIC_EXPORT_YES,
	DEBUG_EXPORT_FULL,
};

#define CropOps_SeedAllGoodBoxes	0
#define CropOps_SingleBoxCrop		1
#define CropOps_MultiBoxCrop		2

static MOREOPTIONS    *g_pMO;
static FILENAMEGROUP  *g_pSpare;
static LIMITS_FIXNCROP g_Limits;
static int             g_AspectInPreferredZone_Min, g_AspectInPreferredZone_Max;
static VEHICLECLASSES  g_Veh;

static int   g_Export_FilenameCount=0, g_ExportMode; // flags above.
static int   g_AllowBlurBoxesInCropZone=0, g_AnyCzFailedBlurArea, g_PictureRejected;
static BBOX  g_ExpansionBoundaries;
static int   g_Remove_Id, g_Blur_Id;
static int   g_BadOnTargetBlurred_Id=0, g_CZdisplay_Id=0, g_Category; // for debug display.
static BYTE  g_AllProbsId;
static int   g_NumExportLines, g_AcceptNoExportCount;
static char *g_pPictureFolder, g_ExportFolder[MAX_PATH];
static int   g_ClassConversions;
static int **g_grid = nullptr;

// true  : if box is in the same 'all probs' group, otherwise false,
// false : if not an 'all probs' group.
#define SAME_ALL_PROBS_GROUP(pSCRL1, pSCRL2) (pSCRL1->rsvd_id==pSCRL2->rsvd_id && pSCRL1->rsvd_id!=0)
#define RANDOM_NORMALISED	((float)rand() / (float)RAND_MAX)
#define EXPORTS_FOLDER_NAME "cp_exports"

static float overlap(float a1, float a2, float b1, float b2)
{
	float back = max(a1, b1);
	float forward = min(a2, b2);
	return forward - back;
}

static float box_intersection(BBOX *a, BBOX *b)
{
	float w = overlap(a->x1, a->x2, b->x1, b->x2);
	if(w <= 0.0f)
		return 0.0f;

	float h = overlap(a->y1, a->y2, b->y1, b->y2);
	if(h <= 0.0f)
		return 0.0f;
	return w * h;
}

static float box_union(BBOX *a, BBOX *b, float intersection)
{
	float aw = a->x2 - a->x1;
	float ah = a->y2 - a->y1;
	float bw = b->x2 - b->x1;
	float bh = b->y2 - b->y1;
	float u = ((aw * ah) + (bw * bh)) - intersection;
	return u;
}

static float box_iou(BBOX *a, BBOX *b)
{
	float I = box_intersection(a, b);
	if (I == 0.0f)
		return 0.0f;
	// if intersection is non-zero, then union will of course be non-zero, so no need to worry about divide-by-zero
	float U = box_union(a, b, I);
	return I / U;	// 0.0 to 1.0
}

static void SetFilenameGroupStatus(FILENAMEGROUP *pFnG, int Status)
{
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++)
		// we can't assign Status to pFnG->Classified, assignment would be lost when the filename groups are remade after this module has finished.
		pSL->Repeats.Pic_classified = Status;
}

static float GetScaleForTarget(float PicSize, float TargetSize)
{
	if(PicSize == 0.0f){ // this cant happen.
		MsgBox("error - PicSize == 0.0f");
		return 0.0f;
	}
	// if image is smaller than target we dont rescale bbox size, if we did bbox would scaled up (bad).
	// only rescale bbox if image is bigger than target, which will make the rescaled bbox smaller.
	float scale = 1.0f;
	if(PicSize > TargetSize)
		scale = TargetSize / PicSize;
	return scale;
}

static void SetBoxMinPixelLimits(int small_vehicle, int large_vehicle, int Desirable_Min)
{
	// 352 * 0.08 = 28.16 
	// remember the intention is primarily for anpr cameras, so the camera will be setup such that the vehicles are fairly large.
	// coping with tiny might have an impact on how well it copes with full screen vehicles. (my last training on coco only worked with small vehicles).
	// 
	// look at original size pic, when i'm looking at enlarged pic it seems to give a false impression of the detail of tiny vehicles.
	g_Limits.BoxPix.SmallVehicle = g_Limits.BoxPix.SmallVehicle = (float)small_vehicle;
	g_Limits.BoxPix.LargeVehicle = g_Limits.BoxPix.LargeVehicle = (float)large_vehicle;
	// desirable min is the same for small_vehicle and large_vehicle.
	g_Limits.BoxPix.Desirable_Min = (float)Desirable_Min; 
	if(Desirable_Min<=small_vehicle || Desirable_Min<=large_vehicle)
		MsgBox2("error - Desirable_Min %d", Desirable_Min);
}

static int ExtendedLimit_BoxSizeOk(float pixel, float OriginalPixel, BOOL CZBiggerThanTarget)
{
	if(CZBiggerThanTarget && g_Limits.BoxPix.UseDesirableMin){
		float ExtendedLimit = min(OriginalPixel, g_Limits.BoxPix.Desirable_Min);
		if(pixel < ExtendedLimit)
			return FALSE;
	}
	return TRUE;
}

static int BoxSizeOk(float pixelWd, float pixelHt, int ClassId, float OriginalPixelWd, float OriginalPixelHt, BOOL CZBiggerThanTarget_Wd, BOOL CZBiggerThanTarget_Ht)
{
	if(ClassId==g_Veh.bus || ClassId==g_Veh.truck){
		
		if(pixelHt>=g_Limits.BoxPix.LargeVehicle && pixelWd>=g_Limits.BoxPix.LargeVehicle){
			// box passes min wd and ht limits, however, extend these limits if crop zone is bigger than target size (no point doing it if cz smaller than target).
			// I do this because otherwise during cz expansion the crop zone could expand big enough to push the box size to the minimum limit, even though the original box size is bigger than the minimum limit.
			// so for example, this is saying although the min limit is 30, if the box is bigger than 30 dont allow crop zone expansion to reduce box size to 30.
			// 
			// limits are extended by a maximum of MinPix.Desirable_Min pixels or to the original box size if its less than g_Limits.BoxMinPixels_XX + MinPix.Desirable_Min.
			// this means that a crop zone will not be able to expand bigger than target size if the box size is less than this extended limit.

			if(!ExtendedLimit_BoxSizeOk(pixelWd, OriginalPixelWd, CZBiggerThanTarget_Wd))
				return 0;
			if(!ExtendedLimit_BoxSizeOk(pixelHt, OriginalPixelHt, CZBiggerThanTarget_Ht))
				return 0;
			return 1;
		}
	}
	else if(ClassId==g_Veh.car || ClassId==g_Veh.pickup || ClassId==g_Veh.van || ClassId==g_Veh.moto || ClassId==g_Veh.moto){

		if(pixelHt>=g_Limits.BoxPix.SmallVehicle && pixelWd>=g_Limits.BoxPix.SmallVehicle){
			if(!ExtendedLimit_BoxSizeOk(pixelWd, OriginalPixelWd, CZBiggerThanTarget_Wd))
				return 0;
			if(!ExtendedLimit_BoxSizeOk(pixelHt, OriginalPixelHt, CZBiggerThanTarget_Ht))
				return 0;
			return 1;
		}
	}
	else MsgBox2("error - BoxSizeOk unrecognised class");

	return 0;
}

static void GetPixelSizeOnOriginal(BBOX *pBox, FILENAMEGROUP *pFnG, float &Wd, float &Ht)
{
	// BoxToBitmapCoordinates_Inc_Exc(L, R, T, B, ...);
	float PicWd = (float)pFnG->pPicInfo->Width, PicHt = (float)pFnG->pPicInfo->Height;
	// PIXEL size on the original picture.
	Wd = (pBox->x2 - pBox->x1) * PicWd;
	Ht = (pBox->y2 - pBox->y1) * PicHt;
}

static int AllProbabilities_BoxSizeOk(FILENAMEGROUP *pFnG, float pixelWd, float pixelHt, SOURCELINE *pObject, BOOL CZBiggerThanTarget_Wd, BOOL CZBiggerThanTarget_Ht)
{
	// problem with having different size limits for different classes, imagine we have an object with 2 probabilities, van/truck, these classes have different limits.
	// to address this do the size limit check on all probabilities individually, and if any fail then all all-probabilities fail.

	float Wd_PixelsOnOriginal, Ht_PixelsOnOriginal;
	GetPixelSizeOnOriginal(&pObject->Box, pFnG, Wd_PixelsOnOriginal, Ht_PixelsOnOriginal); // bit wasteful, this will get re-calculated 1000's of times as crop is expanding.

	if(pObject->rsvd_id == 0){
		// this object does not have multiple probabilites.
		return BoxSizeOk(pixelWd, pixelHt, pObject->ClassId, Wd_PixelsOnOriginal, Ht_PixelsOnOriginal, CZBiggerThanTarget_Wd, CZBiggerThanTarget_Ht);
	}
	int SizeOk = 1;
	for(int i=0; i<pFnG->NumSourceLines && SizeOk; i++){
		if(pFnG->pSourceLines[i].rsvd_id == pObject->rsvd_id){
			// this is fine, no need to re-calculate pixelWd pixelHt for each probability because the boxes 
			// will be identical, or very very similar, in fact they will be identical - thats how i do multiple probs.
			// BBoxIdentical
			SizeOk &= BoxSizeOk(pixelWd, pixelHt, pFnG->pSourceLines[i].ClassId, Wd_PixelsOnOriginal, Ht_PixelsOnOriginal, CZBiggerThanTarget_Wd, CZBiggerThanTarget_Ht);
		}
	}
	return SizeOk;
}

static void GetScaleForTargetEx(float PicWd_pixels, float PicHt_pixels, float &scale_W, float &scale_H)
{
	scale_W = GetScaleForTarget(PicWd_pixels, (float)g_pMO->General.TargetWidth);
	scale_H = GetScaleForTarget(PicHt_pixels, (float)g_pMO->General.TargetHeight);
}

static void GetPixelSizeOnTarget(float originalWd, float originalHt, float scale_W, float scale_H, float &targetWd, float &targetHt)
{
	targetWd = originalWd;
	targetHt = originalHt;
	// scale down the bbox height/width if the bitmap is bigger than the target size, but never 
	// scale up if bitmap is smaller than target (because upscaling results in poor quality).
	if(scale_H < 1.0f) // less than 1.0 if target is smaller than bitmap.
		targetHt = originalHt * scale_H;
	if(scale_W < 1.0f)
		targetWd = originalWd * scale_W;
}

static void FromCropZoneToTarget_GetBoxSize(FILENAMEGROUP *pFnG, float CropWd_pixels, float CropHt_pixels, BBOX *pBox, float &BoxPixelsWd, float &BoxPixelsHt)
{
	float scale_W, scale_H;
	GetScaleForTargetEx(CropWd_pixels, CropHt_pixels, scale_W, scale_H); // scale of crop zone in relation to target.
	float originalWd, originalHt;
	GetPixelSizeOnOriginal(pBox, pFnG, originalWd, originalHt); // pixel size of object box on original picture, would be better to save this rather than keep calculating it over and over.
	float targetWd, targetHt;
	GetPixelSizeOnTarget(originalWd, originalHt, scale_W, scale_H, targetWd, targetHt);
	BoxPixelsWd = targetWd; // bit pointless but makes things bit clearer?
	BoxPixelsHt = targetHt;
}

static void CheckForBadObjects(FILENAMEGROUP *pFnG, int &AllBoxesGoodOnOriginal, int &RemovalBoxes, int &BlurBoxes, int &TooSmallOnTarget)
{
	// return false if any bounding boxes are too small, or have id 'remove'.
#ifdef DELETE_USELESS_PICS
	int delete_pic = 1, NumGoodTarget = 0, NumAll = 0;
#endif
	RemovalBoxes = BlurBoxes = TooSmallOnTarget = 0;
	AllBoxesGoodOnOriginal = 1;
	float scale_W, scale_H;
	GetScaleForTargetEx((float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height, scale_W, scale_H); // scale of original picture in relation to target.
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->ClassId == g_Remove_Id){
			RemovalBoxes = 1;
			AllBoxesGoodOnOriginal = 0;
		}
		else if(pSL->ClassId == g_Blur_Id){
			BlurBoxes = 1;
			AllBoxesGoodOnOriginal = 0;
		}
		else{
			g_Limits.BoxPix.UseDesirableMin = 0; // not actually necessary to set this when the last 2 params of AllProbabilities_BoxSizeOk are zero (as they are below).

			// get the bbox pixel width/height on the bitmap.   
			float Wd_PixelsOnOriginal, Ht_PixelsOnOriginal;
			GetPixelSizeOnOriginal(&pSL->Box, pFnG, Wd_PixelsOnOriginal, Ht_PixelsOnOriginal);

			// size test here must come before the scaling below, want to know whether the dimensions on the original pic are ok for cropping and exporting.
			if(AllProbabilities_BoxSizeOk(pFnG, Wd_PixelsOnOriginal, Ht_PixelsOnOriginal, pSL, 0, 0)){
				// for cropped sections of the original, no matter the size of the crop zone (bigger or smaller than target) these flags are always valid, because theres no translation/scaling, we're looking at real size.
				pSL->DebugCode |= DC_GOOD_ON_ORIGINAL;
#ifdef DELETE_USELESS_PICS
				delete_pic = 0;	// if any boxes are big enough for cropped export dont delete the picture.
#endif
			}
			else AllBoxesGoodOnOriginal = 0;

			float targetWd, targetHt;// width/height pixels on target (after scaling to target).
			GetPixelSizeOnTarget(Wd_PixelsOnOriginal, Ht_PixelsOnOriginal, scale_W, scale_H, targetWd, targetHt);

			if(AllProbabilities_BoxSizeOk(pFnG, targetWd, targetHt, pSL, 0, 0)){
				pSL->DebugCode |= DC_GOOD_ON_TARGET;
#ifdef DELETE_USELESS_PICS
				NumGoodTarget++;
#endif
				g_Limits.BoxPix.UseDesirableMin = 1;
				if(AllProbabilities_BoxSizeOk(pFnG, targetWd, targetHt, pSL, pFnG->pPicInfo->Width>g_pMO->General.TargetWidth, pFnG->pPicInfo->Height>g_pMO->General.TargetHeight))
					pSL->DebugCode |= DC_GOOD_ON_TARGET_DESI_MIN;
			}
			else TooSmallOnTarget = 1; // box too small on the target, but if the original pic is bigger than the target this bbox could be big enough on the original and good for cropping.
		}
#ifdef DELETE_USELESS_PICS
		NumAll++;
#endif
	}

#ifdef DELETE_USELESS_PICS
	if( delete_pic          &&	// ..false if any boxes were marked as CZ seed boxes, pic must be kept for cropped exports. (bear in mind TruncatedObjects_RefusePicture could still delete pic).
		NumGoodTarget!=NumAll)	// ..false if all boxes were good on the target, pic must be kept because all bboxes scaled to target are ok.
		// why are both needed ..because cropped boxes and target boxes have different min size limits, so maybe no boxes were big enough for cropping, but we dont want to delete the pic because all boxes were big enough on the target.
		ObjectsForBlurOrRemoval = 1; need to check all this i've changed this slightly
#endif
}

static void AddDebugCode(FILENAMEGROUP *pFnG, int DebugCodeFlag)
{
	// must be added to each sourceline not FILENAMEGROUP, because the FILENAMEGROUP's will be remade after processing.
	for(int i=0; i<pFnG->NumSourceLines; i++)
		pFnG->pSourceLines[i].DebugCode |= DebugCodeFlag;
}

static void RemoveDebugCode(FILENAMEGROUP *pFnG, int DebugCodeFlag)
{
	for(int i=0; i<pFnG->NumSourceLines; i++)
		pFnG->pSourceLines[i].DebugCode &= ~DebugCodeFlag;
}

static int AspectRatioOk(float Wd, float Ht, int Preferred)
{
	float AR = Wd / Ht;
	if(AR<g_Limits.PictureAspectRatio_Min || AR>g_Limits.PictureAspectRatio_Max)
		return 0;
	if(Preferred) // if Preferred is true then return 1 if aspect is in the preferred range, (stricter).
		if(AR<g_Limits.PictureAspectRatio_Min_Preferred || AR>g_Limits.PictureAspectRatio_Max_Preferred)
			return 0;
	return 1;
}

static int AspectRatio_X_expansionOk(BBOX *pCropZone, float PicWd, float PicHt)
{
	float Wd = (pCropZone->x2-pCropZone->x1) * PicWd;
	float Ht = (pCropZone->y2-pCropZone->y1) * PicHt;
	float AR = Wd / Ht;
	if(AR > g_Limits.PictureAspectRatio_Max)
		return 0;
	if(AR > g_Limits.PictureAspectRatio_Max_Preferred){
		// more strict aspect limit engaged, but only after we have already got into the preferred zone, which wont always be possible.
		if(g_AspectInPreferredZone_Max)
			return 0;
	}
	else g_AspectInPreferredZone_Max = 1;
	return 1;
}

static int AspectRatio_Y_expansionOk(BBOX *pCropZone, float PicWd, float PicHt)
{
	float Wd = (pCropZone->x2-pCropZone->x1) * PicWd;
	float Ht = (pCropZone->y2-pCropZone->y1) * PicHt;
	float AR = Wd / Ht;
	if(AR < g_Limits.PictureAspectRatio_Min)
		return 0;
	if(AR < g_Limits.PictureAspectRatio_Min_Preferred){
		// more strict aspect limit engaged, but only after we have already got into the preferred zone, which wont always be possible.
		if(g_AspectInPreferredZone_Min)
			return 0;
	}
	else g_AspectInPreferredZone_Min = 1;
	return 1;
}

static int CropZoneAspectRatioOk(BBOX *pCropZone, float PicWd, float PicHt, int Preferred)
{
	float Wd = (pCropZone->x2-pCropZone->x1) * PicWd;
	float Ht = (pCropZone->y2-pCropZone->y1) * PicHt;
	return AspectRatioOk(Wd, Ht, Preferred);
}

static int BoxOverlapsCropZone(BBOX *pCZ, BBOX *pBx, int &Inside, int &ovlp_L, int &ovlp_R, int &ovlp_T, int &ovlp_B)
{
	if(pBx->x1>=pCZ->x1 && pBx->x2<=pCZ->x2){
		if(pBx->y1>=pCZ->y1 && pBx->y2<=pCZ->y2){
			// box completely inside cz.
			Inside = 1;
			return 1;
		}
	}
	if(pBx->x1<=pCZ->x2 && pBx->x2>=pCZ->x1){ // or < and > ...????
		if(pBx->y1<=pCZ->y2 && pBx->y2>=pCZ->y1){
			// x and y overlap, ie pBx overlaps pCZ, now find out which crop zone edges are overlapped.
			if(pBx->x1 < pCZ->x1)
				ovlp_L = 1;
			if(pBx->x2 > pCZ->x2)
				ovlp_R = 1;
			if(pBx->y1 < pCZ->y1)
				ovlp_T = 1;
			if(pBx->y2 > pCZ->y2)
				ovlp_B = 1;
			return 1;
		}
	}
	return 0;
}

static int BoxSizesInCropZoneOk(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, BBOX *pCropZone)
{
	// return zero if box(es) inside crop zone have failed min size test.
	float BoxPixelsWd, BoxPixelsHt;
	float cz_pixWd, cz_pixHt;
	GetPixelSizeOnOriginal(pCropZone, pFnG, cz_pixWd, cz_pixHt);
	BOOL CZBiggerThanTarget_Wd = (cz_pixWd >= (float)(g_pMO->General.TargetWidth+1));
	BOOL CZBiggerThanTarget_Ht = (cz_pixHt >= (float)(g_pMO->General.TargetHeight+1)); // if *either* axis is bigger than target we must test box size.

	if(pSeedBoxOnly){
		// only one box in crop zone (pSeedBoxOnly), size check pSeedBoxOnly box only.
		if(CZBiggerThanTarget_Wd || CZBiggerThanTarget_Ht){ // see comments in usage below.
			FromCropZoneToTarget_GetBoxSize(pFnG, cz_pixWd, cz_pixHt, &pSeedBoxOnly->Box, BoxPixelsWd, BoxPixelsHt);
			return AllProbabilities_BoxSizeOk(pFnG, BoxPixelsWd, BoxPixelsHt, pSeedBoxOnly, CZBiggerThanTarget_Wd, CZBiggerThanTarget_Ht);
		}
		return 1;
	}

	// unlimited number of boxes in crop zone, size check all boxes in cz.
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->rsvd_best){
			if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL){
				// DC_GOOD_ON_ORIGINAL - this function is only testing the good boxes in the cz, not any bad boxes in the cz ..that is done by UnwantedBoxOverlapsCropZone.
				int Inside=0, ovlp_L=0, ovlp_R=0, ovlp_T=0, ovlp_B=0;
				if(BoxOverlapsCropZone(pCropZone, &pSL->Box, Inside, ovlp_L, ovlp_R, ovlp_T, ovlp_B)){ // remember, not all DC_GOOD_ON_ORIGINAL boxes will be in this particular crop zone.
					if(CZBiggerThanTarget_Wd || CZBiggerThanTarget_Ht){
						// crop zone bigger than target. 
						// box size is ok on original (DC_GOOD_ON_ORIGINAL already tested) but test its ok when crop zone is rescaled (downsized) to target size..
						FromCropZoneToTarget_GetBoxSize(pFnG, cz_pixWd, cz_pixHt, &pSL->Box, BoxPixelsWd, BoxPixelsHt);
						if(!AllProbabilities_BoxSizeOk(pFnG, BoxPixelsWd, BoxPixelsHt, pSL, CZBiggerThanTarget_Wd, CZBiggerThanTarget_Ht))
							return 0;
					}
					else{
						// crop zone smaller than target, therefore boxes can only get bigger when rescaled to target.
						// box original pixel size is already bigger than limits, we know this because box has DC_GOOD_ON_ORIGINAL flag, so box size is ok in proportion to crop zone.
						// It doesnt ever make sense to stop crop zone expansion whilst crop zone is smaller than target, because if expansion is stopped early there'll 
						// be more upscaling (which is bad) to target.
					}
				}
			}
		}
	}
	return 1;
}

static int UnwantedBoxOverlapsCropZone(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, BBOX *pCropZone, int test_Inside, int test_L, int test_R, int test_T, int test_B, int FinalCropZonePosition=0)
{
	int Inside=0, ovlp_L=0, ovlp_R=0, ovlp_T=0, ovlp_B=0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->rsvd_best){
			if(pSeedBoxOnly != nullptr){
				// single box cropping with no other boxes except blur boxes if g_AllowBlurBoxesInCropZone == true.
				if(pSL != pSeedBoxOnly)
					if(g_AllowBlurBoxesInCropZone==0 || pSL->ClassId!=g_Blur_Id)
						BoxOverlapsCropZone(pCropZone, &pSL->Box, Inside, ovlp_L, ovlp_R, ovlp_T, ovlp_B);
			}
			else{
				int Unwanted = 0; // if Unwanted is set to 1 below, annotation 'pSL' is unwanted and must not be inside or overlapping the crop zone, and so must be tested by BoxOverlapsCropZone.

				if(pSL->ClassId == g_Remove_Id){
					// sections of the picture that must be excluded.
					Unwanted = 1;
				}
				else if(pSL->ClassId == g_Blur_Id){
					// blur boxes are allowed to overlap the crop zone if g_AllowBlurBoxesInCropZone == true.
					if(!g_AllowBlurBoxesInCropZone)
						Unwanted = 1;
				}
				else{
					// not a 'blur box' or 'remove box'.
					if(pSL->FilenameCount&CZ_ALL_GROUPS_GOOD) // box that has already been cropped, and so not wanted.
						Unwanted = 1;

					if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL){
						// good on original boxes can overlap crop zone while we are still expanding CZ, but of course once expansion has finished they must be fully inside and not overlapping crop zone, so must be tested.
						if(FinalCropZonePosition)
							Unwanted = 1;
					}
					else // box not 'good on original', i.e box is too small.
						Unwanted = 1;
				}

				if(Unwanted)
					BoxOverlapsCropZone(pCropZone, &pSL->Box, Inside, ovlp_L, ovlp_R, ovlp_T, ovlp_B);
			}
		}
	}
	if(test_Inside && Inside) return 1;
	if(test_L && ovlp_L) return 1;
	if(test_R && ovlp_R) return 1;
	if(test_T && ovlp_T) return 1;
	if(test_B && ovlp_B) return 1;
	return 0;
}

static void RevertToLastGoodCz(BBOX *pExpanded, BBOX *pLastGoodCropZone)
{
	// made this a function so i can put a breakpoint on it.
	if(pLastGoodCropZone->x2 > 0.0f)
		*pExpanded = *pLastGoodCropZone; // revert to last good crop zone, if such a crop zone exists.
}

static int ExpandCropZoneLeft(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, float Increment, BBOX *pCropZone, BBOX *pExpanded, BBOX *pLastGoodCropZone, int &Left, int &Moved)
{
	Moved = 0;
	if(Left && pCropZone->x1>g_ExpansionBoundaries.x1){
		pCropZone->x1 -= Increment;
		if(pCropZone->x1 < g_ExpansionBoundaries.x1)
			pCropZone->x1 = g_ExpansionBoundaries.x1;
		if(AspectRatio_X_expansionOk(pCropZone, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height)){
			if(!BoxSizesInCropZoneOk(pFnG, pSeedBoxOnly, pCropZone)){
				Left = 0; // expansion failed, expansion made boxes become too small, moving left is finished.
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else if(UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pCropZone, 0,1,0,0,0)){
				Left = 0; // expansion failed (encountered obstacle/box), moving left is finished.
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else{ // successful expansion.
				Moved = 1;
				*pExpanded = *pCropZone; // expansion succeeded.
			}
		}

		if(Moved == 0){
			*pCropZone = *pExpanded; // restore previous position.
			// next - caller needs to know when Left movement is finished due to failure (ie, min box size, or unwanted boxes overlapping), this will only ever return zero once.
			// also note that 'Moved' might be zero due to AspectRatio_X_expansionOk == 0, in which case we return 1, because that is not a terminating failure.
			return Left;
		}
	}
	return 1;
}
static int ExpandCropZoneRight(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, float Increment, BBOX *pCropZone, BBOX *pExpanded, BBOX *pLastGoodCropZone, int &Right, int &Moved)
{
	Moved = 0;
	if(Right && pCropZone->x2<g_ExpansionBoundaries.x2){
		pCropZone->x2 += Increment;
		if(pCropZone->x2 > g_ExpansionBoundaries.x2)
			pCropZone->x2 = g_ExpansionBoundaries.x2;
		if(AspectRatio_X_expansionOk(pCropZone, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height)){
			if(!BoxSizesInCropZoneOk(pFnG, pSeedBoxOnly, pCropZone)){
				Right = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else if(UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pCropZone, 0,0,1,0,0)){
				Right = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else{
				Moved = 1;
				*pExpanded = *pCropZone;
			}
		}
		if(Moved == 0){
			*pCropZone = *pExpanded;
			return Right;
		}
	}
	return 1;
}
static int ExpandCropZoneUp(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, float Increment, BBOX *pCropZone, BBOX *pExpanded, BBOX *pLastGoodCropZone, int &Top, int &Moved)
{
	Moved = 0;
	if(Top && pCropZone->y1>g_ExpansionBoundaries.y1){
		pCropZone->y1 -= Increment;
		if(pCropZone->y1 < g_ExpansionBoundaries.y1)
			pCropZone->y1 = g_ExpansionBoundaries.y1;
		if(AspectRatio_Y_expansionOk(pCropZone, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height)){
			if(!BoxSizesInCropZoneOk(pFnG, pSeedBoxOnly, pCropZone)){
				Top = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else if(UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pCropZone, 0,0,0,1,0)){
				Top = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else{
				Moved = 1;
				*pExpanded = *pCropZone;
			}
		}
		if(Moved == 0){
			*pCropZone = *pExpanded;
			return Top;
		}
	}
	return 1;
}
static int ExpandCropZoneDown(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, float Increment, BBOX *pCropZone, BBOX *pExpanded, BBOX *pLastGoodCropZone, int &Bottom, int &Moved)
{
	Moved = 0;
	if(Bottom && pCropZone->y2<g_ExpansionBoundaries.y2){
		pCropZone->y2 += Increment;
		if(pCropZone->y2 > g_ExpansionBoundaries.y2)
			pCropZone->y2 = g_ExpansionBoundaries.y2;
		if(AspectRatio_Y_expansionOk(pCropZone, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height)){
			if(!BoxSizesInCropZoneOk(pFnG, pSeedBoxOnly, pCropZone)){
				Bottom = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else if(UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pCropZone, 0,0,0,0,1)){
				Bottom = 0;
				RevertToLastGoodCz(pExpanded, pLastGoodCropZone);
			}
			else{
				Moved = 1;
				*pExpanded = *pCropZone;
			}
		}
		if(Moved == 0){
			*pCropZone = *pExpanded;
			return Bottom;
		}
	}
	return 1;
}

static int ExpandCropZone(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBoxOnly, BBOX *pExpanded, int StartWithPreferredAspectLimits)
{
	// check theres no failed/bad boxes inside or overlapping the initial crop zone (remove id boxes or boxes without DC_GOOD_ON_ORIGINAL flag for example).
	if(!UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pExpanded, 1,1,1,1,1)){
		BBOX LastGoodCropZone = {0.0f}; // why not initialise this with pExpanded? ..because then it could get stuck in an infinite loop, expansion fails, crop zone put back to initial position.
		BBOX CropZone = *pExpanded;
		BBOX SeedBox = *pExpanded;
		if(pSeedBoxOnly){ // if null then we're not using a seed box, instead use 'pExpanded' as start box.
			*pExpanded = CropZone = pSeedBoxOnly->Box;
			if(!pSeedBoxOnly->rsvd_best)
				MsgBox2("error - ExpandCropZone rsvd_best");
		}

		g_AspectInPreferredZone_Min = StartWithPreferredAspectLimits; // 'start with' as opposed to locking into after preferred is reached (if it is ever reached).
		g_AspectInPreferredZone_Max = StartWithPreferredAspectLimits;

		float Increment_X = 1.0f / (float)pFnG->pPicInfo->Width; // one pixel increments for crop zone expansion.
		float Increment_Y = 1.0f / (float)pFnG->pPicInfo->Height;

		int Left=1, Right=1, Top=1, Bottom=1;
		int NumMoves=1;
		int MoveRightFirst=0, MoveDownFirst=0;
		while((Left || Right || Top || Bottom) && NumMoves>0){
			// must do at least one move below, otherwise expansion is finished.
			// moves can be disabled (either vertically or horizontally) so crop zone always stays within accepted aspect ratio limits as its growing.
			// the individual flags (Left Right Top Bottom) will permantently stop expansion in each respective direction when no further movement is possible.
			NumMoves = 0;
			int Ok = 1, Moved;

			// MoveRightFirst/MoveDownFirst - need to swap which move goes first so the box expands evenly after bad aspect stoppages.
			if(MoveRightFirst){
				Ok &= ExpandCropZoneRight(pFnG, pSeedBoxOnly, Increment_X, &CropZone, pExpanded, &LastGoodCropZone, Right, Moved);
				if(Moved){ NumMoves++; MoveRightFirst = 0; }
				Ok &= ExpandCropZoneLeft(pFnG, pSeedBoxOnly, Increment_X, &CropZone, pExpanded, &LastGoodCropZone, Left, Moved);
				if(Moved){ NumMoves++; if(!MoveRightFirst) MoveRightFirst = 1; }
			}
			else{
				int DidLeft = 0;
				Ok &= ExpandCropZoneLeft(pFnG, pSeedBoxOnly, Increment_X, &CropZone, pExpanded, &LastGoodCropZone, Left, Moved);
				if(Moved){ NumMoves++; DidLeft = 1; }
				Ok &= ExpandCropZoneRight(pFnG, pSeedBoxOnly, Increment_X, &CropZone, pExpanded, &LastGoodCropZone, Right, Moved);
				if(Moved) NumMoves++;
				else if(DidLeft) MoveRightFirst = 1; // dont swap if there were no moves from left and right.
			}

			if(MoveDownFirst){
				Ok &= ExpandCropZoneDown(pFnG, pSeedBoxOnly, Increment_Y, &CropZone, pExpanded, &LastGoodCropZone, Bottom, Moved);
				if(Moved){ NumMoves++; MoveDownFirst = 0; }
				Ok &= ExpandCropZoneUp(pFnG, pSeedBoxOnly, Increment_Y, &CropZone, pExpanded, &LastGoodCropZone, Top, Moved);
				if(Moved){ NumMoves++; if(!MoveDownFirst) MoveDownFirst = 1; }
			}
			else{
				int DidUp = 0;
				Ok &= ExpandCropZoneUp(pFnG, pSeedBoxOnly, Increment_Y, &CropZone, pExpanded, &LastGoodCropZone, Top, Moved);
				if(Moved){ NumMoves++; DidUp = 1; }
				Ok &= ExpandCropZoneDown(pFnG, pSeedBoxOnly, Increment_Y, &CropZone, pExpanded, &LastGoodCropZone, Bottom, Moved);
				if(Moved) NumMoves++;
				else if(DidUp) MoveDownFirst = 1; // dont swap if there were no moves from up and down.
			}

			if(pSeedBoxOnly == nullptr){
				if(NumMoves == 0){
					// Revert to last good cz. This does no harm if you think about it because a good cz will always get loaded into LastGoodCropZone..
					// ..BUT I'M NOT SURE why I put this line in, I didnt comment it properly. I think it was a some initialisation scenario, it was to fix something - maybe it was before I commented out the 'Ok' flag below.
					RevertToLastGoodCz(pExpanded, &LastGoodCropZone);
				}
				else /*if(Ok)*/{ //  ############# Ok flag is OBSOLETE ################
					// Ok flag - see the expansion functions, if this is false an expansion func has failed and pExpanded has been reverted to LastGoodCropZone, in which case there would be no point doing the assignment to LastGoodCropZone below.
					// ...YES BUT IN WHICH CASE NumMoves wont be incremented for the direction that failed, but other directions might have expanded and incremented NumMoves. I think Ok flag is OBSOLETE ################.

					// Assign LastGoodCropZone. If we've done a move then box size has been tested and is good, so dont need to test box size here.	but 'pExpanded' crop zone might still be bad 
					// at the moment as it cuts through a box, so call to UnwantedBoxOverlapsCropZone is needed.
					if(!UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pExpanded, 0,1,1,1,1, TRUE))
						LastGoodCropZone = *pExpanded; // last good crop zone, ie good box size, no truncations. If there never was a 'last good cz' this will never get assigned.
				}
			}
		}

		if(pExpanded->x1!=SeedBox.x1 || pExpanded->x2!=SeedBox.x2 || pExpanded->y1!=SeedBox.y1 || pExpanded->y2!=SeedBox.y2){
			// crop zone is bigger therefore we've done a move and box size has been tested with BoxSizesInCropZoneOk.
			// next, do FinalCropZonePosition (see param comments) check with UnwantedBoxOverlapsCropZone.
			if(!UnwantedBoxOverlapsCropZone(pFnG, pSeedBoxOnly, pExpanded, 0,1,1,1,1, TRUE)){
				// success, however...
				// CALLING FUNCTION IS RESPONSIBLE FOR CHEKCING ASPECT RATIO AND CROP ZONE SIZE IS ACCEPTABLE.
				return 1;
			}
		}
	}
	return 0;
}

static int CropZoneIsFullPicture(BBOX *pCropZone)
{
	if(pCropZone == nullptr)
		return 1; // theres no crop zone, so we're full picture.
	return (pCropZone->x1<=0.0f && pCropZone->x2>=1.0f && pCropZone->y1<=0.0f && pCropZone->y2>=1.0f);
}

static void GetBoxInPixels(FILENAMEGROUP *pFnG, BBOX Box, int &Wd, int &Ht)
{
	// get the real pixel width height of Box
	float PicWd = (float)pFnG->pPicInfo->Width, PicHt = (float)pFnG->pPicInfo->Height;
	Wd = (int)((Box.x2-Box.x1) * PicWd);
	Ht = (int)((Box.y2-Box.y1) * PicHt);
}

static BBOX ObjectToCropZone(BBOX *pObject, BBOX *pCropZone)
{
	BBOX onCZ = *pObject;
	if(!CropZoneIsFullPicture(pCropZone)){
		onCZ.x1 = (pObject->x1 - pCropZone->x1) / (pCropZone->x2 - pCropZone->x1);
		onCZ.x2 = (pObject->x2 - pCropZone->x1) / (pCropZone->x2 - pCropZone->x1);
		onCZ.y1 = (pObject->y1 - pCropZone->y1) / (pCropZone->y2 - pCropZone->y1);
		onCZ.y2 = (pObject->y2 - pCropZone->y1) / (pCropZone->y2 - pCropZone->y1);
	}
	if(onCZ.x1 < 0.0f) onCZ.x1 = 0.0f;
	if(onCZ.x2 > 1.0f) onCZ.x2 = 1.0f;
	if(onCZ.y1 < 0.0f) onCZ.y1 = 0.0f;
	if(onCZ.y2 > 1.0f) onCZ.y2 = 1.0f;
	return onCZ;
}

static char *GetPathBuffer(char *pPath)
{
	int Len = (int)strlen(pPath);
	Len += 1;	// +1 so Len includes the null.
	if(Len>5 && Len<=MAX_PATH){	// 5 chars would be the shortest filename, eg, a.bmp
		char *pBuf = g_MemPool.GetBuffer(Len);
		if(pBuf)
			if(strcpy_s(pBuf, Len, pPath) == 0)
				return pBuf;
	}
	MsgBox2("error - GetPathBuffer");
	return nullptr;
}

static int AddNewSourceLines_debug(FILENAMEGROUP *pFnG, BOOL IsCropZone, BBOX *pBox, int classId, int AddBlurDebugFlags, int HorizontalFlip)
{
	if(g_NumExportLines < g_pSpare->NumSourceLines){
		SOURCELINE *pNew = &g_pSpare->pSourceLines[g_NumExportLines++];
		memset(pNew, 0, sizeof(SOURCELINE));
		pNew->rsvd_best  = 1; // shouldnt be necessary.
		pNew->Repeats    = *pFnG->pPicInfo;
		pNew->ClassId    = classId;
		pNew->Box        = *pBox;
		pNew->Confidence = 1;
		// these debug sourcelines will be merged with the original filenamegroup so must have the same status, and the original filenamegroup is set to CLASSIFIED_SPECIAL after the filenamegroup is refused.
		pNew->Repeats.Pic_classified = g_PictureRejected ? CLASSIFIED_SPECIAL : CLASSIFIED_ACCEPT;
		if(AddBlurDebugFlags){
			float sgl = 0.08f; // BlurBox_MaxArea
			float cmb = 0.08f;
		//	if(sgl>=g_Limits.BlurBox_MaxArea || cmb>=g_Limits.BlurBox_MaxArea)
		//		MsgBox2("debug values dbg_MaxAreaSingle / dbg_MaxAreaCombined");
			if(g_Limits.dbg_MaxAreaSingle   >= sgl) pNew->DebugCode |= DC_BLUR_AREA_SINGLE; // 11
			if(g_Limits.dbg_MaxAreaCombined >= cmb) pNew->DebugCode |= DC_BLUR_AREA_COMBINED; // 12
		}
		if(HorizontalFlip)
			pNew->DebugCode |= DC_EXPORTED_ANNOTATION_FLIP;
		if(IsCropZone){
			if(!g_Limits.BoxPix.UseDesirableMin)
				pNew->DebugCode |= DC_CROPZONE_DESIRABLE_MAX;
			if(!CropZoneAspectRatioOk(pBox, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height, TRUE))
				pNew->DebugCode |= DC_CROPZONE_NONPREF_ASPECT;
		}
		return 1;
	}
	MsgBox2("error - AddNewSourceLines_debug g_NumExportLines");
	return 0;
}

static void FlipPosition(BBOX *pBox, int HorizFlip, int VertFlip)
{
	BBOX Temp = *pBox;
	if(HorizFlip){
		pBox->x1 = 1.0f - Temp.x2;
		pBox->x2 = 1.0f - Temp.x1;
	}
	if(VertFlip){
		pBox->y1 = 1.0f - Temp.y2;
		pBox->y2 = 1.0f - Temp.y1;
	}
}

static void SnapCropZoneToTarget(long &pix1, long &pix2, int snap_size, float cz1, float cz2, float pic_size)
{
	int d = pix2 - pix1;
	if(abs(d-snap_size) == 1){
		//OutputDbgStr("\n snap %s: (%d - %d) = %d  [snap_size %d, cz1 %f, cz2 %f, d %f]", (d < snap_size)?"< < < <":">>>>>>",pix1,pix2,pix2-pix1,snap_size,(cz1*pic_size),(cz2*pic_size),(cz2*pic_size)-(cz1*pic_size));
		if(d > snap_size){
			if((float)pix1 < (cz1 * pic_size))
				pix1++;
			else if((float)pix2 > (cz2 * pic_size))
				pix2--;
		}
		else if(d < snap_size){
			// this scenario happens in maybe 1 in a 1000 cases, and i reckon the only reason it would happen 
			// is if the crop zone genuinely only could expand to a size of target-1. 
			// So the question is ...do i want to snap to target here or not? Also remember one pixel is tiny/insignificant anyway.
			// ..decided to keep it, i looked the only example, crop zone *width* was 352, and it 'would have been' UNABLE to reach *height* 352 because
			//                       (due to fp expansion precision) it would have gone very slightly over the 1.0 min aspect ratio, so stopped at 351.
			if((float)pix1 > (cz1 * pic_size))
				pix1--;
			else if((float)pix2 < (cz2 * pic_size))
				pix2++;
		}
		//OutputDbgStr("\n ___to___(%d - %d) = %d", pix1, pix2, pix2-pix1);
	}
}

static RECT CropZoneToPixelRECT(FILENAMEGROUP *pFnG, BBOX *pCropZone)
{
	RECT cz = {0};
	if(CropZoneIsFullPicture(pCropZone)){
		cz.right  = pFnG->pPicInfo->Width;
		cz.bottom = pFnG->pPicInfo->Height;
	}
	else{
		float PicWd = (float)pFnG->pPicInfo->Width, PicHt = (float)pFnG->pPicInfo->Height;
	//	BboxToBitmapCoordinates_Inc_Exc(cz.left, cz.right, cz.top, cz.bottom, PicWd, PicHt, pCropZone, TRUE); params are int, RECT is LONG. maybe do long version, function overload.
	
		// initially i thought round was a bad idea here, but thinking again it would be wrong without it.
		cz.left   = (int)round(pCropZone->x1 * PicWd);
		cz.right  = (int)round(pCropZone->x2 * PicWd);
		cz.top    = (int)round(pCropZone->y1 * PicHt);
		cz.bottom = (int)round(pCropZone->y2 * PicHt);
		
		// pCropZone width height can be up to '< (target + 1)', see BoxSizesInCropZoneOk(..) to understand why...
		// ..it has to target plus one because expansion moves in one pixel increments, but using floating point precision, and say target was 100 and the last crop zone expansion size was 
		// 99.01, in this scenario it wouldnt be able to get to the target size without the plus one, blah, blah.
		
		// remember crop zone can be any size, limited by obstacles, boundaries, aspect ratio and box size.

		SnapCropZoneToTarget(cz.left, cz.right, g_pMO->General.TargetWidth, pCropZone->x1, pCropZone->x2, PicWd);
		SnapCropZoneToTarget(cz.top, cz.bottom, g_pMO->General.TargetHeight, pCropZone->y1, pCropZone->y2, PicHt);

		// abs==1 is still possible after SnapCropZoneToTarget if crop zone size is slightly greater than 'target + 1', (possible but very rare).
		// example from OutputDbgStr above :
		//  snap >>>>>>: (926 - 1375) = 449  [snap_size 448, cz1 925.858704, cz2 1375.000000, d 449.141296]
		// ___to___(926 - 1375) = 449
		if(abs((cz.right-cz.left)-g_pMO->General.TargetWidth)==1 || abs((cz.bottom-cz.top)-g_pMO->General.TargetHeight)==1)
			OutputDbgStr("\n SnapCropZoneToTarget abs==1");

		cz.left   = max(cz.left, 0);
		cz.top    = max(cz.top,  0);
		cz.right  = min(cz.right,  pFnG->pPicInfo->Width);
		cz.bottom = min(cz.bottom, pFnG->pPicInfo->Height);

		if(cz.right-cz.left>pFnG->pPicInfo->Width || cz.bottom-cz.top>pFnG->pPicInfo->Height)
			MsgBox2("error - CropZoneToPixelRECT boundaries");
	}
	return cz;
}

static int AddSubfolderToFolder(char *pFolderPath, int BufferCount, char *pSubfolder)
{
	size_t len = strlen(pFolderPath);
	if(pFolderPath[len-1]=='\\' || pFolderPath[len-1]=='/') // i dont use '/' anywhere, so dont actually need to check for it.
		sprintf_s(pFolderPath, BufferCount, "%s%s", pFolderPath, pSubfolder);
	else
		sprintf_s(pFolderPath, BufferCount, "%s\\%s", pFolderPath, pSubfolder);

	if(!CreateDirectory(pFolderPath, NULL)){
		if(GetLastError() != ERROR_ALREADY_EXISTS){
			MsgBox2("error - AddSubfolderToFolder failed, \r\n%s\r\n%s", pFolderPath, pSubfolder);
			return 0;
		}
	}
	return 1;
}

static int LoadSourceLineRepeats(FILENAMEGROUP *pFnG, char *pNewPath, int NewPicWd, int NewPicHt, SOURCELINEREPEATS *pRep)
{
	*pRep = *pFnG->pPicInfo;
	int Len = (int)strlen(g_pPictureFolder) + 1;
	pRep->pfile_name     = GetPathBuffer(&pNewPath[Len]);	// relative path
	pRep->Width          = NewPicWd;
	pRep->Height         = NewPicHt;
	pRep->Pic_classified = CLASSIFIED_ACCEPT;
	pRep->Time_Stamp     = 0;
	if(pRep->pfile_name)
		return 1;
	MsgBox2("error - LoadSourceLineRepeats");
	return 0;
}

static int AddNewSourceLines(FILENAMEGROUP *pFnG, char *pNewPath, BBOX *pCropZone, int NewPicWd, int NewPicHt, int NewPicHorizontalFlip, RECT *pBlurRt, int &NumBlurRt, int MaxBlurRt)
{
	NumBlurRt = 0;
	int NumGoodObjects = 0;
	if(g_ExportMode == DEBUG_EXPORT_FULL){
		// note, FullPicGoodOnTargetExport will have a 'full picture' crop zone, used just for the debug display.
		AddNewSourceLines_debug(pFnG, TRUE, pCropZone, g_CZdisplay_Id, 1, NewPicHorizontalFlip); // crop zone annotation.
	//	if(NewPicHorizontalFlip)
	//		return 1;// horiz flip is for augmentation, annotations below are duplicates, so no point exporting them when horiz flip and DEBUG_EXPORT_FULL - THIS ISNT HELPFUL, NewPicHorizontalFlip doesnt always mean duplicates.
	}

	SOURCELINEREPEATS Rep={0};
	int Init_SourceLineRepeats = 0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){

		if(g_ExportMode == DEBUG_EXPORT_FULL){
			if(pSL->FilenameCount&CZ_CURRENT_GRP_GOOD)
				NumGoodObjects++;
			else if(pSL->FilenameCount&CZ_CURRENT_GRP_BLUR){
				if(pSL->rsvd_best == 0)
					MsgBox2("error - AddNewSourceLines, rsvd_best/blur");
				if(pSL->ClassId!=g_Blur_Id && g_BadOnTargetBlurred_Id!=0) // highlight by 'double classing' good objects (but bad-on-target) that have been converted into blur boxes.
					AddNewSourceLines_debug(pFnG, FALSE, &pSL->Box, g_BadOnTargetBlurred_Id, 0, NewPicHorizontalFlip);
				NumBlurRt++;
			}
		}
		else if(g_ExportMode == PIC_EXPORT_YES){
			if(pSL->FilenameCount&CZ_CURRENT_GRP_GOOD){
				// good box, add a new sourceline for it.
				if(g_NumExportLines < g_pSpare->NumSourceLines){
					SOURCELINE *pNew = &g_pSpare->pSourceLines[g_NumExportLines++];
					*pNew = *pSL;
					if(!Init_SourceLineRepeats){
						if(!LoadSourceLineRepeats(pFnG, pNewPath, NewPicWd, NewPicHt, &Rep))
							return 0;
						Init_SourceLineRepeats = 1;
					}
					pNew->Repeats    = Rep;
					pNew->Box        = ObjectToCropZone(&pSL->Box, pCropZone);
					pNew->DebugCode |= DC_NEW_ANNOTATION;
					NumGoodObjects++;
					if(NewPicHorizontalFlip) FlipPosition(&pNew->Box, 1, 0);
				}
				else{
					MsgBox2("error - AddNewSourceLines g_NumExportLines");
					return 0;
				}
			}
			else if(pSL->FilenameCount&CZ_CURRENT_GRP_BLUR){
				// bad box to be blurred.
				if(NumBlurRt < MaxBlurRt){
					BBOX bx = ObjectToCropZone(&pSL->Box, pCropZone);
					if(NewPicHorizontalFlip) FlipPosition(&bx, 1, 0);
					rect_BboxToBitmapCoordinates_Inc_Exc(pBlurRt[NumBlurRt], NewPicWd, NewPicHt, &bx, TRUE);
					NumBlurRt++;
				}
				else{
					MsgBox2("error - AddNewSourceLines NumBlurRt");
					return 0;
				}
			}
		}
	}
	if(NumGoodObjects > 0)
		return 1;
	MsgBox2("error - AddNewSourceLines\r\n NumGoodObjects %d, NumBlurRt %d", NumGoodObjects, NumBlurRt);
	return 0;
}

static int ExportAnnotationsAndCroppedPic(FILENAMEGROUP *pFnG, BBOX *pCropZone, int HasBlurBox, int HorizontalFlip, int UsingGoodOnOriginal, int FullPicAugmentation) // pCropZone is optional
{
	// returns zero if error, otherwise 1.
	if(pCropZone == nullptr){
		MsgBox2("error - ExportAnnotationsAndCroppedPic pCropZone == nullptr");
		return 0;
	}
	if(!FullPicAugmentation && !HasBlurBox && CropZoneIsFullPicture(pCropZone)){
		// crop zone should never be full picture size if there are no blur boxes, because that would equate to no change compared to the original pic and annotations.
		MsgBox2("error - ExportAnnotationsAndCroppedPic !HasBlurBox");
		return 0;
	}
	if(g_ExportMode == DEBUG_EXPORT_FULL){
		int unused=0;
		return AddNewSourceLines(pFnG, nullptr, pCropZone, 0, 0, HorizontalFlip, nullptr, unused, 0);
	}

	if(g_ExportMode == PIC_EXPORT_YES){
		static char SourcePath[MAX_PATH], NewPath[MAX_PATH];
		FolderAndFilenameToPath(SourcePath, num_entries(SourcePath), g_pPictureFolder, pFnG->pPicInfo->pfile_name);
		// exported pics will be placed in the base picture folder in a sub folder named EXPORTS_FOLDER_NAME, and then in sub 
		// folders inside this folder which replicate the folder structure of the original pics.
		FolderAndFilenameToPath(NewPath, num_entries(NewPath), g_ExportFolder, pFnG->pPicInfo->pfile_name);

		char Filename[MAX_PATH];
		char *pFn = PointerToFilename(NewPath);
		strcpy_s(Filename, num_entries(Filename), pFn);
		*pFn = 0; // null terminate the folder string, in other words - remove the filename from NewPath.

		if(CreateNestedFolders(NewPath, g_ExportFolder)){
			if(NewPath[strlen(NewPath)-1] != '\\')
				strcat_s(NewPath, num_entries(NewPath), "\\");

			int IsFullPic = CropZoneIsFullPicture(pCropZone);
			char FilenameAddon[32];
			if(FullPicAugmentation)
				sprintf_s(FilenameAddon, num_entries(FilenameAddon), "~%d%s~NC", g_Export_FilenameCount++, HorizontalFlip?"~h":"");
			else
				sprintf_s(FilenameAddon, num_entries(FilenameAddon), "~%d%s%s%s%s%s%s", g_Export_FilenameCount++, 
						  HorizontalFlip?"~h":"", HasBlurBox?"~b":"", IsFullPic?"~f":"", UsingGoodOnOriginal?"~o":"~t", g_Limits.BoxPix.UseDesirableMin?"":"~x", g_PictureRejected?"":"~a"); // '~a' to signify classify accept was not changed to classify special.

			char *pExt = PointerToExtension(Filename);
			*(pExt - 1) = 0; // remove the extension by putting null at end of filename (on the dot).
	
			// ALWAYS SAVE AS PNG, except maybe if original is uncompressed/bmp, then save as jpg and png (randomly).
			sprintf_s(NewPath, num_entries(NewPath), "%s%s%s.png", NewPath, Filename, FilenameAddon);

			#define MAXBLURRTS	50
			static RECT BlurRt[MAXBLURRTS];
			int NumBlurRt=0;
			RECT cz;
			cz = CropZoneToPixelRECT(pFnG, pCropZone);
			if(AddNewSourceLines(pFnG, NewPath, pCropZone, cz.right-cz.left, cz.bottom-cz.top, HorizontalFlip, BlurRt, NumBlurRt, MAXBLURRTS)){
				BMP_SPECIAL_FX SE;
				SE.pBoxes = BlurRt;
				SE.NumBoxes = NumBlurRt;
				// save bitmap, there is no resizing (stretching) here, the cropped bitmap we save will be the same size as the crop zone area in the original bitmap.
				if(g_CBmpFile.SaveCroppedBitmap(SourcePath, cz, NewPath, cz.right-cz.left, cz.bottom-cz.top, HorizontalFlip, &SE))
					return 1;
			}
		}
		MsgBox2("error - ExportAnnotationsAndCroppedPic\r\n %s\r\n %s", SourcePath, NewPath);
		return 0;
	}
	MsgBox2("error - ExportAnnotationsAndCroppedPic g_ExportMode");
	return 0;
}

#ifdef unused_atm
static int AddRandomCentering(float &CropZone_p1, float &CropZone_p2, float MinSizeCropZone_p1, float MinSizeCropZone_p2)
{
	g_Limits.CenteringVariability = 0.5f; // constant between 0.0 and 1.0.

	// p1 p2 etc, 'p' stands for position, because this can be used with either axis, ie, 'x' or 'y'.
	float CZ_Dp     = CropZone_p2 - CropZone_p1;
	float Min_CZ_Dp = MinSizeCropZone_p2 - MinSizeCropZone_p1; // ie, cropped around boxes with no border.

	if(CZ_Dp > Min_CZ_Dp){
		float New_p1, New_p2;
		New_p1  = (MinSizeCropZone_p1 + MinSizeCropZone_p2) * 0.5f; // divided by two ..middle of MinSizeCropZone
		New_p1 -= (CZ_Dp * 0.5f);
		New_p2  = New_p1 + CZ_Dp;

		// g_Limits.CenteringVariability - will be less than 1.0 because i dont all the variability used, I dont want objects placed right on the 
		// edge of the crop zone, it might be worth using different constants depending on how many objects, closer to 1.0 if its a single object.
		float TotalVariability = (CZ_Dp - Min_CZ_Dp) * g_Limits.CenteringVariability;
		float Half_TotalVariability = TotalVariability * 0.5f; // half because I need the total variability of one side.
		float RandNormalised = RANDOM_NORMALISED;
		if(RandNormalised<0.0f || RandNormalised>1.0f || TotalVariability<0.0f || TotalVariability>1.0f)
			MsgBox2("error - RANDOM_NORMALISED, %f %f", RandNormalised, TotalVariability);

		float RandomOffset = (TotalVariability * RandNormalised) - Half_TotalVariability;
		New_p1 += RandomOffset; // RandomOffset - values from: negative Half_TotalVariability to positive Half_TotalVariability.
		New_p2 += RandomOffset;

		if(New_p2 > CropZone_p2){
			New_p2 = CropZone_p2;
			New_p1 = New_p2 - CZ_Dp;
		}
		else if(New_p1 < CropZone_p1){
			New_p1 = CropZone_p1;
			New_p2 = New_p1 + CZ_Dp;
		}
		CropZone_p1 = New_p1;
		CropZone_p2 = New_p2;
		return 1;
	}
	return 0;
}
#endif

static float ConvertTargetPixelSizeToCropZone(int TargetSize_Pixels, float CropZoneSize)
{
	// convert this target pixel size, to 0.0-1.0 value on the FULL PIC but scaling for crop zone.
	float constPixelsOnTarget = (float)g_Limits.BlurConvertedBox_ABSMinPixelDist;
	float target_pix = (float)TargetSize_Pixels;
	float val = constPixelsOnTarget / target_pix; // from pixel to 0.0-1.0, height on target (and original).

	// say our limit is 15 pixels, now say this is 0.1 on the target, whatever the size of the crop zone, it is re-sized to target, so 15 pixels will be 0.1 within the crop zone too.
	// (the only exception to this rule is if we want to account/bias for upscaling, but i dont think that is relevent, i'm interested in the physical distance not the pixel detail).
	//
	// WRONG ....thats misunderstanding what this function is doing, 
	// THIS FUNCTION is getting the crop zone equivalant 0.0/1.0 min size on the FULL PICTURE, so yes the below scale to crop zone is necessary.

	// scale to crop zone.
	val = val * CropZoneSize; // height on crop zone.
	return val;
}

static float BlurConvertedBoxes_OverlapOk(FILENAMEGROUP *pFnG, BBOX *pCropZone)
{
	// boxes that are classified/designed as blur boxes (i.e with class id == g_Blur_Id) are not considered here, they are allowed to overlap.
	// this function only tests boxes that have been converted into blur boxes because they are too small.

	float MaxOverlap = 0.0f; // @@@@@@ CURRENTLY NOT ALLOWING ANY OVERLAP
	// MaxOverlap - max overlap of a good box (0.0 to 1.0) by a converted blur box, so if MaxOverlap was 1.0 the good box could be entirely covered.
	//              
	// border - border added to good box, so bad boxes can be kept away slightly. 
	//          THIS IS NOT QUITE logical (or maybe its ok), adding a border would affect the percentage overlap calculated for MaxOverlap, so what, as long as the calculation is consistent for all?
	//            ..however I dont allow any overlap at the moment anyway.

	float absMinBorder_Y = ConvertTargetPixelSizeToCropZone(g_pMO->General.TargetHeight, pCropZone ? (pCropZone->y2-pCropZone->y1) : 1.0f);
	float absMinBorder_X = ConvertTargetPixelSizeToCropZone(g_pMO->General.TargetWidth,  pCropZone ? (pCropZone->x2-pCropZone->x1) : 1.0f);

	for(int s=0; s<pFnG->NumSourceLines; s++){
		if(pFnG->pSourceLines[s].rsvd_best && (pFnG->pSourceLines[s].FilenameCount&CZ_CURRENT_GRP_GOOD)){
			BBOX bx = pFnG->pSourceLines[s].Box;
			float w = bx.x2 - bx.x1;
			float h = bx.y2 - bx.y1;

			float bdr_x = max(w*g_Limits.BlurConvertedBox_MinDistance, absMinBorder_X);
			float bdr_y = max(h*g_Limits.BlurConvertedBox_MinDistance, absMinBorder_Y);
			bx.x1 -= bdr_x;
			bx.x2 += bdr_x;
			bx.y1 -= bdr_y;
			bx.y2 += bdr_y;

			float TotalOverlap = 0.0f;
			SOURCELINE *pSL = pFnG->pSourceLines;
			for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
				if(pSL->FilenameCount&CZ_CURRENT_GRP_BLUR){
					if(pSL->rsvd_best == 0)
						MsgBox2("error - BlurConvertedBoxes_OverlapOk"); // 'all probs' are not saved/flagged for CZ_CURRENT_GRP_BLUR.

					// ignor objects with this 'special exemption' id which allows overlap.
					if(pSL->ClassId != g_Blur_Id)
						TotalOverlap += box_intersection(&bx, &pSL->Box);
				}
			}
			float A = w * h;
			float Overlap = TotalOverlap / A;
			// this is not perfect because any overlap of the blur boxes themselves will get double counted (ie, 2 blur boxes overlapping each 
			// other and the good box), but that doesnt matter at all for this purpose.
			Overlap = min(Overlap, 1.0f);
			if(Overlap > MaxOverlap)
				return 0;
		}
	}
	return 1;
}

#define GRID_SIZE 1000  // Increase for better precision
static void FreeGrid()
{
	// Free memory
	if(g_grid){
		for (int i = 0; i < GRID_SIZE; i++)
			free(g_grid[i]);
		free(g_grid);
	}
	g_grid = nullptr;
}

static int TotalBlurredBoxAreaOk(FILENAMEGROUP *pFnG, BBOX *pCropZone, float MaxArea)
{
	if(g_grid == nullptr){
		g_grid = (int**)malloc(GRID_SIZE * sizeof(int*));
		for(int i = 0; i < GRID_SIZE; i++)
			g_grid[i] = (int*)malloc(GRID_SIZE * sizeof(int));
	}
	for(int x = 0; x < GRID_SIZE; x++)
		for(int y = 0; y < GRID_SIZE; y++)
			g_grid[x][y] = 0;

	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->FilenameCount&CZ_CURRENT_GRP_BLUR){ // note this flag is only given to rsvd_best, and obviously we dont want to add the area of an object twice if its double classed.

			BBOX Bx = ObjectToCropZone(&pSL->Box, pCropZone);
			float Area = (Bx.x2 - Bx.x1) * (Bx.y2 - Bx.y1);
			if(Area > MaxArea)
				return 0;
			g_Limits.dbg_MaxAreaSingle = max(g_Limits.dbg_MaxAreaSingle, Area);

			int x_start = (int)(Bx.x1 * GRID_SIZE);
			int x_end   = (int)(Bx.x2 * GRID_SIZE);
			int y_start = (int)(Bx.y1 * GRID_SIZE);
			int y_end   = (int)(Bx.y2 * GRID_SIZE);
			// Clamp within g_grid
			if(x_start < 0) x_start = 0;
			if(y_start < 0) y_start = 0;
			if(x_end > GRID_SIZE) x_end = GRID_SIZE;
			if(y_end > GRID_SIZE) y_end = GRID_SIZE;

			for(int x = x_start; x < x_end; x++)
				for(int y = y_start; y < y_end; y++)
					g_grid[x][y] = 1;  // Mark cell as covered
		}
	}
	// Count covered cells
	int covered = 0;
	for(int x = 0; x < GRID_SIZE; x++)
		for(int y = 0; y < GRID_SIZE; y++)
			if(g_grid[x][y]) covered++;

	float cell_area = 1.0f / (GRID_SIZE * GRID_SIZE);
	float CombinedArea = (float)covered * cell_area;

	if(CombinedArea > MaxArea)
		return 0;
	g_Limits.dbg_MaxAreaCombined = CombinedArea;
	return 1;
}

static void InitBlurBoxDebugFlags()
{
	g_Limits.dbg_MaxAreaSingle = 0.0f;
	g_Limits.dbg_MaxAreaCombined = 0.0f;
}

static int PictureWithBlurBoxingOk(FILENAMEGROUP *pFnG, BBOX *pCropZone, int HasBlurConvertedBoxes)
{
	// functions below use CZ_CURRENT_GRP_GOOD and CZ_CURRENT_GRP_BLUR, so these flags must be set before calling this function.
	if(HasBlurConvertedBoxes){
		if(BlurConvertedBoxes_OverlapOk(pFnG, pCropZone))
			if(TotalBlurredBoxAreaOk(pFnG, pCropZone, g_Limits.BlurConvertedBox_MaxArea))
				return 1;
	}
	else{
		if(!g_Limits.BlurBox_DoAreaCalculation) // and there are no converted ones either cos HasBlurConvertedBoxes == FALSE, so return Ok.
			return 1;
		if(TotalBlurredBoxAreaOk(pFnG, pCropZone, g_Limits.BlurBox_MaxArea))
			return 1;
	}
	return 0;
}

static void ClearCurrentGroupFlags(FILENAMEGROUP *pFnG)
{
	for(int i=0; i<pFnG->NumSourceLines; i++){
		// these flags are for flagging boxes in the current crop zone, so they must be cleared from the previous usage (crop zone). 
		// Flags for all crop zones ( CZ_ALL_GROUPS_GOOD ) will persist,
		// also the CZ_SEED_BOX_BIGGEST flags must persist as well, hence why cant do pSL->FilenameCount = 0;
		pFnG->pSourceLines[i].FilenameCount &= ~CZ_CURRENT_GRP_GOOD;
		pFnG->pSourceLines[i].FilenameCount &= ~CZ_CURRENT_GRP_BLUR;
	}
}

static int FlagBoxesForExport(FILENAMEGROUP *pFnG, BBOX *pCropZone, CURRENT_CZ_STATS *pCZS)
{
	// this function will also test cz expansion worked correctly and didnt truncate any boxes it shouldnt've.
	memset(pCZS, 0, sizeof(CURRENT_CZ_STATS));
	pCZS->MinBoxHt_InCZ = pCZS->MinBoxWd_InCZ = pCZS->MinBoxHt = pCZS->MinBoxWd = 1.0f;

	ClearCurrentGroupFlags(pFnG);
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		// This will (and must) go through all 'ALL PROBABILITIES' (required for the good boxes, only best prob kept for blur boxes), it would be 
		// more efficient to copy the flags into the all probs, but i'll leave it like this for now.
		int Inside=0, ovlp_L=0, ovlp_R=0, ovlp_T=0, ovlp_B=0;
		BoxOverlapsCropZone(pCropZone, &pSL->Box, Inside, ovlp_L, ovlp_R, ovlp_T, ovlp_B);

		if(ovlp_L || ovlp_R || ovlp_T || ovlp_B){
			if(g_AllowBlurBoxesInCropZone){
				if(pSL->ClassId != g_Blur_Id){
					// no boxes can be overlapping cz other than g_Blur_Id boxes.
					MsgBox2("error - FlagBoxesForExport, ori %d, id %d (r%d, b%d)", (pSL->DebugCode&DC_GOOD_ON_ORIGINAL)!=0, pSL->ClassId, g_Remove_Id, g_Blur_Id);
					return 0;
				}
				if(pSL->rsvd_best){ // 'all probs' not required for a blur box.
					pSL->FilenameCount |= CZ_CURRENT_GRP_BLUR; // flag box for blur.
					pCZS->Num_BlurBoxes++;
				}
			}
			else{
				// error - if we're *not* doing g_AllowBlurBoxesInCropZone no boxes (good or bad or blur or remove) can be truncated by the crop zone.
				MsgBox2("error - FlagBoxesForExport, bad");
				return 0;
			}
		}
		else if(Inside){
			if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL){
				// flag boxes inside the current crop zone so their annotations can be added to the export sourcelines.
				pSL->FilenameCount |= CZ_CURRENT_GRP_GOOD;

				pCZS->NumGoodBoxes++; // (remember, each probabability is still an individual box/object in it own right).
				if(pSL->rsvd_best){
					pCZS->NumGoodBoxes_ExcAllProbs++;
					BBOX Bx = pSL->Box; // getting size on full pic, not size within crop zone, so dont use - ObjectToCropZone(&pSL->Box, pCropZone);
					pCZS->MinBoxHt = min(pCZS->MinBoxHt, Bx.y2-Bx.y1);
					pCZS->MinBoxWd = min(pCZS->MinBoxWd, Bx.x2-Bx.x1);
					Bx = ObjectToCropZone(&pSL->Box, pCropZone);
					pCZS->MinBoxHt_InCZ = min(pCZS->MinBoxHt_InCZ, Bx.y2-Bx.y1);
					pCZS->MinBoxWd_InCZ = min(pCZS->MinBoxWd_InCZ, Bx.x2-Bx.x1);
				}
			}
			else{
				// this is a blur box inside the crop zone.
				if(g_AllowBlurBoxesInCropZone==0 || // error, blur boxes not allowed.
					pSL->ClassId!=g_Blur_Id ){		// or, error, this is not a blur box.
					MsgBox2("error - FlagBoxesForExport, box inside cz thats not allowed");
					return 0;
				}
				if(pSL->rsvd_best){ // 'all probs' not required for a blur box.
					pSL->FilenameCount |= CZ_CURRENT_GRP_BLUR; // flag box for blur.
					pCZS->Num_BlurBoxes++;
				}
			}
		}
	}
	if(pCZS->NumGoodBoxes > 0)
		return 1;
	MsgBox2("error - FlagBoxesForExport, NumGoodBoxes");
	return 0;
}

#ifdef defunct_stuff
static void GetCropZoneFlags(FILENAMEGROUP *pFnG, CROPZONE_RECORD *pCR)
{
/*	ClearCurrentGroupFlags(pFnG);
	for(int i=0; i<pFnG->NumSourceLines; i++){
			 if(pCR->CurrentGroupFlags[i]&CZ_CURRENT_GRP_GOOD) pFnG->pSourceLines[i].FilenameCount |= CZ_CURRENT_GRP_GOOD;
		else if(pCR->CurrentGroupFlags[i]&CZ_CURRENT_GRP_BLUR) pFnG->pSourceLines[i].FilenameCount |= CZ_CURRENT_GRP_BLUR;
	}*/
	// the above is un-necessary.
	for(int i=0; i<pFnG->NumSourceLines; i++)
		pFnG->pSourceLines[i].FilenameCount = pCR->CurrentGroupFlags[i];
}

static int ExportFromCropZoneExportRecord(FILENAMEGROUP *pFnG, EXPORT_RECORD *pExRec)
{
	needs some changes to work if i use it again, 
		debug flags generated in CropZone_ExportFlags(..) would need to be copied into CROPZONE_RECORD similar to CurrentGroupFlags.
		also some other flags added to CROPZONE_RECORD like HorizontalFlip
	int totalBoxExport=0, totalExports=0;
	for(int i=0; i<pExRec->NumCropZoneExports; i++){
		CROPZONE_RECORD *pCR = &pExRec->CZlist[i];
		g_AllowBlurBoxesInCropZone = pCR->AllowBlurBoxesInCZ;
		g_CZdisplay_Id = pCR->debugCZid;
		GetCropZoneFlags(pFnG, pCR);
		if(!ExportAnnotationsAndCroppedPic(pFnG, &pCR->cropzone, pCR->NumBlur, 0, pCR->UsingGoodOnOriginal))
			return 0;
		totalBoxExport += pCR->NumGood;
		totalExports++;
	}
	if(totalBoxExport!=pExRec->NumBoxesExported || totalExports!=pExRec->NumCropZoneExports){
		MsgBox2("error - ExportFromCropZoneExportRecord"); // real export should match the EXPORT_RECORD, but it didnt.
		return 0;
	}
	return 1;
}
#endif

static int LoadCropZoneExportRecord(FILENAMEGROUP *pFnG, BBOX *pCropZone, int NumGoodBoxes, int NumBlur_Boxes, int UsingGoodOnOriginal, EXPORT_RECORD *pExRec)
{
	if(pExRec->NumCropZoneExports < MAX_CZLIST){
		CROPZONE_RECORD *pCR = &pExRec->CZlist[pExRec->NumCropZoneExports];
		memset(pCR, 0, sizeof(CROPZONE_RECORD));
		if(pFnG->NumSourceLines < MAX_BOX_IN_CZ){
			for(int i=0; i<pFnG->NumSourceLines; i++)
				pCR->CurrentGroupFlags[i] = pFnG->pSourceLines[i].FilenameCount;
			pCR->cropzone = *pCropZone;
			pCR->debugCZid = g_CZdisplay_Id;
			pCR->UsingGoodOnOriginal = UsingGoodOnOriginal;
			pCR->AllowBlurBoxesInCZ = g_AllowBlurBoxesInCropZone;
			pCR->NumGood = NumGoodBoxes;
			pCR->NumBlur = NumBlur_Boxes;
		}
		else{
			MsgBox2("error - LoadCropZoneExportRecord MAX_BOX_IN_CZ");
			return 0;
		}
	}
	else{
		MsgBox2("error - LoadCropZoneExportRecord MAX_CZLIST");
		return 0;
	}
	pExRec->NumBoxesExported += NumGoodBoxes; // can be more than the nunmber of boxes(objects) in crop zones overlap.
	pExRec->NumCropZoneExports++;
	return 1;
}

static void AddPassedCurrentGroupToAllGroups(FILENAMEGROUP *pFnG)
{
	for(int i=0; i<pFnG->NumSourceLines; i++)
		if(pFnG->pSourceLines[i].FilenameCount&CZ_CURRENT_GRP_GOOD)
			pFnG->pSourceLines[i].FilenameCount |= CZ_ALL_GROUPS_GOOD;
}

static int DesirableMin_CropZoneWanted(FILENAMEGROUP *pFnG, BBOX *pCropZone, EXPORT_RECORD *pExRec)
{
	// UseDesirableMin==true : crop zone doesnt need to be tested (it is wanted).
	if(g_Limits.BoxPix.UseDesirableMin)
		return 1;
	
	// UseDesirableMin==false : decide if crop zone is wanted, return 1 if it is, otherwise zero.
	if(g_Limits.BoxPix.DesirableMaxFactor<=0.0f || g_Limits.BoxPix.DesirableMaxFactor>=1.0f)
		MsgBox2("error - DesirableMinOff_CropZoneWanted");
	BBOX Bx;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->FilenameCount&CZ_CURRENT_GRP_GOOD){
			// box size within pCropZone
			Bx = ObjectToCropZone(&pSL->Box, pCropZone);
			float new_wd = (Bx.x2 - Bx.x1);
			float new_ht = (Bx.y2 - Bx.y1);
			BOOL AlreadyCropped = FALSE;
			float existing_wd=1.0f, existing_ht=1.0f;
			for(int cz=0; cz<pExRec->NumCropZoneExports; cz++){
				CROPZONE_RECORD *pCR = &pExRec->CZlist[cz];
				if(pCR->CurrentGroupFlags[i]&CZ_CURRENT_GRP_GOOD){
					// found a crop zone where box [i] has already been cropped.
					Bx = ObjectToCropZone(&pSL->Box, &pCR->cropzone);
					existing_wd = min(existing_wd, Bx.x2-Bx.x1);
					existing_ht = min(existing_ht, Bx.y2-Bx.y1);
					AlreadyCropped = TRUE;
				}
			}
			if(!AlreadyCropped){
				// if the box hasnt been cropped at all in any previous crop zone we want this crop zone.
				// sometimes there are boxes that are un-croppable in desirable min true mode - which often happens when tiny objects are positioned close to 
				// big objects therefore meaning its impossible to keep the crop zone small.
				return 1;
			}
			// UseDesirableMin==true can make some objects too big, because it is intended to maximise the size of small objects.
			// now we are using UseDesirableMin==false, which is for the benefit of big objects, i.e, making them smaller, so...
			// ...need to see if box size is significantly *smaller* with this crop zone than it was with any UseDesirableMin==true crop zone (or any previous cz in fact).
			//
			// existing_wd and existing_ht is the smallest size for box[i] in any previous crop zone, if box[i] is significantly smaller
			// in the current crop zone proposal return true.
			if( existing_wd>g_Limits.BoxPix.DesirableMaxLimit && 
				new_wd<existing_wd*g_Limits.BoxPix.DesirableMaxFactor)
			{
				//if(new_ht <= existing_ht ??? dont think this is necessary.
				return 1;
			}
			if( existing_ht>g_Limits.BoxPix.DesirableMaxLimit && 
				new_ht<existing_ht*g_Limits.BoxPix.DesirableMaxFactor)
			{
				//if(new_wd <= existing_wd ??? dont think this is necessary.
				return 1;
			}
		}
	}
	return 0;
}

static int CropZone_Export(FILENAMEGROUP *pFnG, BBOX *pCropZone, EXPORT_RECORD *pExRec, CURRENT_CZ_STATS *pCZS, BOOL UsingGoodOnOriginal, BOOL HorizontalFlip)
{
	//if(UsingGoodOnOriginal) not necessary currently to call this from FullPicGoodOnTargetExport because it comes last, but it does no harm.
	{
		AddPassedCurrentGroupToAllGroups(pFnG); 
	}
	// add some export flag to the original annotations.
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->FilenameCount&CZ_CURRENT_GRP_GOOD){
			// this is adding DebugCode flags to the original pic annotations, and because exported pic annotations  
			// copy the flags from the originals, flags will also be in the exported pic annotations.
			pSL->DebugCode |= DC_EXPORTED_ANNOTATION;
			if(HorizontalFlip)
				pSL->DebugCode |= DC_EXPORTED_ANNOTATION_FLIP;
			/*	BBOX Bx = ObjectToCropZone(&pSL->Box, pCropZone);
			if(DesirableMinOff_Ok(pCropZone, Bx.x2-Bx.x1, Bx.y2-Bx.y1))
			pSL->DebugCode |= DC_DESIRABLE_MAX_OK; */
		}
	}
	if(!LoadCropZoneExportRecord(pFnG, pCropZone, pCZS->NumGoodBoxes, pCZS->Num_BlurBoxes, UsingGoodOnOriginal, pExRec))
		return 0;
	if(!ExportAnnotationsAndCroppedPic(pFnG, pCropZone, pCZS->Num_BlurBoxes, HorizontalFlip, UsingGoodOnOriginal, FALSE))
		return 0;
	return 1;
}

static int PicAccept_CropMaxOk(BBOX *pCropZone)
{
	// if crop zone is barely any smaller than the full picture theres no point cropping when the picture is classified as accept.
	return (pCropZone->x2-pCropZone->x1<=g_Limits.Acc.MaxCZ || pCropZone->y2-pCropZone->y1<=g_Limits.Acc.MaxCZ);
}

static int PicAccept_BoxProportionsOk(CURRENT_CZ_STATS *pCZS)
{
	// another check for non-rejected-picture crop zones, we've done the WorthyReduction check above, but also there is no point having a crop zone that only crops an object
	// thats massive in the full picture. (Ok one could say have it anyway for the sake of augmentation but thats not the aim here).
	if(pCZS->MinBoxWd<g_Limits.Acc.MaxBoxSize && pCZS->MinBoxHt<g_Limits.Acc.MaxBoxSize)
		if(pCZS->MinBoxWd_InCZ<g_Limits.Acc.MaxBoxSize && pCZS->MinBoxHt_InCZ<g_Limits.Acc.MaxBoxSize)
			return 1;
	return 0;
}

static int MakeCropZoneAndExport(FILENAMEGROUP *pFnG, SOURCELINE *pSeedBox, BBOX *pInitialCZ, int MultiBoxCropping, EXPORT_RECORD *pExRec)
{
	BBOX CropZone, initial;
	if(MultiBoxCropping){
		pSeedBox = nullptr;
		CropZone = initial = *pInitialCZ;
	}
	else
		CropZone = initial = pSeedBox->Box;

	if(ExpandCropZone(pFnG, pSeedBox, &CropZone, TRUE)){
		int Wd, Ht;
		GetBoxInPixels(pFnG, CropZone, Wd, Ht);
		if(Ht >= g_Limits.Pixel_MinPicHeight){
			// CropZoneAspectRatioOk - ExpandCropZone doesnt check the final crop zone aspect, it just aims towards the aspect 
			// ratio limits by expanding one axis if the other axis falls outside limits.
			if(CropZoneAspectRatioOk(&CropZone, (float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height, g_Limits.AllCropZonesPreferredAspect)){

				// if the picture is rejected then a full-pic export is acceptable because it will have blur boxes in it, (logic error checks for this are done in ExportAnnotationsAndCroppedPic).
				// if the picture is *not* rejected then only allow export if the picture is cropped by a considerable amount, i.e, its not almost the same size as the full pic.
				if(g_PictureRejected || PicAccept_CropMaxOk(&CropZone)){

					CURRENT_CZ_STATS CZS = {0};
					if(!FlagBoxesForExport(pFnG, &CropZone, &CZS))
						return 0; // error

					if(g_PictureRejected || PicAccept_BoxProportionsOk(&CZS)){

						// if MultiBoxCropping==true has croppped a single box then we need to try again with single box cropping because single box cropping should do a better job.
						if(!MultiBoxCropping || CZS.NumGoodBoxes_ExcAllProbs>1){
							// todo - AddRandomCentering(float &CropZone_p1, float &CropZone_p2, float MinSizeCropZone_p1, float MinSizeCropZone_p2)
							// also needs to know picture and other box boundaries/positions, so see where there is freedom to move the cz.
							// better idea ?? - random could be applied during crop zone expansion, so using random we fix the expansion speed of each direction (actually by stopping the opposite direction).

							if(DesirableMin_CropZoneWanted(pFnG, &CropZone, pExRec)){
								InitBlurBoxDebugFlags(); // must be called even if there are no blur boxes, so dont put in 'if' below.
								if(CZS.Num_BlurBoxes > 0){
									if(!PictureWithBlurBoxingOk(pFnG, &CropZone, FALSE)){ // check blur boxes are acceptable by area and position in relation to good boxes.
										g_AnyCzFailedBlurArea = 1;
										return 1; // no error, crop zone failed blur box tests.
									}
								}
								BOOL HorizontalFlip = (!g_Limits.BoxPix.UseDesirableMin || !g_PictureRejected);
								if(!CropZone_Export(pFnG, &CropZone, pExRec, &CZS, TRUE, HorizontalFlip))
									return 0; // error
							}
						}
					}
				}
			}
		}
	}
	return 1;
}

static int CropAllGoodBoxes(FILENAMEGROUP *pFnG, BBOX *pCropZone, SOURCELINE **ppLast)
{
	pCropZone->x1 = 1.0f;
	pCropZone->x2 = 0.0f;
	pCropZone->y1 = 1.0f;
	pCropZone->y2 = 0.0f;
	*ppLast = nullptr;
	int NumBoxes_ExcAllProbs = 0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(  pSL->rsvd_best && // necessary because we're counting unique boxes, excluding the all-probs.
			(pSL->DebugCode&DC_GOOD_ON_ORIGINAL) )
		{
			if( pSL->Box.x1>=g_ExpansionBoundaries.x1 && pSL->Box.x2<=g_ExpansionBoundaries.x2 && 
				pSL->Box.y1>=g_ExpansionBoundaries.y1 && pSL->Box.y2<=g_ExpansionBoundaries.y2 )
			{
				// all good-on-original included in crop zone.
				pCropZone->x1 = min(pCropZone->x1, pSL->Box.x1);
				pCropZone->x2 = max(pCropZone->x2, pSL->Box.x2);
				pCropZone->y1 = min(pCropZone->y1, pSL->Box.y1);
				pCropZone->y2 = max(pCropZone->y2, pSL->Box.y2);
				*ppLast = pSL;
				NumBoxes_ExcAllProbs++;
			}
		}
	}
	return NumBoxes_ExcAllProbs;
}

static SOURCELINE *GetNextGoodUntestedUnusedBox(FILENAMEGROUP *pFnG, int Min_else_Max)
{
	float Area, Extreme = Min_else_Max ? 1.0f : 0.0f;
	SOURCELINE *pSLextreme=nullptr, *pSL=pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->rsvd_best){
			if( (pSL->FilenameCount&CZ_SEED_BOX_BIGGEST)==0 && // so a crop zone seed box can only be used once.
				(pSL->FilenameCount&CZ_ALL_GROUPS_GOOD )==0 )  // so as not to include boxes that have already been exported in any crop zone, because its not just the seed box thats exported but all boxes contained in the cz.
			{
				if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL){
					Area = (pSL->Box.x2 - pSL->Box.x1) * (pSL->Box.y2 - pSL->Box.y1);
					if(Min_else_Max){
						if(Area < Extreme){ Extreme = Area; pSLextreme = pSL; }
					}
					else{
						if(Area > Extreme){ Extreme = Area; pSLextreme = pSL; }
					}
				}
			}
		}
	}
	if(pSLextreme)
		pSLextreme->FilenameCount |= CZ_SEED_BOX_BIGGEST;
	return pSLextreme;
}

static void ResetCropZoneFlags(FILENAMEGROUP *pFnG, int ResetSeedFlagOnly=0)
{
	// FilenameCount = 0 once at start then used for following export flags...
	// 
	// CZ_SEED_BOX_BIGGEST                      ...reset each time CropAndExport_FromSeedBox is used, for keeping record of tested seed boxes.
	// CZ_CURRENT_GRP_GOOD, CZ_CURRENT_GRP_BLUR ...reset and set for the current/latest crop zone.
	// CZ_ALL_GROUPS_GOOD                       ...record of all good boxes exported in any crop zone.
	if(ResetSeedFlagOnly){
		for(int i=0; i<pFnG->NumSourceLines; i++){
			if(pFnG->pSourceLines[i].FilenameCount&CZ_ALL_GROUPS_GOOD)
				pFnG->pSourceLines[i].FilenameCount = CZ_ALL_GROUPS_GOOD; // dont use |= because we are removing all flags except CZ_ALL_GROUPS_GOOD, so direct assignment.
			else
				pFnG->pSourceLines[i].FilenameCount = 0;
		}
	}
	else{
		for(int i=0; i<pFnG->NumSourceLines; i++)
			pFnG->pSourceLines[i].FilenameCount = 0;
	}
}

static void SetCZdisplayId(char *plabel)
{
	if(g_ExportMode != PIC_EXPORT_YES)
		g_CIdM.GetClassId(plabel, &g_CZdisplay_Id);
}

static int CropAndExport_FromSeedBox(FILENAMEGROUP *pFnG, int MultiBoxCropping, EXPORT_RECORD *pExRec)
{
	ResetCropZoneFlags(pFnG, 1); // reset temp seed boxes only, CZ_ALL_GROUPS_GOOD will persist.
	while(1){
		// test all good boxes as the start position for the crop zone.
		// 2nd param below - START WITH the smallest or biggest
		SOURCELINE *pSLnext = GetNextGoodUntestedUnusedBox(pFnG, g_Limits.BoxPix.UseDesirableMin/*seems logical*/);
		if(pSLnext == nullptr)
			break;
		if(!MakeCropZoneAndExport(pFnG, pSLnext, &pSLnext->Box, MultiBoxCropping, pExRec))
			return 0;
	}
	return 1;
}

static int CropAndExport(FILENAMEGROUP *pFnG, BBOX *pExpansionBoundaries, int Operation, int AllowBlurBoxes, EXPORT_RECORD *pExRec, char *pDebugLabel)
{
	// returns zero if error.
	// g_ExportMode - PIC_EXPORT_YES, DEBUG_EXPORT_FULL, 
	SetCZdisplayId(pDebugLabel);
	g_AllowBlurBoxesInCropZone = AllowBlurBoxes;

	g_ExpansionBoundaries.x1 = g_ExpansionBoundaries.y1 = 0.0f;
	g_ExpansionBoundaries.x2 = g_ExpansionBoundaries.y2 = 1.0f;
	if(pExpansionBoundaries)
		g_ExpansionBoundaries = *pExpansionBoundaries;

	if(Operation == CropOps_SeedAllGoodBoxes){
		BBOX CropZone;
		SOURCELINE *pLast=nullptr;
		int NumBoxes_ExcAllProbs = CropAllGoodBoxes(pFnG, &CropZone, &pLast);
		if(NumBoxes_ExcAllProbs == 1){
			if(!MakeCropZoneAndExport(pFnG, pLast, &CropZone, FALSE, pExRec))
				return 0;
		}
		else if(NumBoxes_ExcAllProbs > 1){
			if(!MakeCropZoneAndExport(pFnG, nullptr, &CropZone, TRUE, pExRec))
				return 0;
		}
	}
	else if(Operation == CropOps_MultiBoxCrop){
		if(!CropAndExport_FromSeedBox(pFnG, 1, pExRec))
			return 0;
	}
	else if(Operation == CropOps_SingleBoxCrop){
		// SINGLE BOX CROPPING - IS IT USEFUL? YES, most of the time CropOps_MultiBoxCrop does the same thing.
		// occassionally this does help, just because it does things slightly differently.
		if(!CropAndExport_FromSeedBox(pFnG, 0, pExRec))
			return 0;
	}
	else return 0;

	return 1;
}

static void FlagBoxesForFullPicGOTExp(FILENAMEGROUP *pFnG, int &Num_BlurBoxBothTypes, int &Num_BlurConvertedBox, int &NumGoodBox)
{
	ClearCurrentGroupFlags(pFnG);
	Num_BlurConvertedBox = 0;
	Num_BlurBoxBothTypes = 0; // both types meaning - a classified 'blurr' box, and a blurred object box (blur converted box).
	NumGoodBox = 0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->DebugCode&DC_GOOD_ON_TARGET){
			// add this flag for all 'all probabilities'.
			pSL->FilenameCount |= CZ_CURRENT_GRP_GOOD;
			NumGoodBox++;
		}
		else if(pSL->rsvd_best){ // obviously 'all probs' not required for a blur box, only need to blur it once.
			// (note this box here could be a good-on-original or a g_Blur_Id).
			pSL->FilenameCount |= CZ_CURRENT_GRP_BLUR;
			Num_BlurBoxBothTypes++;
			if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL) // yes, *is* good on original, but converted to blur when using full pic (because its bad on target).
				Num_BlurConvertedBox++;
		}
	}
}

static int FullPicGoodOnTargetExport(FILENAMEGROUP *pFnG, EXPORT_RECORD *pExRec, BOOL HorizontalFlip, char *pDebugLabel)
{
	// bad-on-target boxes are blurred, good-on-target boxes are kept, and full pic is exported.
	int Num_BlurBoxBothTypes = 0, Num_BlurConvertedBox = 0;
	int NumGoodBox = 0;
	FlagBoxesForFullPicGOTExp(pFnG, Num_BlurBoxBothTypes, Num_BlurConvertedBox, NumGoodBox);

	if(Num_BlurBoxBothTypes == 0){ // if every box is good on target this func shouldnt have been called in the first place.
		MsgBox2("refuse error - FullPicGoodOnTargetExport\r\n NumGoodBox %d, Num_BlurBoxBothTypes %d", NumGoodBox, Num_BlurBoxBothTypes);
		return 0;
	}
	// NumGoodBox - obviously we must have at least one good object to do the full pic export.
	// Num_BlurConvertedBox - theres no point continuing unless we have a blur converted box, otherwise the result wont be any 
	//                        different to what the normal crop and export could do (will have done).
	if(NumGoodBox>0 && Num_BlurConvertedBox>0){
		// next checks to see if full pic can be used if some boxes are blured.
		InitBlurBoxDebugFlags();
		if(PictureWithBlurBoxingOk(pFnG, nullptr, TRUE)){
			SetCZdisplayId(pDebugLabel);
			BBOX cz = {0.0f};
			cz.x2 = cz.y2 = 1.0f;
			CURRENT_CZ_STATS CZS = {0}; // get rid of this ?
			CZS.NumGoodBoxes = NumGoodBox;
			CZS.Num_BlurBoxes = Num_BlurBoxBothTypes;
			if(!CropZone_Export(pFnG, &cz, pExRec, &CZS, FALSE, HorizontalFlip))
				return 0;
		}
	}
	return 1;
}

static int Allow_FullPicGoodOnTargetExport(EXPORT_RECORD *pExRec, int NumGoodOnOriginal, int &HorizontalFlip)
{
	HorizontalFlip = 1;
	for(int i=0; i<pExRec->NumCropZoneExports; i++){

	/*	if(pExRec->CZlist[i].NumGood == NumGoodOnOriginal)
			// all boxes have already been cropped in a single crop zone, so dont allow FullPicGoodOnTargetExport.
			return 0; */
		
		// this below is more liberal, allows FullPicGoodOnTargetExport to be used more, but its still isnt used much.
		BBOX cz = pExRec->CZlist[i].cropzone;
		float dx = cz.x2 - cz.x1;
		float dy = cz.y2 - cz.y1;
		if(pExRec->CZlist[i].NumGood == NumGoodOnOriginal)
			if(dx>=g_Limits.FullPicGOTmaxDelta && dy>=g_Limits.FullPicGOTmaxDelta)
				return 0;
	}
	return 1;
}

static void AddMoreExportFlags(FILENAMEGROUP *pFnG, EXPORT_RECORD *pExRec)
{
	int exp=0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int i=0; i<pFnG->NumSourceLines; i++, pSL++){
		if(pSL->DebugCode&DC_EXPORTED_ANNOTATION)
			exp++;
		else if(pSL->DebugCode&DC_GOOD_ON_ORIGINAL)
			if(g_PictureRejected)
				pSL->DebugCode |= DC_EXPORT_GD_ON_ORI_FAILED; // good on original that failed to export.
	}

	if(g_PictureRejected){
		if(pExRec->NumCropZoneExports == 0)
			AddDebugCode(pFnG, DC_NO_EXPORT_POSSIBLE);
	}
	else{
		if(exp > 0){
			// if exp is zero then no exports will have been done, ie picture is accepted as it is, cropping wasnt necessary, or wasnt possible.
			int flag = (exp == pFnG->NumSourceLines) ? DC_ACCEPT_CROP_ALL : DC_ACCEPT_CROP_SOME;
			//static int once=0; if(once++ == 0) MsgBox2("!g_PictureRejected");
			for(int i=0; i<pFnG->NumSourceLines; i++)
				if(pFnG->pSourceLines[i].DebugCode&DC_EXPORTED_ANNOTATION)
					pFnG->pSourceLines[i].DebugCode |= flag;
			// remember these flags are going on the original sourcelines not the exported pic sourcelines.
		}
	}
}

static void GetBoxCounts(FILENAMEGROUP *pFnG, int &NumGoodOnOriginal, int &NumGoodOnTargetDesirableMin)
{
	NumGoodOnOriginal = NumGoodOnTargetDesirableMin = 0;
	for(int i=0; i<pFnG->NumSourceLines; i++){
		if(pFnG->pSourceLines[i].DebugCode&DC_GOOD_ON_ORIGINAL)
			NumGoodOnOriginal++;
		if(pFnG->pSourceLines[i].DebugCode&DC_GOOD_ON_TARGET_DESI_MIN) // ###### UNUSED, UNNEEDED
			NumGoodOnTargetDesirableMin++;
	}
}

static int PictureCropAndExport(FILENAMEGROUP *pFnG, int RemovalBoxes, int BadPicAspect, int TooSmallOnTarget)
{
	g_Export_FilenameCount = 1;

	int Ok = 1; 
	EXPORT_RECORD ExRec = {0}; // fairly big structure, maybe should make it static (global), or malloc, and memset zero here.
	int NumGoodOnOriginal=0, NumGoodOnTargetDesirableMin=0;
	GetBoxCounts(pFnG, NumGoodOnOriginal, NumGoodOnTargetDesirableMin);

	//if(g_PictureRejected || NumGoodOnTargetDesirableMin!=pFnG->NumSourceLines) not necessary because checks are done when crop zones are generated.
	for(g_Limits.BoxPix.UseDesirableMin=1; g_Limits.BoxPix.UseDesirableMin>=0; g_Limits.BoxPix.UseDesirableMin--){

		ResetCropZoneFlags(pFnG);
			
		Ok &= CropAndExport(pFnG, nullptr, CropOps_SeedAllGoodBoxes, 1, &ExRec, "cz_contain_all");
		g_AnyCzFailedBlurArea = 0;
		Ok &= CropAndExport(pFnG, nullptr, CropOps_MultiBoxCrop, 1, &ExRec, "cz_multi");
		Ok &= CropAndExport(pFnG, nullptr, CropOps_SingleBoxCrop, 1, &ExRec, "cz_single");
		Ok &= CropAndExport(pFnG, nullptr, CropOps_MultiBoxCrop, 1, &ExRec, "cz_multi_2nd"); // because sometimes cz_multi fails, then cz_single picks up boxes and then with these boxes discounted another cz_multi is possible.

		// try cropping again without allowing blur boxes, WHY? ..because blur area might have failed the crop zone - see flag g_AnyCzFailedBlurArea.
		// if(g_Limits.BlurBox_DoAreaCalculation && g_AnyCzFailedBlurArea) not necessary 
		Ok &= CropAndExport(pFnG, nullptr, CropOps_MultiBoxCrop, 0, &ExRec, "cz_multi_noblr");
		Ok &= CropAndExport(pFnG, nullptr, CropOps_SingleBoxCrop, 0, &ExRec, "cz_single_noblr");

		// ###############################################################################################
		// next, condition(s) for using UseDesirableMin==false, break out of loop if it should not be used

		if(!g_PictureRejected){
			// if picture was not rejected then only use UseDesirableMin==true, which will make objects bigger, the full picture will also keep its 'accept' 
			// classification therefore we'll also have all objects at their minimum size, i.e, the alternative to having UseDesirableMin==false.
			break;
		}
	}

	if(!BadPicAspect && !RemovalBoxes && TooSmallOnTarget){ // blur boxes are allowed for FullPicGoodOnTargetExport
		int HorizontalFlip = 0;
		// see CROP_ZONE_DIF_FOR_AUG
		if(Allow_FullPicGoodOnTargetExport(&ExRec, NumGoodOnOriginal, HorizontalFlip)){
			// if crop zone(s) generated in attempts above are too big, some/all good-on-original boxes become bad-on-target and CropAndExport will fail, in this scenario
			// a full pic export using good-on-target and where bad-on-target boxes are blurred might be the better option. 
			Ok &= FullPicGoodOnTargetExport(pFnG, &ExRec, HorizontalFlip, "cz_fullpic_GOT");
			// ToDo ? - maybe FullPicGoodOnTargetExport should work with RemovalBoxes, by first create a crop zone, and then from that calculate which boxes are good/bad on target, then blur accordingly. NO
		}
	}

	if(!g_PictureRejected && ExRec.NumCropZoneExports==0){
		// If picture is 'accept' classified and there have been no exports then export with a horizontal flip for the purpose of augmentation.
		if((g_AcceptNoExportCount % g_Limits.AugmentationFreq) == 0){
			SetCZdisplayId("cz_augm_only");
			for(int i=0; i<pFnG->NumSourceLines; i++)
				pFnG->pSourceLines[i].FilenameCount = CZ_CURRENT_GRP_GOOD;
			BBOX cz = {0.0f};
			cz.x2 = cz.y2 = 1.0f;
			if(!ExportAnnotationsAndCroppedPic(pFnG, &cz, 0, 1, 1, TRUE))
				return 0;
		}
		g_AcceptNoExportCount++;
	}

	AddMoreExportFlags(pFnG, &ExRec);
	return Ok;
}

static int ClassifiedPic_ExportGoodObjects(FILENAMEGROUP *pFnG)
{
/*	if(strcmp(pFnG->pPicInfo->pfile_name, "coco2017\\Veh\\000000034828.jpg") == 0)
		int s = 0; */

	g_PictureRejected = 0;
	int BadPicAspect=0, AllBoxesGoodOnOriginal=0, RemovalBoxes=0, BlurBoxes=0, TooSmallOnTarget=0;
	CheckForBadObjects(pFnG, AllBoxesGoodOnOriginal, RemovalBoxes, BlurBoxes, TooSmallOnTarget);

/*	static int do10 = 0;
	if(do10 > 10) return 1;
	if(DoneExport) do10++; */

	if(!AspectRatioOk((float)pFnG->pPicInfo->Width, (float)pFnG->pPicInfo->Height, FALSE)){
		BadPicAspect = 1;
		AddDebugCode(pFnG, DC_PIC_ASPECT_FAILED); // mark all annotations with 'pic aspect failed' flag just so we can pick up these pics for review in the GUI.
	}

	if(BadPicAspect || RemovalBoxes || BlurBoxes || TooSmallOnTarget)
		g_PictureRejected = 1;

	if(g_ExportMode != PIC_EXPORT_NO){
		// 3 special box types:
		// 1) 'blurr' - blur_box.
		// 2) 'remove' - excluded from picture, cannot be blured, must be cropped out.
		// 3) USER specified crop zone - havent implemented this and I probably wont, prefer having it automated.
		if(!PictureCropAndExport(pFnG, RemovalBoxes, BadPicAspect, TooSmallOnTarget))
			return 0;
	}

	if(g_PictureRejected){
		// CLASSIFIED_SPECIAL
		// by hand the picture was classified as accept, but that just means the pic has some good objects and is correctly annotated, processing here may deem the picture  
		// as a whole, bad. I dont want to mark as classified refuse as i already use 'refuse' when classifying by hand, so to distinguish these mark as CLASSIFIED_SPECIAL.
		// when doing yolo export, i'll filter out refuse and special.
		SetFilenameGroupStatus(pFnG, CLASSIFIED_SPECIAL);
	}
	else{
		// DoneExport can be true or false - if the picture was not rejected it will keep its 'accept' classification, and 
		// the code above might have also done some cropping and exports which will act as augmentation.
	}
	return 1;
}

static int GotPositiveAnnotations(FILENAMEGROUP *pFnG)
{
	int b, NegativeCount=0, PositiveCount=0;
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(b=0; b<pFnG->NumSourceLines; b++){
		if(pSL[b].ClassId < 0)
			NegativeCount++;
		else
			PositiveCount++;
	}
	if(NegativeCount>0 && PositiveCount>0){
		MsgBox2("error - GotPositiveAnnotations");
		return 0;
	}
	return (PositiveCount > 0);
}

static int ClassIdsOk(FILENAMEGROUP *pFnG)
{
	SOURCELINE *pSL = pFnG->pSourceLines;
	for(int b=0; b<pFnG->NumSourceLines; b++, pSL++){
		pSL->DebugCode = 0;
		if(pSL->ClassId >= 0){
			if( g_CIdM.Id_inCategory(pSL->ClassId, g_Category)==0 && 
				g_CIdM.Id_inCategory(pSL->ClassId, CATEGORY_REMOVE)==0	)
			{
				MsgBox2("error - ClassIdsOk, class of wrong category");
				return 0;
			}
		}
	}
	return 1;
}

static int BBoxIdentical(BBOX *pA, BBOX *pB)
{
	float tol = 0.001f; // identical to 3 decimal points.
	return ( fabs(pA->x1-pB->x1)<tol && fabs(pA->x2-pB->x2)<tol &&
			 fabs(pA->y1-pB->y1)<tol && fabs(pA->y2-pB->y2)<tol );
/*	return ( pA->x1==pB->x1 && pA->x2==pB->x2 &&
			 pA->y1==pB->y1 && pA->y2==pB->y2 );*/
}

static int MakeAllProbabilitiesGroupings(FILENAMEGROUP *pFnG)
{
	static int warning = 0;

	int a, b;
	SOURCELINE *p_B, *pA = pFnG->pSourceLines;
	for(a=0; a<pFnG->NumSourceLines; a++, pA++){
		pA->rsvd_best = 1;	// will be non-zero for the best probability in a 'all probs' group, also non-zero if this is *not* an 'all probs' group (or think of it as an 'all probs' group of one).
		pA->rsvd_id   = 0;	// all annotations in an 'all probs' group will have a non-zero value for rsvd_id, this is the group 'id'.
	}
	g_AllProbsId = 1;
	pA = pFnG->pSourceLines;
	for(a=0; a<pFnG->NumSourceLines-1; a++, pA++){
		if(pA->rsvd_id == 0){
			int confidence  = pA->Confidence;
			int HasAllProbs = 0;
			for(p_B=&pFnG->pSourceLines[b=a+1]; b<pFnG->NumSourceLines; b++, p_B++){
				if(p_B->rsvd_id == 0){

					if(pA->model_code != p_B->model_code){
						// remember this code is designed for classified csv's, so an example where model codes can be different...
						// ...one annotation is untouched and originating from YOLOv7, and then say while classifying i decided to 'double class' it, and added a 'by hand' 2nd probability.
						// now this object would have multiple probabilities from different model codes.
						if(pA->model_code!=MODEL_BYHAND && p_B->model_code!=MODEL_BYHAND){
							MsgBox2("error - MakeAllProbabilitiesGroupings model codes");
							return 0;
						}
					}

					// 'all probabilities' for a single detection, the coordinates are the same for each probability of course.
					if(BBoxIdentical(&pA->Box, &p_B->Box)){
						if(p_B->Confidence > confidence){
							// all probabilites must be in order, with best probabilities first.
							// in the case of a classified csv all probs will have the same conf, which will be ok here too.
							MsgBox2("error - MakeAllProbabilitiesGroupings");
							return 0;
						}
						p_B->DebugCode |= DC_ALL_PROBABILITIES;
						p_B->rsvd_id    = g_AllProbsId;
						p_B->rsvd_best  = 0;
						HasAllProbs     = 1;
						confidence      = p_B->Confidence;
					}
				}
			}
			if(HasAllProbs){
				// pA is the best probability/prediction, i was considering putting a DebugCode flag on this too, but i think better without.
				pA->rsvd_id = g_AllProbsId;
				g_AllProbsId++;
				if(warning){
					warning = 0;
					MsgBox2("annotations with all probabilities");
				}
			}
		}
	}
	return 1;
}

static void KeepOneProbability(FILENAMEGROUP *pFnG, SOURCELINE *pKeep, int DiscardIdx, int new_class_id)
{
	pKeep->ClassId = new_class_id;
	pKeep->rsvd_best = 1;
	pKeep->rsvd_id = 0;
	//g_ConfidenceZeroRemovals = 1;

	int k = DiscardIdx;
	while(k < pFnG->NumSourceLines-1){
		pFnG->pSourceLines[k] = pFnG->pSourceLines[k+1];
		k++;
	}
	memset(&pFnG->pSourceLines[pFnG->NumSourceLines-1], 0, sizeof(SOURCELINE)); // so Confidence is zero, so it can be removed at end, because this entry still exists in sourceline array.
	pFnG->NumSourceLines--;
}

static int ReclassAnnotation(FILENAMEGROUP *pFnG)
{
	// currently pickup is the only class i dont want to use, so here I convert it to car.
	// BUT MAYBE I SHOULD CONVERT TO TRUCK 
	int Removed2ndProb;
	do{
		Removed2ndProb = 0;
		SOURCELINE *pSL = pFnG->pSourceLines;
		for(int i=0; i<pFnG->NumSourceLines && !Removed2ndProb; i++, pSL++){
			if(pSL->rsvd_best){
				if(pSL->rsvd_id == 0){
					// single probability
					// ##########################
					if(pSL->ClassId == g_Veh.pickup) // pickup to car
						pSL->ClassId = g_Veh.car;
				}
				else{
					// multiple probabilities
					// ##########################
					int b, NumProbabilities=0, _2ndProbIdx=0;
					SOURCELINE *p_B;
					for(p_B=&pFnG->pSourceLines[b=i+1]; b<pFnG->NumSourceLines; b++, p_B++){
						if(p_B->rsvd_id == pSL->rsvd_id){
							NumProbabilities++;
							_2ndProbIdx = b;
						}
					}

					if(NumProbabilities==0 || i==pFnG->NumSourceLines-1){
						MsgBox2("error - ReclassAnnotation all probs missing"); // error, there has to be probabilities cos 'pSL->rsvd_id != 0'.
						return 0;
					}
					else if(NumProbabilities == 1){
						int match = 0;
						int k = pSL->ClassId, j = pFnG->pSourceLines[_2ndProbIdx].ClassId;
					//	int van    = (k==g_Veh.van    || j==g_Veh.van);
						int car    = (k==g_Veh.car    || j==g_Veh.car);
					//	int bus    = (k==g_Veh.bus    || j==g_Veh.bus);
						int truck  = (k==g_Veh.truck  || j==g_Veh.truck);
						int pickup = (k==g_Veh.pickup || j==g_Veh.pickup);

						// #### I might train without van, in which case changes needed here, car/van, truck/van etc

						if(pickup){
							if(truck){
								// we are not converting pickup to car here? yes i think that is correct.
								KeepOneProbability(pFnG, pSL, _2ndProbIdx, g_Veh.truck);
								Removed2ndProb = match = 1; // main loop will start again from zero after removal.
							}
							if(car){
								KeepOneProbability(pFnG, pSL, _2ndProbIdx, g_Veh.car);
								Removed2ndProb = match = 1; // main loop will start again from zero after removal.
							}
						}
						else{
							// every combination is acceptable providing it doesnt include pickup.
							match = 1;
							// can keep these, so dont remove.
						}

						if(!match){
							MsgBox2("error - ReclassAnnotation all probs no match, k %d, j %d", k, j);
							return 0;
						}
					}
					else{
						MsgBox2("error - ReclassAnnotation cant cope with %d probabilities", NumProbabilities+1);
						return 0;
					}
				}
			}
		}
	} while(Removed2ndProb);
	return 1;
}

static int ClassifiedCsvLogic(FILENAMEGROUP_ALL *pAllFnG)
{
	g_AcceptNoExportCount = 0;

	int   UI_updateinterval = (g_ExportMode==PIC_EXPORT_YES) ? 10 : 100;
	CWnd *pCWnd = ::AfxGetApp()->m_pMainWnd;
	int   Ok = 1;
	// NumFnG_LastIsReserved - the last filename group is spare space reserved for the annotations of cropped exported bitmaps (also see CAnnotations::AddSpaceForCroppedAnnotations()).
	int NumFnG_LastIsReserved = pAllFnG->Num - 1;

	FILENAMEGROUP *pFnG = pAllFnG->pFNG;
	for(int i=0; i<NumFnG_LastIsReserved && Ok; i++, pFnG++){
		if(pFnG->Classified == CLASSIFIED_ACCEPT){
		/*	if(strcmp(pFnG->pPicInfo->pfile_name, "coco2017\\Veh\\000000031965.jpg") == 0)
 				Ok = Ok; */
			Ok &= ClassIdsOk(pFnG);
			if(GotPositiveAnnotations(pFnG)){
				Ok &= MakeAllProbabilitiesGroupings(pFnG);
				if(g_ClassConversions)
					Ok &= ReclassAnnotation(pFnG);

				if(pFnG->pPicInfo->Height<g_Limits.Pixel_MinPicHeight || pFnG->pPicInfo->Width<g_Limits.Pixel_MinPicHeight){
					// if pic is too small as a full picture then obviously no exports are possible either.
					SetFilenameGroupStatus(pFnG, CLASSIFIED_SPECIAL);
					AddDebugCode(pFnG, DC_PIC_SIZE_FAILED);
				}
				else{
					Ok &= ClassifiedPic_ExportGoodObjects(pFnG);
				}
			}
		}
		else if(pFnG->Classified == 0){
			// remove non-classified sourcelines from the new csv that will be generated, accept/special/refuse will be kept.
			for(int g=0; g<pFnG->NumSourceLines; g++)
				pFnG->pSourceLines[g].Confidence = 0;
		}

		// UI progress display
		if((i%UI_updateinterval)==0 || i==NumFnG_LastIsReserved-1)
			ProgressUpdate_SetWindowText(pCWnd, "FixAndCrop", i, NumFnG_LastIsReserved);
	}
	return Ok;
}

static int RemoveZeroConfidenceSourceLines(SOURCELINE_CONTAINER *pSLC)
{
	int i=0, Last=0;
	while(i < pSLC->Num){
		// skip over zero confidence entries
		while(i<pSLC->Num && pSLC->pCsvLns[i].Confidence==0)
			i++;
		if(i < pSLC->Num){
			// non-zero-confidence entries will all be 'shunted' down to fill in the (unwanted) zero confidence entries.
			if(Last != i)
				pSLC->pCsvLns[Last] = pSLC->pCsvLns[i];
			Last++;// no this isnt a mistake, Last++ should not be in the above 'if' statement.
			i++;
		}
	}
	pSLC->Num = Last;
	if(pSLC->Num > 0)
		return 1;
	MsgBox2("error - RemoveZeroConfidenceSourceLines, Num==0");
	return 0;
}

static int InitVehicleIds()
{
	int Ok = 1;
	Ok &= g_CIdM.GetClassId("car",    &g_Veh.car);
	Ok &= g_CIdM.GetClassId("pickup", &g_Veh.pickup);
	Ok &= g_CIdM.GetClassId("van",    &g_Veh.van);
	Ok &= g_CIdM.GetClassId("moto",   &g_Veh.moto);
	Ok &= g_CIdM.GetClassId("truck",  &g_Veh.truck);
	Ok &= g_CIdM.GetClassId("bus",    &g_Veh.bus);
	if(!Ok) MsgBox2("error - InitVehicleIds");
	return Ok;
}

void InitialiseLimitsAndConstants()
{
	memset(&g_Limits, 0, sizeof(LIMITS_FIXNCROP));
	g_Limits.AugmentationFreq = 1;

	// width / height = aspect ratio
	g_Limits.PictureAspectRatio_Min           = 0.8f;	// increased from 0.7  (work it in reverse, if width was 10, height would be 10/0.7 = 14.2)
	g_Limits.PictureAspectRatio_Min_Preferred = 1.0f;	// (448/352 = 1.272727) have preferred zone around target, remember wide aspect will be more usual in real life.
	g_Limits.PictureAspectRatio_Max           = 1.78f;	// 1920w/1080h (16/9) = 1.77777 
	g_Limits.PictureAspectRatio_Max_Preferred = 1.57f;

	float w = (float)g_pMO->General.TargetWidth, h = (float)g_pMO->General.TargetHeight;
	if(fabs((w/h) - 1.2727f) > 0.0001f)
		MsgBox2("error update tolerances related to target aspect"); // tolerances above might need adjusting if we use a different target aspect.

	SetBoxMinPixelLimits(30, 40, g_pMO->General.TargetHeight/4);

	g_Limits.Pixel_MinPicHeight = (int)((float)g_pMO->General.TargetHeight * 0.35f);

	g_Limits.BlurBox_DoAreaCalculation = 1;
	g_Limits.BlurBox_MaxArea = 0.2f;

	g_Limits.BlurConvertedBox_MaxArea = 0.1f;
	g_Limits.BlurConvertedBox_MinDistance = 0.05f; // this might be un-necessary because we have BlurConvertedBox_ABSMinPixelDist ?
	g_Limits.BlurConvertedBox_ABSMinPixelDist = 20;

	g_Limits.Acc.MaxCZ = 0.92f;
	g_Limits.Acc.MaxBoxSize = 0.7f;

	g_Limits.AllCropZonesPreferredAspect = TRUE; // tried false, prefer the cropping with true.
	g_Limits.BoxPix.DesirableMaxFactor = 0.9f; // INCREASED FOR MORE EXPORTS, ie augmentation
	g_Limits.BoxPix.DesirableMaxLimit = 0.35f;

	g_Limits.FullPicGOTmaxDelta = 0.85f;

	MsgBox2("export box min sizes\r\n small veh min %f\r\n large veh min %f", g_Limits.BoxPix.SmallVehicle, g_Limits.BoxPix.LargeVehicle);
}

int AnnotationProcessing(FILENAMEGROUP_ALL *pAllFnG, SOURCELINE_CONTAINER *pSLC, char *pPictureFolder, MOREOPTIONS *pMO, int &FilenameMergeRequired, BOOL DebugDisplayMode)
{
	MsgBox2("this code was written for my specific design and purpose - vehicle detection, but could be adapted for other use cases");

	if(pMO->General.TargetWidth<=0 || pMO->General.TargetHeight<=0){
		MsgBox2("error - must set target width/height");
		return 0;
	}
	else if(!MsgBox_YN("Have you set TARGET SIZE correctly? ..if not dataset will be wrong\n\nTargetWidth %d\nTargetHeight %d", pMO->General.TargetWidth, pMO->General.TargetHeight))
		return 0;

	srand(1234); // random not in use currently.
	g_pMO = pMO;
	g_pPictureFolder = pPictureFolder;
	strcpy_s(g_ExportFolder, num_entries(g_ExportFolder), pPictureFolder);
	if(!AddSubfolderToFolder(g_ExportFolder, num_entries(g_ExportFolder), EXPORTS_FOLDER_NAME))
		return 0;

	InitialiseLimitsAndConstants();
	int Ok = 1;
	Ok &= InitVehicleIds();
	Ok &= g_CIdM.GetClassId("remove", &g_Remove_Id);
	Ok &= g_CIdM.GetClassId("blurr",  &g_Blur_Id);
	Ok &= g_CIdM.GetClassId("cz_obj_blurred", &g_BadOnTargetBlurred_Id);
	if(Ok){
		g_NumExportLines = 0;
		g_pSpare = &pAllFnG->pFNG[pAllFnG->Num-1];
		g_ClassConversions = 1;
		g_Category = CATEGORY_DETECTIONS;
		//g_ExportMode = PIC_EXPORT_NO;
		g_ExportMode = DebugDisplayMode ? DEBUG_EXPORT_FULL : PIC_EXPORT_YES;

		if(g_ExportMode == PIC_EXPORT_YES)
			MsgBox2("doing real export not just debug display!\r\nexported pictures saved as .png to:\r\n%s", g_ExportFolder);
		else if(g_ExportMode == DEBUG_EXPORT_FULL)
			MsgBox2("debug display mode");

		for(int i=0; i<pSLC->Num; i++)
			pSLC->pCsvLns[i].Confidence = 1;

		if(ClassifiedCsvLogic(pAllFnG)){
			FilenameMergeRequired = 0;
			if(g_ExportMode!=PIC_EXPORT_YES && g_ExportMode!=PIC_EXPORT_NO)
				// ONLY WHEN DOING debug display, annotations (crop zone etc) added at the end need to be merged into the original filename group annotations.
				// if we're doing an export, then all new annotations will be for new bitmaps, so no merge with existing annotations is needed.
				FilenameMergeRequired = 1;

			pSLC->Num = (pSLC->Num - g_pSpare->NumSourceLines) + g_NumExportLines;
			Ok &= RemoveZeroConfidenceSourceLines(pSLC);
		}
		else Ok = 0;
	}
	else MsgBox2("error - AnnotationProcessing missing classes");
	FreeGrid();
	return Ok;
}

	
