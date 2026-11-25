
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
#include "application.h"
#include "window.h"
#include "logger.h"
#include "locale.h"
#include "settings.h"
#include "printer.h"
#include "reggae_export.h"
#include "system/functions.h"

#include "mcc/documentview_class.h"
#include "mcc/toolbar_class.h"
#include "mcc/renderer_class.h"
#include "mcc/title_class.h"
#include "mcc/documentlayout_class.h"

#include <mui/Aboutbox_mcc.h>
#include "version.h"

#define MAXRECENT 10

struct Data
{
	Object *renderer;
	/*Object *menu; not used! */
	int windowid;
	Object *logger;
	Object *loggerwin;
	Object *settings;
	Object *printer;
	Object *reggae_export;
	char *recent[MAXRECENT];
	Object *aboutbox;
};

/* MEnu description */

enum
{
	MEN_PROJECT = 1,
	MEN_ABOUT, MEN_OPEN_FILE, MEN_OPEN_URL, MEN_SAVE_FILE, MEN_TAB_NEW, MEN_TAB_CLOSE, MEN_WINDOW_NEW, MEN_WINDOW_CLOSE,
	MEN_EXPORT,
	MEN_OPEN_RECENT,
	MEN_PRINT,
	MEN_QUIT,
	MEN_EDIT, MEN_EDIT_COPY,
	MEN_VIEW, MEN_VIEW_FULLSCREEN, MEN_VIEW_OUTLINE, MEN_VIEW_TOOLBAR,
		MEN_VIEW_SET_LAYOUT, MEN_VIEW_SET_LAYOUT_SINGLE, MEN_VIEW_SET_LAYOUT_CONTINOUS, MEN_VIEW_SET_LAYOUT_FACING, MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING,
		MEN_VIEW_SET_ZOOM, MEN_VIEW_SET_ZOOM_1, MEN_VIEW_SET_ZOOM_16 = MEN_VIEW_SET_ZOOM_1 + 15, MEN_VIEW_SET_ZOOM_FIT_WIDTH, MEN_VIEW_SET_ZOOM_FIT_PAGE,
	MEN_WINDOWS, MEN_WINDOWS_LOG,
	MEN_SETTINGS, MEN_SETTINGS_SETTINGS, MEN_SETTINGS_SAVESETTINGS, MEN_SETTINGS_MUI,
	MEN_OPEN_RECENT0 = 100 /* keep it last*/
};

Object *menu[MEN_OPEN_RECENT0 + 1];

/* rexx handling */

#define REXXHOOK(name, param)	static const struct Hook name = { { NULL, NULL }, (HOOKFUNC)&RexxEmul, NULL, (APTR)(param) }

enum
{
    REXX_ABOUT,
    REXX_SAVE,
    REXX_OPEN,
    REXX_APPTOFRONT
};

static LONG Rexx(void);
static const struct EmulLibEntry RexxEmul = { TRAP_LIB, 0, (void (*)())&Rexx };
REXXHOOK(RexxHookAbout, REXX_ABOUT);
REXXHOOK(RexxHookSave, REXX_SAVE);
REXXHOOK(RexxHookOpen, REXX_OPEN);
REXXHOOK(RexxHookAppToFront, REXX_APPTOFRONT);

/* repeated from toolbar_class.c - needs refactoring... */
static float zoomPresets[] = {8.33f, 12.5f, 25.0f, 33.33f, 50.0f, 66.67f, 75.0f, 100.0f, 125.0f, 150.0f, 200.0f, 300.0f, 400.0f, 600.0f, 800.0f, 1000.0f};

static const struct MUI_Command rexxcommands[] =
{
	{ "ABOUT",        NULL,                0, (struct Hook *)&RexxHookAbout      },
	{ "SAVE",         NULL,                0, (struct Hook *)&RexxHookSave       },
	{ "OPEN",         "FILE/K,NEWTAB/S",   2, (struct Hook *)&RexxHookOpen       },
	{ "APPTOFRONT",   "APPTOFRONT/S",      1, (struct Hook *)&RexxHookAppToFront },
	{ NULL,           NULL,                0, NULL                               }
};

/* credits */

static CONST_STRPTR __credits =
	"\33b%p\33n\n"
	"\tMichal 'kiero' Wozniak\n\n"
	"\33b%W\33n\n"
	"\twozniak_m@interia.pl\n\n"
	"\33b%P\33n\n"
	"\tMichal Zukowski\n\tFrank Mariak\n\n"
	"\33b%§\33n\n"
	"\tVPDF uses following libraries:\n"
	"\tPoppler [libpoppler-"POPPLERVER"] (http://www.poppler.freedesktop.org)\n"
	"\tXPDF (http://www.foolabs.com/xpdf/)\n"
	"\tFreeType (http://www.freetype.org)\n\n"
	"\tThis software is based in part on the work of the\n"
	"\tIndependent JPEG Group";
/* */

static void reqInit(char *path);
static void reqEnd(void);


Object *CreateMenuitem(CONST_STRPTR text, CONST_STRPTR shortcut)
{
	Object *obj;

	obj = MUI_NewObject(MUIC_Menuitem,
		MUIA_Menuitem_Title, text,
		(shortcut) ? MUIA_Menuitem_Shortcut : TAG_IGNORE, shortcut,
	TAG_END);

	return obj;
}

Object *CreateMenuitemCheckit(CONST_STRPTR text, CONST_STRPTR shortcut)
{
	Object *obj;

	obj = MUI_NewObject(MUIC_Menuitem,
		MUIA_Menuitem_Title, text,
		(shortcut) ? MUIA_Menuitem_Shortcut : TAG_IGNORE, shortcut,
		MUIA_Menuitem_Toggle, TRUE,
		MUIA_Menuitem_Checkit, TRUE,
		MUIA_Menuitem_Checked, FALSE,
	TAG_END);

	return obj;
}

DEFNEW
{
	Object * settings, *settingswin, *printerwin, *exportwin;
	Object * btnSave, *btnUse, *btnCancel;

	obj = DoSuperNew(cl, obj,
		MUIA_Application_Menustrip, menu[0] = MUI_NewObject(MUIC_Menustrip,
			MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
				MUIA_Menu_Title, LOCSTR(MSG_MENU_PROJECT),
				MUIA_Family_Child, menu[MEN_WINDOW_NEW] = CreateMenuitem(LOCSTR(MSG_MENU_NEWWIN), "N"),
				MUIA_Family_Child, menu[MEN_TAB_NEW] = CreateMenuitem(LOCSTR(MSG_MENU_NEWTAB), "T"),
				/*
				MUIA_Family_Child, menu[MEN_OPEN_URL] = CreateMenuitem(LOCSTR(MSG_MENU_URL), "T"),
				*/
				MUIA_Family_Child, menu[MEN_OPEN_FILE] = CreateMenuitem(LOCSTR(MSG_MENU_FILE), "O"),
				MUIA_Family_Child, menu[MEN_OPEN_RECENT] = CreateMenuitem(LOCSTR(MSG_MENU_RECENT), 0),
				MUIA_Family_Child, menu[MEN_SAVE_FILE] = CreateMenuitem(LOCSTR(MSG_MENU_SAVE), "W"),
				MUIA_Family_Child, menu[MEN_EXPORT] = CreateMenuitem(LOCSTR(MSG_MENU_EXPORT), "E"),
				MUIA_Family_Child, menu[MEN_PRINT] = CreateMenuitem(LOCSTR(MSG_MENU_PRINT), "P"),
				MUIA_Family_Child, CreateMenuitem(NM_BARLABEL, NULL),
				MUIA_Family_Child, menu[MEN_ABOUT] = CreateMenuitem(LOCSTR(MSG_MENU_ABOUT), "?"),
				MUIA_Family_Child, CreateMenuitem(NM_BARLABEL, NULL),
				MUIA_Family_Child, menu[MEN_QUIT] = CreateMenuitem(LOCSTR(MSG_MENU_QUIT), "Q"),
			TAG_END),
			MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
				MUIA_Menu_Title, LOCSTR(MSG_MENU_EDIT),
				MUIA_Family_Child, menu[MEN_EDIT_COPY] = CreateMenuitem(LOCSTR(MSG_MENU_COPY), "C"),
			TAG_END),
			MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
				MUIA_Menu_Title, LOCSTR(MSG_MENU_VIEW),
				MUIA_Family_Child, menu[MEN_VIEW_FULLSCREEN] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_FULLSCREEN), "F"),
				MUIA_Family_Child, CreateMenuitem(NM_BARLABEL, NULL),
				MUIA_Family_Child, menu[MEN_VIEW_OUTLINE] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_OUTLINE), 0),
				MUIA_Family_Child, menu[MEN_VIEW_TOOLBAR] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_TOOLBAR), 0),
				MUIA_Family_Child, CreateMenuitem(NM_BARLABEL, NULL),
				MUIA_Family_Child, menu[MEN_VIEW_SET_ZOOM] = MUI_NewObject(MUIC_Menu,
					MUIA_Menu_Title, LOCSTR(MSG_MENU_SET_ZOOM),
				TAG_END),
				MUIA_Family_Child, CreateMenuitem(NM_BARLABEL, NULL),
				MUIA_Family_Child, menu[MEN_VIEW_SET_LAYOUT] = MUI_NewObject(MUIC_Menu,
					MUIA_Menu_Title, LOCSTR(MSG_MENU_SET_LAYOUT),
					MUIA_Family_Child, menu[MEN_VIEW_SET_LAYOUT_SINGLE] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_SET_LAYOUT_SINGLE), 0),
					MUIA_Family_Child, menu[MEN_VIEW_SET_LAYOUT_CONTINOUS] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_SET_LAYOUT_CONTINOUS), 0),
					MUIA_Family_Child, menu[MEN_VIEW_SET_LAYOUT_FACING] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_SET_LAYOUT_FACING), 0),
					MUIA_Family_Child, menu[MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING] = CreateMenuitemCheckit(LOCSTR(MSG_MENU_VIEW_SET_LAYOUT_CONTINOUS_FACING), 0),
				TAG_END),
			TAG_END),
			MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
				MUIA_Menu_Title, LOCSTR(MSG_MENU_WINDOWS),
				MUIA_Family_Child, menu[MEN_WINDOWS_LOG] = CreateMenuitem(LOCSTR(MSG_MENU_LOG), 0),
			TAG_END),
			MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
				MUIA_Menu_Title, LOCSTR(MSG_MENU_SETTINGS),
				MUIA_Family_Child, menu[MEN_SETTINGS_SETTINGS] = CreateMenuitem(LOCSTR(MSG_MENU_SETTINGS_SETTINGS), "S"),
				MUIA_Family_Child, menu[MEN_SETTINGS_SAVESETTINGS] = CreateMenuitem(LOCSTR(MSG_MENU_SETTINGS_SAVE), 0),
				MUIA_Family_Child, menu[MEN_SETTINGS_MUI] = CreateMenuitem(LOCSTR(MSG_MENU_SETTINGS_MUI), 0),
			TAG_END),
		TAG_END),
		MUIA_Application_Commands, &rexxcommands,
		MUIA_Application_Window, settingswin = MUI_NewObject(MUIC_Window,
			MUIA_Window_Title, "VPDF · Setttings",
			MUIA_Window_Width, MUIV_Window_Width_Screen(30),
			MUIA_Window_Height, MUIV_Window_Height_Screen(40),
			MUIA_Window_ID, MAKE_ID('V', 'P', 'S', 'T'),
			MUIA_Window_RootObject,	MUI_NewObject(MUIC_Group,
				MUIA_Group_Child, settings = NewObject(getVPDFSettingsClass(), NULL,
					MUIA_Frame, MUIV_Frame_Group,
				TAG_END),
				MUIA_Group_Child, MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					MUIA_Group_Child, btnSave = SimpleButton(LOCSTR(MSG_SAVE)),
					MUIA_Group_Child, HSpace(0),
					MUIA_Group_Child, btnUse = SimpleButton(LOCSTR(MSG_USE)),
					MUIA_Group_Child, HSpace(0),
					MUIA_Group_Child, btnCancel = SimpleButton(LOCSTR(MSG_CANCEL)),
				TAG_END),
			TAG_END),
		TAG_END),
		MUIA_Application_Window, printerwin = VPDFPrinterObject, End,
		MUIA_Application_Window, exportwin = VPDFExporterObject, End,
	TAG_MORE, INITTAGS);

	if(obj != NULL)
	{
		GETDATA;
		
		int i;
		memset(data, 0, sizeof(struct Data));
		data->settings      = settings;
		data->printer       = printerwin;
		data->reggae_export = exportwin;
		char vbuf[ 128 ];

		/* spawn document renderer common for all views */
		data->renderer = RendererObject, End;

		/* TODO: make some separate singleton for reqs? */
		reqInit("");

		/* menu notifications */
		DoMethod(menu[MEN_ABOUT], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 1, MUIM_VPDF_About);
		DoMethod(menu[MEN_QUIT], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(menu[MEN_TAB_NEW], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 2, MUIM_VPDF_CreateTab, 0);
		DoMethod(menu[MEN_WINDOW_NEW], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 1, MUIM_VPDF_CreateWindow);
		DoMethod(menu[MEN_OPEN_FILE], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 4, MUIM_VPDF_OpenFile, 0, NULL, MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty);
		DoMethod(menu[MEN_SAVE_FILE], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 2, MUIM_VPDF_SaveFile, 0); // #rzk check this later
		DoMethod(menu[MEN_SETTINGS_SETTINGS], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, settingswin, 3, MUIM_Set, MUIA_Window_Open, TRUE);
		DoMethod(menu[MEN_SETTINGS_SAVESETTINGS], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, settingswin, 1, MUIM_VPDFSettings_Save);
		DoMethod(menu[MEN_SETTINGS_MUI], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 2, MUIM_Application_OpenConfigWindow, 0);
		DoMethod(menu[MEN_PRINT], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 1, MUIM_VPDF_PrintDocument);
		DoMethod(menu[MEN_EXPORT], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 1, MUIM_VPDF_ExportDocument);
		DoMethod(menu[MEN_WINDOWS_LOG], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 3, MUIM_VPDF_LogMessage, 0, NULL);
		DoMethod(menu[MEN_EDIT_COPY], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 1, MUIM_VPDF_SelectionCopy);

		DoMethod(menu[MEN_VIEW_FULLSCREEN], MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, obj, 3, MUIM_VPDF_FullScreen, MUIV_TriggerValue, 0);

		DoMethod(settingswin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, settingswin, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(btnCancel, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDF_SettingsCancel);
		DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDF_SettingsSave);
		DoMethod(btnUse, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDF_SettingsUse);

		DoMethod(printerwin, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, obj, 3, MUIM_Set, MUIA_VPDF_Locked, MUIV_TriggerValue);
		DoMethod(exportwin, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, obj, 3, MUIM_Set, MUIA_VPDF_Locked, MUIV_TriggerValue);

		set(printerwin, MUIA_VPDFPrinter_PSName, xget(_app(obj), MUIA_VPDFSettings_PSName));
		DoMethod(printerwin, MUIM_Notify, MUIA_VPDFPrinter_PSName, MUIV_EveryTime, obj, 3, MUIM_Set, MUIA_VPDFSettings_PSName, MUIV_TriggerValue);

		if(GetVar("NOTURBOPRINT", vbuf, sizeof(vbuf), GVF_GLOBAL_ONLY) >= 0)
		{
			set(_app(obj), MUIA_VPDFSettings_PrintingMode, 1);
		}

		set(printerwin, MUIA_VPDFPrinter_PrintingMode, xget(_app(obj), MUIA_VPDFSettings_PrintingMode));
		DoMethod(printerwin, MUIM_Notify, MUIA_VPDFPrinter_PrintingMode, MUIV_EveryTime, obj, 3, MUIM_Set, MUIA_VPDFSettings_PrintingMode, MUIV_TriggerValue);
		set(printerwin, MUIA_VPDFPrinter_PSMode, xget(_app(obj), MUIA_VPDFSettings_PSMode));
		DoMethod(printerwin, MUIM_Notify, MUIA_VPDFPrinter_PSMode, MUIV_EveryTime, obj, 3, MUIM_Set, MUIA_VPDFSettings_PSMode, MUIV_TriggerValue);

		for(i = 0; i < sizeof(zoomPresets) / sizeof(*zoomPresets); i++)
		{
			char pval[16];
			snprintf(pval, sizeof(pval), "%.2f%%", zoomPresets[i]);

			menu[MEN_VIEW_SET_ZOOM_1 + i] = MUI_NewObject(MUIC_Menuitem,
				MUIA_Menuitem_Title, pval,
				MUIA_Menuitem_CopyStrings, TRUE,
				//(zoomPresets[i] == 100.0f) ? MUIA_Menuitem_CommandString : TAG_IGNORE, TRUE,
				(zoomPresets[i] == 100.0f) ? MUIA_Menuitem_Shortcut : TAG_IGNORE, "0",
			TAG_END);

			DoMethod(menu[MEN_VIEW_SET_ZOOM], MUIM_Family_AddTail, menu[MEN_VIEW_SET_ZOOM_1 + i]);
			DoMethod(menu[MEN_VIEW_SET_ZOOM_1 + i], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 3, MUIM_VPDF_SetZoomInWindow, 0, i);
		}

		DoMethod(menu[MEN_VIEW_SET_ZOOM], MUIM_Family_AddTail, menu[MEN_VIEW_SET_ZOOM_FIT_WIDTH] = MUI_NewObject(MUIC_Menuitem,
			MUIA_Menuitem_Title, LOCSTR(MSG_MENU_ZOOM_FIT_WIDTH),
		TAG_END));

		DoMethod(menu[MEN_VIEW_SET_ZOOM], MUIM_Family_AddTail, menu[MEN_VIEW_SET_ZOOM_FIT_PAGE] = MUI_NewObject(MUIC_Menuitem,
			MUIA_Menuitem_Title, LOCSTR(MSG_MENU_ZOOM_FIT_PAGE),
		TAG_END));

		DoMethod(menu[MEN_VIEW_SET_ZOOM_FIT_WIDTH], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 3, MUIM_VPDF_SetZoomInWindow, 0, i++);
		DoMethod(menu[MEN_VIEW_SET_ZOOM_FIT_PAGE], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 3, MUIM_VPDF_SetZoomInWindow, 0, i);
	}

	return (ULONG)obj;
}

DEFDISP
{
	GETDATA;

	/* always 'use' settings. even if not modified */
	DoMethod(data->settings, MUIM_VPDFSettings_Use);

	/* delete all windows first as they might be using renderer object.
	 * skip settings window so all windows which save to settings have last chance now.
	 */
	{
		struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
		Object *winobj_state = (Object*)winlist->lh_Head;
		Object *winobj,*next_winobj;
		next_winobj = NextObject(&winobj_state);
		while((winobj = next_winobj) != NULL)
		{
			set(winobj, MUIA_Window_Open, FALSE);
			next_winobj = NextObject(&winobj_state);
			if(winobj != data->settings)
			{
				DoMethod(obj, OM_REMMEMBER, winobj);
				MUI_DisposeObject(winobj);
			}
		}
	}

	/* dispose renderer and the rest */

	MUI_DisposeObject(data->renderer);
	reqEnd();

	return DOSUPER;
}

static void setuprecent(Object *obj, struct Data *data);


DEFMMETHOD(VPDF_SaveFile)
{
	GETDATA;
	char *filename = msg->filename;
	int mode = msg->mode;
	Object *window = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active : msg->windowid);

	/*if(window == NULL)
	{
	  window = (Object*)DoMethod(obj, MUIM_VPDF_CreateWindow);
	  mode = MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty;
	}*/

	if(filename == NULL)
	{
		filename = (char*)DoMethod(_app(obj), MUIM_VPDF_RequestFile, MUIV_VPDF_RequestFile_Save, NULL, NULL, NULL);

		if(filename == NULL)
			return FALSE;
	}

	if(window != NULL)
	{
		if(DoMethod(window, MUIM_VPDFWindow_SaveFile, filename, mode))
		{

		}
	}

	return TRUE;
}

DEFMMETHOD(VPDF_OpenFile)
{
	GETDATA;
	char *filename = msg->filename;
	int mode = msg->mode;
	Object *window = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active : msg->windowid);

	if(window == NULL)
	{
		window = (Object*)DoMethod(obj, MUIM_VPDF_CreateWindow);
		mode = MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty;
	}

	if(filename == NULL)
	{
		filename = (char*)DoMethod(_app(obj), MUIM_VPDF_RequestFile, MUIV_VPDF_RequestFile_Load, NULL, NULL, NULL);

		if(filename == NULL)
			return FALSE;
	}

	if(window != NULL)
	{
		if(DoMethod(window, MUIM_VPDFWindow_OpenFile, filename, mode))
		{
			/* add to history */
			int i;
			int dupe = FALSE;

			for(i = 0; i < MAXRECENT && data->recent[i] != NULL && *data->recent[i] != '\0' && dupe == FALSE; i++)
				dupe = 0 == stricmp(data->recent[i], filename);

			if(dupe == FALSE)
			{
				free(data->recent[MAXRECENT - 1]);
				for(i = MAXRECENT - 1; i >= 1; i--)
					data->recent[i] = data->recent[i - 1];

				data->recent[0] = strdup(filename);
				setuprecent(obj, data);
			}
		}
	}

	return TRUE;
}

DEFMMETHOD(VPDF_OpenRecentFile)
{
	GETDATA;
	if(data->recent[msg->idx] != NULL && *data->recent[msg->idx] != '\0')
	{
		return DoMethod(obj, MUIM_VPDF_OpenFile, msg->windowid, data->recent[msg->idx], MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty);

		//KPrintF("Open recent file %s\n", data->recent[msg->idx]);
	}
	return FALSE;
}

DEFMMETHOD(VPDF_CreateTab)
{
	Object *window = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active :  msg->windowid);

	if(window != NULL)
	{
		return DoMethod(window, MUIM_VPDFWindow_CreateTab);
	}

	return (ULONG) - 1;
}

DEFMMETHOD(VPDF_CreateWindow)
{
	GETDATA;
	Object *refwindow = (Object *)DoMethod(obj, MUIM_VPDF_FindActiveWindow);
	Object *window = VPDFWindowObject,
		MUIA_VPDFWindow_ID, ++data->windowid,
		MUIA_Window_ID, MAKE_ID('V', 'P', 'W', 'O'), /* this is going to be nullified for all non-active windows */
		MUIA_Window_LeftEdge, 20,
		MUIA_Window_TopEdge, 20,
		MUIA_Window_RefWindow, refwindow,
		MUIA_Window_OverrideStoredPosition, refwindow != NULL ? TRUE : FALSE,
		End;

	if(window != NULL)
	{
		int i, id = xget(window, MUIA_VPDFWindow_ID);

		DoMethod(obj, OM_ADDMEMBER, window);
		DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, _app(obj), 5, MUIM_Application_PushMethod, obj, 2, MUIM_VPDF_CloseWindow, id);
		DoMethod(window, MUIM_Notify, MUIA_Window_Activate, TRUE, obj, 2, MUIM_VPDF_NewActiveWindow, window);

		/* find the "100% Zoom" menuitem and use it for the hotkey "0" */
		for(i = 0; i < sizeof(zoomPresets) / sizeof(*zoomPresets); i++)
		{
			if (zoomPresets[i] == 100.0f)
			{
				DoMethod(window, MUIM_Notify, MUIA_Window_InputEvent, "0", obj, 3, MUIM_VPDF_SetZoomInWindow, 0, i);
				break;
			}
		}
		set(window, MUIA_Window_Open, TRUE);
	}

	return (ULONG)window;
}

DEFMMETHOD(VPDF_HandleAppMessage)
{
	struct AppMessage *amsg = msg->appmessage;
	struct WBArg *ap = amsg->am_ArgList;
	char buf[1024];

	NameFromLock(ap->wa_Lock, buf, sizeof(buf));
	AddPart((STRPTR)buf, (STRPTR)ap->wa_Name, sizeof(buf));

	if(strMatch(buf, "#?.pdf"))
		DoMethod(obj, MUIM_VPDF_OpenFile, msg->windowid, buf, MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty);

	return 0;
}

/// Helper Functions (requesters)

M_HOOK(AslRefresh, struct FileRequester *req, struct IntuiMessage *imsg)
{
	if(imsg->Class == IDCMP_REFRESHWINDOW)
		DoMethod(req->fr_UserData, MUIM_Application_CheckRefresh);
}

static struct FileRequester *reqLoad = NULL;
static struct FileRequester *reqSave = NULL;

static void reqInit(char *path)
{
	char *dir = strdup(path);
	char *file = FilePart(path);
	char *fPart = PathPart(dir);

	if(fPart)
		dir[ fPart - dir ] = 0;

	reqSave = MUI_AllocAslRequestTags(ASL_FileRequest,
		ASLFR_TitleText, LOCSTR(MSG_SAVE_IMAGE),
		ASLFR_InitialPattern, "",
		ASLFR_DoSaveMode, TRUE,
		ASLFR_InitialDrawer, dir,
		ASLFR_InitialFile, file,
		ASLFR_DoPatterns, TRUE,
		ASLFR_RejectIcons, TRUE,
	TAG_DONE);

	reqLoad = MUI_AllocAslRequestTags(ASL_FileRequest,
		ASLFR_TitleText, LOCSTR(MSG_LOAD_IMAGE),
		ASLFR_InitialPattern, "",
		ASLFR_DoSaveMode, FALSE,
		ASLFR_InitialDrawer, dir,
		ASLFR_InitialFile, file,
		ASLFR_DoPatterns, TRUE,
		ASLFR_RejectIcons, TRUE,
	TAG_DONE);

	free(dir);
}

static void reqEnd(void)
{
	if(reqSave)
		MUI_FreeAslRequest(reqSave);
	if(reqLoad)
		MUI_FreeAslRequest(reqLoad);
}

DEFMMETHOD(VPDF_RequestFile)
{
	static char buf[1024];
	char *res = NULL;
	struct FileRequester *req;
	Object *window = (Object*)DoMethod(obj, MUIM_VPDF_FindActiveWindow);

	if(msg->mode & MUIV_VPDF_RequestFile_Load)
		req = reqLoad;
	else
		req = reqSave;

	if(req != NULL)
	{
		char *title = NULL;

		set(obj, MUIA_Application_Sleep, TRUE);

		/* will extract directory path first */

		buf[ 0 ] = 0;

		if(msg->initialdir != NULL)
			stccpy(buf, msg->initialdir, sizeof(buf));
		else if(msg->initialpath != NULL)
		{
			stccpy(buf, msg->initialpath, sizeof(buf));
			buf[(int)PathPart(buf) - (int)&buf[ 0 ] ] = 0;
		}

		/* setup proper title */

		if(msg->mode & MUIV_VPDF_RequestFile_DirectoryOnly)
			title = LOCSTR(MSG_REQ_DRAWER);
		else if(msg->mode & MUIV_VPDF_RequestFile_Load)
			title =  LOCSTR(MSG_REQ_OPEN);
		else if(msg->mode & MUIV_VPDF_RequestFile_Save)
			title =  LOCSTR(MSG_REQ_SAVE);

		if(MUI_AslRequestTags(req,
			title != NULL ? ASLFR_TitleText : TAG_IGNORE, title,
			ASLFR_UserData, obj,
			ASLFR_IntuiMsgFunc, &AslRefreshHook,
			ASLFR_PrivateIDCMP, FALSE,
			buf[ 0 ] ? ASLFR_InitialDrawer : TAG_IGNORE, buf,
			ASLFR_InitialFile, msg->initialfile ? msg->initialfile : msg->initialpath ? (char*)FilePart(msg->initialpath) : (char*)"",
			ASLFR_DrawersOnly, msg->mode & MUIV_VPDF_RequestFile_DirectoryOnly,
			ASLFR_Window, window != NULL ? xget(window, MUIA_Window_Window) : (ULONG)NULL,
		TAG_DONE))
		{
			if(*req->fr_File)
			{
				res = buf;
				stccpy(buf, req->fr_Drawer, sizeof(buf));
				if(!(msg->mode & MUIV_VPDF_RequestFile_DirectoryOnly))
					AddPart(buf, req->fr_File, sizeof(buf));
			}
			else if(*req->fr_Drawer && (msg->mode & MUIV_VPDF_RequestFile_DirectoryOnly))
			{
				res = buf;
				stccpy(buf, req->fr_Drawer, sizeof(buf));
			}
		}

		set(obj, MUIA_Application_Sleep, FALSE);
	}

	return (ULONG)res;

}

////

static void setuprecent(Object *obj, struct Data *data)
{
	int i;
	Object *o;
	DoMethod(menu[MEN_OPEN_RECENT] , MUIM_Menustrip_InitChange);

	/* cleanup recents */
	while((o = (Object*)DoMethod(menu[MEN_OPEN_RECENT], MUIM_Family_GetChild, MUIV_Family_GetChild_First)))
	{
		DoMethod(menu[MEN_OPEN_RECENT], MUIM_Family_Remove, o);
		MUI_DisposeObject(o);
	};

	for(i = 0; i < MAXRECENT && data->recent[i] != NULL && *data->recent[i] != '\0'; i++)
	{
		menu[MEN_OPEN_RECENT0 - i] = MenuitemObject,
			MUIA_Menuitem_Title, data->recent[i],
			MUIA_Menuitem_CopyStrings, TRUE,
			End;
		DoMethod(menu[MEN_OPEN_RECENT0 - i], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, obj, 3, MUIM_VPDF_OpenRecentFile, 0, i);
		DoMethod(menu[MEN_OPEN_RECENT], MUIM_Family_AddTail, menu[MEN_OPEN_RECENT0 - i]);
	}

	DoMethod(menu[MEN_OPEN_RECENT], MUIM_Menustrip_ExitChange);
}

/* this method finds window which was last activated, not currently active one! */

DEFMMETHOD(VPDF_FindActiveWindow)
{
	GETDATA;
	struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
	Object *winobj_state = (Object*)winlist->lh_Head;
	Object *winobj;
	ULONG v;

	while((winobj = NextObject(&winobj_state)))
	{
		//if (xget(winobj, MUIA_Window_Activate))
		if(xget(winobj, MUIA_Window_ID) != 0 && get(winobj, MUIA_VPDFWindow_ID, &v))
			return (ULONG)winobj;
	}

	return 0;
}

DEFMMETHOD(VPDF_FindWindowByID)
{
	GETDATA;

	if(msg->windowid == MUIV_VPDF_FindWindowByID_Active)
	{
		return DoMethod(obj, MUIM_VPDF_FindActiveWindow);
	}
	else
	{
		struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
		Object *winobj_state = (Object*)winlist->lh_Head;
		Object *winobj;
		while((winobj = NextObject(&winobj_state)))
		{
			/* validate if it's document view window */
			int v;
			if(get(winobj, MUIA_VPDFWindow_ID, &v) == FALSE)
				continue;

			if(msg->windowid == MUIV_VPDF_FindWindowByID_First)
				return (ULONG)winobj;
			else if(msg->windowid == xget(winobj, MUIA_VPDFWindow_ID))
				return (ULONG)winobj;
		}
	}

	return 0;
}

DEFMMETHOD(VPDF_CloseWindow)
{
	Object *window = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active :  msg->windowid);

	if(window != NULL)
	{
		set(window, MUIA_Window_Open, FALSE);
		DoMethod(obj, OM_REMMEMBER, window);
		MUI_DisposeObject(window);

		if(DoMethod(obj, MUIM_VPDF_FindWindowByID, MUIV_VPDF_FindWindowByID_First) == (ULONG)NULL)
			DoMethod(obj, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		return TRUE;
	}
	return FALSE;
}

DEFMMETHOD(Export)
{
	GETDATA;
	int i;

	for(i = 0; i < MAXRECENT && data->recent[i] != NULL && *data->recent[i] != '\0'; i++)
	{
		DoMethod(msg->dataspace, MUIM_Dataspace_Add, data->recent[i], strlen(data->recent[i]) + 1, ID_VPDF_RECENT0 + i);
	}
	return DOSUPER;
}

DEFMMETHOD(Import)
{
	GETDATA;
	int i, j;

	for(i = 0, j = 0; i < MAXRECENT; i++)
	{
		char *recent = (char*)DoMethod(msg->dataspace, MUIM_Dataspace_Find, ID_VPDF_RECENT0 + i);
		if(recent != NULL)
		{
			BPTR lock;
			struct Process *theProc = (struct Process *)FindTask(NULL);
			struct Window *oldWindowPtr = theProc->pr_WindowPtr;
			theProc->pr_WindowPtr = (APTR)(-1L);

			if((lock = Lock(recent, ACCESS_READ)))
			{
				data->recent[j] = strdup(recent);
				j++;
				UnLock(lock);
			}
			theProc->pr_WindowPtr = oldWindowPtr;
		}
	}

	setuprecent(obj, data);

	return DOSUPER;
}

DEFGET
{
	GETDATA;
	ULONG v;

	switch(msg->opg_AttrID)
	{
		case MUIA_VPDF_Renderer:
			*(ULONG*)msg->opg_Storage = (ULONG)data->renderer;
		return TRUE;
	}

	/* check for settings attribute */

	if(msg->opg_AttrID > MUIA_VPDFSettings_First && msg->opg_AttrID < MUIA_VPDFSettings_Last)
	{
		if(get(data->settings, msg->opg_AttrID, &v))
		{
			*(ULONG*)msg->opg_Storage = v;
			return TRUE;
		}
	}

	/* */

	return DOSUPER;
}

DEFSET
{
	GETDATA;

	FORTAG(INITTAGS)
	{
		case MUIA_VPDF_Locked:
		{
			struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
			Object *winobj_state = (Object*)winlist->lh_Head;
			Object *winobj;
			while((winobj = NextObject(&winobj_state)))
			{
				/* validate if it's document view window */
				int v;
				if(get(winobj, MUIA_VPDFWindow_ID, &v) == FALSE)
					continue;

				set(winobj, MUIA_Window_Sleep, tag->ti_Data);
			}
		}
		break;

		default:
		/* check for settings attribute */
		if(tag->ti_Tag > MUIA_VPDFSettings_First && tag->ti_Tag < MUIA_VPDFSettings_Last)
		{
			set(data->settings, tag->ti_Tag, tag->ti_Data);
		}
	}
	NEXTTAG

	return DOSUPER;
}

DEFMMETHOD(VPDF_LogMessage)
{
	GETDATA;

	if(data->logger == NULL)
	{
		data->loggerwin = WindowObject,
			MUIA_Window_Title, "VPDF · Log",
			MUIA_Window_Width, MUIV_Window_Width_Screen(30),
			MUIA_Window_Height, MUIV_Window_Height_Screen(10),
			MUIA_Window_ID, MAKE_ID('V', 'L', 'O', 'G'),
			MUIA_Window_RootObject,
			data->logger = LoggerObject, End,
			End;

		if(data->loggerwin != NULL)
		{
			DoMethod(obj, OM_ADDMEMBER, data->loggerwin);
			DoMethod(data->loggerwin, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, data->loggerwin, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		}
	}

	if(data->logger != NULL)
	{
		if(msg->message == NULL || xget(_app(obj), MUIA_VPDFSettings_OpenLog))
			set(data->loggerwin, MUIA_Window_Open, TRUE);
	}

	if(data->logger != NULL && msg->message != NULL)
	{
		DoMethod(data->logger, MUIM_Logger_LogMessage, msg->severity, msg->message);

		/* message is always strdupped() so dispose it here. logger has to make a copy! */
		free(msg->message);
	}

	return 0;
}

DEFMMETHOD(VPDF_NewActiveWindow)
{
	/* only setup window id for active window so it will be used when the application is restarted*/

	struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
	Object *winobj_state = (Object*)winlist->lh_Head;
	Object *winobj;
	while((winobj = NextObject(&winobj_state)))
	{
		/* validate if it's document view window */
		int v;
		if(get(winobj, MUIA_VPDFWindow_ID, &v) == FALSE)
			continue;

		if(msg->window == winobj)
		{
			set(winobj, MUIA_Window_ID, MAKE_ID('V', 'P', 'W', 'O'));
			DoMethod(obj, MUIM_VPDF_RefreshViewMenu, winobj);
		}
		else
			set(winobj, MUIA_Window_ID, 0);
	}

	return 0;
}

DEFMMETHOD(VPDF_SettingsCancel)
{
	GETDATA;
	DoMethod(data->settings, MUIM_VPDFSettings_Cancel);
	set(_win(data->settings), MUIA_Window_Open, FALSE);
	return 0;
}


DEFMMETHOD(VPDF_SettingsUse)
{
	GETDATA;
	DoMethod(obj, MUIM_VPDF_SettingsApply, DoMethod(data->settings, MUIM_VPDFSettings_Use));
	return 0;
}


DEFMMETHOD(VPDF_SettingsSave)
{
	GETDATA;
	DoMethod(obj, MUIM_VPDF_SettingsApply, DoMethod(data->settings, MUIM_VPDFSettings_Save));
	return 0;
}

DEFMMETHOD(VPDF_SettingsApply)
{
	GETDATA;
	set(_win(data->settings), MUIA_Window_Open, FALSE);
	return 0;
}

DEFMMETHOD(VPDF_About)
{
	GETDATA;

	if(data->aboutbox == NULL)
	{
		data->aboutbox = AboutboxObject,
			MUIA_Aboutbox_Credits, __credits,
			End;

		DoMethod(obj, OM_ADDMEMBER, data->aboutbox);
	}

	if(data->aboutbox != NULL)
		set(data->aboutbox, MUIA_Window_Open, TRUE);

	return 0;
}

DEFMMETHOD(VPDF_PrintDocument)
{
	GETDATA;
	Object *activewindow = (Object*)DoMethod(obj, MUIM_VPDF_FindActiveWindow);
	
	if(activewindow != NULL)
	{
		void *doc = (void*)xget(activewindow, MUIA_VPDFWindow_PDFDocument);
		if(doc != NULL)
		{
			ULONG active_page = 1;
			Object *docView = (Object *)xget(activewindow, MUIA_VPDFWindow_ActiveDocumentView);
			if (docView)
			{
				active_page = (ULONG)xget(docView, MUIA_DocumentView_Page); 
			}
			DoMethod(data->printer, MUIM_VPDFPrinter_PrintDocument, doc, active_page);
		}
	}
	return TRUE;
}

DEFMMETHOD(VPDF_ExportDocument)
{
	GETDATA;
	Object *activewindow = (Object*)DoMethod(obj, MUIM_VPDF_FindActiveWindow);

	if(activewindow != NULL)
	{
		void *doc = (void*)xget(activewindow, MUIA_VPDFWindow_PDFDocument);
		//KPrintF("VPDF_ExportDocument\n");
		if(doc != NULL)
			DoMethod(data->reggae_export, MUIM_VPDFExporter_ExportDocument, doc);
	}
	return TRUE;
}

DEFMMETHOD(DragDrop)
{
	GETDATA;
	int v;

	/* check if it was title object */

	if(_parent(msg->obj) != NULL && get(_parent(msg->obj), MUIA_VPDFTitle_IsTitle, &v))
	{
		struct List *winlist = (struct List*)xget(obj, MUIA_Application_WindowList);
		int ownwindow = FALSE;

		if(xget(_parent(msg->obj), MUIA_Group_ChildCount) <= 1)
			return DOSUPER;

		if(winlist != NULL)
		{
			Object *winobj_state = (Object*)winlist->lh_Head;
			Object *winobj;
			while((winobj = NextObject(&winobj_state)))
			{
				/* validate if it's document view window */
				int v;
				if(get(winobj, MUIA_VPDFWindow_ID, &v))
				{
					if(msg->x > xget(winobj, MUIA_Window_LeftEdge) && msg->x < xget(winobj, MUIA_Window_LeftEdge) + xget(winobj, MUIA_Window_Width)
					        && msg->y > xget(winobj, MUIA_Window_TopEdge) && msg->y < xget(winobj, MUIA_Window_TopEdge) + xget(winobj, MUIA_Window_Height))
					{
						ownwindow = TRUE;
					}
					break;
				}

			}
		}

		/* detach document from window (create new and add the document) */

		if(ownwindow == FALSE)
		{
			Object *oldwindow = _win(msg->obj);
			Object *newwindow = (Object*)DoMethod(obj, MUIM_VPDF_CreateWindow);

			if(newwindow != NULL)
			{
				struct MUIP_VPDFWindow_DetachView msg;
				msg.MethodID = MUIM_VPDFWindow_DetachView;
				msg.tabind = MUIV_VPDFWindow_DetachView_Active;

				if(DoMethodA(oldwindow, &msg))
				{
					DoMethod(newwindow, MUIM_VPDFWindow_AttachView, MUIV_VPDFWindow_AttachView_CurrentTabIfEmpty, msg.docview, msg.title);
				}
			}
		}
	}

	return DOSUPER;
}

/* rexx handling */

static LONG Rexx(void)
{
	struct Hook *h = (struct Hook*)(APTR)REG_A0;
	Object *obj = (Object*)REG_A2;
	IPTR *params = (IPTR*)REG_A1;

	switch((ULONG)h->h_Data)
	{
		case REXX_ABOUT:
		break;

		case REXX_OPEN:
		{
			char *file = (char*)*params;
			ULONG newtab = params[1];

			char *ftemp = file;
			while(*ftemp != '\0')
			{
				if(*ftemp == '\\')
					*ftemp = ' ';
				ftemp++;
			}

			DoMethod(obj, MUIM_VPDF_OpenFile, 0, file, newtab ? MUIV_VPDFWindow_OpenFile_CurrentTab : MUIV_VPDFWindow_OpenFile_CurrentTabIfEmpty);
			break;
		}

		case REXX_SAVE:
		break;

		case REXX_APPTOFRONT:
		break;
	}

	return 0;
}

DEFMMETHOD(VPDF_SelectionCopy)
{
	GETDATA;
	Object *activewindow = (Object*)DoMethod(obj, MUIM_VPDF_FindActiveWindow);

	if(activewindow != NULL)
	{
		Object *documentview = (Object*)xget(activewindow, MUIA_VPDFWindow_ActiveDocumentView);
		if(documentview != NULL)
		{
			DoMethod(documentview, MUIM_DocumentView_SelectionCopy);
		}
	}

	return TRUE;
}

DEFMMETHOD(VPDF_FullScreen)
{
	Object *win = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active : msg->windowid);

	if(!win)
		return FALSE;

	return set(win, MUIA_VPDFWindow_FullScreen, msg->fullScreen);
}

DEFMMETHOD(VPDF_RefreshViewMenu)
{
	Object *docView;
	int i;

	if(!msg->window || !xget(msg->window, MUIA_Window_Activate))
		return FALSE;

	nnset(menu[MEN_VIEW_FULLSCREEN], MUIA_Menuitem_Checked, xget(msg->window, MUIA_VPDFWindow_FullScreen));

	/* clear state */
	nnset(menu[MEN_VIEW_OUTLINE], MUIA_Menuitem_Checked, FALSE);

	for(i = MEN_VIEW_SET_LAYOUT_SINGLE; i <= MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING; i++)
		nnset(menu[i], MUIA_Menuitem_Checked, FALSE);

	nnset(menu[MEN_VIEW_TOOLBAR], MUIA_Menuitem_Checked, FALSE);

	for(i = MEN_VIEW_OUTLINE; i <= MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING; i++)
	{
		DoMethod(menu[i], MUIM_KillNotify, MUIA_Menuitem_Trigger);
		DoMethod(menu[i], MUIM_KillNotify, MUIA_Menuitem_Checked);
	}

	/* get state and set to menu */
	docView = (Object *)xget(msg->window, MUIA_VPDFWindow_ActiveDocumentView);

	if(docView)
	{
		int i;

		nnset(menu[MEN_VIEW_OUTLINE], MUIA_Menuitem_Checked, xget(docView, MUIA_DocumentView_Outline));

		for(i = MEN_VIEW_SET_LAYOUT_SINGLE; i <= MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING; i++)
			nnset(menu[i], MUIA_Menuitem_Checked, FALSE);

		switch(xget(docView, MUIA_DocumentView_Layout))
		{
			case MUIV_DocumentView_Layout_Single:
				nnset(menu[MEN_VIEW_SET_LAYOUT_SINGLE], MUIA_Menuitem_Checked, TRUE);
			break;

			case MUIV_DocumentView_Layout_ContinuousSingle:
				nnset(menu[MEN_VIEW_SET_LAYOUT_CONTINOUS], MUIA_Menuitem_Checked, TRUE);
			break;

			case MUIV_DocumentView_Layout_Facing:
				nnset(menu[MEN_VIEW_SET_LAYOUT_FACING], MUIA_Menuitem_Checked, TRUE);
			break;

			case MUIV_DocumentView_Layout_ContinuousFacing:
				nnset(menu[MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING], MUIA_Menuitem_Checked, TRUE);
			break;
		}

		nnset(menu[MEN_VIEW_TOOLBAR], MUIA_Menuitem_Checked, xget(docView, MUIA_DocumentView_Toolbar));

		DoMethod(menu[MEN_VIEW_OUTLINE], MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set, MUIA_DocumentView_Outline, MUIV_TriggerValue);
		DoMethod(menu[MEN_VIEW_TOOLBAR], MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set, MUIA_DocumentView_Toolbar, MUIV_TriggerValue);

		DoMethod(menu[MEN_VIEW_SET_LAYOUT_SINGLE],           MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set,
			MUIA_DocumentView_Layout, MUIV_DocumentView_Layout_Single);
		DoMethod(menu[MEN_VIEW_SET_LAYOUT_CONTINOUS],        MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set,
			MUIA_DocumentView_Layout, MUIV_DocumentView_Layout_ContinuousSingle);
		DoMethod(menu[MEN_VIEW_SET_LAYOUT_FACING],           MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set,
			MUIA_DocumentView_Layout, MUIV_DocumentView_Layout_Facing);
		DoMethod(menu[MEN_VIEW_SET_LAYOUT_CONTINOUS_FACING], MUIM_Notify, MUIA_Menuitem_Checked, MUIV_EveryTime, docView, 3, MUIM_Set,
			MUIA_DocumentView_Layout, MUIV_DocumentView_Layout_ContinuousFacing);

		return TRUE;
	}

	return 0;
}

DEFMMETHOD(VPDF_SetZoomInWindow)
{
	Object *win = (Object*)DoMethod(obj, MUIM_VPDF_FindWindowByID, msg->windowid == 0 ? MUIV_VPDF_FindWindowByID_Active : msg->windowid);
	Object *docView;
	float zoom;

	if(!win)
		return FALSE;

	docView = (Object *)xget(win, MUIA_VPDFWindow_ActiveDocumentView);

	if(!docView)
		return FALSE;

	if(msg->zoom == sizeof(zoomPresets) / sizeof(*zoomPresets))
	{
		set(docView, MUIA_DocumentLayout_Scaling, MUIV_DocumentLayout_Scaling_FitWidth);
	}
	else if(msg->zoom >= sizeof(zoomPresets) / sizeof(*zoomPresets))
	{
		set(docView, MUIA_DocumentLayout_Scaling, MUIV_DocumentLayout_Scaling_FitPage);
	}
	else
	{
		zoom = zoomPresets[msg->zoom] / 100.0f;
		SetAttrs(docView,
			MUIA_DocumentLayout_Scaling, MUIV_DocumentLayout_Scaling_Zoom,
			MUIA_DocumentLayout_Zoom, (int)(zoom * 65536),
		TAG_DONE);
	}
	return TRUE;
}

/* */

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSET
DECMMETHOD(Import)
DECMMETHOD(Export)
DECMMETHOD(DragDrop)
DECMMETHOD(VPDF_OpenFile)
DECMMETHOD(VPDF_SaveFile)
DECMMETHOD(VPDF_CreateTab)
DECMMETHOD(VPDF_CreateWindow)
DECMMETHOD(VPDF_HandleAppMessage)
DECMMETHOD(VPDF_RequestFile)
DECMMETHOD(VPDF_OpenRecentFile)
DECMMETHOD(VPDF_FindActiveWindow)
DECMMETHOD(VPDF_FindWindowByID)
DECMMETHOD(VPDF_CloseWindow)
DECMMETHOD(VPDF_LogMessage)
DECMMETHOD(VPDF_NewActiveWindow)
DECMMETHOD(VPDF_SettingsCancel)
DECMMETHOD(VPDF_SettingsSave)
DECMMETHOD(VPDF_SettingsUse)
DECMMETHOD(VPDF_SettingsApply)
DECMMETHOD(VPDF_About)
DECMMETHOD(VPDF_PrintDocument)
DECMMETHOD(VPDF_ExportDocument)
DECMMETHOD(VPDF_SelectionCopy)
DECMMETHOD(VPDF_FullScreen)
DECMMETHOD(VPDF_RefreshViewMenu)
DECMMETHOD(VPDF_SetZoomInWindow)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Application, VPDFClass)
