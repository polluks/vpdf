#ifndef DOCUMENTVIEW_MCC_CLASS_H
#define DOCUMENTVIEW_MCC_CLASS_H

#include "system/chunky.h"
#include "classes.h"

#define MUIA_DocumentView_FileName         ( MUIA_DocumentView_TagBase +  1 )
#define MUIA_DocumentView_Page             ( MUIA_DocumentView_TagBase +  2 )
#define MUIA_DocumentView_Renderer         ( MUIA_DocumentView_TagBase +  3 )
#define MUIA_DocumentView_PDFDocument      ( MUIA_DocumentView_TagBase +  4 )
#define MUIA_DocumentView_Layout           ( MUIA_DocumentView_TagBase +  5 )
#define MUIA_DocumentView_RenderPriority   ( MUIA_DocumentView_TagBase +  6 )
#define MUIA_DocumentView_Outline          ( MUIA_DocumentView_TagBase +  7 )
#define MUIA_DocumentView_DragAction       ( MUIA_DocumentView_TagBase +  8 )
#define MUIA_DocumentView_FullScreen       ( MUIA_DocumentView_TagBase +  9 ) //[.S.] BOOL
#define MUIA_DocumentView_Toolbar          ( MUIA_DocumentView_TagBase + 10 )
#define MUIA_DocumentView_ColorReproduction ( MUIA_DocumentView_TagBase + 10 )
#define MUIA_DocumentView_PageSlider        ( MUIA_DocumentView_TagBase + 11 )
#define MUIA_DocumentView_OwnerPassword    ( MUIA_DocumentView_TagBase +  12 )
#define MUIA_DocumentView_UserPassword     ( MUIA_DocumentView_TagBase +  13 )
#define MUIA_DocumentView_ErrCodePtr       ( MUIA_DocumentView_TagBase +  14 )
#define MUIA_DocumentView_SyncTeX          ( MUIA_DocumentView_TagBase +  15 ) /* [R] BOOL */

#define MUIM_DocumentView_Render             ( MUIM_DocumentView_Dummy +  1 )
#define MUIM_DocumentView_EnqueueRender      ( MUIM_DocumentView_Dummy +  2 )
#define MUIM_DocumentView_SetupPageViewImage ( MUIM_DocumentView_Dummy +  3 )
#define MUIM_DocumentView_FindViewForPage    ( MUIM_DocumentView_Dummy +  5 )
#define MUIM_DocumentView_Layout             ( MUIM_DocumentView_Dummy +  4 )
#define MUIM_DocumentView_RotateRight        ( MUIM_DocumentView_Dummy +  6 )
#define MUIM_DocumentView_RotateLeft         ( MUIM_DocumentView_Dummy +  7 )
#define MUIM_DocumentView_UpdateAnnotations  ( MUIM_DocumentView_Dummy +  8 )
#define MUIM_DocumentView_ClearSelection     ( MUIM_DocumentView_Dummy +  9 )
#define MUIM_DocumentView_SelectionCopy      ( MUIM_DocumentView_Dummy + 10 )
#define MUIM_DocumentView_SyncTeXForward     ( MUIM_DocumentView_Dummy + 11 )
#define MUIM_DocumentView_SyncTeXInverse     ( MUIM_DocumentView_Dummy + 12 )

#define DocumentViewObject   NewObject(getDocumentViewClass(), NULL

struct MUIP_DocumentView_Render{ULONG MethodID;};
struct MUIP_DocumentView_EnqueueRender{ULONG MethodID; LONG page;};
struct MUIP_DocumentView_SetupPageViewImage{ULONG MethodID; APTR pageview; APTR image;};
struct MUIP_DocumentView_Layout{ULONG MethodID; LONG layout;};
struct MUIP_DocumentView_FindViewForPage{ULONG MethodID; LONG page;};
struct MUIP_DocumentView_RotateRight{ULONG MethodID;};
struct MUIP_DocumentView_RotateLeft{ULONG MethodID;};
struct MUIP_DocumentView_ClearSelection{ULONG MethodID;};
struct MUIP_DocumentView_UpdateAnnotations{ULONG MethodID; int page;};
struct MUIP_DocumentView_SelectionCopy{ULONG MethodID;};
struct MUIP_DocumentView_SyncTeXForward{ULONG MethodID; CONST_STRPTR file; LONG line; LONG column;};
struct MUIP_DocumentView_SyncTeXInverse{ULONG MethodID; LONG page; double x; double y; char *file; int fileSize; LONG *line; LONG *column;};

#define MUIV_DocumentView_Layout_Single 0
#define MUIV_DocumentView_Layout_ContinuousSingle 1
#define MUIV_DocumentView_Layout_Facing 2
#define MUIV_DocumentView_Layout_ContinuousFacing 3

#define MUIV_DocumentView_DragAction_Scroll 0
#define MUIV_DocumentView_DragAction_Mark 1

#define MUIV_DocumentView_ColorReproduction_Fast 0
#define MUIV_DocumentView_ColorReproduction_Accurate 1

DEFCLASS(DocumentView);

#endif
