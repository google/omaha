/* Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _NP_EXTENSIONS_H_
#define _NP_EXTENSIONS_H_

// Use the shorter include path here so that this file can be used in non-
// Chromium projects, such as the Native Client SDK.
#include "npapi.h"

/*
 * A fake "enum" value for getting browser-implemented Pepper extensions.
 * The variable returns a pointer to an NPNExtensions structure. */
#define NPNVPepperExtensions ((NPNVariable) 4000)

/*
 * A fake "enum" value for getting plugin-implemented Pepper extensions.
 * The variable returns a pointer to an NPPExtensions structure. */
#define NPPVPepperExtensions ((NPPVariable) 4001)

typedef void NPDeviceConfig;
typedef void NPDeviceContext;
typedef void NPUserData;

/* unique id for each device interface */
typedef int32 NPDeviceID;

typedef struct _NPPoint {
  uint16 x;
  uint16 y;
} NPPoint;

typedef enum {
  NPThemeItemScrollbarDownArrow       = 0,
  NPThemeItemScrollbarLeftArrow       = 1,
  NPThemeItemScrollbarRightArrow      = 2,
  NPThemeItemScrollbarUpArrow         = 3,
  NPThemeItemScrollbarHorizontalThumb = 4,
  NPThemeItemScrollbarVerticalThumb   = 5,
  NPThemeItemScrollbarHoriztonalTrack = 6,
  NPThemeItemScrollbarVerticalTrack   = 7
} NPThemeItem;

typedef enum {
  NPThemeStateDisabled = 0,
  // Mouse is over this item.
  NPThemeStateHot      = 1,
  // Mouse is over another part of this component.  This is only used on Windows
  // Vista and above.  The plugin should pass it in, and the host will convert
  // it to NPThemeStateNormal if on other platforms or on Windows XP.
  NPThemeStateHover    = 2,
  NPThemeStateNormal   = 3,
  NPThemeStatePressed  = 4
} NPThemeState;

typedef struct _NPThemeParams {
  NPThemeItem item;
  NPThemeState state;
  NPRect location;
  // Used for scroll bar tracks, needed for classic theme in Windows which draws
  // a checkered pattern.
  NPPoint align;
} NPThemeParams;

typedef struct _NPDeviceBuffer {
  void* ptr;
  size_t size;
} NPDeviceBuffer;

/* completion callback for flush device */
typedef void (*NPDeviceFlushContextCallbackPtr)(
    NPP instance,
    NPDeviceContext* context,
    NPError err,
    NPUserData* userData);

/* query single capabilities of device */
typedef NPError (
    *NPDeviceQueryCapabilityPtr)(NPP instance,
    int32 capability,
    int32 *value);
/* query config (configuration == a set of capabilities) */
typedef NPError (
    *NPDeviceQueryConfigPtr)(NPP instance,
    const NPDeviceConfig* request,
    NPDeviceConfig* obtain);
/* device initialization */
typedef NPError (*NPDeviceInitializeContextPtr)(
    NPP instance,
    const NPDeviceConfig* config,
    NPDeviceContext* context);
/* peek at device state */
typedef NPError (*NPDeviceGetStateContextPtr) (
    NPP instance,
    NPDeviceContext* context,
    int32 state,
    intptr_t* value);
/* poke device state */
typedef NPError (*NPDeviceSetStateContextPtr) (
    NPP instance,
    NPDeviceContext* context,
    int32 state,
    intptr_t value);
/* flush context, if callback, userData are NULL */
/* this becomes a blocking call */
typedef NPError (*NPDeviceFlushContextPtr)(
    NPP instance,
    NPDeviceContext* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* userData);
/* destroy device context.  Application responsible for */
/* freeing context, if applicable */
typedef NPError (*NPDeviceDestroyContextPtr)(
    NPP instance,
    NPDeviceContext* context);
/* Create a buffer associated with a particular context. The usage of the */
/* buffer is device specific. The lifetime of the buffer is scoped with the */
/* lifetime of the context. */
typedef NPError (*NPDeviceCreateBufferPtr)(
    NPP instance,
    NPDeviceContext* context,
    size_t size,
    int32* id);
/* Destroy a buffer associated with a particular context. */
typedef NPError (*NPDeviceDestroyBufferPtr)(
    NPP instance,
    NPDeviceContext* context,
    int32 id);
/* Map a buffer id to its address. */
typedef NPError (*NPDeviceMapBufferPtr)(
    NPP instance,
    NPDeviceContext* context,
    int32 id,
    NPDeviceBuffer* buffer);
/* Gets the size of the given theme component.  For variable sized items like */
/* vertical scrollbar tracks, the width will be the required width of the */
/* track while the height will be the minimum height. */
typedef NPError (*NPDeviceThemeGetSize)(
    NPP instance,
    NPThemeItem item,
    int* width,
    int* height);
/* Draw a themed item (i.e. scrollbar arrow). */
typedef NPError (*NPDeviceThemePaint)(
    NPP instance,
    NPDeviceContext* context,
    NPThemeParams* params);


/* forward decl typdef structs */
typedef struct NPDevice NPDevice;
typedef struct NPNExtensions NPNExtensions;

// DEPRECATED: this typedef is just for the NaCl code until they switch to NPNExtensions.
// PLEASE REMOVE THIS WHEN THE NACL CODE IS UPDATED.
typedef struct NPNExtensions NPExtensions;

/* generic device interface */
struct NPDevice {
  NPDeviceQueryCapabilityPtr queryCapability;
  NPDeviceQueryConfigPtr queryConfig;
  NPDeviceInitializeContextPtr initializeContext;
  NPDeviceSetStateContextPtr setStateContext;
  NPDeviceGetStateContextPtr getStateContext;
  NPDeviceFlushContextPtr flushContext;
  NPDeviceDestroyContextPtr destroyContext;
  NPDeviceCreateBufferPtr createBuffer;
  NPDeviceDestroyBufferPtr destroyBuffer;
  NPDeviceMapBufferPtr mapBuffer;
  NPDeviceThemeGetSize themeGetSize;
  NPDeviceThemePaint themePaint;
};

/* returns NULL if deviceID unavailable / unrecognized */
typedef NPDevice* (*NPAcquireDevicePtr)(
    NPP instance,
    NPDeviceID device);

/* Copy UTF-8 string into clipboard */
typedef void (*NPCopyTextToClipboardPtr)(
    NPP instance,
    const char* content);

/* Updates the number of find results for the current search term.  If
 * there are no matches 0 should be passed in.  Only when the plugin has
 * finished searching should it pass in the final count with finalResult set to
 * true. */
typedef void (*NPNumberOfFindResultsChangedPtr)(
    NPP instance,
    int total,
    bool finalResult);

 /* Updates the index of the currently selected search item. */
typedef void (*NPSelectedFindResultChangedPtr)(
    NPP instance,
    int index);

/* Supports opening files anywhere on the system after prompting the user to
 * pick one.
 *
 * This API is asynchronous. It will return immediately and the user will be
 * prompted in parallel to pick a file. The plugin may continue to receive
 * events while the open file dialog is up, and may continue to paint. Plugins
 * may want to ignore input events between the call and the callback to avoid
 * reentrant behavior. If the return value is not NPERR_NO_ERROR, the callback
 * will NOT be executed.
 *
 * It is an error to call BrowseForFile before a previous call has executed
 * the callback.
 *
 * Setting the flags to "Open" requires that the file exist to allow picking.
 * Setting the flags to "Save" allows selecting nonexistant files (which will
 * then be created), and will prompt the user if they want to overwrite an
 * existing file if it exists.
 *
 * The plugin may specify a comma-separated list of possible mime types in
 * the "extensions" parameter. If no extensions are specified, the dialog box
 * will default to allowing all extensions. The first extension in the list
 * will be the default.
 *
 * TODO(brettw) On Windows the extensions traditionally include a text
 * description with the extension in the popup, do we want to allow this?
 * We should probably also allow the ability to put "All files" in the
 * list on Windows.
 *
 * Once the user has picked a file or has canceled the dialog box, the given
 * callback will be called with the results of the operation and the passed in
 * "user data" pointer. If the user successfully picked a file, the filename
 * will be non-NULL and will contain a pointer to an array of strings, one for
 * each file picked (the first file will be file_paths[0]). This buffer will
 * become invalid as soon as the call completes, so it is the plugin's
 * responsibility to copy the filename(sp if it needs future access to them.
 * A NULL file_paths in the callback means the user canceled the dialog box.
 *
 * The filename will be in UTF-8. It may not actually correspond to the actual
 * file on disk on a Linux system, because we'll do our best to convert it from
 * the filesystem's locale to UTF-8. Instead, the string will be appropriate for
 * displaying to the user which file they picked.
 * */
typedef enum {
  NPChooseFile_Open = 1,
  NPChooseFile_OpenMultiple = 2,
  NPChooseFile_Save = 3,
} NPChooseFileMode;
typedef void (*NPChooseFileCallback)(const char** filePaths,
                                     uint32 pathCount,
                                     void* userData);
typedef NPError (*NPChooseFilePtr)(
    NPP instance,
    const char* mimeTypes,
    NPChooseFileMode mode,
    NPChooseFileCallback callback,
    void* userData);

/* Pepper extensions */
struct NPNExtensions {
  /* Device interface acquisition */
  NPAcquireDevicePtr acquireDevice;
  /* Clipboard functionality */
  NPCopyTextToClipboardPtr copyTextToClipboard;
  /* Find */
  NPNumberOfFindResultsChangedPtr numberOfFindResultsChanged;
  NPSelectedFindResultChangedPtr selectedFindResultChanged;
  /* File I/O extensions */
  NPChooseFilePtr chooseFile;
};

/* Events -------------------------------------------------------------------*/

typedef enum {
  NPMouseButton_None    = -1,
  NPMouseButton_Left    = 0,
  NPMouseButton_Middle  = 1,
  NPMouseButton_Right   = 2
} NPMouseButtons;

typedef enum {
  NPEventType_Undefined   = -1,
  NPEventType_MouseDown   = 0,
  NPEventType_MouseUp     = 1,
  NPEventType_MouseMove   = 2,
  NPEventType_MouseEnter  = 3,
  NPEventType_MouseLeave  = 4,
  NPEventType_MouseWheel  = 5,
  NPEventType_RawKeyDown  = 6,
  NPEventType_KeyDown     = 7,
  NPEventType_KeyUp       = 8,
  NPEventType_Char        = 9,
  NPEventType_Minimize    = 10,
  NPEventType_Focus       = 11,
  NPEventType_Device      = 12
} NPEventTypes;

typedef enum {
  NPEventModifier_ShiftKey         = 1 << 0,
  NPEventModifier_ControlKey       = 1 << 1,
  NPEventModifier_AltKey           = 1 << 2,
  NPEventModifier_MetaKey          = 1 << 3,
  NPEventModifier_IsKeyPad         = 1 << 4,
  NPEventModifier_IsAutoRepeat     = 1 << 5,
  NPEventModifier_LeftButtonDown   = 1 << 6,
  NPEventModifier_MiddleButtonDown = 1 << 7,
  NPEventModifier_RightButtonDown  = 1 << 8
} NPEventModifiers;

typedef struct _NPKeyEvent
{
  uint32 modifier;
  uint32 normalizedKeyCode;
} NPKeyEvent;

typedef struct _NPCharacterEvent
{
  uint32 modifier;
  uint16 text[4];
  uint16 unmodifiedText[4];
} NPCharacterEvent;

typedef struct _NPMouseEvent
{
  uint32 modifier;
  int32 button;
  int32 x;
  int32 y;
  int32 clickCount;
} NPMouseEvent;

typedef struct _NPMouseWheelEvent
{
  uint32 modifier;
  float deltaX;
  float deltaY;
  float wheelTicksX;
  float wheelTicksY;
  uint32 scrollByPage;
} NPMouseWheelEvent;

typedef struct _NPDeviceEvent {
  uint32 device_uid;
  uint32 subtype;
  /* uint8 generic[0]; */
} NPDeviceEvent;

typedef struct _NPMinimizeEvent {
  int32 value;
} NPMinimizeEvent;

typedef struct _NPFocusEvent {
  int32 value;
} NPFocusEvent;

typedef struct _NPPepperEvent
{
  uint32 size;
  int32 type;
  double timeStampSeconds;
  union {
    NPKeyEvent key;
    NPCharacterEvent character;
    NPMouseEvent mouse;
    NPMouseWheelEvent wheel;
    NPMinimizeEvent minimize;
    NPFocusEvent focus;
    NPDeviceEvent device;
  } u;
} NPPepperEvent;

/* 2D -----------------------------------------------------------------------*/

#define NPPepper2DDevice 1

typedef struct _NPDeviceContext2DConfig {
} NPDeviceContext2DConfig;

typedef struct _NPDeviceContext2D
{
  /* Internal value used by the browser to identify this device. */
  void* reserved;

  /* A pointer to the pixel data. This data is 8-bit values in BGRA order in
   * memory. Each row will start |stride| bytes after the previous one.
   *
   * THIS DATA USES PREMULTIPLIED ALPHA. This means that each color channel has
   * been multiplied with the corresponding alpha, which makes compositing
   * easier. If any color channels have a value greater than the alpha value,
   * you'll likely get crazy colors and weird artifacts. */
  void* region;

  /* Length of each row of pixels in bytes. This may be larger than width * 4
   * if there is padding at the end of each row to help with alignment. */
  int32 stride;

  /* The dirty region that the plugin has painted into the buffer. This
   * will be initialized to the size of the plugin image in
   * initializeContextPtr. The plugin can change the values to only
   * update portions of the image. */
  struct {
    int32 left;
    int32 top;
    int32 right;
    int32 bottom;
  } dirty;
} NPDeviceContext2D;

/* 3D -----------------------------------------------------------------------*/

#define NPPepper3DDevice 2

typedef struct _NPDeviceContext3DConfig {
  int32 commandBufferSize;
} NPDeviceContext3DConfig;

typedef enum _NPDeviceContext3DError {
  // No error has ocurred.
  NPDeviceContext3DError_NoError,

  // The size of a command was invalid.
  NPDeviceContext3DError_InvalidSize,

  // An offset was out of bounds.
  NPDeviceContext3DError_OutOfBounds,

  // A command was not recognized.
  NPDeviceContext3DError_UnknownCommand,

  // The arguments to a command were invalid.
  NPDeviceContext3DError_InvalidArguments,

  // The 3D context was lost, for example due to a power management event. The
  // context must be destroyed and a new one created.
  NPDeviceContext3DError_LostContext,

  // Any other error.
  NPDeviceContext3DError_GenericError
} NPDeviceContext3DError;

typedef struct _NPDeviceContext3D NPDeviceContext3D;

typedef void (*NPDeviceContext3DRepaintPtr)(NPP npp,
                                            NPDeviceContext3D* context);

typedef struct _NPDeviceContext3D
{
  void* reserved;

  // If true, then a flush will only complete once the get offset has advanced
  // on the GPU thread. If false, then the get offset might have changed but
  // the GPU thread will respond as quickly as possible without guaranteeing
  // having made any progress in executing pending commands. Set to true
  // to ensure that progress is made or when flushing in a loop waiting for the
  // GPU to reach a certain state, for example in advancing beyond a particular
  // token. Set to false when flushing to query the current state, for example
  // whether an error has occurred.
  bool waitForProgress;

  // Buffer in which commands are stored.
  void* commandBuffer;
  int32 commandBufferSize;

  // Offset in command buffer reader has reached. Synchronized on flush.
  int32 getOffset;

  // Offset in command buffer writer has reached. Synchronized on flush.
  int32 putOffset;

  // Last processed token. Synchronized on flush.
  int32 token;

  // Callback invoked on the main thread when the context must be repainted.
  // TODO(apatrick): move this out of the context struct like the rest of the
  // fields.
  NPDeviceContext3DRepaintPtr repaintCallback;

  // Error status. Synchronized on flush.
  NPDeviceContext3DError error;
} NPDeviceContext3D;

/* Audio --------------------------------------------------------------------*/

#define NPPepperAudioDevice 3

/* min & max sample frame count */
typedef enum {
  NPAudioMinSampleFrameCount = 64,
  NPAudioMaxSampleFrameCount = 32768
} NPAudioSampleFrameCounts;

/* supported sample rates */
typedef enum {
  NPAudioSampleRate44100Hz = 44100,
  NPAudioSampleRate48000Hz = 48000,
  NPAudioSampleRate96000Hz = 96000
} NPAudioSampleRates;

/* supported sample formats */
typedef enum {
  NPAudioSampleTypeInt16   = 0,
  NPAudioSampleTypeFloat32 = 1
} NPAudioSampleTypes;

/* supported channel layouts */
/* there is code that depends on these being the actual number of channels */
typedef enum {
  NPAudioChannelNone     = 0,
  NPAudioChannelMono     = 1,
  NPAudioChannelStereo   = 2,
  NPAudioChannelThree    = 3,
  NPAudioChannelFour     = 4,
  NPAudioChannelFive     = 5,
  NPAudioChannelFiveOne  = 6,
  NPAudioChannelSeven    = 7,
  NPAudioChannelSevenOne = 8
} NPAudioChannels;

/* audio context states */
typedef enum {
  NPAudioContextStateCallback = 0,
  NPAudioContextStateUnderrunCounter = 1
} NPAudioContextStates;

/* audio context state values */
typedef enum {
  NPAudioCallbackStop = 0,
  NPAudioCallbackStart = 1
} NPAudioContextStateValues;

/* audio query capabilities */
typedef enum {
  NPAudioCapabilitySampleRate              = 0,
  NPAudioCapabilitySampleType              = 1,
  NPAudioCapabilitySampleFrameCount        = 2,
  NPAudioCapabilitySampleFrameCount44100Hz = 3,
  NPAudioCapabilitySampleFrameCount48000Hz = 4,
  NPAudioCapabilitySampleFrameCount96000Hz = 5,
  NPAudioCapabilityOutputChannelMap        = 6,
  NPAudioCapabilityInputChannelMap         = 7
} NPAudioCapabilities;

typedef struct _NPDeviceContextAudio NPDeviceContextAudio;

/* user supplied callback function */
typedef void (*NPAudioCallback)(NPDeviceContextAudio *context);

typedef struct _NPDeviceContextAudioConfig {
  int32 sampleRate;
  int32 sampleType;
  int32 outputChannelMap;
  int32 inputChannelMap;
  int32 sampleFrameCount;
  uint32 startThread;
  uint32 flags;
  NPAudioCallback callback;
  void *userData;
} NPDeviceContextAudioConfig;

struct _NPDeviceContextAudio {
  NPDeviceContextAudioConfig config;
  void *outBuffer;
  void *inBuffer;
  void *reserved;
};

/* Printing related APIs ---------------------------------------------------*/

/* Being a print operation. Returns the total number of pages to print at the
 * given printableArea size and DPI. printableArea is in points (a point is 1/72
 * of an inch). The plugin is expected to remember the values of printableArea
 * and printerDPI for use in subsequent print interface calls. These values
 * should be cleared in printEnd. */
typedef NPError (*NPPPrintBeginPtr) (
    NPP instance,
    NPRect* printableArea,
    int32 printerDPI,
    int32* numPages);
/* Returns the required raster dimensions for the given page. */
typedef NPError (*NPPGetRasterDimensionsPtr) (
    NPP instance,
    int32 pageNumber,
    int32* widthInPixels,
    int32* heightInPixels);
/* Prints the specified page This allows the plugin to print a raster output. */
typedef NPError (*NPPPrintPageRasterPtr) (
    NPP instance,
    int32 pageNumber,
    NPDeviceContext2D* printSurface);
/* Ends the print operation */
typedef NPError (*NPPPrintEndPtr) (NPP instance);

/* TODO(sanjeevr) : Provide a vector interface for printing. We need to decide
 * on a vector format that can support embedded fonts. A vector format will
 * greatly reduce the size of the required output buffer. */

typedef struct _NPPPrintExtensions {
  NPPPrintBeginPtr printBegin;
  NPPGetRasterDimensionsPtr getRasterDimensions;
  NPPPrintPageRasterPtr printPageRaster;
  NPPPrintEndPtr printEnd;
} NPPPrintExtensions;

/* Returns NULL if the plugin does not support print extensions */
typedef NPPPrintExtensions* (*NPPGetPrintExtensionsPtr)(NPP instance);

/* Find ---------------------------------------------------------------------*/

/* Finds the given UTF-8 text starting at the current selection.  The number of
 * results will be updated asynchronously via numberOfFindResultsChanged.  Note
 * that multiple StartFind calls can happen before StopFind is called in the
 * case of the search term changing. */
typedef NPError (*NPPStartFindPtr) (
    NPP instance,
    const char* text,
    bool caseSensitive);

/* Go to the next/previous result. */
typedef NPError (*NPPSelectFindResultPtr) (
    NPP instance,
    bool forward);

/* Tells the plugin that the find operation has stopped, so it should clear
 * any highlighting. */
typedef NPError (*NPPStopFindPtr) (
    NPP instance);

typedef struct _NPPFindExtensions {
  NPPStartFindPtr startFind;
  NPPSelectFindResultPtr selectFindResult;
  NPPStopFindPtr stopFind;
} NPPFindExtensions;

/* Returns NULL if the plugin does not support find extensions. */
typedef NPPFindExtensions* (*NPPGetFindExtensionsPtr)(NPP instance);

/* Zooms plugins.  0 means reset, -1 means zoom out, and +1 means zoom in. */
typedef NPError (*NPPZoomPtr) (
    NPP instance,
    int factor);

typedef struct _NPPExtensions {
  NPPGetPrintExtensionsPtr getPrintExtensions;
  NPPGetFindExtensionsPtr getFindExtensions;
  NPPZoomPtr zoom;
} NPPExtensions;

#endif  /* _NP_EXTENSIONS_H_ */
