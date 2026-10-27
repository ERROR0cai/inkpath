#ifndef INKPATH
#define INKPATH

#ifdef _WIN32
#define WINEXPORT __declspec(dllexport)
#else
#define WINEXPORT
#endif //_WIN32

/*
 * This is a barebones interface for allowing Lua to get data directly™ from
 * OpenCV
 * */

// Lua C API
#include <lua.hpp>
// C++ input/output streams
#include <iostream>
#include <utility>
#include <opencv2/core/types.hpp>

#include "../cv/ipcv.h"

using namespace inkp;

typedef std::vector<std::vector<cv::Point>> StrokeList;

// Plugin class
class Inkpath {
  private:
    StrokeList strokes;
    std::string diagnosticReport;

  public:
    Inkpath() {}
    void set(StrokeList strokes) { this->strokes = std::move(strokes); }
    const StrokeList& get() const noexcept { return this->strokes; }
    void setDiagnosticReport(const std::string& report) { diagnosticReport = report; }
    const std::string& getDiagnosticReport() const { return diagnosticReport; }
};

// inkpath identifier for the Lua metatable
#define LUA_INKPATH "Inkpath"

static void registerInkpath(lua_State* L);
extern "C" WINEXPORT int luaopen_loadInkpath(lua_State* L);

static int inkpath_new(lua_State* L);

// For managing metadata about our strokes
static int inkpath_getStrokeCount(lua_State* L);
static int inkpath_getStrokeLength(lua_State* L);
static int inkpath_getStroke(lua_State* L);

// Free inkpath instance by Lua garbage collection
static int inkpath_delete(lua_State* L);

// XXX (wdn): Make CV object and make a new function for it
int cv_perform_processing(const char* image_path, Inkpath* data);

#endif // INKPATH
