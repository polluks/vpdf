/// System includes
#define AROS_ALMOST_COMPATIBLE
#include <proto/muimaster.h>
#include <libraries/mui.h>

#include <proto/exec.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <exec/libraries.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/locale.h>
#include <mui/Rawimage_mcc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <memory.h>

#include <proto/alib.h>
#include <proto/utility.h>
#include <proto/ppd.h>

////

#include <private/vapor/vapor.h>
#include "util.h"
#include "poppler_printer.h"
#include "printer.h"
#include "pagenumberclass.h"
#include "arrowstring_class.h"
#include "system/functions.h"
#include "locale.h"
#include "settings.h"
#include "poppler.h"
#include "printer_image.h"

#define MAX_PROFILE_NAMES	64

struct Data
{
	Object *strOutput;
	Object *sldFirst;
	Object *sldLast;
	Object *cycOddEven;
	Object *btnPrint;
	Object *btnCancel;
	Object *chkPS;
	Object *btnPrefs;
	Object *chkAutorotatescale;
	Object *strNumbers;
	Object *grpRight;
	Object *chkReverse;
	Object *chkCollate;
	Object *strCopies;
	Object *cycPSVersion;
	Object *grpCollate;
	Object *btnReset;
	Object *grpPage;
	Object *gauge;
	Object *cycPageScaling;
	Object *cycPagesPerSheet;
	Object *cycPagesOrder;
	Object *cycProfiles;
	Object *btnCurrentPage;
	Object *proc;
	
	void *doc;
	char status[256];
	int printing;
	int quit;
	int close;
	int max;
	int active_page;
	
	struct printerjob pj;

	STRPTR ProfileNames[MAX_PROFILE_NAMES];
	BOOL profiles_exist;
};


#define D(x)
void kprintf(char *fmt,...);

static STRPTR oddeven[4] = {NULL};
static STRPTR scaling_modes[5] = {NULL};
static STRPTR pages_per_sheet[] =  {"2", "4", "6", "9", "Custom", NULL};
static STRPTR pages_order[10] = {NULL}; 
static char *ps_levels[] = {"Level 2", "Level 3", NULL};
const STRPTR layouts[] = { "lrtb", "lrbt", "rltb", "rlbt", "tblr", "btlr", "tbrl", "btrl"};


int pages_per_sheet_table[] = {2, 4, 6, 9, 99};

#define  PROFILES_DIR "Sys:Prefs/Printers/Profiles"

int refresh_profiles(struct Data *data)
{
	BPTR lock;
	int ret = TRUE;
	struct FileInfoBlock *FIB;
	int i=0;

	for(i=0; i<MAX_PROFILE_NAMES && data->ProfileNames[i]; i++)
	{
		FreeVec(data->ProfileNames[i]);
		data->ProfileNames[i] = 0;
	}

	i=0;

	if (( lock = Lock(PROFILES_DIR, SHARED_LOCK)))
	{
		BOOL success;
		if ((FIB  = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR)))
		{
		  int profile_count = 0;

		  success = Examine(lock, FIB);
		  success = ExNext(lock, FIB);

		  while (success != DOSFALSE)
		  {
			if((i+1) >= MAX_PROFILE_NAMES)
				break;

			/* don't show dirs */	
			if (FIB->fib_DirEntryType < 0)
			{
				STRPTR profname;

				D(kprintf("Adding profiles: %s\n", FIB->fib_FileName));
				if (( profname = (STRPTR)AllocVec(strlen(FIB->fib_FileName)+1, MEMF_ANY)) != NULL)
				{
					int ins_pos=1;	// offset 0 is occupied by (ToFile) already
					strcpy(profname, FIB->fib_FileName);

					if(i > 0)
					{
						while(data->ProfileNames[ins_pos] && (strcmp(profname,data->ProfileNames[ins_pos]) > 0)  && (ins_pos <= profile_count)) ins_pos++;

						if(ins_pos <= profile_count)
						{
							int mov_pos = profile_count+1;

							while(mov_pos >= ins_pos)
							{
								data->ProfileNames[mov_pos+1] = data->ProfileNames[mov_pos];
								mov_pos--;
							}
						}
					}
					
					data->ProfileNames[ins_pos] = profname;
					i++;
					profile_count++;
				}
			}
			success = ExNext(lock, FIB);
		  }
		  FreeVec(FIB);
		}
		UnLock(lock);
	}

	ret = i ? TRUE : FALSE;

	//if(i==0)
	{
		char *tofile=LOCSTR( MSG_PRINTER_OUTFILE );

		if ((data->ProfileNames[0] = AllocVec(strlen(tofile)+1, MEMF_ANY)) != NULL)
		{
			strcpy(data->ProfileNames[0], tofile);
			i++;
		}
	}

	data->ProfileNames[i] = NULL;
	
	return ret;
}


DEFNEW
{
	Object *grpPage, *gauge, *sldFirst, *sldLast, *cycOddEven, *btnPrint, *btnCancel, 
	       *btnCancel2, *chkPS, *btnPrefs, *chkAutorotatescale;
	Object *right_group, *strNumbers, *grpRight, *grpLeft, *chkReverse, 
	       *strCopies, *chkCollate, *grpCollate, *btnReset, *cycPSVersion, *grpPSOutput;
	Object *second_page, *cycPageScaling, *grpPageHandling, *grpDriver, *grpPagesPerSheet1, 
	       *grpPagesPerSheet2, *grpPagesOrder1, *grpPagesOrder2, *cycPagesPerSheet, *cycPagesOrder;
	Object *free_grp;
	Object *cycProfiles, *strOutput;
	Object *btnCurrentPage;
	
	Object *grpPSOutputPath;

	//BOOL profiles_exist = FALSE;
	
	oddeven[0]       = LOCSTR(MSG_PRINTER_ODDEVEN);
	oddeven[1]       = LOCSTR(MSG_PRINTER_ODD); 
	oddeven[2]       = LOCSTR(MSG_PRINTER_EVEN);
	scaling_modes[0] = LOCSTR(MSG_PRINTER_NONE);
	scaling_modes[1] = LOCSTR(MSG_PRINTER_BEST);
	scaling_modes[2] = LOCSTR(MSG_PRINTER_MULTIPLE);
	scaling_modes[3] = LOCSTR(MSG_PRINTER_BOOKLET);
	pages_per_sheet[4] = LOCSTR(MSG_PRINTER_CUSTOM);
	pages_order[0]   = LOCSTR(MSG_PRINTER_HORIZONTAL_VERTICAL);
	pages_order[1]   = LOCSTR(MSG_PRINTER_HORIZONTAL_VERTICALREVERSED);
	pages_order[2]   = LOCSTR(MSG_PRINTER_HORIZONTALREVERSED_VERTICAL) ;
	pages_order[3]   = LOCSTR(MSG_PRINTER_HORIZONTALREVERSED_VERTICAL_REVERSED);
	pages_order[4]   = LOCSTR(MSG_PRINTER_VERTICAL_HORIZONTAL);
	pages_order[5]   = LOCSTR(MSG_PRINTER_VERTICALREVERSED_HORIZONTAL);
	pages_order[6]   = LOCSTR(MSG_PRINTER_VERTICAL_HORIZONTALREVERSED) ;
	pages_order[7]   = LOCSTR(MSG_PRINTER_VERTICAL_REVERSED_HORIZONTALREVERSED);

	second_page  = VGroup,
		Child, HVSpace,
		Child, HGroup,
			Child, HSpace(0),
			Child, 	RawimageObject, MUIA_Rawimage_Data, Printer, End,

				//MUI_NewObject("Dtpic.mui",MUIA_Dtpic_Name, "progdir:printer.png",	End,
			Child,  HSpace(0),
		End,	
		Child, HVSpace,
		Child, HGroup,
			Child, HSpace(0),
			Child, gauge = GaugeObject,  
				MUIA_Gauge_Horiz, TRUE, 
				MUIA_Frame, MUIV_Frame_Gauge,
				MUIA_Gauge_InfoText, " ",
			End,
			Child, HSpace(0),
		End,
		Child, HGroup,
			Child, HSpace(0),
			Child, btnCancel = SimpleButton( LOCSTR( MSG_PRINTER_ABORT )),
			Child, HSpace(0),
		End,
		Child, HVSpace,
	End;
	
	right_group = MUI_NewObject(MUIC_Group,
		MUIA_Group_Child, MUI_NewObject(MUIC_Group,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_Background, MUII_GroupBack,
			MUIA_FrameTitle, LOCSTR( MSG_PRINTER_RANGE ),
			MUIA_Group_Child, MUI_NewObject(MUIC_Group,
				MUIA_Group_Columns, 2,
				MUIA_Group_Child, MUI_NewObject(MUIC_Text,
					MUIA_Text_Contents, LOCSTR( MSG_PRINTER_PAGES ),
					MUIA_Text_SetMax, FALSE,
					MUIA_Text_PreParse, (LONG)"\33r",
					MUIA_Frame, MUIV_Frame_Button,
					MUIA_FramePhantomHoriz, TRUE,
					MUIA_HorizWeight, 1,
				TAG_END),
				MUIA_Group_Child, cycOddEven = MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, oddeven,
				TAG_END),
				MUIA_Group_Child, (grpLeft = MUI_NewObject(MUIC_Group,
					MUIA_HorizWeight, 1,
					MUIA_Group_Child, MUI_NewObject(MUIC_Group,
						MUIA_Group_Child, MUI_NewObject(MUIC_Group,	
							MUIA_Frame, MUIV_Frame_Slider,
							MUIA_FramePhantomHoriz, TRUE,
							MUIA_Group_Child, MUI_NewObject(MUIC_Text,
								MUIA_Text_Contents,  LOCSTR( MSG_PRINTER_FIRSTPAGE  ),
								MUIA_Text_PreParse, "\33r",
								MUIA_Frame, MUIV_Frame_SliderKnob,
								MUIA_FramePhantomHoriz, TRUE,
							TAG_END),
						TAG_END),
						MUIA_Group_Child, MUI_NewObject(MUIC_Group,
							MUIA_Frame, MUIV_Frame_Slider,
							MUIA_FramePhantomHoriz, TRUE,
							MUIA_Group_Child, MUI_NewObject(MUIC_Text,
								MUIA_Text_Contents,  LOCSTR( MSG_PRINTER_LASTPAGE  ),
								MUIA_Text_PreParse, "\33r",
								MUIA_Frame, MUIV_Frame_SliderKnob,
								MUIA_FramePhantomHoriz, TRUE,
							TAG_END),
						TAG_END),
					TAG_END),
					MUIA_Group_Child, MUI_NewObject(MUIC_Group,
						MUIA_Group_Child, MUI_NewObject(MUIC_Text,
							MUIA_Text_Contents, LOCSTR(MSG_PRINTER_MANUAL),
							MUIA_Text_PreParse, "\33r",
							MUIA_Frame, MUIV_Frame_String,
							MUIA_FramePhantomHoriz, TRUE,
						TAG_END),
						MUIA_Group_Child, MUI_NewObject(MUIC_Rectangle,
						TAG_END),
					TAG_END),
				TAG_END)),
				MUIA_Group_Child, (grpRight  = MUI_NewObject(MUIC_Group,
					MUIA_Group_Child, MUI_NewObject(MUIC_Group,
						MUIA_Group_Child, sldFirst = MUI_NewObject(MUIC_Slider,
							MUIA_Slider_Min, 1,
							MUIA_Slider_Max, 1, // will be corrected when opened
							MUIA_Frame, MUIV_Frame_Slider,
						TAG_END),
						MUIA_Group_Child, sldLast = MUI_NewObject(MUIC_Slider,
							MUIA_Slider_Min, 1,
							MUIA_Slider_Max, 1, // will be corrected when opened      
							MUIA_Frame, MUIV_Frame_Slider,
						TAG_END),
					TAG_END),
					MUIA_Group_Child, VGroup, 
						Child, MUI_NewObject(MUIC_Group,
							MUIA_Group_Child, strNumbers =  VPDFNumberGenerator, End,	
						TAG_END),
						Child, HGroup,
						Child, 	btnReset = TextObject,
											ButtonFrame,
												//MUIA_ShowMe, FALSE,
												MUIA_Font , MUIV_Font_Button,
												MUIA_Text_Contents , LOCSTR(MSG_PRINTER_RESET),
												MUIA_Text_PreParse , "\33c",
												MUIA_InputMode     , MUIV_InputMode_RelVerify,
												MUIA_Background    , MUII_ButtonBack,
												MUIA_Text_SetMax, TRUE,
											End,
												Child, 	btnCurrentPage = TextObject,
											ButtonFrame,
												//MUIA_ShowMe, FALSE,
												MUIA_Font , MUIV_Font_Button,
												MUIA_Text_Contents , LOCSTR(MSG_PRINTER_CURRENT_PAGE),
												MUIA_Text_PreParse , "\33c",
												MUIA_InputMode     , MUIV_InputMode_RelVerify,
												MUIA_Background    , MUII_ButtonBack,
												MUIA_Text_SetMax, TRUE,
											End,
											Child, HVSpace,
					   End,
					   
					End,
				TAG_END)),
			
			TAG_END),
				Child, HGroup,
					Child, chkReverse = CheckMark(FALSE),
					Child, TextObject, MUIA_Text_Contents, LOCSTR(MSG_PRINTER_REVERSE), End,
				End, 
		TAG_END),
	TAG_END);
	
	grpPageHandling =  VGroup,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_Background, MUII_GroupBack,
			MUIA_FrameTitle, (LONG)LOCSTR(MSG_PRINTER_ORIENTATION),
			Child, ColGroup(2),
				Child, Label(LOCSTR(MSG_PRINTER_COPIES)),
				Child, HGroup, 
					Child, strCopies= ArrowStringObject,
						MA_ArrowString_Min, 1,
					End,
					Child, HVSpace,
					Child, grpCollate = HGroup, 
						MUIA_ShowMe, FALSE,
						Child, chkCollate = CheckMark(FALSE),
						Child, 	MUI_NewObject(MUIC_Text,
							MUIA_Text_Contents, LOCSTR(MSG_PRINTER_COLLATE),
							MUIA_Text_SetMax, TRUE,
						TAG_END),
					End,
				End,
				Child, Label(LOCSTR(MSG_PRINTER_SCALING)),
				Child, cycPageScaling = CycleObject,
						MUIA_Cycle_Entries, scaling_modes,
				End,
				Child,  grpPagesPerSheet1 = PageGroup,
				MUIA_HorizWeight, 1,
						Child, Label(" "),
						Child, Label(LOCSTR(MSG_PRINTER_PAGESPERSHEET)),
				End,
				Child, grpPagesPerSheet2 = PageGroup,
				MUIA_HorizWeight, 1,
					Child, Label("          "),
					Child, HGroup, 
						Child,  cycPagesPerSheet = CycleObject,
								MUIA_Cycle_Entries, pages_per_sheet,
						End,
						Child,  free_grp = HGroup, 
								MUIA_ShowMe, FALSE,
								Child, StringObject, 
									MUIA_String_Accept, "0123456789",
									MUIA_Frame, MUIV_Frame_String,
									MUIA_String_MaxLen, 3,
									MUIA_FixWidthTxt, "12",
								End,
								Child, MUI_NewObject(MUIC_Text,
									MUIA_Text_Contents, "/",
									MUIA_Text_SetMax, TRUE,
								TAG_END),
								Child, StringObject, 
									MUIA_String_Accept, "0123456789",
									MUIA_Frame, MUIV_Frame_String,
									MUIA_String_MaxLen, 3,
									MUIA_FixWidthTxt, "12",
								End,
						
						End,
					End,
				End,
				Child, grpPagesOrder1 = PageGroup,
				MUIA_HorizWeight, 1,
					Child, Label(" "),
					Child, Label(LOCSTR(MSG_PRINTER_ORDER)),
				End,
				Child, grpPagesOrder2 = PageGroup,
				MUIA_HorizWeight, 1,
					Child, Label("             "),
					Child, cycPagesOrder = CycleObject,
						MUIA_Cycle_Entries, pages_order,
					End,
				End,
			End,
			Child, HVSpace,	
			Child, HGroup, 
				Child, chkAutorotatescale = CheckMark(TRUE),
				Child, TextObject, 
					MUIA_Text_Contents, LOCSTR(MSG_PRINTER_AUTOROTATE),
				End,
			End,	
		
	End;

	grpDriver = VGroup,
		MUIA_Frame, MUIV_Frame_Group,
		MUIA_Background, MUII_GroupBack,
		MUIA_FrameTitle, (LONG)LOCSTR(MSG_PRINTER_DRIVER),
		Child, btnPrefs = SimpleButton(LOCSTR(MSG_PRINTER_PREFERENCES)),


		Child, HGroup, 

				Child, VGroup,

		 	Child, HGroup,
		  	    MUIA_Weight, 1,
				MUIA_Frame, MUIV_Frame_Button,
				MUIA_FramePhantomHoriz, TRUE,
				Child, chkPS = CheckMark(TRUE),


				Child, TextObject, 
					MUIA_Text_Contents, "PostScript",
                 
				End,
			Child, grpPSOutput = HGroup, 
                MUIA_Weight, 90,
                Child, cycPSVersion = MUI_NewObject(MUIC_Cycle,
					MUIA_Cycle_Entries, ps_levels,
				TAG_END),



				Child, HVSpace,
				Child, Label(LOCSTR(MSG_PRINTER_PROFILE)), 
				Child, cycProfiles = CycleObject,							
						MUIA_CycleChain, 1,
						//MUIA_Cycle_Entries, data->ProfileNames,
						//MUIA_Disabled, !profiles_exist,
				End,



				End,
			End,


				End,


		End,

				Child, grpPSOutputPath = HGroup,

				Child, Label(LOCSTR(MSG_PRINTER_OUTPUTPATH)), 

				Child, strOutput = StringObject,
					MUIA_Frame, MUIV_Frame_String,
					MUIA_ShowMe, TRUE,
				End,

			End,

	End;
	
	
	obj = DoSuperNew(cl, obj,
						MUIA_Window_Title, LOCSTR(MSG_PRINTER_WINTITLE),
					//	MUIA_Window_Width, MUIV_Window_Width_Screen(50),
					//	MUIA_Window_Height, MUIV_Window_Height_Screen(50),
						MUIA_Window_ID, MAKE_ID('P','R','T','N'),
						MUIA_Window_AppWindow, FALSE,
						MUIA_Window_RootObject, grpPage = PageGroup,
					    Child, VGroup,
							Child, HGroup,
								Child, VGroup,
									Child, grpDriver,
									Child, right_group,
								End,
								Child, grpPageHandling,
							End,
						
							Child, HGroup,
								Child, btnPrint = SimpleButton(LOCSTR(MSG_PRINTER_PRINT)),
								Child, HSpace(0),
								Child, btnCancel2 = SimpleButton(LOCSTR(MSG_PRINTER_CANCEL)),
							End,
						End,	
						Child, second_page,
					End,
					TAG_MORE, INITTAGS);
	if (obj != NULL)
	{
   		char vbuf[ 128 ];
		GETDATA;
		memset(data, 0, sizeof(struct Data));	

		data->sldFirst   = sldFirst;
		data->sldLast    = sldLast;
		data->btnPrefs   = btnPrefs;
		data->btnPrint   = btnPrint;
		data->btnCancel  = btnCancel;
		data->cycOddEven = cycOddEven;
		data->chkPS      = chkPS;
		data->chkAutorotatescale = chkAutorotatescale;
		data->strNumbers = strNumbers;
		data->grpRight   = grpRight;
		data->chkReverse = chkReverse;
		data->chkCollate = chkCollate;
		data->strCopies  = strCopies;
		data->grpCollate = grpCollate;
		data->btnReset   = btnReset;
		data->grpPage    = grpPage;
		data->cycPSVersion = cycPSVersion;
		data->gauge      = gauge;
		data->cycPageScaling   = cycPageScaling;
		data->cycPagesPerSheet = cycPagesPerSheet;
		data->cycPagesOrder    = cycPagesOrder;
		data->cycProfiles      = cycProfiles;
		data->strOutput        = strOutput;
		data->btnCurrentPage   = btnCurrentPage;
		
		data->profiles_exist = refresh_profiles(data);
		D(kprintf("profiles refreshed\n"));
		set(data->cycProfiles,MUIA_Cycle_Entries, data->ProfileNames);

		data->max = 1;
		
		data->proc = MUI_NewObject(MUIC_Process,
					MUIA_Process_SourceClass , cl,
					MUIA_Process_SourceObject, obj,
					MUIA_Process_Name        , "[VPDF]: Printing process",
					MUIA_Process_Priority    , -1,
					MUIA_Process_AutoLaunch  , FALSE,
					MUIA_Process_StackSize   , 1000000,
					TAG_DONE);

		DoMethod(data->proc, MUIM_Process_Launch);
		
		DoMethod(btnPrint,  MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDFPrinter_Start);
		DoMethod(btnCancel, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDFPrinter_Stop);
		DoMethod(btnPrefs,  MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDFPrinter_PrinterPrefs);

		if(GetVar("NOTURBOPRINT", vbuf, sizeof(vbuf), GVF_GLOBAL_ONLY) >= 0)
		{
			set(chkPS, MUIA_ShowMe, FALSE);
		}

		DoMethod(chkPS, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, grpPSOutput, 3, MUIM_Set, MUIA_ShowMe, MUIV_TriggerValue);

		DoMethod(chkPS, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, grpPSOutputPath, 3, MUIM_Set, MUIA_ShowMe, MUIV_TriggerValue);

		DoMethod(strCopies, MUIM_Notify, MUIA_String_Integer, MUIV_EveryTime, obj, 2, MUIM_VPDFPrinter_HideCollate, MUIV_TriggerValue);
		
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 2, grpPagesPerSheet1, 3, MUIM_Set, MUIA_Group_ActivePage, 1); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 2, grpPagesPerSheet2, 3, MUIM_Set, MUIA_Group_ActivePage, 1); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 0, grpPagesPerSheet1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 0, grpPagesPerSheet2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 1, grpPagesPerSheet1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 1, grpPagesPerSheet2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 3, grpPagesPerSheet1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 3, grpPagesPerSheet2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 2, grpPagesOrder1, 3, MUIM_Set, MUIA_Group_ActivePage, 1); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 2, grpPagesOrder2, 3, MUIM_Set, MUIA_Group_ActivePage, 1); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 0, grpPagesOrder2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 0, grpPagesOrder1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 1, grpPagesOrder2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 1, grpPagesOrder1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 	
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 3, grpPagesOrder2, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 
		DoMethod(cycPageScaling, MUIM_Notify, MUIA_Cycle_Active, 3, grpPagesOrder1, 3, MUIM_Set, MUIA_Group_ActivePage, 0); 	

		DoMethod(cycPagesPerSheet, MUIM_Notify, MUIA_Cycle_Active, 4, free_grp, 3, MUIM_Set, MUIA_ShowMe, TRUE); 
		DoMethod(cycPagesPerSheet, MUIM_Notify, MUIA_Cycle_Active, 1, free_grp, 3, MUIM_Set, MUIA_ShowMe, FALSE);
		DoMethod(cycPagesPerSheet, MUIM_Notify, MUIA_Cycle_Active, 0, free_grp, 3, MUIM_Set, MUIA_ShowMe, FALSE);
		DoMethod(cycPagesPerSheet, MUIM_Notify, MUIA_Cycle_Active, 2, free_grp, 3, MUIM_Set, MUIA_ShowMe, FALSE);
		DoMethod(cycPagesPerSheet, MUIM_Notify, MUIA_Cycle_Active, 3, free_grp, 3, MUIM_Set, MUIA_ShowMe, FALSE);
		
		DoMethod(sldFirst, MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime, data->strNumbers, 3, MUIM_Set, MUIA_VPDFNumberGeneratorPageStart, MUIV_TriggerValue);
		DoMethod(sldLast,  MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime, data->strNumbers, 3, MUIM_Set, MUIA_VPDFNumberGeneratorPageEnd, MUIV_TriggerValue);  
		DoMethod(btnReset, MUIM_Notify, MUIA_Pressed, FALSE, data->sldFirst, 3, MUIM_Set, MUIA_Slider_Level, 1);  
	
		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime,  obj, 1, MUIM_VPDFPrinter_Close);
		DoMethod(btnCancel2, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MUIM_VPDFPrinter_Close);	
		
		DoMethod(data->chkPS, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,  obj, 3,  MUIM_Set,  MUIA_VPDFPrinter_PrintingMode, MUIV_TriggerValue);
		DoMethod(data->cycProfiles, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,  obj, 3,  MUIM_Set,  MUIA_VPDFPrinter_PSProfile, MUIV_TriggerValue);
		DoMethod(data->cycPSVersion, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,  obj, 3,  MUIM_Set,  MUIA_VPDFPrinter_PSMode, MUIV_TriggerValue);
	}       

	return (ULONG)obj;
}



DEFDISP
{
	int i;
	GETDATA;
	
	data->quit = TRUE;
	
	for(i=0; i<MAX_PROFILE_NAMES && data->ProfileNames[i]; i++)
	{
		FreeVec(data->ProfileNames[i]);
		data->ProfileNames[i] = 0;
	}
	
	MUI_DisposeObject(data->proc);
	
	return DOSUPER;
}


DEFMMETHOD(VPDFPrinter_PrintDocument)
{
	GETDATA;
	
	int pagesNum = pdfGetPagesNum(msg->doc);
	
	set(data->sldFirst, MUIA_Slider_Max,   pagesNum);
	set(data->sldLast,  MUIA_Slider_Max,   pagesNum); 
	set(data->sldLast,  MUIA_Slider_Level, pagesNum);
	
	set(data->strNumbers, MUIA_VPDFNumberGeneratorPageMax, pagesNum);
	set(data->strNumbers, MUIA_VPDFNumberGeneratorPageEnd, pagesNum);
	data->max = pagesNum;
	data->active_page = msg->active_page;
	data->doc = msg->doc;
	data->close = FALSE;
	data->quit = FALSE;
	DoMethod(data->btnReset, MUIM_Notify, MUIA_Pressed, FALSE, data->sldLast, 3, MUIM_Set, MUIA_Slider_Level, data->max);  
	DoMethod(data->btnCurrentPage, MUIM_Notify, MUIA_Pressed, FALSE, data->sldFirst, 3, MUIM_Set, MUIA_Slider_Level, data->active_page); 
	DoMethod(data->btnCurrentPage, MUIM_Notify, MUIA_Pressed, FALSE, data->sldLast, 3, MUIM_Set, MUIA_Slider_Level, data->active_page); 

	set(obj, MUIA_Window_Open, TRUE);
	
	return TRUE;
}



void getPSPageSizeVersion(int *w, int *h, char *ver, char *profile)
{
	struct Library *PPDBase;
	PPD_ERROR ret;
	PPD *myppd = NULL;
	PAGE_SIZE_NODE *ps_node = NULL;
	char  ppd_path[1024] = 	PROFILES_DIR;
	
	// predefined A4
	*w=595;
	*h=842;
	
	if (profile && (PPDBase = OpenLibrary("ppd.library", 50)))
	{       
		AddPart(ppd_path, profile, 1024);
			
		// if we cant open given profile
		if(!(myppd =OpenPPDFromIFF(ppd_path, &ret)))
		{
			// just check default one
			if (-1 != GetVar("DefaultPrinter", ppd_path, 1024, 0))
			{
				myppd =OpenPPDFromIFF(ppd_path, &ret);
			}
		}
		// if profile opened, get the pagesize
		if(myppd)
		{			
			if ((ps_node = FindPPD_PageSize(myppd, NULL)) != NULL)
			{
				*w = ps_node->Width;
				*h = ps_node->Height;
			}
			*ver = myppd->Parameters.PS_Level;
			
			ClosePPD(myppd);
		}
		CloseLibrary(PPDBase);
	}	
}		 


DEFMMETHOD(VPDFPrinter_Start)
{
	GETDATA;
	char params[1024], args[512], arg[48];
	int profile_nbr = 0;
	data->pj.first = 1;
	data->pj.last = pdfGetPagesNum(data->doc); 

	data->pj.mode = !xget(data->chkPS, MUIA_Selected);  // PS or Turboprint
	
	if (data->pj.mode==1)
        data->pj.mode = VPDF_PRINT_TURBOPRINT;
    else 
	{
        data->pj.mode = xget(data->cycPSVersion, MUIA_Cycle_Active);
	}
	data->pj.autorotatescale = xget(data->chkAutorotatescale, MUIA_Radio_Active);
	data->pj.step = 1;
	data->pj.reverse = xget(data->chkReverse, MUIA_Selected);
	
	data->pj.step = xget(data->cycOddEven, MUIA_Cycle_Active);
	
	data->pj.page_scaling    = xget(data->cycPageScaling,   MUIA_Cycle_Active);
	
	data->pj.total_pages = DoMethod(data->strNumbers,  MUIM_VPDFNumberGeneratorFirst, data->pj.reverse, data->pj.page_scaling==3); 
	D(kprintf("total pages: %d", data->pj.total_pages));

	data->pj.collate = xget(data->chkCollate, MUIA_Selected);
	data->pj.copies = xget(data->strCopies, MUIA_String_Integer);
	
	if (data->pj.copies==0)
	{
		data->pj.copies = 1;
		set(data->strCopies, MUIA_String_Integer, 1);
	}
	
	if (data->pj.collate)
		data->pj.total_pages *= data->pj.copies;
		
	
	data->pj.pages_per_sheet = xget(data->cycPagesPerSheet, MUIA_Cycle_Active);
	data->pj.pages_per_sheet = pages_per_sheet_table[data->pj.pages_per_sheet];
    data->pj.pages_order     = xget(data->cycPagesOrder,    MUIA_Cycle_Active);
		
	set (data->gauge, MUIA_Gauge_Max, data->pj.total_pages);	 
	
	if ( data->pj.mode != VPDF_PRINT_TURBOPRINT)
	{
		get(data->cycProfiles, MUIA_Cycle_Active, &profile_nbr);

		// PPD profile selected
		if(profile_nbr > 0)
		{
			strcpy(data->pj.output, "PRINTER:");
		
			getPSPageSizeVersion(&data->pj.w, &data->pj.h, &data->pj.mode, data->ProfileNames[profile_nbr]);
		
			if (data->pj.mode<=2)
				data->pj.mode = VPDF_PRINT_POSTSCRIPT_2;
			else
				data->pj.mode = VPDF_PRINT_POSTSCRIPT_3;
			
			if((data->pj.pages_order < 0) || (data->pj.pages_order >= sizeof(layouts)))
			{
				data->pj.pages_order = 0;
			}

			//if (!IsFileSystem(data->pj.output)) 
			{
				args[0]=0;
				switch (data->pj.page_scaling)
				{
					case 1:
						strcat(args,  " fit-to-page ");
					break;
					case 2:
						sprintf(arg, "number-up=%d number-up-layout=%s", data->pj.pages_per_sheet, layouts[(int)data->pj.pages_order]);
						strcat(args,  arg);
						break;
					case 3:
						sprintf(arg, "number-up=%d number-up-layout=%s", 2, "lrtb");
						strcat(args,  arg);
						break;
					break;
					default:
					break;
				}

				sprintf(params, "PROFILE=\"%s\" COPIES=%d ARGS=\"%s\" FORMAT=PS",  data->ProfileNames[profile_nbr], (data->pj.collate &&  (data->pj.page_scaling!=2) )?1:data->pj.copies ,  args);
				strcat(data->pj.output, params);
			}
		}
		else // postscript output to file
		{	
			APTR str;

			get(data->strOutput, MUIA_String_Contents, &str);

			if(strlen(str))
			{
				strcpy(data->pj.output, str);
			}
			else
			{
				strcpy(data->pj.output, "ram:out");
			}

			//*w=595;
			//*h=842;

			/* use the width&height provided by the pdf document media size */
			data->pj.w = -1;
			data->pj.h = -1;

		}
	}	
	// assuming printer thread is in ready status siting on Wait()
	DoMethod(data->proc, MUIM_Process_Signal, SIGBREAKF_CTRL_D);
	DoMethod(obj, MUIM_VPDFPrinter_StatusUpdate, 0, 0);
	set(data->grpPage,  MUIA_Group_ActivePage, 1);
	return TRUE;
}

DEFMMETHOD(VPDFPrinter_Stop)
{
	GETDATA;
	if (data->printing)
		data->quit = TRUE; // this will signal printing thread which will 'recall' this method later
	else
	{
		data->quit = FALSE;
		
		DoMethod(obj, MUIM_VPDFPrinter_Done);	
	}
	return TRUE;
}


DEFMMETHOD(VPDFPrinter_Done)
{
	GETDATA;
	
	data->printing = FALSE;
	data->quit = FALSE;

	if (data->close)
		DoMethod(obj, MUIM_VPDFPrinter_Close);
		
	set(data->grpPage,  MUIA_Group_ActivePage, 0);
	
	return TRUE;
}

DEFMMETHOD(VPDFPrinter_Close)
{
	int i=0;
	GETDATA;
	
	if (!data->printing)
	{
		data->close = FALSE;
		set(obj, MUIA_Window_Open, FALSE);
	}
	else
	{	
		data->quit = TRUE;
		data->close = TRUE;
	}
	
	get(data->cycProfiles, MUIA_Cycle_Active, &i);		

	if(i == 0)
	{
		// make sure to update output path for rawfile output in settings
		set(obj, MUIA_VPDFPrinter_PSName, xget(data->strOutput,MUIA_String_Contents));
	}

	return TRUE;
}




DEFMMETHOD(Process_Process)
{
	GETDATA;

	while(!*msg->kill)
	{
		Wait(SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_C);

		if(data->quit == FALSE)
		{
			int i, j, k, h, pages_no;
			struct printerjob *pj = &data->pj;
			void *pctx;

			D(kprintf("started printing:%d to %d\n", pj->first, pj->last));

			data->printing = TRUE;
			pctx = pdfPrintInit(data->doc, pj->output, pj->first, pj->last, pj);
			if(pctx != NULL)
			{
				j = 0;
				h = 0;
				do
				{	
                    pages_no = 0; // for multipage mode
					while((MUIV_VPDFNumberGeneratorEndPos != (i = DoMethod(data->strNumbers, MUIM_VPDFNumberGeneratorNext))) || 
                          (((pj->page_scaling==2 || pj->page_scaling==3)&& (pages_no))))
					{
                        if((pj->step == 1) && (i % 2 == 0)) continue;
                        if((pj->step == 2) && (i % 2 == 1)) continue;
                     
                        DoMethod(_app(data->sldFirst), MUIM_Application_PushMethod, obj, 3, MUIM_VPDFPrinter_StatusUpdate, i, h);
                            
                        k = 0;
                        do
                        {
                            //int aaa;
							pj->pages[0] = i;
                            pdfPrintPage(pctx, pj->pages, pj->autorotatescale, (pj->page_scaling==2)?pj->pages_per_sheet:1);

                            if(data->quit)
                                break;
                            k++;
                        } while ((k < pj->copies) && !pj->collate  && ( data->pj.mode != VPDF_PRINT_TURBOPRINT));	
                        
                        if(data->quit || (i==MUIV_VPDFNumberGeneratorEndPos))
                            break;
                        h++;
                        pages_no = 0;
					}				
					j++;
					if(data->quit)
						break;
						
					DoMethod(data->strNumbers,  MUIM_VPDFNumberGeneratorFirst, pj->reverse, pj->page_scaling==3); 
				}
				while((j<pj->copies) && pj->collate);
			}
			else
			{
                DoMethod(_app(data->sldFirst), MUIM_Application_PushMethod, obj, 1, MUIM_VPDFPrinter_Error);
			}


			pdfPrintEnd(pctx);
			D(kprintf("printing done...\n"));
			DoMethod(_app(data->sldFirst), MUIM_Application_PushMethod, obj, 1, MUIM_VPDFPrinter_Done);


		}

		data->printing = FALSE;
	}

	return 0;
}

DEFMMETHOD(VPDFPrinter_StatusUpdate)
{
	GETDATA;
	if (msg->page_number == 0)
	{
			snprintf(data->status, sizeof(data->status), "Printing...");
	}
	else
	{
		snprintf(data->status, sizeof(data->status), "Printing page %ld...", msg->page_number);
	}
	D(kprintf("%s\n", data->status));
	set(data->gauge, MUIA_Gauge_Current, msg->actual_page);
	set(data->gauge, MUIA_Gauge_InfoText, data->status);
	
	return TRUE;
}


DEFMMETHOD(VPDFPrinter_PrinterPrefs)
{
	GETDATA;	
	if (xget(data->chkPS, MUIA_Selected))
	{
		char mprefs_cmdline[128];
		int profile_nbr = 0;
		get(data->cycProfiles, MUIA_Cycle_Active, &profile_nbr);

		if(profile_nbr > 0)
		{
			int profile_idx = profile_nbr;	// idx starts with 1
			sprintf(mprefs_cmdline, "MOSSYS:Prefs/Preferences MOSSYS:Prefs/MPrefs/Printers.mprefs TAB %d", profile_idx<<4);	// we use bits 31-4 for the profile selection

			//kprintf("%s: SystemTagList(\"%s\",TAG_DONE);\n",__func__,mprefs_cmdline);
			SystemTagList(mprefs_cmdline, TAG_DONE);
			data->profiles_exist = refresh_profiles(data);
			set(data->cycProfiles,MUIA_Cycle_Entries, data->ProfileNames);
		}
	}
	else
		SystemTagList("MOSSYS:Prefs/Preferences MOSSYS:Prefs/MPrefs/Printer.mprefs", TAG_DONE);

	return TRUE;
}

DEFMMETHOD(VPDFPrinter_HideCollate)
{
	GETDATA;
	
	if (msg->copies == 1)
	{
		set(data->grpCollate, MUIA_ShowMe, FALSE);
	}
	else
	{
		set(data->grpCollate, MUIA_ShowMe, TRUE);
	}
	
	return 0;
}


DEFSET
{
	GETDATA;

	FORTAG(INITTAGS)
	{	
        case MUIA_VPDFPrinter_PSName:
		{
			int i;
			int foundProfile=0;

			D(kprintf("MUIA_VPDFPrinter_PSName: \"%s\"\n",  tag->ti_Data));
			for (i=0; i<MAX_PROFILE_NAMES && data->ProfileNames[i]; i++)
			{
				if (!strcmp(data->ProfileNames[i], (char *)tag->ti_Data))
				{
					nnset(data->cycProfiles, MUIA_Cycle_Active, i);		
					D(kprintf("MUIA_VPDFPrinter_PSName: set active %d\n",  i));
					foundProfile = 1;
					break;
				}
				D(kprintf("MUIA_VPDFPrinter_PSName:profile name \"%s\"\n", data->ProfileNames[i]));
			}

			if(!foundProfile)
			{
				nnset(data->cycProfiles, MUIA_Cycle_Active, 0);		
				set(data->strOutput,MUIA_String_Contents, tag->ti_Data);
			}
			else
			{
				char outputpath[256];
				char outputenv[256];

				sprintf(outputenv,"env:OutputPath.%s",data->ProfileNames[i]);

				if(GetVar(outputenv,outputpath,sizeof(outputpath)-1,0) <= 0)
				{
					strcpy(outputpath,"ram:out");	// this is the default path if no env:OutputPath.<Profile> is set
				}

				set(data->strOutput,MUIA_String_Contents, outputpath);
			}
			set(data->btnPrefs, MUIA_Disabled, foundProfile ? FALSE : TRUE);
			set(data->cycPSVersion, MUIA_Disabled, foundProfile ? TRUE : FALSE);
			set(data->strOutput, MUIA_Disabled, foundProfile ? TRUE : FALSE);
	        }
	        break;
		case MUIA_VPDFPrinter_PSProfile:
		{
			int i=0;
			
			get(data->cycProfiles, MUIA_Cycle_Active, &i);		

			if(i)
			{
				set(obj, MUIA_VPDFPrinter_PSName, data->ProfileNames[i]);				

			}
			else
			{
				set(obj, MUIA_VPDFPrinter_PSName, xget(data->strOutput, MUIA_String_Contents));
			}
			
			set(data->cycPSVersion, MUIA_Disabled, i ? TRUE : FALSE);
			set(data->strOutput, MUIA_Disabled, i ? TRUE : FALSE);

			set(data->cycPSVersion, MUIA_Disabled, i ? TRUE : FALSE);

	        }
	        break;
        case MUIA_VPDFPrinter_PrintingMode:
		{
			set(data->chkPS, MUIA_Selected, tag->ti_Data);
        }
        break;      
        case MUIA_VPDFPrinter_PSMode:
		{
			set(data->cycPSVersion, MUIA_Cycle_Active, tag->ti_Data);
        }
        break;
    }
	NEXTTAG

	return DOSUPER;
}

DEFMMETHOD(VPDFPrinter_Error)
{
	GETDATA;
	MUI_Request(_app(data->sldFirst), _window(obj), 0, "VPDF  Print Document Error", "OK", "Failed to initialize output device", NULL);
	return TRUE;
}

BEGINMTABLE
	DECNEW
	DECDISP
	DECSET
	DECMMETHOD(VPDFPrinter_PrintDocument)
	DECMMETHOD(VPDFPrinter_Start)
	DECMMETHOD(VPDFPrinter_Stop)
	DECMMETHOD(VPDFPrinter_Done)
	DECMMETHOD(VPDFPrinter_Close)
	DECMMETHOD(VPDFPrinter_StatusUpdate)
	DECMMETHOD(VPDFPrinter_PrinterPrefs)
	DECMMETHOD(VPDFPrinter_HideCollate)
	DECMMETHOD(VPDFPrinter_Error)
	DECMMETHOD(Process_Process)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, VPDFPrinterClass)   
