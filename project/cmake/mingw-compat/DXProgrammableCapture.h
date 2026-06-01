#pragma once
#include <unknwn.h>

#ifndef __IDXGraphicsAnalysis_INTERFACE_DEFINED__
#define __IDXGraphicsAnalysis_INTERFACE_DEFINED__
struct IDXGraphicsAnalysis : public IUnknown {
    virtual void STDMETHODCALLTYPE BeginCapture() = 0;
    virtual void STDMETHODCALLTYPE EndCapture() = 0;
};
#endif
