
#pragma once

#include "Annotations.h"
#include "BitmapFile.h"

extern int AnnotationProcessing(FILENAMEGROUP_ALL *pSourceFnG, SOURCELINE_CONTAINER *pSLC, char *pPictureFolder, MOREOPTIONS *pMO, int &FilenameMergeRequired, BOOL DebugDisplayMode);
extern int LicencePlateWithinVehicle(BBOX *pPlate, BBOX *pVehicle);
