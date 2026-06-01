#pragma once

#ifdef _WIN32

#ifdef TlsAlloc
#undef TlsAlloc
#endif

#ifdef TlsFree
#undef TlsFree
#endif

#ifdef TlsGetValue
#undef TlsGetValue
#endif

#ifdef TlsSetValue
#undef TlsSetValue
#endif

#ifdef GetSystemTime
#undef GetSystemTime
#endif

#endif
