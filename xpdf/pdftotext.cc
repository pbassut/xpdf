//========================================================================
//
// pdftotext.cc
//
// Copyright 1997-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#ifdef DEBUG_FP_LINUX
#  include <fenv.h>
#  include <fpu_control.h>
#endif
#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "TextString.h"
#include "Error.h"
#include "config.h"

static GBool tableLayout = gTrue;
static char textEncName[128] = "UTF-8";
static GBool quiet = gTrue;

static ArgDesc argDesc[] = {
  {"-table",   argFlag,     &tableLayout,   0,
   "similar to -layout, but optimized for tables"},
  {"-enc",     argString,   textEncName,    sizeof(textEncName),
   "output text encoding name"},
  {"-q",       argFlag,     &quiet,         0,
   "don't print any messages or errors"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  TextOutputControl textOutControl;
  TextOutputDev *textOut;
  UnicodeMap *uMap;
  Object info;
  GBool ok;
  char *p;
  int exitCode;

#ifdef DEBUG_FP_LINUX
  // enable exceptions on floating point div-by-zero
  feenableexcept(FE_DIVBYZERO);
  // force 64-bit rounding: this avoids changes in output when minor
  // code changes result in spills of x87 registers; it also avoids
  // differences in output with valgrind's 64-bit floating point
  // emulation (yes, this is a kludge; but it's pretty much
  // unavoidable given the x87 instruction set; see gcc bug 323 for
  // more info)
  fpu_control_t cw; 
  _FPU_GETCW(cw);
  cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
  _FPU_SETCW(cw);
#endif

  exitCode = 99;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdftotext version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdftotext", "<PDF-file> [<text-file>]", argDesc);
    }
    goto err0;
  }
  fileName = new GString(argv[1]);

  // read config file
  globalParams = new GlobalParams(cfgFileName);
  if (textEncName[0]) {
    globalParams->setTextEncoding(textEncName);
  }
  if (quiet) {
    globalParams->setErrQuiet(quiet);
  }

  // get mapping to output encoding
  if (!(uMap = globalParams->getTextEncoding())) {
    error(errConfig, -1, "Couldn't get text encoding");
    delete fileName;
    goto err1;
  }

  // open PDF file
  doc = new PDFDoc(fileName, NULL, NULL);
  if (!doc->isOk()) {
    exitCode = 1;
    goto err2;
  }

  // get page range
  if (firstPage < 1) {
    firstPage = 1;
  }
  if (lastPage < 1 || lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // write text file
  textOutControl.mode = textOutTableLayout;
  textOutControl.fixedPitch = fixedPitch;
  textOutControl.clipText = clipText;
  textOut = new TextOutputDev(textFileName->getCString(), &textOutControl,
			      gFalse);
  if (textOut->isOk()) {
    doc->displayPages(textOut, firstPage, lastPage, 72, 72, 0,
		      gFalse, gTrue, gFalse);
  } else {
    delete textOut;
    exitCode = 2;
    goto err3;
  }
  delete textOut;

  exitCode = 0;

  // clean up
 err3:
  delete textFileName;
 err2:
  delete doc;
  uMap->decRefCnt();
 err1:
  delete globalParams;
 err0:

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return exitCode;
}
