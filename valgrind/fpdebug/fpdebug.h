
/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (fpdebug.h) only.  The rest of FpDebug tool is licensed
   under the terms of the GNU General Public License, version 2,
   unless otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of FpDebug, a heavyweight Valgrind tool for
   detecting floating-point accuracy problems.

   Copyright (C) 2010-2017 Florian Benz.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must
      not claim that you wrote the original software.  If you use this
      software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (fpdebug.h) only.  The entire rest of FpDebug tool is licensed
   under the terms of the GNU General Public License, version 2.
   See the COPYING file in the source distribution for details.

   ----------------------------------------------------------------
*/

#ifndef __FPDEBUG_H
#define __FPDEBUG_H

/* This file is for inclusion into client code.

   You can uses these macros to influence the analysis.

   See comment near the top of valgrind.h on how to use them.
*/

#include "../include/valgrind.h"

typedef
	enum {
		VG_USERREQ__PRINT_ERROR,
		VG_USERREQ__COND_PRINT_ERROR,
		VG_USERREQ__DUMP_ERROR_GRAPH,
		VG_USERREQ__COND_DUMP_ERROR_GRAPH,
		VG_USERREQ__BEGIN_STAGE,
		VG_USERREQ__END_STAGE,
		VG_USERREQ__CLEAR_STAGE,
		VG_USERREQ__ERROR_GREATER,
		VG_USERREQ__RESET,
		VG_USERREQ__INSERT_SHADOW,
		VG_USERREQ__BEGIN,
		VG_USERREQ__END
   } Vg_FpDebugClientRequest;


#define VALGRIND_PRINT_ERROR(_qzz_str, _qzz_fp)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__PRINT_ERROR,       \
                            _qzz_str, _qzz_fp, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_COND_PRINT_ERROR(_qzz_str, _qzz_fp)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__COND_PRINT_ERROR,       \
                            _qzz_str, _qzz_fp, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_DUMP_ERROR_GRAPH(_qzz_str, _qzz_fp)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__DUMP_ERROR_GRAPH,       \
                            _qzz_str, _qzz_fp, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_COND_DUMP_ERROR_GRAPH(_qzz_str, _qzz_fp)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__DUMP_ERROR_GRAPH,       \
                            _qzz_str, _qzz_fp, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_BEGIN_STAGE(_qzz_num)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__BEGIN_STAGE,       \
                            _qzz_num, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_END_STAGE(_qzz_num)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__END_STAGE,       \
                            _qzz_num, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_CLEAR_STAGE(_qzz_num)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__CLEAR_STAGE,       \
                            _qzz_num, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_ERROR_GREATER(_qzz_fp, _qzz_err)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__ERROR_GREATER,       \
                            _qzz_fp, _qzz_err, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_RESET()           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__RESET,       \
                            0, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_INSERT_SHADOW(_qzz_fp)           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__INSERT_SHADOW,       \
                            _qzz_fp, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_BEGIN()           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__BEGIN,       \
                            0, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#define VALGRIND_END()           \
   (__extension__({unsigned long _qzz_res;                       \
    VALGRIND_DO_CLIENT_REQUEST(_qzz_res, 0 /* default return */, \
                            VG_USERREQ__END,       \
                            0, 0, 0, 0, 0);       \
    _qzz_res;                                                    \
   }))

#endif
