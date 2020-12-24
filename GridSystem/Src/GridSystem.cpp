// *****************************************************************************
// Source code for the GridSystem ARCHICAD Add-On
// API Development Kit; Mac/Win
//
// Namespaces:			Contact person:
//		-None-
//
// [SG compatible] - Yes
// *****************************************************************************

// ---------------------------------- Includes ---------------------------------
//
//#include	"APIEnvir.h"
//#include	"ACAPinc.h"					// also includes APIdefs.h
#include	"ResourceIDs.h"

#include	"DumpWriter.hpp"
#include	"DumpUtils.hpp"
#include	"APICommon.h"

#include	<time.h>

#include	"GSRoot.hpp"
#include	"BM.hpp"

#include	"DG.h"

#include	"GenArc2DData.h"

// --- Variable declarations ---------------------------------------------------
static DumpWriter* writer;

static double			dScale;

// -----------------------------------------------------------------------------
//	Return the length unit as text
// -----------------------------------------------------------------------------

static void		SetLengthUnit(API_LengthTypeID lengthUnit, char* s)
{
	switch (lengthUnit) {
	case API_LengthTypeID::Meter:			strcpy(s, "M");	break;
	case API_LengthTypeID::Decimeter:		strcpy(s, "DM");	break;
	case API_LengthTypeID::Centimeter:		strcpy(s, "CM");	break;
	case API_LengthTypeID::Millimeter:		strcpy(s, "MM");	break;
	case API_LengthTypeID::FootFracInch:	strcpy(s, "FFI");	break;
	case API_LengthTypeID::FootDecInch:		strcpy(s, "FDI");	break;
	case API_LengthTypeID::DecFoot:			strcpy(s, "DF");	break;
	case API_LengthTypeID::FracInch:		strcpy(s, "FI");	break;
	case API_LengthTypeID::DecInch:			strcpy(s, "DI");	break;
	default:								strcpy(s, "XXX");	break;
	}
}

// -----------------------------------------------------------------------------
//	Return the angle unit as text
// -----------------------------------------------------------------------------

static	void	SetAngleUnit(API_AngleTypeID angleUnit, char* s)
{
	switch (angleUnit) {
	case API_AngleTypeID::DecimalDegree:	strcpy(s, "DD");	break;
	case API_AngleTypeID::DegreeMinSec:		strcpy(s, "DMS");	break;
	case API_AngleTypeID::Grad:				strcpy(s, "GR");	break;
	case API_AngleTypeID::Radian:			strcpy(s, "RAD");	break;
	case API_AngleTypeID::Surveyors:		strcpy(s, "SURV");	break;
	default:								strcpy(s, "XXX");	break;
	}
}

static GSErrCode	Do_SaveAs_Object(API_ElemTypeID elemsTypeID)
{
	API_Element			element;
	API_LibPart 		libPart;
	GSErrCode			err;

	GS::Array<API_Guid> elemList;
	err = ACAPI_Element_GetElemList(elemsTypeID, &elemList);

	for (GS::Array<API_Guid>::ConstIterator it = elemList.Enumerate(); it != nullptr && err == NoError; ++it) {
		BNZeroMemory(&element, sizeof(API_Element));
		element.header.guid = *it;
		err = ACAPI_Element_Get(&element);

		if (err == NoError) {
			BNZeroMemory(&libPart, sizeof(API_LibPart));

			GS::UniString infoString;
			switch (elemsTypeID) {
			case API_ObjectID:	writer->WriteBlock("SYMBOL", element.header.guid);
				ACAPI_Database(APIDb_GetCompoundInfoStringID, &element.header.guid, &infoString);
				writer->WriteElemHead(&element, infoString.ToCStr().Get(), nullptr);
				libPart.index = element.object.libInd;
				break;
			default:			writer->WriteBlock("???", element.header.guid);
			}

			err = ACAPI_LibPart_Get(&libPart);
			if (libPart.location != nullptr) {
				delete libPart.location;
				libPart.location = nullptr;
			}

			if (err == NoError) {
				writer->WriteName(GS::UniString(libPart.docu_UName), DumpWriter::WithNewLine);

				if (elemsTypeID == API_ZoneID) {
					writer->WriteFloat(element.zone.pos.x);
					writer->WriteFloat(element.zone.pos.y);
					writer->WriteFloat(0.0);
					writer->WriteAngle(0.0);
					writer->WrNewLine();
					writer->WriteFloat(1.0);
					writer->WriteFloat(1.0);
					writer->WrNewLine();
					writer->WriteInt(0);
					writer->WriteInt(0, DumpWriter::WithNewLine);
					writer->WriteInt(0, DumpWriter::WithNewLine);
				}
				else {
					writer->WriteFloat(element.object.pos.x);
					writer->WriteFloat(element.object.pos.y);
					writer->WriteFloat(element.object.level);
					writer->WriteAngle(element.object.angle);
					writer->WrNewLine();
					writer->WriteFloat(element.object.xRatio);
					writer->WriteFloat(element.object.yRatio);
					writer->WrNewLine();
					writer->WriteInt(element.object.reflected);
					writer->WriteInt(element.object.lightIsOn, DumpWriter::WithNewLine);
					writer->WriteInt(element.object.mat, DumpWriter::WithNewLine);
				}

				writer->WriteParams(&element);
				err = writer->WriteSurfVol(element.header.typeID, element.header.guid);

				if (err == NoError)
					err = writer->WrProperties(element.header.guid);
			}
			if (err == NoError)
				err = writer->WrEndblk();
		}
		else {
			if (err == APIERR_DELETED)
				err = NoError;
		}
	}

	return err;
}		// Do_SaveAs_Object

static GSErrCode	Do_SaveAs_Elements(void)
{
	GSErrCode err;

	if (err == NoError)
		err = Do_SaveAs_Object(API_ObjectID);

	return err;
}		// Do_SaveAs_Elements

static void		Do_SaveAs(void)
{
	API_StoryInfo		storyInfo;
	time_t				t;
	char				buffer[256];
	char				dateStr[128];
	short 				i;
	GSErrCode			err = NoError;;

	IO::Location		temporaryFolderLoc;
	API_SpecFolderID	temporaryFolderID = API_TemporaryFolderID;
	err = ACAPI_Environment(APIEnv_GetSpecFolderID, &temporaryFolderID, &temporaryFolderLoc);
	if (err == NoError)
		err = writer->Open(IO::Location(temporaryFolderLoc, IO::Name("PlanDump.txt")));
	if (err == NoError) {
		GS::UniString	path = IO::Location(temporaryFolderLoc, IO::Name("PlanDump.txt")).ToLogText();
		sprintf(buffer, "Plan Dump path is \"%s\"", path.ToCStr().Get());
		ACAPI_WriteReport(buffer, false);
	}

	// ========== header ==========
	if (err == NoError) {
		time(&t);
		strcpy(dateStr, ctime(&t));
		dateStr[24] = 0;													// end of string: 0

		sprintf(buffer, "# ARCHICAD PlanDump with API,   %s", dateStr);
		writer->WriteStr(buffer, DumpWriter::WithNewLine);
	}

	// ======= environment ========
	if (err == NoError) {

		writer->WriteBlock("ENVIR", 0);
		API_WorkingUnitPrefs	prefs;
		err = ACAPI_Environment(APIEnv_GetPreferencesID, &prefs, (void*)APIPrefs_WorkingUnitsID);
		if (err == NoError) {
			SetLengthUnit(prefs.lengthUnit, buffer);
			writer->WriteStr(buffer);
			writer->WriteInt(prefs.lenDecimals, DumpWriter::WithNewLine);

			SetAngleUnit(prefs.angleUnit, buffer);
			writer->WriteStr(buffer);
			writer->WriteInt(prefs.angleDecimals, DumpWriter::WithNewLine);

			err = ACAPI_Database(APIDb_GetDrawingScaleID, &dScale, nullptr);
			if (err == NoError) {
				writer->WriteInt((Int32)dScale);
				writer->WrEndblk();
				err = writer->WrNewLine();
			}
		}
	}

	// ========= elements =========
	if (err == NoError)
		err = Do_SaveAs_Elements();

	if (err != NoError) {
		sprintf(buffer, "Error code:  %d", (int)err);
		ACAPI_WriteReport(buffer, true);
		writer->WriteStr("ErrorCode: ");
		writer->WriteInt(err);
	}

	writer->Close();
}		// Do_SaveAs


// -----------------------------------------------------------------------------
// Handles menu commands
// -----------------------------------------------------------------------------

GSErrCode __ACENV_CALL MenuCommandHandler (const API_MenuParams *menuParams)
{
	/*try {*/
		switch (menuParams->menuItemRef.itemIndex) {
		case ID_MENU_STRINGS:
			switch (menuParams->menuItemRef.itemIndex) {
			case 1:		Do_SaveAs();  break;
			}
			break;
		default: break;
		}
		
	/*}
	catch (...) {
		return APIERR_CANCEL;
	}*/

	/*switch (menuParams->menuItemRef.menuResID) {
	case ID_MENU_STRINGS:
		switch (menuParams->menuItemRef.itemIndex) {
		case 1:		break;
		}
		break;
	}*/

	return NoError;
}		// MenuCommandHandler

// =============================================================================
//
// Required functions
//
// =============================================================================

// -----------------------------------------------------------------------------
// Dependency definitions
// -----------------------------------------------------------------------------

API_AddonType	__ACENV_CALL	CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, ID_ADDON_INFO, 1, ACAPI_GetOwnResModule());
	RSGetIndString (&envir->addOnInfo.description, ID_ADDON_INFO, 2, ACAPI_GetOwnResModule());

	return APIAddon_Normal;
}		// CheckEnvironment


// -----------------------------------------------------------------------------
// Interface definitions
// -----------------------------------------------------------------------------

GSErrCode	__ACENV_CALL	RegisterInterface (void)
{
	GSErrCode err = ACAPI_Register_Menu (ID_MENU_STRINGS, ID_MENU_PROMPT_STRINGS, MenuCode_Tools, MenuFlag_Default);

	return err;
}		// RegisterInterface


// -----------------------------------------------------------------------------
// Called when the Add-On has been loaded into memory
// to perform an operation
// -----------------------------------------------------------------------------

GSErrCode	__ACENV_CALL Initialize	(void)
{
	GSErrCode err = ACAPI_Install_MenuHandler (ID_MENU_STRINGS, MenuCommandHandler);

	return err;
}		// Initialize


// -----------------------------------------------------------------------------
// FreeData
//		called when the Add-On is going to be unloaded
// -----------------------------------------------------------------------------

GSErrCode __ACENV_CALL	FreeData (void)
{
	return NoError;
}		// FreeData
