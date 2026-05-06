#pragma once

#ifdef DPB_HAS_TRACY
#include <tracy/Tracy.hpp>
#define DPB_ZONE_SCOPED ZoneScoped
#define DPB_ZONE_TEXT(name, text) ZoneText(name, text)
#define DPB_FRAME_MARK FrameMark
#define DPB_ZONE_NAMED(varname, name) ZoneNamed(varname, name)
#else
#define DPB_ZONE_SCOPED ((void)0)
#define DPB_ZONE_TEXT(name, text) ((void)0)
#define DPB_FRAME_MARK ((void)0)
#define DPB_ZONE_NAMED(varname, name) ((void)0)
#endif
