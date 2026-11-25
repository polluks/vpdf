
/// System includes
#define AROS_ALMOST_COMPATIBLE
#include <proto/muimaster.h>
#include <libraries/mui.h>

#include <libraries/asl.h>
#include <workbench/workbench.h>

#include <proto/exec.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <exec/libraries.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <memory.h>

#include <proto/alib.h>
#include <proto/utility.h>

#include <proto/datatypes.h>
#include <clib/dtclass_protos.h>
#include <datatypes/pictureclass.h>
#include <devices/rawkeycodes.h>

#include <libraries/gadtools.h>

#include <emul/emulregs.h>
#include <emul/emulinterface.h>
////

#include <private/vapor/vapor.h>
#include "util.h"
#include "poppler.h"
#include "locale.h"
#include "application.h"
#include "window.h"
#include "settings.h"
#include "system/functions.h"

#include "mcc/documentview_class.h"
#include "mcc/documentlayout_class.h"
#include "mcc/toolbar_class.h"
#include "mcc/renderer_class.h"
#include "mcc/title_class.h"

#define MAXRECENT 10

struct Data
{
	unsigned int id;
	char muiid[64];
	Object *grpRoot;
	Object *grpTitles;
	Object *btnErrors;
	char title[256];
	int fullScreen;
	int win_width, win_height, win_leftEdge, win_topEdge;
};

DEFNEW
{
	Object * grpRoot, *grpTitles;

	obj = DoSuperNew(cl, obj,
		MUIA_Window_Title, "VPDF",
		MUIA_Window_Width, MUIV_Window_Width_Screen(50),
		MUIA_Window_Height, MUIV_Window_Height_Screen(50),
		MUIA_Window_AppWindow, TRUE,
		MUIA_Window_RootObject,grpRoot = MUI_NewObject(MUIC_Group,
			MUIA_Background, MUII_RegisterBack,
			MUIA_Frame, MUIV_Frame_Register,
			MUIA_Group_PageMode, TRUE,
			MUIA_Group_Child, grpTitles = NewObject(getTitleClass(), NULL,
				MUIA_Title_Closable, TRUE,
				MUIA_Title_Sortable, TRUE,
			TAG_END),
		TAG_END),
	TAG_MORE, INITTAGS);

	if(obj != NULL)
	{
		GETDATA;
		memset(data, 0, sizeof(struct Data));
		data->id = GetTagData(MUIA_VPDFWindow_ID, 0, INITTAGS);
		data->grpRoot = grpRoot;
		data->grpTitles = grpTitles;

		/* setup window id */

		DoMethod(obj, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_VPDF_HandleAppMessage, data->id, MUIV_TriggerValue);
		DoMethod(grpRoot, MUIM_Notify, MUIA_Group_ActivePage, MUIV_EveryTime, obj, 2, MUIM_VPDFWindow_UpdateActive, MUIV_TriggerValue);
		DoMethod(grpRoot, MUIM_Notify, MUIA_Group_ActivePage, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_VPDF_RefreshViewMenu, obj);

		DoMethod(obj, MUIM_VPDFWindow_CreateTab);
	}

	return (ULONG)obj;
}

DEFMMETHOD(VPDFWindow_CreateTab)
{
	GETDATA;

	/* place empty object into tab */

	if (DoMethod(data->grpRoot, MUIM_Group_InitChange))
	{
		if (DoMethod(data->grpTitles, MUIM_Group_InitChange))
		{
			DoMethod(data->grpTitles, MUIM_Family_AddTail, NewObject(getVPDFTitleButtonClass(), NULL,
				MUIA_Group_Child,  MUI_NewObject(MUIC_Text,
					MUIA_Text_Contents, LOCSTR(MSG_NOFILE),
					MUIA_Text_Copy, TRUE,
				TAG_END),
			TAG_END));

			DoMethod(data->grpRoot, MUIM_Family_AddTail, MUI_NewObject(MUIC_Group,
				MUIA_UserData, TRUE, // move it to subclass
				MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle, TAG_END),
			TAG_END));

			DoMethod(data->grpTitles, MUIM_Group_ExitChange);
		}
		DoMethod(data->grpRoot, MUIM_Group_ExitChange);
	}

	/* return index of new group member */

	set(data->grpRoot, MUIA_Group_ActivePage, xget(data->grpRoot, MUIA_Family_ChildCount) - 1);
	return xget(data->grpRoot, MUIA_Family_ChildCount) - 1;
}


DEFMMETHOD(VPDFWindow_SaveFile)
{
	GETDATA;
	int tabind = xget(data->grpRoot, MUIA_Group_ActivePage);
	Object *group = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1); /* +1 for title object */
	group = (Object*)DoMethod(group, MUIM_Family_GetChild, 0);

	if(group != NULL)
	{
		void *doc = (void *)xget(group, MUIA_DocumentView_PDFDocument);

		if(doc != NULL)
		{
			pdfSaveFile(doc, msg->filename, 0);
		}
	}

	return 0;
}

#ifndef errNone
#define errNone 0 // no error
#endif

#ifndef errEncrypted
#define errEncrypted 4
#endif

BOOL RequestPW(Object *obj, char *pass, ULONG pass_len, char *user, ULONG user_len)
{
	BOOL rc = FALSE;
	void *app = _app(obj);
	Object *window, *OK, *U;
	CONST_STRPTR str;
				
	window = MUI_NewObject(MUIC_Window,
			MUIA_Window_Title, (IPTR)LOCSTR(MSG_REQ_PASSWORD_TITLE),
			MUIA_Window_CloseGadget, TRUE,
			MUIA_Window_DepthGadget, TRUE,
			MUIA_Window_SizeGadget, TRUE,
			MUIA_Window_DragBar, TRUE,
			MUIA_Window_Width, 400,
			MUIA_Window_RefWindow, obj,
			MUIA_Window_LeftEdge,MUIV_Window_LeftEdge_Centered,
			MUIA_Window_TopEdge,MUIV_Window_TopEdge_Centered,

			WindowContents,
						(IPTR)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, FALSE,
						
							Child, (IPTR)MUI_NewObject(MUIC_Group, MUIA_Group_Columns, 2,
								Child, (IPTR)Label(LOCSTR(MSG_REQ_PASSWORD_ENTER)),
								Child, (IPTR)(U = MUI_NewObject(MUIC_String, 
									MUIA_CycleChain, TRUE, 
									MUIA_String_MaxLen, pass_len, 
									MUIA_String_Secret, TRUE, StringFrame,
									MUIA_String_AdvanceOnCR, TRUE,
									TAG_DONE)),
								TAG_DONE),
							Child, (IPTR)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
								Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE),
								Child, (IPTR)(OK = SimpleButton("OK")),
								Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE),
							TAG_DONE),
						TAG_DONE),
			TAG_DONE);

	DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,(IPTR)U,3,MUIM_Set, MUIA_String_Contents, (IPTR)"");
	DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,(IPTR)app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
	DoMethod(OK,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
	
	SetAttrs(OK, MUIA_CycleChain, TRUE, TAG_DONE);

	set(app,MUIA_Application_Sleep,TRUE);
	DoMethod(app,OM_ADDMEMBER,window);
	set(window,MUIA_Window_Open,TRUE);

	set(window, MUIA_Window_Open,TRUE);
	set(window, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Next);

	{
		ULONG sigs = 0;
		while (DoMethod(app,MUIM_Application_NewInput,(IPTR)&sigs) != MUIV_Application_ReturnID_Quit)
		{
			if (sigs)
			{
				sigs = Wait(sigs | SIGBREAKF_CTRL_C);
				if (sigs & SIGBREAKF_CTRL_C) break;
			}
		}
	}

	set(window,MUIA_Window_Open,FALSE);

	get(U, MUIA_String_Contents, &str);
		
	if (str)
	{
		stccpy(user, str, user_len);
		rc |= user[0];
	}

	set(window,MUIA_Window_Open,FALSE);
	DoMethod(app,OM_REMMEMBER,window);
	set(app,MUIA_Application_Sleep,FALSE);
	MUI_DisposeObject(window);

	return rc;
}


DEFMMETHOD(VPDFWindow_OpenFile)
{
	GETDATA;
	char *filename = msg->filename;
	int mode = msg->mode;
	int newtab;
	char userpw[128]="";
	ULONG user_len=128;
	char ownerpw[128]="";
	ULONG pass_len=128;

	if(mode == MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty)
	{
		int tabind = xget(data->grpRoot, MUIA_Group_ActivePage);
		Object *tcontents = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1); // +1 for title object
		if(xget(tcontents, MUIA_UserData) == TRUE) // marker for empty page. fix
			newtab = FALSE;
		else
			newtab = TRUE;
	}
	else if(mode == MUIV_VPDFWindow_OpenFile_NewTab)
	{
		newtab = TRUE;
	}
	else // if (MUIV_VPDFWindow_OpenFile_CurrentTab)
	{
		newtab = FALSE;
	}

	{
		Object *document=NULL;
		int tabind;
		Object *contents, *tcontents;
		int errCode=0;

		if(newtab)
			tabind = DoMethod(obj, MUIM_VPDFWindow_CreateTab);
		else
			tabind = xget(data->grpRoot, MUIA_Group_ActivePage);

		/* setup new label */

		tcontents = (Object*)DoMethod(data->grpTitles, MUIM_Family_GetChild, tabind);
		set(tcontents, MUIA_Text_Contents, filename);

		/* dispose old contents and add new browser */

		contents = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1); /*  +1 for title object */

		DoMethod(obj, MUIM_Window_ToFront); // push application window to front

		do
		{
			if(errCode == errEncrypted)
			{
				int ret=RequestPW(obj,  ownerpw, pass_len, userpw, user_len);

				if(ret == 0)
					break;
			}
			else
			if(errCode != errNone)
			{
				break;
			}

			document = NewObject(getDocumentViewClass(), NULL,
						MUIA_DocumentView_Renderer, xget(_app(obj), MUIA_VPDF_Renderer),
						MUIA_DocumentView_FileName, filename,
						MUIA_DocumentView_UserPassword, userpw,
						MUIA_DocumentView_OwnerPassword, ownerpw,
						MUIA_DocumentView_ErrCodePtr, &errCode,
						MUIA_DocumentView_Layout, xget(_app(obj), MUIA_VPDFSettings_Layout),
						MUIA_DocumentView_Outline, xget(_app(obj), MUIA_VPDFSettings_Outline),
						MUIA_DocumentLayout_Scaling, xget(_app(obj), MUIA_VPDFSettings_Scaling),
						MUIA_DocumentView_ColorReproduction, xget(_app(obj), MUIA_VPDFSettings_ColorReproduction),
					TAG_END);
		}
		while(errCode != 0);

		if (DoMethod(contents, MUIM_Group_InitChange))
		{
			tcontents = (Object*)DoMethod(contents, MUIM_Family_GetChild, 0);

			DoMethod(contents, OM_REMMEMBER, tcontents);
			MUI_DisposeObject(tcontents);

			tcontents = MUI_NewObject(MUIC_Group,
				MUIA_Group_Child, document,
			TAG_END);

			if(tcontents != NULL)
			{
				set(document, MUIA_DocumentView_FullScreen, data->fullScreen);
				DoMethod(contents, MUIM_Family_AddTail, tcontents);
				DoMethod(tcontents, MUIM_Notify, MUIA_DocumentView_Outline, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_VPDF_RefreshViewMenu, obj);
				DoMethod(tcontents, MUIM_Notify, MUIA_DocumentView_Layout,  MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_VPDF_RefreshViewMenu, obj);
				DoMethod(_app(obj), MUIM_VPDF_RefreshViewMenu, obj);
			}
			set(contents, MUIA_UserData, FALSE); // mark as not empty
			DoMethod(contents, MUIM_Group_ExitChange);
		}
		DoMethod(obj, MUIM_VPDFWindow_UpdateActive, tabind);
		return tcontents != NULL;
	}
}

DEFMMETHOD(VPDFWindow_UpdateActive)
{
	GETDATA;

	Object *group = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, msg->active + 1); /* +1 for title object */
	group = (Object*)DoMethod(group, MUIM_Family_GetChild, 0);

	if(group != NULL)
	{
		void *doc = (void *)xget(group, MUIA_DocumentView_PDFDocument);
		struct pdfAttribute *attr = NULL;

		if(doc != NULL)
			attr = pdfGetAttr(doc, PDFATTR_TITLE);

		if(attr != NULL && attr->value.s != NULL)
			snprintf(data->title, sizeof(data->title), "VPDF [%s]", attr->value.s);
		else if(xget(group, MUIA_DocumentView_FileName))
			snprintf(data->title, sizeof(data->title), "VPDF [%s]", FilePart((char*)xget(group, MUIA_DocumentView_FileName)));
		else
			snprintf(data->title, sizeof(data->title), "VPDF");

		set(obj, MUIA_Window_Title, data->title);
		pdfFreeAttr(doc, attr);
	}

	return 0;
}

DEFMMETHOD(VPDFWindow_DetachView)
{
	GETDATA;

	if(msg->tabind == MUIV_VPDFWindow_DetachView_Active)
	{
		int tabind = xget(data->grpRoot, MUIA_Group_ActivePage);
		Object *tcontents = (Object*)DoMethod(data->grpTitles, MUIM_Family_GetChild, tabind);
		Object *contents = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1);

		if (DoMethod(data->grpTitles, MUIM_Group_InitChange))
		{
			if (DoMethod(data->grpRoot, MUIM_Group_InitChange))
			{
				DoMethod(data->grpTitles, OM_REMMEMBER, tcontents);
				DoMethod(data->grpRoot, OM_REMMEMBER, contents);

				DoMethod(data->grpRoot, MUIM_Group_ExitChange);
			}
			DoMethod(data->grpTitles, MUIM_Group_ExitChange);
		}

		msg->docview = contents;
		msg->title = tcontents;

		/* hmm, should we really close the window here? */

		if(xget(data->grpTitles, MUIA_Family_ChildCount) == 0)
			DoMethod(_app(data->grpTitles), MUIM_Application_PushMethod, obj, 3, MUIM_Set, MUIA_Window_CloseRequest, TRUE);

		return TRUE;
	}

	return FALSE;
}

DEFMMETHOD(VPDFWindow_AttachView)
{
	GETDATA;
	int tabind = msg->tabind;
	int newtab;
	Object *contents, *tcontents;

	if(tabind == MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty)
	{
		int tabind = xget(data->grpRoot, MUIA_Group_ActivePage);
		Object *tcontents = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1); // +1 for title object
		if(xget(tcontents, MUIA_UserData) == TRUE)	// marker for empty page. fix
			newtab = FALSE;
		else
			newtab = TRUE;
	}
	else if(tabind == MUIV_VPDFWindow_OpenFile_NewTab)
	{
		newtab = TRUE;
	}
	else // if (MUIV_VPDFWindow_OpenFile_CurrentTab)
	{
		newtab = FALSE;
	}

	if(newtab)
		tabind = DoMethod(obj, MUIM_VPDFWindow_CreateTab);
	else
		tabind = xget(data->grpRoot, MUIA_Group_ActivePage);

	/* dispose old title and put whole new object inplace */

	tcontents = (Object*)DoMethod(data->grpTitles, MUIM_Family_GetChild, tabind);
	if (DoMethod(data->grpTitles, MUIM_Group_InitChange))
	{
		DoMethod(data->grpTitles, OM_REMMEMBER, tcontents);
		MUI_DisposeObject(tcontents);
		DoMethod(data->grpTitles, MUIM_Family_AddTail, msg->title);
		DoMethod(data->grpTitles, MUIM_Group_ExitChange);
	}

	/* dispose old contents and add new browser */

	contents = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, tabind + 1); /*  +1 for title object */
	if (DoMethod(data->grpRoot, MUIM_Group_InitChange))
	{
		DoMethod(data->grpRoot, OM_REMMEMBER, contents);
		MUI_DisposeObject(contents);
		DoMethod(data->grpRoot, MUIM_Family_AddTail, msg->docview);
		DoMethod(data->grpRoot, MUIM_Group_ExitChange);
	}

	DoMethod(obj, MUIM_VPDFWindow_UpdateActive, tabind);
	return TRUE;
}

DEFGET
{
	GETDATA;

	switch(msg->opg_AttrID)
	{
		case MUIA_VPDFWindow_ID:
			*(ULONG*)msg->opg_Storage = data->id;
		return TRUE;

		case MUIA_VPDFWindow_PDFDocument:
		{
			void *doc = NULL;
			Object *group = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, xget(data->grpRoot, MUIA_Group_ActivePage) + 1); /* +1 for title object */
			group = (Object*)DoMethod(group, MUIM_Family_GetChild, 0);

			if(group != NULL)
				doc = (void *)xget(group, MUIA_DocumentView_PDFDocument);

			*(ULONG*)msg->opg_Storage = (ULONG)doc;
			return TRUE;
		}

		case MUIA_VPDFWindow_ActiveDocumentView:
		{
			Object *group = (Object*)DoMethod(data->grpRoot, MUIM_Family_GetChild, xget(data->grpRoot, MUIA_Group_ActivePage) + 1); /* +1 for title object */
			group = (Object*)DoMethod(group, MUIM_Family_GetChild, 0);

			if(group != NULL)
			{
				*(ULONG*)msg->opg_Storage = (ULONG)group;
				return TRUE;
			}
			break;
		}

		case MUIA_VPDFWindow_FullScreen:
		{
			*(ULONG*)msg->opg_Storage = (ULONG)data->fullScreen;
			break;
		}
	}

	return DOSUPER;
}

DEFSET
{
	GETDATA;

	FORTAG(INITTAGS)
	{
	case MUIA_VPDFWindow_FullScreen:
		{
			switch(tag->ti_Data)
			{
				case MUIV_VPDFWindow_FullScreen_Toggle:
					data->fullScreen = !data->fullScreen;
				break;

				case MUIV_VPDFWindow_FullScreen_Off:
					data->fullScreen = FALSE;
				break;

				default:
					data->fullScreen = TRUE;
			}

			if(data->fullScreen)
			{
				data->win_height = xget(obj, MUIA_Window_Height);
				data->win_width = xget(obj, MUIA_Window_Width);
				data->win_leftEdge = xget(obj, MUIA_Window_LeftEdge);
				data->win_topEdge = xget(obj, MUIA_Window_TopEdge);

				set(obj, MUIA_Window_Open, FALSE);

				SetAttrs(obj,
					MUIA_Window_CloseGadget, (IPTR)FALSE,
					MUIA_Window_DragBar, (IPTR)FALSE,
					MUIA_Window_DepthGadget, (IPTR)FALSE,
					MUIA_Window_Borderless, (IPTR)TRUE,
					MUIA_Window_Title, NULL,
					MUIA_Window_ID, NULL,
					MUIA_Window_Width, MUIV_Window_Width_Visible(100),
					MUIA_Window_Height, MUIV_Window_Height_Visible(100),
					MUIA_Window_LeftEdge, 0,
					MUIA_Window_TopEdge, 0,
				TAG_END);

				set(obj, MUIA_Window_Open, TRUE);
			}
			else
			{
				set(obj, MUIA_Window_Open, FALSE);

				SetAttrs(obj,
					MUIA_Window_CloseGadget, (IPTR)TRUE,
					MUIA_Window_DragBar, (IPTR)TRUE,
					MUIA_Window_DepthGadget, (IPTR)TRUE,
					MUIA_Window_Borderless, (IPTR)FALSE,
					MUIA_Window_ID, NULL,
					MUIA_Window_Width, data->win_width,
					MUIA_Window_Height, data->win_height,
					MUIA_Window_LeftEdge, data->win_leftEdge,
					MUIA_Window_TopEdge, data->win_topEdge,
				TAG_END);

				set(obj, MUIA_Window_Open, TRUE);
				DoMethod(obj, MUIM_VPDFWindow_UpdateActive, 0);
			}
			set(data->grpRoot, MUIA_DocumentView_FullScreen, data->fullScreen);
		}
		break;
	}
	NEXTTAG

	return DOSUPER;
}

BEGINMTABLE
DECNEW
DECGET
DECSET
DECMMETHOD(VPDFWindow_CreateTab)
DECMMETHOD(VPDFWindow_OpenFile)
DECMMETHOD(VPDFWindow_SaveFile)
DECMMETHOD(VPDFWindow_UpdateActive)
DECMMETHOD(VPDFWindow_DetachView)
DECMMETHOD(VPDFWindow_AttachView)

ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, VPDFWindowClass)

struct VPDFTitleButton_Data
{
	Object *obj;
};

BEGINMTABLE2(VPDFTitleButtonClass)
ENDMTABLE

DECSUBCLASS2_NC(MUIC_Group, VPDFTitleButtonClass)

