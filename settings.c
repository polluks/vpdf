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
#include "settings.h"
#include "locale.h"
#include "system/functions.h"

#include "mcc/documentview_class.h"
#include "mcc/documentlayout_class.h"
#include "mcc/toolbar_class.h"
#include "mcc/renderer_class.h"
#include "mcc/title_class.h"

#define ATTR(a) a, #a

struct Data
{
	ULONG dummy;
	int groupsmodified;
};

struct attribute
{
	Object *object;
	unsigned int attribute;
	int group;
	unsigned int value;
	char svalue[512];        // TODO: group them in an union? fix length limit? -- kiero
	int etype;
	char name[64];
};

// GCC6 blows? -- kiero
enum { TOTAL_SETTINGS = MUIA_VPDFSettings_Last - MUIA_VPDFSettings_First + 1 };
static struct attribute attributes[TOTAL_SETTINGS];
static struct attribute attributes_old[TOTAL_SETTINGS];

Object *register_attribute(int id, char *name, Object *obj, unsigned int attribute, int group, int etype, unsigned int initial)
{
	id = id - MUIA_VPDFSettings_First;

	attributes[id].object = obj;
	attributes[id].attribute = attribute;
	attributes[id].group = group;
	attributes[id].value = 0;
	attributes[id].svalue[0] = 0;
	attributes[id].etype = etype;
	stccpy(attributes[id].name, name, sizeof(attributes[id].name));
	if(strlen(name) >= sizeof(attributes[id].name))
		printf("************SETTINGS ERROR: Attribute name too long:%s****************\n", name);

	switch(etype)
	{
		case ETYPE_NUMERIC:
			attributes[id].value = initial;
		break;

		case ETYPE_STRING:
			stccpy(attributes[id].svalue, (char*)initial, sizeof(attributes[id].svalue));
		break;
	}

	return obj;
}

/* save settings to disk */

static int setWrite(char *fname)
{
	FILE *f = fopen(fname, "w");

	if(f != NULL)
	{
		int i;

		for(i = MUIA_VPDFSettings_First + 1; i < MUIA_VPDFSettings_Last; i++)
		{
			int id = i - MUIA_VPDFSettings_First;
			if(attributes[id].object != NULL)
			{
				switch(attributes[id].etype)
				{
					case ETYPE_NUMERIC:
						fprintf(f, "%s 0x%lx\n", attributes[id].name, (IPTR)attributes[id].value);
					break;

					case ETYPE_STRING:
					{
						char *enter;
						if((enter = strrchr(attributes[id].svalue, '\n')))
							* enter = '\0';
						fprintf(f, "%s %s\n", attributes[id].name, attributes[id].svalue);
						break;
					}
				}
			}
		}
		fclose(f);
		return TRUE;
	}
	return FALSE;
}

static int setRead(char *fname)
{
	FILE *f = fopen(fname, "r");

	if(f != NULL)
	{
		int i;
		char buffer[2048];

		while(fgets(buffer, sizeof(buffer), f))
		{
			char *delim = strchr(buffer, ' ');
			char *enter = strrchr(buffer, '\n');
			
			if (enter)
			{
				*enter = '\0';
			}
			
			if(delim != NULL)
			{
				char *name = buffer;
				char *value = delim + 1;

				*delim = '\0';

				/* lookup attribute and set value */

				for(i = MUIA_VPDFSettings_First + 1; i < MUIA_VPDFSettings_Last; i++)
				{
					int id = i - MUIA_VPDFSettings_First;

					if(attributes[id].object != NULL && 0 == strcmp(name, attributes[id].name))
					{
						switch(attributes[id].etype)
						{
							case ETYPE_NUMERIC:
							{
								unsigned int lvalue = strtol(value, NULL, 0); /* values are written as hex, but in case user modifies them... */
								attributes[id].value = lvalue;
								break;
							}

							case ETYPE_STRING:
							{
								stccpy(attributes[id].svalue, value, sizeof(attributes[id].svalue));
								break;
							}
						}
					}
				}

			}
		}
		fclose(f);
		return TRUE;
	}
	return FALSE;
}

static int setSave(void)
{
	return setWrite("ENVARC:VPDF.config");
}

static int setUse(void)
{
	return setWrite("ENV:VPDF.config");
}

DEFNEW
{
	static const char *layouts_msgs[5] = {0};
	static const char *scalings_msgs[4] = {0};
	static const char *color_reproduction_msgs[2] = {0};
	static char *ps_levels[] = {"Level 2", "Level 3", NULL};
	Object *layout_cyc, *scaling_cyc, *color_reproduction_cyc, *outline_check, *log_check;
	Object *psmode_cyc, *ps_string, *psmode_check;

	if(!layouts_msgs[0])
	{
		layouts_msgs[0] = LOCSTR(MSG_SETTINGS_LAYOUT_SINGLE);
		layouts_msgs[1] = LOCSTR(MSG_SETTINGS_LAYOUT_CONTINOUS);
		layouts_msgs[2] = LOCSTR(MSG_SETTINGS_LAYOUT_FACING);
		layouts_msgs[3] = LOCSTR(MSG_SETTINGS_LAYOUT_CONTINOUS_FACING);
	}

	if(!scalings_msgs[0])
	{
		scalings_msgs[0] = LOCSTR(MSG_SETTINGS_SCALINGS_WINDOW);
		scalings_msgs[1] = LOCSTR(MSG_SETTINGS_SCALINGS_WIDTH);
		scalings_msgs[2] = LOCSTR(MSG_SETTINGS_SCALINGS_ORIGINAL);
	}

	if(!color_reproduction_msgs[0])
	{
		color_reproduction_msgs[0] = LOCSTR(MSG_SETTINGS_COLOR_REPRODUCTION_FAST);
		color_reproduction_msgs[1] = LOCSTR(MSG_SETTINGS_COLOR_REPRODUCTION_ACCURATE);
	}

	obj = DoSuperNew(cl, obj,
		MUIA_Group_Child, HVSpace,

		/* document settings group */
		MUIA_Group_Child, MUI_NewObject(MUIC_Group,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_FrameTitle, LOCSTR(MSG_SETTINGS_DOCUMENT),
			MUIA_Background, MUII_GroupBack,
			MUIA_Group_Child, (layout_cyc = MUI_NewObject(MUIC_Group,
				MUIA_Group_Horiz, TRUE,
				MUIA_Group_Child, LLabel1(LOCSTR(MSG_SETTINGS_LAYOUT)),
				MUIA_Group_Child, MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, layouts_msgs,
				TAG_END),
			TAG_END)),
			MUIA_Group_Child, (scaling_cyc = MUI_NewObject(MUIC_Group,
				MUIA_Group_Horiz, TRUE,
				MUIA_Group_Child, LLabel1(LOCSTR(MSG_SETTINGS_SCALING)),
				MUIA_Group_Child, MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, scalings_msgs,
				TAG_END),
			TAG_END)),
			MUIA_Group_Child, (color_reproduction_cyc = MUI_NewObject(MUIC_Group,
				MUIA_Group_Horiz, TRUE,
				MUIA_Group_Child, LLabel1(LOCSTR(MSG_SETTINGS_COLOR_REPRODUCTION)),
				MUIA_Group_Child, MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, color_reproduction_msgs,
				TAG_END),
			TAG_END)),
		TAG_END),

		/* windows settings group */
		MUIA_Group_Child, MUI_NewObject(MUIC_Group,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_FrameTitle, LOCSTR(MSG_WINDOWS),
			MUIA_Background, MUII_GroupBack,
			MUIA_Group_Columns, 2,
			MUIA_Group_Child, (outline_check = MUI_NewObject(MUIC_Image,
				MUIA_Image_Spec, "6:15",
				MUIA_ShowSelState, FALSE,
				MUIA_Selected, TRUE,
				MUIA_InputMode, MUIV_InputMode_Toggle,
				MUIA_CycleChain, TRUE,
			TAG_END)),
			MUIA_Group_Child, MUI_NewObject(MUIC_Group,
				MUIA_Group_Horiz, TRUE,
				MUIA_Group_Child, LLabel1(LOCSTR(MSG_SETTINGS_OUTLINE)),
				MUIA_Group_Child, HVSpace,
			TAG_END),
			MUIA_Group_Child, (log_check = MUI_NewObject(MUIC_Image,
				MUIA_Image_Spec, "6:15",
				MUIA_ShowSelState, FALSE,
				MUIA_Selected, TRUE,
				MUIA_InputMode, MUIV_InputMode_Toggle,
				MUIA_CycleChain, TRUE,
			TAG_END)),
			MUIA_Group_Child, MUI_NewObject(MUIC_Group,
				MUIA_Group_Horiz, TRUE,
				MUIA_Group_Child, LLabel1(LOCSTR(MSG_SETTINGS_LOGWINDOW)),
				MUIA_Group_Child, HVSpace,
			TAG_END),
		TAG_END),

		/* printing settings group */
		MUIA_Group_Child, MUI_NewObject(MUIC_Group,
			MUIA_ShowMe, FALSE,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_FrameTitle, LOCSTR(MSG_PRINTING),
			MUIA_Background, MUII_GroupBack,
				MUIA_Group_Child, (ps_string = MUI_NewObject(MUIC_String,
					MUIA_Frame, MUIV_Frame_String,
					MUIA_String_Contents, "PS:",
					MUIA_String_MaxLen, 512,
				TAG_END)),
				MUIA_Group_Child, (psmode_check = CheckMark(TRUE)),
				MUIA_Group_Child, (psmode_cyc = MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, ps_levels,
				TAG_END)),
		TAG_END),

		MUIA_Group_Child, HVSpace,
	TAG_MORE, INITTAGS);

	if(obj != NULL)
	{
		register_attribute(ATTR(MUIA_VPDFSettings_Layout),            layout_cyc,             MUIA_Cycle_Active, VPDF_SGROUP_LAYOUT,  ETYPE_NUMERIC, MUIV_DocumentView_Layout_Single);
		register_attribute(ATTR(MUIA_VPDFSettings_Scaling),           scaling_cyc,            MUIA_Cycle_Active, VPDF_SGROUP_SCALING, ETYPE_NUMERIC, MUIV_DocumentLayout_Scaling_FitPage);
		register_attribute(ATTR(MUIA_VPDFSettings_ColorReproduction), color_reproduction_cyc, MUIA_Cycle_Active, VPDF_SGROUP_COLOR,   ETYPE_NUMERIC, MUIV_DocumentView_ColorReproduction_Fast);

		register_attribute(ATTR(MUIA_VPDFSettings_Outline), outline_check, MUIA_Selected, VPDF_SGROUP_GUI, ETYPE_NUMERIC, TRUE);
		register_attribute(ATTR(MUIA_VPDFSettings_OpenLog), log_check,     MUIA_Selected, VPDF_SGROUP_GUI, ETYPE_NUMERIC, TRUE);

		register_attribute(ATTR(MUIA_VPDFSettings_PSName),       ps_string,    MUIA_String_Contents, VPDF_SGROUP_PRINTER, ETYPE_STRING,  (IPTR)"PS:");
		register_attribute(ATTR(MUIA_VPDFSettings_PrintingMode), psmode_check, MUIA_Selected,        VPDF_SGROUP_PRINTER, ETYPE_NUMERIC, TRUE);
		register_attribute(ATTR(MUIA_VPDFSettings_PSMode),       psmode_cyc,   MUIA_Cycle_Active,    VPDF_SGROUP_PRINTER, ETYPE_NUMERIC, 0);

		/* load from file */
		setRead("ENV:VPDF.config");

		/* move to objects and make a snapshot */

		DoMethod(obj, MUIM_VPDFSettings_ToObjects);
		memcpy(attributes_old, attributes, sizeof(attributes));

		// setWrite("ENVARC:VPDF.config"); why are the settings written here?
	}

	return (ULONG)obj;
}

DEFGET
{
	if(msg->opg_AttrID > MUIA_VPDFSettings_First && msg->opg_AttrID < MUIA_VPDFSettings_Last)
	{
		int id = msg->opg_AttrID - MUIA_VPDFSettings_First;

		switch(attributes[id].etype)
		{
			case ETYPE_NUMERIC:
				*(ULONG*)msg->opg_Storage = (ULONG)attributes[id].value;
			break;

			case ETYPE_STRING:
				*(ULONG*)msg->opg_Storage = (ULONG)attributes[id].svalue;
			break;
		}

		return TRUE;
	}

	return DOSUPER;

}

DEFSET
{
	//GETDATA;

	struct TagItem * tag, *_tags = INITTAGS;
	while((tag = NextTagItem(&_tags)))
	{
		if(tag->ti_Tag > MUIA_VPDFSettings_First && tag->ti_Tag < MUIA_VPDFSettings_Last)
		{
			int id = tag->ti_Tag - MUIA_VPDFSettings_First;

			switch(attributes[id].etype)
			{
				case ETYPE_NUMERIC:
					attributes[id].value = tag->ti_Data;
				break;

				case ETYPE_STRING:
					stccpy(attributes[id].svalue, (char*)tag->ti_Data, sizeof(attributes[id].svalue));
				break;
			}

			if(attributes[id].object != NULL)
				nnset(attributes[id].object, attributes[id].attribute, tag->ti_Data);
		}
	}

	return DOSUPER;

}

DEFMMETHOD(VPDFSettings_FromObjects)
{
	GETDATA;
	int i;

	data->groupsmodified = 0;

	for(i = MUIA_VPDFSettings_First + 1; i < MUIA_VPDFSettings_Last; i++)
	{
		int id = i - MUIA_VPDFSettings_First;
		if(attributes[id].object != NULL)
		{
			switch(attributes[id].etype)
			{
				case ETYPE_NUMERIC:
					attributes[id].value = xget(attributes[id].object, attributes[id].attribute);
					if(attributes_old[id].value != attributes[id].value)
						data->groupsmodified |= attributes[id].group;
				break;
				case ETYPE_STRING:
					stccpy(attributes[id].svalue, (char*)xget(attributes[id].object, attributes[id].attribute), sizeof(attributes[id].svalue));
					if(strcmp(attributes_old[id].svalue, attributes[id].svalue))
						data->groupsmodified |= attributes[id].group;
				break;
			}
		}
	}

	return data->groupsmodified;
}

DEFMMETHOD(VPDFSettings_ToObjects)
{
	GETDATA;
	int i;
	int modified = 0;

	for(i = MUIA_VPDFSettings_First + 1; i < MUIA_VPDFSettings_Last; i++)
	{
		int id = i - MUIA_VPDFSettings_First;
		if(attributes[id].object != NULL)  /* always set. no matter what old state looked like. snapshot might be invalid */
		{
			switch(attributes[id].etype)
			{
				case ETYPE_NUMERIC:
					nnset(attributes[id].object, attributes[id].attribute, attributes[id].value);
				break;

				case ETYPE_STRING:
					nnset(attributes[id].object, attributes[id].attribute, attributes[id].svalue);
				break;
			}
		}

		switch(attributes[id].etype)
		{
			case ETYPE_NUMERIC:
				if(attributes[id].value != attributes_old[id].value)
					modified |= attributes[id].group;
			break;

			case ETYPE_STRING:
				if(strcmp(attributes[id].svalue, attributes_old[id].svalue))
					modified |= attributes[id].group;
			break;
		}
	}

	data->groupsmodified = 0;

	return modified;
}

DEFMMETHOD(VPDFSettings_Use)
{
	unsigned int modified = DoMethod(obj, MUIM_VPDFSettings_FromObjects);
	setUse();
	memcpy(attributes_old, attributes, sizeof(attributes));
	return modified;
}

DEFMMETHOD(VPDFSettings_Save)
{
	unsigned int modified = DoMethod(obj, MUIM_VPDFSettings_FromObjects);
	setUse();
	setSave();
	memcpy(attributes_old, attributes, sizeof(attributes));
	return modified;
}

DEFMMETHOD(VPDFSettings_Cancel)
{
	unsigned int modified;
	memcpy(attributes, attributes_old, sizeof(attributes));
	return DoMethod(obj, MUIM_VPDFSettings_ToObjects);
}

BEGINMTABLE
DECNEW
DECGET
DECSET
DECMMETHOD(VPDFSettings_FromObjects)
DECMMETHOD(VPDFSettings_ToObjects)
DECMMETHOD(VPDFSettings_Use)
DECMMETHOD(VPDFSettings_Save)
DECMMETHOD(VPDFSettings_Cancel)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, VPDFSettingsClass)
