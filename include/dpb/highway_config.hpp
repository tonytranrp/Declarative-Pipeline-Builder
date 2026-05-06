#pragma once

#ifdef DPB_HAS_HIGHWAY
#include <hwy/highway.h>
#endif

namespace dpb::simd {

#ifdef DPB_HAS_HIGHWAY
namespace hn = hwy::HWY_NAMESPACE;
#endif

}
