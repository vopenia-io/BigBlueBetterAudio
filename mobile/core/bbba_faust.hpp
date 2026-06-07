// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Standalone (non-DPF) wrapper around the committed Faust-generated `mydsp`.
// Supplies the Faust base classes (dsp/UI/Meta) and a parameter-mapping UI so
// the DSP can be driven by Faust symbol (e.g. "vad_ext", "pre_gain") without
// the DISTRHO plugin framework.
#pragma once

#include <cstring>
#include <map>
#include <string>

#define FAUSTFLOAT float

// Drive the generated code into the global namespace with public fields.
#define FAUSTPP_BEGIN_NAMESPACE
#define FAUSTPP_END_NAMESPACE
#define FAUSTPP_PRIVATE public
#define FAUSTPP_PROTECTED public
// leave FAUSTPP_VIRTUAL at its default (virtual)

// --- Faust base classes (standard signatures the generated code calls) -------
struct Meta {
    virtual ~Meta() {}
    virtual void declare(const char* /*key*/, const char* /*value*/) {}
};

class UI {
public:
    virtual ~UI() {}
    virtual void openTabBox(const char*) {}
    virtual void openHorizontalBox(const char*) {}
    virtual void openVerticalBox(const char*) {}
    virtual void closeBox() {}
    virtual void addButton(const char*, FAUSTFLOAT*) {}
    virtual void addCheckButton(const char*, FAUSTFLOAT*) {}
    virtual void addVerticalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) {}
    virtual void addHorizontalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) {}
    virtual void addNumEntry(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) {}
    virtual void addHorizontalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) {}
    virtual void addVerticalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) {}
    virtual void declare(FAUSTFLOAT*, const char*, const char*) {}
};

class dsp {
public:
    virtual ~dsp() {}
};

// Pull in the generated mydsp class (depends only on the above).
#include "bbba_faust_generated.h"

// --- Parameter map: capture Faust symbol/label -> control zone ---------------
class BbbaParamUI : public UI {
public:
    std::map<std::string, FAUSTFLOAT*> byLabel;
    std::map<std::string, FAUSTFLOAT*> bySymbol;

    void openTabBox(const char*) override {}
    void openHorizontalBox(const char*) override {}
    void openVerticalBox(const char*) override {}
    void closeBox() override {}

    void addButton(const char* l, FAUSTFLOAT* z) override { byLabel[l] = z; }
    void addCheckButton(const char* l, FAUSTFLOAT* z) override { byLabel[l] = z; }
    void addVerticalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { byLabel[l] = z; }
    void addHorizontalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { byLabel[l] = z; }
    void addNumEntry(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { byLabel[l] = z; }
    void addHorizontalBargraph(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT) override { byLabel[l] = z; }
    void addVerticalBargraph(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT) override { byLabel[l] = z; }

    // Faust emits declare(zone, "symbol", "<name>") just before the widget.
    void declare(FAUSTFLOAT* z, const char* key, const char* val) override {
        if (std::strcmp(key, "symbol") == 0) bySymbol[val] = z;
    }
};
