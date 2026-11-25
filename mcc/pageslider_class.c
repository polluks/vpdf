
#include <proto/alib.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <private/vapor/vapor.h>
#include <proto/utility.h>

#include <stdio.h> // for sprintf()

#include "pageslider_class.h"
#include "util.h"

struct Data
{
	char buf[32];
};

DEFMMETHOD(Numeric_Stringify)
{
	GETDATA;

	sprintf(data->buf, (char*)xget(obj, MUIA_Numeric_Format), msg->value, xget(obj, MUIA_Slider_Max));
	return ((ULONG)data->buf);
}

BEGINMTABLE
	DECMMETHOD(Numeric_Stringify)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Slider, PageSliderClass)
