/* CFCalendar.h
   
   Copyright (C) 2011 Free Software Foundation, Inc.
   
   Written by: Stefan Bidigaray
   Date: March, 2011
   
   This file is part of the GNUstep CoreBase Library.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; see the file COPYING.LIB.
   If not, see <http://www.gnu.org/licenses/> or write to the 
   Free Software Foundation, 51 Franklin Street, Fifth Floor, 
   Boston, MA 02110-1301, USA.
*/

#include "CoreFoundation/CFBase.h"
#include "CoreFoundation/CFDate.h"
#include "CoreFoundation/CFLocale.h"
#include "CoreFoundation/CFCalendar.h"
#include "CoreFoundation/CFString.h"
#include "CoreFoundation/CFRuntime.h"

#include <pthread.h>
#include <unicode/ucal.h>

struct __CFCalendar
{
  CFRuntimeBase _parent;
  UCalendar    *_ucal;
  CFStringRef   _ident;
  CFStringRef   _localeIdent;
  CFStringRef   _tzIdent;
};

static CFTypeID _kCFCalendarTypeID;
static CFCalendarRef _kCFCalendarCurrent = NULL;
static pthread_mutex_t _kCFCalendarLock = PTHREAD_MUTEX_INITIALIZER;

static void
CFCalendarSetupUCalendar (CFCalendarRef cal)
{
  char localeIdent[ULOC_FULLNAME_CAPACITY];
  UniChar *tzIdent = NULL; // FIXME
  UCalendar *ucal;
  UErrorCode err = U_ZERO_ERROR;
  
  if (cal->_localeIdent != NULL)
    {
      char calIdent[ULOC_KEYWORDS_CAPACITY];
      CFStringGetCString (cal->_localeIdent, localeIdent,
        ULOC_FULLNAME_CAPACITY, kCFStringEncodingASCII);
      CFStringGetCString (cal->_ident, calIdent, ULOC_KEYWORDS_CAPACITY,
        kCFStringEncodingASCII);
      uloc_setKeywordValue ("calendar", (const char*)calIdent, localeIdent,
        ULOC_FULLNAME_CAPACITY, &err);
    }
  
  ucal = ucal_open (tzIdent, 0, localeIdent, UCAL_TRADITIONAL, &err);
  if (U_FAILURE(err))
    ucal = NULL;
  
  cal->_ucal = ucal;
}

static inline void
CFCalendarOpenUCalendar (CFCalendarRef cal)
{
  if (cal->_ucal == NULL)
    CFCalendarSetupUCalendar (cal);
}

static inline void
CFCalendarCloseUCalendar (CFCalendarRef cal)
{
  ucal_close (cal->_ucal);
  cal->_ucal = NULL;
}

static void CFCalendarDealloc (CFTypeRef cf)
{
  CFCalendarRef cal = (CFCalendarRef)cf;
  
  CFCalendarCloseUCalendar (cal);
  CFRelease (cal->_ident);
  CFRelease (cal->_localeIdent);
  CFRelease (cal->_tzIdent);
}

static Boolean CFCalendarEqual (CFTypeRef cf1, CFTypeRef cf2)
{
  CFCalendarRef o1 = (CFCalendarRef)cf1;
  CFCalendarRef o2 = (CFCalendarRef)cf2;
  
  return (Boolean)(CFEqual(o1->_ident, o2->_ident)
                   && CFEqual(o1->_localeIdent, o2->_localeIdent)
                   && CFEqual(o1->_tzIdent, o2->_tzIdent));
}

static CFHashCode CFCalendarHash (CFTypeRef cf)
{
  return CFHash (((CFCalendarRef)cf)->_ident);
}

static CFStringRef CFCalendarCopyFormattingDesc (CFTypeRef cf,
  CFDictionaryRef formatOptions)
{
  return CFRetain (((CFCalendarRef)cf)->_ident);
}

static const CFRuntimeClass CFCalendarClass =
{
  0,
  "CFLocale",
  NULL,
  NULL,
  CFCalendarDealloc,
  CFCalendarEqual,
  CFCalendarHash,
  CFCalendarCopyFormattingDesc,
  NULL
};

void CFCalendarInitialize (void)
{
  _kCFCalendarTypeID = _CFRuntimeRegisterClass (&CFCalendarClass);
}

static inline UCalendarDateFields
CFCalendarUnitToUCalendarDateFields (CFCalendarUnit unit)
{
  UCalendarDateFields ret;
  switch (unit)
    {
      case kCFCalendarUnitEra:
        ret = UCAL_ERA;
        break;
      case kCFCalendarUnitYear:
        ret = UCAL_YEAR;
        break;
      case kCFCalendarUnitMonth:
        ret = UCAL_MONTH;
        break;
      case kCFCalendarUnitDay:
        ret = UCAL_DATE;
        break;
      case kCFCalendarUnitHour:
        ret = UCAL_HOUR_OF_DAY;
        break;
      case kCFCalendarUnitMinute:
        ret = UCAL_MINUTE;
        break;
      case kCFCalendarUnitSecond:
        ret = UCAL_SECOND;
        break;
      case kCFCalendarUnitWeek:
        ret = UCAL_WEEK_OF_YEAR;
        break;
      case kCFCalendarUnitWeekday:
        ret = UCAL_DAY_OF_WEEK;
        break;
      case kCFCalendarUnitWeekdayOrdinal:
        ret = UCAL_DAY_OF_WEEK_IN_MONTH;
        break;
      case kCFCalendarUnitQuarter:
        ret = UCAL_MONTH; // FIXME
        break;
      default:
        ret = -1;
    }
  return ret;
}

static inline CFCalendarUnit
CFCalendarGetCalendarUnitsFromDescription (const char *description)
{
  CFCalendarUnit ret;
  
  if (description == NULL)
    return 0;
  
  ret = 0;
  while (*description != '\0')
    {
      switch (*description)
        {
          case 'y':
            ret |= kCFCalendarUnitYear;
            break;
          case 'M':
            ret |= kCFCalendarUnitMonth;
            break;
          case 'd':
            ret |= kCFCalendarUnitDay;
            break;
          case 'H':
            ret |= kCFCalendarUnitHour;
            break;
          case 'm':
            ret |= kCFCalendarUnitMinute;
            break;
          case 's':
            ret |= kCFCalendarUnitSecond;
            break;
        }
      ++description;
    }
  return ret;
}



CFTypeID
CFCalendarGetTypeID (void)
{
  return _kCFCalendarTypeID;
}

CFCalendarRef
CFCalendarCopyCurrent (void)
{
  if (_kCFCalendarCurrent == NULL)
    {
      pthread_mutex_lock (&_kCFCalendarLock);
      if (_kCFCalendarCurrent == NULL)
        {
          CFLocaleRef locale;
          CFStringRef calIdent;
          CFCalendarRef cal;
          
          locale = CFLocaleCopyCurrent ();
          calIdent = CFLocaleGetValue (locale, kCFLocaleCalendarIdentifier);
          cal = CFCalendarCreateWithIdentifier (NULL, calIdent);
          CFCalendarSetLocale (cal, locale);
          
          CFRelease (calIdent);
          CFRelease (locale);
          _kCFCalendarCurrent = cal;
        }
      pthread_mutex_unlock (&_kCFCalendarLock);
    }
  
  return _kCFCalendarCurrent;
}

CFCalendarRef
CFCalendarCreateWithIdentifier (CFAllocatorRef allocator, CFStringRef ident)
{
  CFLocaleRef locale;
  CFCalendarRef new;
  
  if (ident != kCFGregorianCalendar
      && ident != kCFBuddhistCalendar
      && ident != kCFChineseCalendar
      && ident != kCFHebrewCalendar
      && ident != kCFIslamicCalendar
      && ident != kCFIslamicCivilCalendar
      && ident != kCFJapaneseCalendar
      && ident != kCFRepublicOfChinaCalendar
      && ident != kCFPersianCalendar
      && ident != kCFIndianCalendar
      && ident != kCFISO8601Calendar)
    {
      if (CFEqual(ident, kCFGregorianCalendar))
        ident = kCFGregorianCalendar;
      else if (CFEqual(ident, kCFBuddhistCalendar))
        ident = kCFBuddhistCalendar;
      else if (CFEqual(ident, kCFChineseCalendar))
        ident = kCFChineseCalendar;
      else if (CFEqual(ident, kCFHebrewCalendar))
        ident = kCFHebrewCalendar;
      else if (CFEqual(ident, kCFIslamicCalendar))
        ident = kCFIslamicCalendar;
      else if (CFEqual(ident, kCFIslamicCivilCalendar))
        ident = kCFIslamicCivilCalendar;
      else if (CFEqual(ident, kCFJapaneseCalendar))
        ident = kCFJapaneseCalendar;
      else if (CFEqual(ident, kCFRepublicOfChinaCalendar))
        ident = kCFRepublicOfChinaCalendar;
      else if (CFEqual(ident, kCFPersianCalendar))
        ident = kCFPersianCalendar;
      else if (CFEqual(ident, kCFIndianCalendar))
        ident = kCFIndianCalendar;
      else if (CFEqual(ident, kCFISO8601Calendar))
        ident = kCFISO8601Calendar;
      else
        return NULL;
    }
  
  new = (CFCalendarRef)_CFRuntimeCreateInstance (allocator, _kCFCalendarTypeID,
    sizeof(struct __CFCalendar) - sizeof(CFRuntimeBase), NULL);
  new->_ident = ident;
  
  locale = CFLocaleCopyCurrent ();
  new->_localeIdent = CFRetain (CFLocaleGetIdentifier(locale));
  CFRelease (locale);
  
  new->_tzIdent = NULL; // FIXME
  
  return new;
}

CFTimeZoneRef
CFCalendarCopyTimeZone (CFCalendarRef cal)
{
  return NULL;
}

void
CFCalendarSetTimeZone (CFCalendarRef cal, CFTimeZoneRef tz)
{
}

CFStringRef
CFCalendarGetIdentifier (CFCalendarRef cal)
{
  return cal->_ident;
}

CFLocaleRef
CFCalendarCopyLocale (CFCalendarRef cal)
{
  return CFLocaleCreate (NULL, cal->_localeIdent);
}

void
CFCalendarSetLocale (CFCalendarRef cal, CFLocaleRef locale)
{
  if (cal->_localeIdent != NULL)
    CFRelease (cal->_localeIdent);
  cal->_localeIdent = CFRetain (CFLocaleGetIdentifier(locale));
  CFCalendarCloseUCalendar (cal);
}

CFIndex
CFCalendarGetFirstWeekday (CFCalendarRef cal)
{
  CFCalendarOpenUCalendar (cal);
  return ucal_getAttribute (cal->_ucal, UCAL_FIRST_DAY_OF_WEEK);
}

void
CFCalendarSetFirstWeekday (CFCalendarRef cal, CFIndex wkdy)
{
  CFCalendarOpenUCalendar (cal);
  ucal_setAttribute (cal->_ucal, UCAL_FIRST_DAY_OF_WEEK, wkdy);
}

CFIndex
CFCalendarGetMinimumDaysInFirstWeek (CFCalendarRef cal)
{
  CFCalendarOpenUCalendar (cal);
  return ucal_getAttribute (cal->_ucal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
}

void
CFCalendarSetMinimumDaysInFirstWeek (CFCalendarRef cal, CFIndex mwd)
{
  CFCalendarOpenUCalendar (cal);
  ucal_setAttribute (cal->_ucal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK, mwd);
}

Boolean
CFCalendarAddComponents (CFCalendarRef cal, CFAbsoluteTime *at,
  CFOptionFlags options, const unsigned char *componentDesc, ...)
{
  return false;
}

Boolean
CFCalendarComposeAbsoluteTime (CFCalendarRef cal, CFAbsoluteTime *at,
  const unsigned char *componentDesc, ...)
{
  return false;
}

Boolean
CFCalendarDecomposeAbsoluteTime (CFCalendarRef cal, CFAbsoluteTime at,
  const unsigned char *componentDesc, ...)
{
  return false;
}

Boolean
CFCalendarGetComponentDifference (CFCalendarRef cal, CFAbsoluteTime startinAT,
  CFAbsoluteTime resultAT, CFOptionFlags options,
  const unsigned char *componentDesc, ...)
{
  return false;
}

Boolean
CFCalendarGetTimeRangeOfUnit (CFCalendarRef cal, CFCalendarUnit unit,
  CFAbsoluteTime at, CFAbsoluteTime *startp, CFTimeInterval *tip)
{
  return false;
}

CFRange
CFCalendarGetRangeOfUnit (CFCalendarRef cal, CFCalendarUnit smallerUnit,
  CFCalendarUnit biggerUnit, CFAbsoluteTime at)
{
  return CFRangeMake (kCFNotFound, 0);
}

CFIndex
CFCalendarGetOrdinalityOfUnit (CFCalendarRef cal, CFCalendarUnit smallerUnit,
  CFCalendarUnit biggerUnit, CFAbsoluteTime at)
{
  return 0;
}

CFRange
CFCalendarGetMaximumRangeOfUnit (CFCalendarRef cal, CFCalendarUnit unit)
{
  CFRange range;
  UErrorCode err = U_ZERO_ERROR;
  UCalendarDateFields field = CFCalendarUnitToUCalendarDateFields (unit);
  
  CFCalendarOpenUCalendar (cal);
  
  range.location = ucal_getLimit (cal->_ucal, field, UCAL_GREATEST_MINIMUM,
    &err);
  range.length = ucal_getLimit (cal->_ucal, field, UCAL_LEAST_MAXIMUM, &err)
    - range.location + 1;
  if (unit == kCFCalendarUnitMonth)
    range.location += 1;
  
  return range;
}

CFRange
CFCalendarGetMinimumRangeOfUnit (CFCalendarRef cal, CFCalendarUnit unit)
{
  CFRange range;
  UErrorCode err = U_ZERO_ERROR;
  UCalendarDateFields field = CFCalendarUnitToUCalendarDateFields (unit);
  
  CFCalendarOpenUCalendar (cal);
  
  range.location = ucal_getLimit (cal->_ucal, field, UCAL_MINIMUM,
    &err);
  range.length = ucal_getLimit (cal->_ucal, field, UCAL_MAXIMUM, &err)
    - range.location + 1;
  if (unit == kCFCalendarUnitMonth)
    range.location += 1;
  
  return range;
}
