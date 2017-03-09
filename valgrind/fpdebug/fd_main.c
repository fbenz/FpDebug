
/*--------------------------------------------------------------------*/
/*--- FpDebug: Floating-point arithmetic debugger	     fd_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of FpDebug, a heavyweight Valgrind tool for
   detecting floating-point accuracy problems.

   Copyright (C) 2010-2017 Florian Benz
      florianbenz1@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

/*
   The first function called by Valgrind is fd_pre_clo_init.

   For each super block (similar to a basic block) Valgrind
   calls fd_instrument and here the instrumentation is done.
   This means that instructions needed for the analysis are added.

   The function fd_instrument does not add instructions itself
   but calls functions named instrumentX where X stands for the
   operation for which instructions should be added.

   For instance, the instructions for analysis a binary operation
   are added in instrumentBinOp. Bascially a call to processBinOp
   is added. So each time the client program performs a binary
   floating-point operation, processBinOp is called.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_machine.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_oset.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"
#include "pub_tool_options.h"
#include "pub_tool_xarray.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_redir.h"

#include "fd_include.h"
/* for client requests */
#include "fpdebug.h"

#include "opToString.c"


#define mkU32(_n)                			IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n)                			IRExpr_Const(IRConst_U64(_n))

#define	MAX_STAGES							100
#define	MAX_TEMPS							1000
#define	MAX_REGISTERS						1000
#define	CANCEL_LIMIT						10
#define TMP_COUNT							4
#define CONST_COUNT   						4

/* 10,000 entries -> ~6 MB file */
#define MAX_ENTRIES_PER_FILE				10000
#define MAX_LEVEL_OF_GRAPH					10
#define MAX_DUMPED_GRAPHS					10

#define MPFR_BUFSIZE						100
#define FORMATBUF_SIZE						256
#define FILENAME_SIZE						256
#define FWRITE_BUFSIZE 						32000
#define FWRITE_THROUGH 						10000

/* standard rounding mode: round to nearest */
static mpfr_rnd_t 	STD_RND 				= MPFR_RNDN;

/* precision for float: 24, double: 53*/
static mpfr_prec_t	clo_precision 			= 120;
static Bool 		clo_computeMeanValue 	= True;
static Bool 		clo_ignoreLibraries 	= False;
static Bool 		clo_ignoreAccurate 		= True;
static Bool			clo_simulateOriginal	= False;
static Bool			clo_analyze				= True;
static Bool			clo_bad_cancellations	= True;
static Bool			clo_ignore_end			= False;

static UInt activeStages 					= 0;
static ULong sbExecuted 					= 0;
static ULong fpOps 							= 0;
static Int fwrite_pos						= -1;
static Int fwrite_fd 						= -1;

static ULong sbCounter 						= 0;
static ULong totalIns 						= 0;
static UInt getCount 						= 0;
static UInt getsIgnored 					= 0;
static UInt storeCount 						= 0;
static UInt storesIgnored 					= 0;
static UInt loadCount 						= 0;
static UInt loadsIgnored					= 0;
static UInt putCount 						= 0;
static UInt putsIgnored 					= 0;
static UInt maxTemps 						= 0;

static Bool fd_process_cmd_line_options(const HChar* arg) {
	if VG_BINT_CLO(arg, "--precision", clo_precision, MPFR_PREC_MIN, MPFR_PREC_MAX) {}
	else if VG_BOOL_CLO(arg, "--mean-error", clo_computeMeanValue) {}
	else if VG_BOOL_CLO(arg, "--ignore-libraries", clo_ignoreLibraries) {}
	else if VG_BOOL_CLO(arg, "--ignore-accurate", clo_ignoreAccurate) {}
	else if VG_BOOL_CLO(arg, "--sim-original", clo_simulateOriginal) {}
	else if VG_BOOL_CLO(arg, "--analyze-all", clo_analyze) {}
    else if VG_BOOL_CLO(arg, "--ignore-end", clo_ignore_end) {}
	else
		return False;

	return True;
}

static void fd_print_usage(void) {
	VG_(printf)(
"    --precision=<number>      the precision of the shadow values [120]\n"
"    --mean-error=no|yes       compute mean and max error for each operation [yes]\n"
"    --ignore-libraries=no|yes libraries are not analyzed [no]\n"
"    --ignore-accurate=no|yes  do not show variables/lines without errors [yes]\n"
"    --sim-original=no|yes     simulate original precision [no]\n"
"    --analyze-all=no|yes      analyze everything [yes]\n"
"    --ignore-end=no|yes       ignore end requests [no]\n"
	);
}

static void fd_print_debug_usage(void) {
	VG_(printf)("    (none)\n");
}

static int abs(int x) {
	if (x >= 0) {
		return x;
	} else {
		return -x;
	}
}

/* globalMemory is the hash table for maping the addresses of the
   original floating-point values to the shadow values. */
static VgHashTable* globalMemory 	= NULL; /* Hash table with ShadowValue* */
static VgHashTable* meanValues 		= NULL; /* Hash table with MeanValue* */
static OSet* originAddrSet 			= NULL;
static OSet* unsupportedOps			= NULL;

static Store* 			storeArgs 	= NULL;
static ITE* 			muxArgs 	= NULL;
static UnOp* 			unOpArgs 	= NULL;
static BinOp* 			binOpArgs 	= NULL;
static TriOp* 			triOpArgs 	= NULL;
static CircularRegs* 	circRegs	= NULL;

static ShadowValue*** 	threadRegisters = NULL; /* threadRegisters[VG_N_THREADS][MAX_REGISTERS] */
static ShadowValue* 	localTemps[MAX_TEMPS];
static ShadowTmp* 		sTmp[TMP_COUNT];
static ShadowConst* 	sConst[CONST_COUNT];
static Stage* 			stages[MAX_STAGES];
static VgHashTable*	stageReports[MAX_STAGES]; /* Hash table for each stage with StageReport* */

static HChar 			formatBuf[FORMATBUF_SIZE];
static HChar 			filename[FILENAME_SIZE];
static HChar 			fwrite_buf[FWRITE_BUFSIZE];

static mpfr_t meanOrg, meanRelError;
static mpfr_t stageOrg, stageDiff, stageRelError;
static mpfr_t dumpGraphOrg, dumpGraphRel, dumpGraphDiff, dumpGraphMeanError, dumpGraphErr1, dumpGraphErr2;
static mpfr_t endAnalysisOrg, endAnalysisRelError;
static mpfr_t introMaxError, introErr1, introErr2;
static mpfr_t compareIntroErr1, compareIntroErr2;
static mpfr_t writeSvOrg, writeSvDiff, writeSvRelError;
static mpfr_t cancelTemp;
static mpfr_t arg1tmpX, arg2tmpX, arg3tmpX;


static void mpfrToStringShort(HChar* str, mpfr_t* fp) {
	if (mpfr_cmp_ui(*fp, 0) == 0) {
		str[0] = '0'; str[1] = '\0';
		return;
	}

	Int sgn = mpfr_sgn(*fp);
	if (sgn >= 0) {
		str[0] = ' '; str[1] = '0'; str[2] = '\0';
	} else {
		str[0] = '-'; str[1] = '\0';
	}

	HChar mpfr_str[4]; /* digits + 1 */
	mpfr_exp_t exp;
	/* digits_base10 = log10 ( 2^(significant bits) ) */
	mpfr_get_str(mpfr_str, &exp, /* base */ 10, 3, *fp, STD_RND);
	exp--;
	VG_(strcat)(str, mpfr_str);

	if (sgn >= 0) {
		str[1] = str[2];
		str[2] = '.';
	} else {
		str[1] = str[2];
		str[2] = '.';
	}

	HChar exp_str[10];
	VG_(sprintf)(exp_str, " * 10^%ld", exp);
	VG_(strcat)(str, exp_str);
}

static void mpfrToString(HChar* str, mpfr_t* fp) {
	Int sgn = mpfr_sgn(*fp);
	if (sgn >= 0) {
		str[0] = ' '; str[1] = '0'; str[2] = '\0';
	} else {
		str[0] = '-'; str[1] = '\0';
	}

	HChar mpfr_str[60]; /* digits + 1 */
	mpfr_exp_t exp;
	/* digits_base10 = log10 ( 2^(significant bits) ) */
	mpfr_get_str(mpfr_str, &exp, /* base */ 10, /* digits, float: 7, double: 15 */ 15, *fp, STD_RND);
	exp--;
	VG_(strcat)(str, mpfr_str);

	if (sgn >= 0) {
		str[1] = str[2];
		str[2] = '.';
	} else {
		str[1] = str[2];
		str[2] = '.';
	}

	HChar exp_str[50];
	VG_(sprintf)(exp_str, " * 10^%ld", exp);
	VG_(strcat)(str, exp_str);

	mpfr_prec_t pre_min = mpfr_min_prec(*fp);
	mpfr_prec_t pre = mpfr_get_prec(*fp);
	HChar pre_str[50];
	VG_(sprintf)(pre_str, ", %ld/%ld bit", pre_min, pre);
	VG_(strcat)(str, pre_str);
}

static Bool ignoreFile(const HChar* desc) {
	if (!clo_ignoreLibraries) {
		return False;
	}
	/* simple patern matching - only for one short pattern */
	const HChar* pattern = ".so";
	Int pi = 0;
	Int i = 0;
	while (desc[i] != '\0' && i < 256) {
		if (desc[i] == pattern[pi]) {
			pi++;
		} else {
			pi = 0;
		}
		if (pattern[pi] == '\0') return True;
		i++;
	}
	return False;
}

static Bool isInLibrary(Addr64 addr) {
	DebugInfo* dinfo = VG_(find_DebugInfo)((Addr)addr);
	if (!dinfo) return False; /* be save if not sure */

	const HChar* soname = VG_(DebugInfo_get_soname)(dinfo);
	tl_assert(soname);
	if (0) VG_(printf)("%s\n", soname);

	return ignoreFile(soname);
}

static __inline__
mpfr_exp_t maxExp(mpfr_exp_t x, mpfr_exp_t y) {
	if (x > y) {
		return x;
	} else {
		return y;
	}
}

static __inline__
mpfr_exp_t getCanceledBits(mpfr_t* res, mpfr_t* arg1, mpfr_t* arg2) {
	/* consider zero, NaN and infinity */
	if (mpfr_regular_p(*arg1) == 0 || mpfr_regular_p(*arg2) == 0 || mpfr_regular_p(*res) == 0) {
		return 0;
	}

	mpfr_exp_t resExp = mpfr_get_exp(*res);
	mpfr_exp_t arg1Exp = mpfr_get_exp(*arg1);
	mpfr_exp_t arg2Exp = mpfr_get_exp(*arg2);

	mpfr_exp_t max = maxExp(arg1Exp, arg2Exp);
	if (resExp < max) {
		mpfr_exp_t diff = max - resExp;
		if (diff < 0) {
			return -diff;
		}
		return diff;
	}
	return 0;
}

static ULong avMallocs = 0;
static ULong avFrees = 0;

static __inline__
ShadowValue* initShadowValue(UWord key) {
	ShadowValue* sv = VG_(malloc)("fd.initShadowValue.1", sizeof(ShadowValue));
	sv->key = key;
	sv->active = True;
	sv->version = 0;
	sv->opCount = 0;
	sv->origin = 0;
	sv->cancelOrigin = 0;
	sv->orgType = Ot_INVALID;

	mpfr_init(sv->value);

	avMallocs++;
	return sv;
}

static __inline__
void freeShadowValue(ShadowValue* sv, Bool freeSvItself) {
	tl_assert(sv != NULL);

	mpfr_clear(sv->value);
	if (freeSvItself) {
		VG_(free)(sv);
	}
	avFrees++;
}

static __inline__
void copyShadowValue(ShadowValue* newSv, ShadowValue* sv) {
	tl_assert(newSv != NULL && sv != NULL);

	if (clo_simulateOriginal) {
		mpfr_set_prec(newSv->value, mpfr_get_prec(sv->value));
	}

	mpfr_set(newSv->value, sv->value, STD_RND);
	newSv->opCount = sv->opCount;
	newSv->origin = sv->origin;
	newSv->canceled = sv->canceled;
	newSv->cancelOrigin = sv->cancelOrigin;
	newSv->orgType = Ot_INVALID;

	/* Do not overwrite active or version!
	   They should be set before. */
}

static __inline__
ShadowValue* getTemp(IRTemp tmp) {
	tl_assert(tmp >= 0 && tmp < MAX_TEMPS);

	if (localTemps[tmp] && localTemps[tmp]->version == sbExecuted) {
		return localTemps[tmp];
	} else {
		return NULL;
	}
}

static __inline__
ShadowValue* setTemp(IRTemp tmp) {
	tl_assert(tmp >= 0 && tmp < MAX_TEMPS);

	if (localTemps[tmp]) {
		localTemps[tmp]->active = True;
	} else {
		localTemps[tmp] = initShadowValue((UWord)tmp);
	}
	localTemps[tmp]->version = sbExecuted;

	return localTemps[tmp];
}

static void updateMeanValue(UWord key, IROp op, mpfr_t* shadow, mpfr_exp_t canceled, Addr arg1, Addr arg2, UInt cancellationBadness) {
	if (mpfr_cmp_ui(meanOrg, 0) != 0 || mpfr_cmp_ui(*shadow, 0) != 0) {
		mpfr_reldiff(meanRelError, *shadow, meanOrg, STD_RND);
		mpfr_abs(meanRelError, meanRelError, STD_RND);
	} else {
		mpfr_set_ui(meanRelError, 0, STD_RND);
	}

	MeanValue* val = VG_(HT_lookup)(meanValues, key);
	if (val == NULL) {
		val = VG_(malloc)("fd.updateMeanValue.1", sizeof(MeanValue));
		val->key = key;
		val->op = op;
		val->count = 1;
		val->visited = False;
		val->overflow = False;
		mpfr_init_set(val->sum, meanRelError, STD_RND);
		mpfr_init_set(val->max, meanRelError, STD_RND);
		val->canceledSum = canceled;
		val->canceledMax = canceled;
		val->cancellationBadnessSum = cancellationBadness;
		val->cancellationBadnessMax = cancellationBadness;
		VG_(HT_add_node)(meanValues, val);
		val->arg1 = arg1;
		val->arg2 = arg2;
	} else {
		val->count++;
		mpfr_add(val->sum, val->sum, meanRelError, STD_RND);

		mpfr_exp_t oldSum = val->canceledSum;
		val->canceledSum += canceled;
		/* check for overflow */
		if (oldSum > val->canceledSum) {
			val->overflow = True;
		}

		val->cancellationBadnessSum += cancellationBadness;

		if (mpfr_cmp(meanRelError, val->max) > 0) {
			mpfr_set(val->max, meanRelError, STD_RND);
			val->arg1 = arg1;
			val->arg2 = arg2;
		}

		if (canceled > val->canceledMax) {
			val->canceledMax = canceled;
		}

		if (cancellationBadness > val->cancellationBadnessMax) {
			val->cancellationBadnessMax = cancellationBadness;
		}
	}
}

static void stageClearVals(VgHashTable* t) {
	if (t == NULL) {
		return;
	}

	VG_(HT_ResetIter)(t);
	StageValue* next;
	while (next = VG_(HT_Next)(t)) {
		mpfr_clears(next->val, next->relError, NULL);
	}
	VG_(HT_destruct)(t, VG_(free));
}

static void stageStart(Int num) {
	tl_assert(num < MAX_STAGES);

	if (stages[num]) {
		tl_assert(!stages[num]->active);
		stages[num]->active = True;
		stages[num]->count++;
	} else {
		stages[num] = VG_(malloc)("fd.stageStart.1", sizeof(Stage));
		stages[num]->active = True;
		stages[num]->count = 1;
		stages[num]->oldVals = NULL;
		stages[num]->limits = VG_(HT_construct)("Stage limits");
	}
	stages[num]->newVals = VG_(HT_construct)("Stage values");
	activeStages++;
}

static void stageEnd(Int num) {
	tl_assert(stages[num]);
	tl_assert(stages[num]->active);

	Int mateCount = -1;

	if (stages[num]->newVals && stages[num]->oldVals) {
		mateCount = 0;

		VG_(HT_ResetIter)(stages[num]->newVals);
		StageValue* next;
		StageValue* mate;

		while (next = VG_(HT_Next)(stages[num]->newVals)) {
			mate = VG_(HT_lookup)(stages[num]->oldVals, next->key);
			if (!mate) {
				VG_(dmsg)("no mate: %d\n", num);
				continue;
			}

			mateCount++;
			StageLimit* sl = VG_(HT_lookup)(stages[num]->limits, next->key);

			mpfr_sub(stageDiff, mate->relError, next->relError, STD_RND);
			mpfr_abs(stageDiff, stageDiff, STD_RND);

			HChar mpfrBuf[MPFR_BUFSIZE];
			if (sl) {
				if (mpfr_cmp(stageDiff, sl->limit) > 0) {
					mpfrToString(mpfrBuf, &(sl->limit));
					mpfrToString(mpfrBuf, &(stageDiff));

					/* adjust limit for the following iterations */
					mpfr_set(sl->limit, stageDiff, STD_RND);

					/* create stage report */
					StageReport* report = NULL;
					if (stageReports[num]) {
						report = VG_(HT_lookup)(stageReports[num], next->key);
					} else {
						stageReports[num] = VG_(HT_construct)("Stage reports");
					}
					if (report) {
						report->count++;
						report->iterMax = stages[num]->count;
					} else {
						report = VG_(malloc)("fd.stageEnd.1", sizeof(StageReport));
						report->key = next->key;
						report->count = 1;
						report->iterMin = stages[num]->count;
						report->iterMax = stages[num]->count;
						report->origin = 0;
						ShadowValue* sv = VG_(HT_lookup)(globalMemory, next->key);
						if (sv) {
							report->origin = sv->origin;
						}
						VG_(HT_add_node)(stageReports[num], report);
					}
				}
			} else {
				sl = VG_(malloc)("fd.stageEnd.1", sizeof(StageLimit));
				sl->key = next->key;
				mpfr_init_set(sl->limit, stageDiff, STD_RND);
				VG_(HT_add_node)(stages[num]->limits, sl);
			}
		}
	}

	stages[num]->active = False;
	stageClearVals(stages[num]->oldVals);
	stages[num]->oldVals = stages[num]->newVals;
	stages[num]->newVals = NULL;
	activeStages--;
}

static void updateStages(Addr addr, Bool isFloat) {
	if (isFloat) {
		Float f = *(Float*)addr;
		mpfr_set_flt(stageOrg, f, STD_RND);
	} else {
		Double d = *(Double*)addr;
		mpfr_set_d(stageOrg, d, STD_RND);
	}
	ShadowValue* svalue = VG_(HT_lookup)(globalMemory, addr);

	if (svalue && svalue->active) {
		mpfr_sub(stageDiff, svalue->value, stageOrg, STD_RND);

		if (mpfr_cmp_ui(svalue->value, 0) != 0 || mpfr_cmp_ui(stageOrg, 0) != 0) {
			mpfr_reldiff(stageRelError, svalue->value, stageOrg, STD_RND);
			mpfr_abs(stageRelError, stageRelError, STD_RND);
		} else {
			mpfr_set_ui(stageRelError, 0, STD_RND);
		}

		Int i;
		for (i = 0; i < MAX_STAGES; i++) {
			if (!stages[i] || !(stages[i]->active) || !(stages[i]->newVals)) {
				continue;
			}

			StageValue* sv = VG_(HT_lookup)(stages[i]->newVals, addr);
			if (sv) {
				if (mpfr_cmpabs(stageRelError, sv->relError) > 0) {
					mpfr_set(sv->val, svalue->value, STD_RND);
					mpfr_set(sv->relError, stageRelError, STD_RND);
				}
			} else {
				sv = VG_(malloc)("fd.updateStages.1", sizeof(StageValue));
				sv->key = addr;
				mpfr_init_set(sv->val, svalue->value, STD_RND);
				mpfr_init_set(sv->relError, stageRelError, STD_RND);
				VG_(HT_add_node)(stages[i]->newVals, sv);
			}
		}
	}
}

static void stageClear(Int num) {
	if (stages[num] == NULL) {
		return;
	}
	stageClearVals(stages[num]->oldVals);
	stageClearVals(stages[num]->newVals);
	if (stages[num]->limits != NULL) {
		VG_(HT_ResetIter)(stages[num]->limits);
		StageLimit* next;
		while ( (next = VG_(HT_Next)(stages[num]->limits)) ) {
			mpfr_clear(next->limit);
		}
		VG_(HT_destruct)(stages[num]->limits, VG_(free));
	}
	stages[num] = NULL;
}

static void writeSConst(IRSB* sb, IRConst* c, Int num) {
	IRConstTag tag = c->tag;

	IRExpr* addr = NULL;
	switch (tag) {
		case Ico_F64:
			addr = mkU64(&(sConst[num]->Val.F64));
			break;
		case Ico_V128:
			addr = mkU64(&(sConst[num]->Val.V128));
			break;
		default:
			break;
	}

	if (addr) {
		IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(sConst[num]->tag)), mkU32(tag));
		addStmtToIRSB(sb, store);

		IRExpr* expr_const = IRExpr_Const(c);
		store = IRStmt_Store(Iend_LE, addr, expr_const);
		addStmtToIRSB(sb, store);
	} else {
		VG_(tool_panic)("Unhandled case in writeSConst\n");
	}
}

static __inline__
void readSConst(Int num, mpfr_t* fp) {
	Int i;
	ULong v128 = 0;
	Double* db;

	switch (sConst[num]->tag) {
		case Ico_F64:
			mpfr_set_d(*fp, (Double)sConst[num]->Val.F64, STD_RND);
			break;
		case Ico_V128:
			/* 128-bit restricted vector constant with 1 bit (repeated 8 times)
			   for each of the 16 1-byte lanes */
			for (i = 7; i >= 0; i--) {
				if ((sConst[num]->Val.V128 >> (i + 8)) & 1) {
					v128 &= 0xFF;
				}
				v128 <<= 8;
			}
			db = &v128;
			mpfr_set_d(*fp, *db, STD_RND);
			break;
		default:
			VG_(tool_panic)("Unhandled case in readSConst\n");
			break;
	}
}

static void writeSTemp(IRSB* sb, IRTypeEnv* env, IRTemp tmp, Int num) {
	IRType type = typeOfIRTemp(env, tmp);
	IRExpr* addr = NULL;
	switch (type) {
		case Ity_F32:
			addr = mkU64(&(sTmp[num]->Val.F32));
			break;
		case Ity_F64:
			addr = mkU64(&(sTmp[num]->Val.F64));
			break;
		case Ity_V128:
			addr = mkU64(sTmp[num]->U128);
			break;
		default:
			break;
	}

	if (addr) {
		IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(sTmp[num]->type)), mkU32(type));
		addStmtToIRSB(sb, store);

		IRExpr* rdTmp = IRExpr_RdTmp(tmp);
		store = IRStmt_Store(Iend_LE, addr, rdTmp);
		addStmtToIRSB(sb, store);
	} else {
		VG_(tool_panic)("Unhandled case in writeSTemp\n");
	}
}

static __inline__
void readSTemp(Int num, mpfr_t* fp) {
	IRType type = sTmp[num]->type;
	switch (type) {
		case Ity_F32:
			if (clo_simulateOriginal) mpfr_set_prec(*fp, 24);
			mpfr_set_flt(*fp, sTmp[num]->Val.F32, STD_RND);
			break;
		case Ity_F64:
			if (clo_simulateOriginal) mpfr_set_prec(*fp, 53);
			mpfr_set_d(*fp, sTmp[num]->Val.F64, STD_RND);
			break;
		case Ity_V128:
			/* Not a general solution, because this does not work if vectors are used
			   e.g. two/four additions with one SSE instruction */
			if (sTmp[num]->U128[1] == 0) {
				if (clo_simulateOriginal) mpfr_set_prec(*fp, 24);

				Float* flp = &(sTmp[num]->U128[0]);
				mpfr_set_flt(*fp, *flp, STD_RND);
			} else {
				if (clo_simulateOriginal) mpfr_set_prec(*fp, 53);

				ULong ul = sTmp[num]->U128[1];
				ul <<= 32;
				ul |= sTmp[num]->U128[0];
				Double* db = &ul;
				mpfr_set_d(*fp, *db, STD_RND);
			}
			break;
		default:
			VG_(tool_panic)("Unhandled case in readSTemp\n");
			break;
	}
}

static Bool isOpFloat(IROp op) {
	switch (op) {
		/* unary float */
		case Iop_Sqrt32F0x4:
		case Iop_NegF32:
		case Iop_AbsF32:
		/* binary float */
		case Iop_Add32F0x4:
		case Iop_Sub32F0x4:
		case Iop_Mul32F0x4:
		case Iop_Div32F0x4:
		case Iop_Min32F0x4:
		case Iop_Max32F0x4:
			return True;
		/* unary double */
		case Iop_Sqrt64F0x2:
		case Iop_NegF64:
		case Iop_AbsF64:
		/* binary double */
		case Iop_Add64F0x2:
		case Iop_Sub64F0x2:
		case Iop_Mul64F0x2:
		case Iop_Div64F0x2:
		case Iop_Min64F0x2:
		case Iop_Max64F0x2:
		/* ternary double */
		case Iop_AddF64:
		case Iop_SubF64:
		case Iop_MulF64:
		case Iop_DivF64:
			return False;
		default:
			// TODO warn
			//VG_(tool_panic)("Unhandled operation in isOpFloat\n");
			return False;
	}
}

static VG_REGPARM(2) void processUnOp(Addr addr, UWord ca) {
	if (!clo_analyze) return;

	Int constArgs = (Int)ca;
	ULong argOpCount = 0;
	Addr argOrigin = 0;
	mpfr_exp_t argCanceled = 0;
	Addr argCancelOrigin = 0;

	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(arg1tmpX, 24);
		} else {
			mpfr_set_prec(arg1tmpX, 53);
		}
	}

	if (constArgs & 0x1) {
		readSConst(0, &(arg1tmpX));
	} else {
		ShadowValue* argTmp = getTemp(unOpArgs->arg);
		if (argTmp) {
			mpfr_set(arg1tmpX, argTmp->value, STD_RND);
			argOpCount = argTmp->opCount;
			argOrigin = argTmp->origin;
			argCanceled = argTmp->canceled;
			argCancelOrigin = argTmp->cancelOrigin;
		} else {
			readSTemp(0, &(arg1tmpX));
		}
	}

	ShadowValue* res = setTemp(unOpArgs->wrTmp);
	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(res->value, 24);
		} else {
			mpfr_set_prec(res->value, 53);
		}
	}
	res->opCount = argOpCount + 1;
	res->origin = addr;

	fpOps++;

	IROp op = unOpArgs->op;
	switch (op) {
		case Iop_Sqrt32F0x4:
		case Iop_Sqrt64F0x2:
			mpfr_sqrt(res->value, arg1tmpX, STD_RND);
			break;
		case Iop_NegF32:
		case Iop_NegF64:
			mpfr_neg(res->value, arg1tmpX, STD_RND);
			break;
		case Iop_AbsF32:
		case Iop_AbsF64:
			mpfr_abs(res->value, arg1tmpX, STD_RND);
			break;
		default:
			VG_(tool_panic)("Unhandled case in processUnOp\n");
			break;
	}

	res->canceled = argCanceled;
	res->cancelOrigin = argCancelOrigin;

	if (clo_computeMeanValue) {
		if (isOpFloat(unOpArgs->op)) {
			mpfr_set_flt(meanOrg, unOpArgs->orgFloat, STD_RND);
		} else {
			mpfr_set_d(meanOrg, unOpArgs->orgDouble, STD_RND);
		}
		updateMeanValue(addr, unOpArgs->op, &(res->value), 0, argOrigin, 0, 0);
	}
}

static void instrumentUnOp(IRSB* sb, IRTypeEnv* env, Addr addr, IRTemp wrTemp, IRExpr* unop, Int argTmpInstead) {
	tl_assert(unop->tag == Iex_Unop);

	if (clo_ignoreLibraries && isInLibrary(addr)) {
		return;
	}

	IRExpr* arg = unop->Iex.Unop.arg;
	tl_assert(arg->tag == Iex_RdTmp || arg->tag == Iex_Const);

	IROp op = unop->Iex.Unop.op;
	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->op)), mkU32(op));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->wrTmp)), mkU32(wrTemp));
	addStmtToIRSB(sb, store);

	Int constArgs = 0;

	if (arg->tag == Iex_RdTmp) {
		if (argTmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->arg)), mkU32(argTmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->arg)), mkU32(unop->Iex.Unop.arg->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);

		writeSTemp(sb, env, arg->Iex.RdTmp.tmp, 0);
	} else {
		tl_assert(arg->tag == Iex_Const);
		writeSConst(sb, arg->Iex.Const.con, 0);
		constArgs |= 0x1;
	}

	if (isOpFloat(op)) {
		store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->orgFloat)), IRExpr_RdTmp(wrTemp));
		addStmtToIRSB(sb, store);
	} else {
		store = IRStmt_Store(Iend_LE, mkU64(&(unOpArgs->orgDouble)), IRExpr_RdTmp(wrTemp));
		addStmtToIRSB(sb, store);
	}

	IRExpr** argv = mkIRExprVec_2(mkU64(addr), mkU64(constArgs));
	IRDirty* di = unsafeIRDirty_0_N(2, "processUnOp", VG_(fnptr_to_fnentry)(&processUnOp), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(2) void processBinOp(Addr addr, UWord ca) {
	if (!clo_analyze) return;

	Int constArgs = (Int)ca;

	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(arg1tmpX, 24);
			mpfr_set_prec(arg2tmpX, 24);
		} else {
			mpfr_set_prec(arg1tmpX, 53);
			mpfr_set_prec(arg2tmpX, 53);
		}
	}

	ULong arg1opCount = 0;
	ULong arg2opCount = 0;
	Addr arg1origin = 0;
	Addr arg2origin = 0;
	mpfr_exp_t arg1canceled = 0;
	mpfr_exp_t arg2canceled = 0;
	mpfr_exp_t canceled = 0;
	Addr arg1CancelOrigin = 0;
	Addr arg2CancelOrigin = 0;

	Int exactBitsArg1, exactBitsArg2;
	if (isOpFloat(binOpArgs->op)) {
		exactBitsArg1 = 23;
		exactBitsArg2 = 23;
	} else {
		exactBitsArg1 = 52;
		exactBitsArg2 = 52;
	}

	if (constArgs & 0x1) {
		readSConst(0, &(arg1tmpX));
	} else {
		ShadowValue* arg1tmp = getTemp(binOpArgs->arg1);
		if (arg1tmp) {
			mpfr_set(arg1tmpX, arg1tmp->value, STD_RND);
			arg1opCount = arg1tmp->opCount;
			arg1origin = arg1tmp->origin;
			arg1canceled = arg1tmp->canceled;
			arg1CancelOrigin = arg1tmp->cancelOrigin;

			if (clo_bad_cancellations) {
				readSTemp(0, &cancelTemp);
				if (mpfr_get_exp(cancelTemp) == mpfr_get_exp(arg1tmpX)) {
					mpfr_sub(cancelTemp, arg1tmpX, cancelTemp, STD_RND);
					if (mpfr_cmp_ui(cancelTemp, 0) != 0) {
						exactBitsArg1 = abs(mpfr_get_exp(arg1tmpX) - mpfr_get_exp(cancelTemp)) - 2;
						if (arg1tmp->orgType == Ot_FLOAT && exactBitsArg1 > 23) {
							exactBitsArg1 = 23;
						} else if (arg1tmp->orgType == Ot_DOUBLE && exactBitsArg1 > 52) {
							exactBitsArg1 = 52;
						}
					}
				} else {
					exactBitsArg1 = 0;
				}
			}
		} else {
			readSTemp(0, &(arg1tmpX));
		}
	}

	if (constArgs & 0x2) {
		readSConst(1, &(arg2tmpX));
	} else {
		ShadowValue* arg2tmp = getTemp(binOpArgs->arg2);
		if (arg2tmp) {
			mpfr_set(arg2tmpX, arg2tmp->value, STD_RND);
			arg2opCount = arg2tmp->opCount;
			arg2origin = arg2tmp->origin;
			arg2canceled = arg2tmp->canceled;
			arg2CancelOrigin = arg2tmp->cancelOrigin;

			if (clo_bad_cancellations) {
				readSTemp(1, &cancelTemp);
				if (mpfr_get_exp(cancelTemp) == mpfr_get_exp(arg2tmpX)) {
					mpfr_sub(cancelTemp, arg2tmpX, cancelTemp, STD_RND);
					if (mpfr_cmp_ui(cancelTemp, 0) != 0) {
						exactBitsArg2 = abs(mpfr_get_exp(arg2tmpX) - mpfr_get_exp(cancelTemp)) - 2;
						if (arg2tmp->orgType == Ot_FLOAT && exactBitsArg2 > 23) {
							exactBitsArg2 = 23;
						} else if (arg2tmp->orgType == Ot_DOUBLE && exactBitsArg2 > 52) {
							exactBitsArg2 = 52;
						}
					}
				} else {
					exactBitsArg2 = 0;
				}
			}
		} else {
			readSTemp(1, &(arg2tmpX));
		}
	}

	ShadowValue* res = setTemp(binOpArgs->wrTmp);
	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(res->value, 24);
		} else {
			mpfr_set_prec(res->value, 53);
		}
	}
	res->opCount = 1;
	if (arg1opCount > arg2opCount) {
		res->opCount += arg1opCount;
	} else {
		res->opCount += arg2opCount;
	}
	res->origin = addr;

	fpOps++;

	switch (binOpArgs->op) {
		case Iop_Add32F0x4:
		case Iop_Add64F0x2:
			mpfr_add(res->value, arg1tmpX, arg2tmpX, STD_RND);
			canceled = getCanceledBits(&(res->value), &(arg1tmpX), &(arg2tmpX));
			break;
		case Iop_Sub32F0x4:
		case Iop_Sub64F0x2:
			mpfr_sub(res->value, arg1tmpX, arg2tmpX, STD_RND);
			canceled = getCanceledBits(&(res->value), &(arg1tmpX), &(arg2tmpX));
			break;
		case Iop_Mul32F0x4:
		case Iop_Mul64F0x2:
			mpfr_mul(res->value, arg1tmpX, arg2tmpX, STD_RND);
			break;
		case Iop_Div32F0x4:
		case Iop_Div64F0x2:
			mpfr_div(res->value, arg1tmpX, arg2tmpX, STD_RND);
			break;
		case Iop_Min32F0x4:
		case Iop_Min64F0x2:
			mpfr_min(res->value, arg1tmpX, arg2tmpX, STD_RND);
			break;
		case Iop_Max32F0x4:
		case Iop_Max64F0x2:
			mpfr_max(res->value, arg1tmpX, arg2tmpX, STD_RND);
			break;
		default:
			VG_(tool_panic)("Unhandled case in processBinOp\n");
			break;
	}

	mpfr_exp_t maxC = canceled;
	Addr maxCorigin = addr;
	if (arg1canceled > maxC) {
		maxC = arg1canceled;
		maxCorigin = arg1CancelOrigin;
	}
	if (arg2canceled > maxC) {
		maxC = arg2canceled;
		maxCorigin = arg2CancelOrigin;
	}
	res->canceled = maxC;
	res->cancelOrigin = maxCorigin;

	if (clo_computeMeanValue) {
		UInt cancellationBadness = 0;
		if (clo_bad_cancellations && canceled > 0) {
			Int exactBits = exactBitsArg1 < exactBitsArg2 ? exactBitsArg1 : exactBitsArg2;
			if (canceled > exactBits) {
				cancellationBadness = canceled - exactBits;
			}
		}

		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_flt(meanOrg, binOpArgs->orgFloat, STD_RND);
		} else {
			mpfr_set_d(meanOrg, binOpArgs->orgDouble, STD_RND);
		}
		updateMeanValue(addr, binOpArgs->op, &(res->value), canceled, arg1origin, arg2origin, cancellationBadness);
	}
}

static void instrumentBinOp(IRSB* sb, IRTypeEnv* env, Addr addr, IRTemp wrTemp, IRExpr* binop, Int arg1tmpInstead, Int arg2tmpInstead) {
	tl_assert(binop->tag == Iex_Binop);

	if (clo_ignoreLibraries && isInLibrary(addr)) {
		return;
	}

	IROp op = binop->Iex.Binop.op;
	IRExpr* arg1 = binop->Iex.Binop.arg1;
	IRExpr* arg2 = binop->Iex.Binop.arg2;

	tl_assert(arg1->tag == Iex_RdTmp || arg1->tag == Iex_Const);
	tl_assert(arg2->tag == Iex_RdTmp || arg2->tag == Iex_Const);

	Int constArgs = 0;

	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->op)), mkU32(op));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->wrTmp)), mkU32(wrTemp));
	addStmtToIRSB(sb, store);

	if (arg1->tag == Iex_RdTmp) {
		if (arg1tmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->arg1)), mkU32(arg1tmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->arg1)), mkU32(arg1->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);

		writeSTemp(sb, env, arg1->Iex.RdTmp.tmp, 0);
	} else {
		tl_assert(arg1->tag == Iex_Const);

		writeSConst(sb, arg1->Iex.Const.con, 0);
		constArgs |= 0x1;
	}
	if (arg2->tag == Iex_RdTmp) {
		if (arg2tmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->arg2)), mkU32(arg2tmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->arg2)), mkU32(arg2->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);

		writeSTemp(sb, env, arg2->Iex.RdTmp.tmp, 1);
	} else {
		tl_assert(arg2->tag == Iex_Const);

		writeSConst(sb, arg2->Iex.Const.con, 1);
		constArgs |= 0x2;
	}

	if (isOpFloat(op)) {
		store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->orgFloat)), IRExpr_RdTmp(wrTemp));
		addStmtToIRSB(sb, store);
	} else {
		store = IRStmt_Store(Iend_LE, mkU64(&(binOpArgs->orgDouble)), IRExpr_RdTmp(wrTemp));
		addStmtToIRSB(sb, store);
	}

	IRExpr** argv = mkIRExprVec_2(mkU64(addr), mkU64(constArgs));
	IRDirty* di = unsafeIRDirty_0_N(2, "processBinOp", VG_(fnptr_to_fnentry)(&processBinOp), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(2) void processTriOp(Addr addr, UWord ca) {
	if (!clo_analyze) return;

	Int constArgs = (Int)ca;
	IROp op = triOpArgs->op;

	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(arg2tmpX, 24);
			mpfr_set_prec(arg3tmpX, 24);
		} else {
			mpfr_set_prec(arg2tmpX, 53);
			mpfr_set_prec(arg3tmpX, 53);
		}
	}

	ULong arg2opCount = 0;
	ULong arg3opCount = 0;
	Addr arg2origin = 0;
	Addr arg3origin = 0;
	mpfr_exp_t arg2canceled = 0;
	mpfr_exp_t arg3canceled = 0;
	mpfr_exp_t canceled = 0;
	Addr arg2CancelOrigin = 0;
	Addr arg3CancelOrigin = 0;

	Int exactBitsArg2, exactBitsArg3;
	if (isOpFloat(binOpArgs->op)) {
		exactBitsArg2 = 23;
		exactBitsArg3 = 23;
	} else {
		exactBitsArg2 = 52;
		exactBitsArg3 = 52;
	}

	if (constArgs & 0x2) {
		readSConst(1, &(arg2tmpX));
	} else {
		ShadowValue* arg2tmp = getTemp(triOpArgs->arg2);
		if (arg2tmp) {
			mpfr_set(arg2tmpX, arg2tmp->value, STD_RND);
			arg2opCount = arg2tmp->opCount;
			arg2origin = arg2tmp->origin;
			arg2canceled = arg2tmp->canceled;
			arg2CancelOrigin = arg2tmp->cancelOrigin;

			if (clo_bad_cancellations) {
				readSTemp(1, &cancelTemp);
				if (mpfr_get_exp(cancelTemp) == mpfr_get_exp(arg2tmpX)) {
					mpfr_sub(cancelTemp, arg2tmpX, cancelTemp, STD_RND);
					if (mpfr_cmp_ui(cancelTemp, 0) != 0) {
						exactBitsArg2 = abs(mpfr_get_exp(arg2tmpX) - mpfr_get_exp(cancelTemp)) - 2;
						if (arg2tmp->orgType == Ot_FLOAT && exactBitsArg2 > 23) {
							exactBitsArg2 = 23;
						} else if (arg2tmp->orgType == Ot_DOUBLE && exactBitsArg2 > 52) {
							exactBitsArg2 = 52;
						}
					}
				} else {
					exactBitsArg2 = 0;
				}
			}
		} else {
			readSTemp(1, &(arg2tmpX));
		}
	}

	if (constArgs & 0x4) {
		readSConst(2, &(arg3tmpX));
	} else {
		ShadowValue* arg3tmp = getTemp(triOpArgs->arg3);
		if (arg3tmp) {
			mpfr_set(arg3tmpX, arg3tmp->value, STD_RND);
			arg3opCount = arg3tmp->opCount;
			arg3origin = arg3tmp->origin;
			arg3canceled = arg3tmp->canceled;
			arg3CancelOrigin = arg3tmp->cancelOrigin;

			if (clo_bad_cancellations) {
				readSTemp(2, &cancelTemp);
				if (mpfr_get_exp(cancelTemp) == mpfr_get_exp(arg3tmpX)) {
					mpfr_sub(cancelTemp, arg3tmpX, cancelTemp, STD_RND);
					if (mpfr_cmp_ui(cancelTemp, 0) != 0) {
						exactBitsArg3 = abs(mpfr_get_exp(arg3tmpX) - mpfr_get_exp(cancelTemp)) - 2;
						if (arg3tmp->orgType == Ot_FLOAT && exactBitsArg3 > 23) {
							exactBitsArg3 = 23;
						} else if (arg3tmp->orgType == Ot_DOUBLE && exactBitsArg3 > 52) {
							exactBitsArg3 = 52;
						}
					}
				} else {
					exactBitsArg3 = 0;
				}
			}
		} else {
			readSTemp(2, &(arg3tmpX));
		}
	}

	ShadowValue* res = setTemp(triOpArgs->wrTmp);
	if (clo_simulateOriginal) {
		if (isOpFloat(binOpArgs->op)) {
			mpfr_set_prec(res->value, 24);
		} else {
			mpfr_set_prec(res->value, 53);
		}
	}
	res->opCount = 1;
	if (arg2opCount > arg3opCount) {
		res->opCount += arg2opCount;
	} else {
		res->opCount += arg3opCount;
	}
	res->origin = addr;

	fpOps++;

	switch (op) {
		case Iop_AddF64:
			mpfr_add(res->value, arg2tmpX, arg3tmpX, STD_RND);
			canceled = getCanceledBits(&(res->value), &(arg2tmpX), &(arg3tmpX));
			break;
		case Iop_SubF64:
			mpfr_sub(res->value, arg2tmpX, arg3tmpX, STD_RND);
			canceled = getCanceledBits(&(res->value), &(arg2tmpX), &(arg3tmpX));
			break;
		case Iop_MulF64:
			mpfr_mul(res->value, arg2tmpX, arg3tmpX, STD_RND);
			break;
		case Iop_DivF64:
			mpfr_div(res->value, arg2tmpX, arg3tmpX, STD_RND);
			break;
		default:
			VG_(tool_panic)("Unhandled case in processTriOp");
			break;
	}

	mpfr_exp_t maxC = canceled;
	Addr maxCorigin = addr;
	if (arg2canceled > maxC) {
		maxC = arg2canceled;
		maxCorigin = arg2CancelOrigin;
	}
	if (arg3canceled > maxC) {
		maxC = arg3canceled;
		maxCorigin = arg3CancelOrigin;
	}
	res->canceled = maxC;
	res->cancelOrigin = maxCorigin;

	if (clo_computeMeanValue) {
		UInt cancellationBadness = 0;
		if (clo_bad_cancellations && canceled > 0) {
			Int exactBits = exactBitsArg2 < exactBitsArg3 ? exactBitsArg2 : exactBitsArg3;
			if (canceled > exactBits) {
				cancellationBadness = canceled - exactBits;
			}
		}

		mpfr_set_d(meanOrg, triOpArgs->orgDouble, STD_RND);
		updateMeanValue(addr, op, &(res->value), canceled, arg2origin, arg3origin, cancellationBadness);
	}
}

static void instrumentTriOp(IRSB* sb, IRTypeEnv* env, Addr addr, IRTemp wrTemp, IRExpr* triop, Int arg2tmpInstead, Int arg3tmpInstead) {
	tl_assert(triop->tag == Iex_Triop);

	if (clo_ignoreLibraries && isInLibrary(addr)) {
		return;
	}

	IROp op = triop->Iex.Triop.details->op;
	IRExpr* arg1 = triop->Iex.Triop.details->arg1;
	IRExpr* arg2 = triop->Iex.Triop.details->arg2;
	IRExpr* arg3 = triop->Iex.Triop.details->arg3;

	tl_assert(arg1->tag == Iex_Const);
	tl_assert(arg2->tag == Iex_RdTmp || arg2->tag == Iex_Const);
	tl_assert(arg3->tag == Iex_RdTmp || arg3->tag == Iex_Const);

	Int constArgs = 0;

	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->op)), mkU32(op));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->wrTmp)), mkU32(wrTemp));
	addStmtToIRSB(sb, store);

	/* arg1 is ignored because it only contains the rounding mode for
	   the operations instructed at the moment */

	if (arg2->tag == Iex_RdTmp) {
		if (arg2tmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->arg2)), mkU32(arg2tmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->arg2)), mkU32(triop->Iex.Triop.details->arg2->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);
		writeSTemp(sb, env, triop->Iex.Triop.details->arg2->Iex.RdTmp.tmp, 1);
	} else {
		tl_assert(arg2->tag == Iex_Const);
		writeSConst(sb, arg2->Iex.Const.con, 1);
		constArgs |= 0x2;
	}

	if (arg3->tag == Iex_RdTmp) {
		if (arg3tmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->arg3)), mkU32(arg3tmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->arg3)), mkU32(triop->Iex.Triop.details->arg3->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);
		writeSTemp(sb, env, triop->Iex.Triop.details->arg3->Iex.RdTmp.tmp, 2);
	} else {
		tl_assert(arg3->tag == Iex_Const);
		writeSConst(sb, arg3->Iex.Const.con, 2);
		constArgs |= 0x4;
	}

	store = IRStmt_Store(Iend_LE, mkU64(&(triOpArgs->orgDouble)), IRExpr_RdTmp(wrTemp));
	addStmtToIRSB(sb, store);

	IRExpr** argv = mkIRExprVec_2(mkU64(addr), mkU64(constArgs));
	IRDirty* di = unsafeIRDirty_0_N(2, "processTriOp", VG_(fnptr_to_fnentry)(&processTriOp), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(1) void processITE(UWord ca) {
	if (!clo_analyze) return;

	Int constArgs = (Int)ca;
	ShadowValue *aexpr0 = NULL;
	ShadowValue *aexprX = NULL;

	if (constArgs & 0x2) {
		if (!muxArgs->condVal) {
			return;
		}
	} else {
		aexpr0 = getTemp(muxArgs->expr0);
		if (!aexpr0 && !muxArgs->condVal) {
			return;
		}
	}

	if (constArgs & 0x4) {
		if (muxArgs->condVal) {
			return;
		}
	} else {
		aexprX = getTemp(muxArgs->exprX);
		if (!aexprX && muxArgs->condVal) {
			return;
		}
	}

	ShadowValue* res = setTemp(muxArgs->wrTmp);
	if (muxArgs->condVal) {
		copyShadowValue(res, aexprX);
	} else {
		copyShadowValue(res, aexpr0);
	}
}

static void instrumentITE(IRSB* sb, IRTypeEnv* env, IRTemp wrTemp, IRExpr* mux, Int arg0tmpInstead, Int argXtmpInstead) {
	tl_assert(mux->tag == Iex_ITE);

	IRExpr* cond = mux->Iex.ITE.cond;
	IRExpr* expr0 = mux->Iex.ITE.iftrue;
	IRExpr* exprX = mux->Iex.ITE.iffalse;

	tl_assert(cond->tag == Iex_RdTmp);
	tl_assert(expr0->tag == Iex_RdTmp || expr0->tag == Iex_Const);
	tl_assert(exprX->tag == Iex_RdTmp || exprX->tag == Iex_Const);

	Int constArgs = 0;
	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->wrTmp)), mkU32(wrTemp));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->condVal)), mkU32(mux->Iex.ITE.cond));
	addStmtToIRSB(sb, store);

	if (expr0->tag == Iex_RdTmp) {
		if (arg0tmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->expr0)), mkU32(arg0tmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->expr0)), mkU32(mux->Iex.ITE.iftrue->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);
	} else {
		constArgs |= 0x2;
	}

	if (exprX->tag == Iex_RdTmp) {
		if (argXtmpInstead >= 0) {
			store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->exprX)), mkU32(argXtmpInstead));
		} else {
			store = IRStmt_Store(Iend_LE, mkU64(&(muxArgs->exprX)), mkU32(mux->Iex.ITE.iffalse->Iex.RdTmp.tmp));
		}
		addStmtToIRSB(sb, store);
	} else {
		constArgs |= 0x4;
	}

	IRExpr** argv = mkIRExprVec_1(mkU64(constArgs));
	IRDirty* di = unsafeIRDirty_0_N(1, "processITE", VG_(fnptr_to_fnentry)(&processITE), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(2) void processLoad(UWord tmp, Addr addr) {
	if (!clo_analyze) return;

	/* check if this memory address is shadowed */
	ShadowValue* av = VG_(HT_lookup)(globalMemory, addr);
	if (!av || !(av->active)) {
		return;
	}
	ShadowValue* res = setTemp(tmp);
	copyShadowValue(res, av);
}

static void instrumentLoad(IRSB* sb, IRTypeEnv* env, IRStmt* wrTmp) {
	tl_assert(wrTmp->tag == Ist_WrTmp);
	tl_assert(wrTmp->Ist.WrTmp.data->tag == Iex_Load);
	IRExpr* load = wrTmp->Ist.WrTmp.data;

	if (load->Iex.Load.addr->tag != Iex_RdTmp) {
		return;
	}

	IRExpr** argv = mkIRExprVec_2(mkU64(wrTmp->Ist.WrTmp.tmp), load->Iex.Load.addr);
	IRDirty* di = unsafeIRDirty_0_N(2, "processLoad", VG_(fnptr_to_fnentry)(&processLoad), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(3) void processStore(Addr addr, UWord t, UWord isFloat) {
	Int tmp = (Int)t;
	ShadowValue* res = NULL;
	ShadowValue* currentVal = VG_(HT_lookup)(globalMemory, addr);

	if (clo_analyze && tmp >= 0) {
		/* check if this memory address is shadowed */
		ShadowValue* av = getTemp(tmp);
		if (av) {
			if (currentVal) {
				res = currentVal;
				copyShadowValue(res, av);
				res->active = True;
			} else {
				res = initShadowValue((UWord)addr);
				copyShadowValue(res, av);
				VG_(HT_add_node)(globalMemory, res);
			}

			if ((Bool)isFloat) {
				res->orgType = Ot_FLOAT;
			} else {
				res->orgType = Ot_DOUBLE;
			}
			if (res->orgType == Ot_FLOAT) {
				res->Org.fl = storeArgs->orgFloat;
			} else if (res->orgType == Ot_DOUBLE) {
				res->Org.db = storeArgs->orgDouble;
			} else {
				tl_assert(False);
			}

			if (activeStages > 0) {
				updateStages(addr, res->orgType == Ot_FLOAT);
			}
		}
	}

	if (currentVal && !res) {
		currentVal->active = False;
	}
}

static void instrumentStore(IRSB* sb, IRTypeEnv* env, IRStmt* store, Int argTmpInstead) {
	tl_assert(store->tag == Ist_Store);

	Bool isFloat = True;
	IRExpr* data = store->Ist.Store.data;
	if (data->tag == Iex_RdTmp) {
		/* I32 and I64 have to be instrumented due to SSE */
		switch (typeOfIRTemp(env, data->Iex.RdTmp.tmp)) {
			case Ity_I64:
			case Ity_F64:
			case Ity_V128:
				isFloat = False;
				break;
			default:
				break;
		}
	}

	IRExpr* addr = store->Ist.Store.addr;
	/* const needed, but only to delete */
	tl_assert(data->tag == Iex_RdTmp || data->tag == Iex_Const);

	Int num = -1;
	IRType type = Ity_F32;
	if (data->tag != Iex_Const) {
		if (argTmpInstead >= 0) {
			num = argTmpInstead;
		} else {
			num = data->Iex.RdTmp.tmp;
		}
		type = typeOfIRTemp(env, num);

		if (isFloat) {
			IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(storeArgs->orgFloat)), IRExpr_RdTmp(data->Iex.RdTmp.tmp));
			addStmtToIRSB(sb, store);
		} else {
			IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(storeArgs->orgDouble)), IRExpr_RdTmp(data->Iex.RdTmp.tmp));
			addStmtToIRSB(sb, store);
		}
	}

	IRExpr** argv = mkIRExprVec_3(addr, mkU64(num), mkU64(isFloat));
	IRDirty* di = unsafeIRDirty_0_N(3, "processStore", VG_(fnptr_to_fnentry)(&processStore), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(2) void processPut(UWord offset, UWord t) {
	ShadowValue* res = NULL;
	ThreadId tid = VG_(get_running_tid)();
	ShadowValue* currentVal = threadRegisters[tid][offset];

	Int tmp = (Int)t;
	if (clo_analyze && tmp >= 0) {
		/* check if a shadow value exits */
		ShadowValue* av = getTemp(tmp);
		if (av) {
			if (currentVal) {
				/* reuse allocated space if possible ... */
				res = currentVal;
				copyShadowValue(res, av);
			} else {
				/* ... else allocate new space */
				res = initShadowValue((UWord)offset);
				copyShadowValue(res, av);
				threadRegisters[tid][offset] = res;
			}
			res->active = True;
		}
	}

	if (currentVal && !res) {
		/* Invalidate existing shadow value (not free) because
           something was stored in this register */
		currentVal->active = False;
	}
}

static void instrumentPut(IRSB* sb, IRTypeEnv* env, IRStmt* st, Int argTmpInstead) {
	tl_assert(st->tag == Ist_Put);
	IRExpr* data = st->Ist.Put.data;
	tl_assert(data->tag == Iex_RdTmp || data->tag == Iex_Const);

	Int offset = st->Ist.Put.offset;
	tl_assert(offset >= 0 && offset < MAX_REGISTERS);

	Int tmpNum = -1;
	if (data->tag == Iex_RdTmp) {
		if (argTmpInstead >= 0) {
			tmpNum = argTmpInstead;
		} else {
			tmpNum = data->Iex.RdTmp.tmp;
		}
	}

	IRExpr** argv = mkIRExprVec_2(mkU64(offset), mkU64(tmpNum));
	IRDirty* di = unsafeIRDirty_0_N(2, "processPut", VG_(fnptr_to_fnentry)(&processPut), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(2) void processGet(UWord offset, UWord tmp) {
	if (!clo_analyze) return;

	ThreadId tid = VG_(get_running_tid)();
	ShadowValue* av = threadRegisters[tid][offset];
	if (!av || !(av->active)) {
		return;
	}

	ShadowValue* res = setTemp((Int)tmp);
	copyShadowValue(res, av);
}

static void instrumentGet(IRSB* sb, IRTypeEnv* env, IRStmt* st) {
	tl_assert(st->tag == Ist_WrTmp);
	tl_assert(st->Ist.WrTmp.data->tag == Iex_Get);

	Int tmpNum = st->Ist.WrTmp.tmp;
	Int offset = st->Ist.WrTmp.data->Iex.Get.offset;
	tl_assert(offset >= 0 && offset < MAX_REGISTERS);

	IRExpr** argv = mkIRExprVec_2(mkU64(offset), mkU64(tmpNum));
	IRDirty* di = unsafeIRDirty_0_N(2, "processGet", VG_(fnptr_to_fnentry)(&processGet), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(3) void processPutI(UWord t, UWord b, UWord n) {
	Int tmp = (Int)t;
	Int nElems = (Int)n;
	Int base = (Int)b;
	Int bias = (Int)(circRegs->bias);

	/* (ix + bias) % num-of-elems-in-the-array */
	Int offset = base + ((circRegs->ix + bias) % nElems);
	tl_assert(offset >= 0 && offset < MAX_REGISTERS);

	ShadowValue* res = NULL;
	ThreadId tid = VG_(get_running_tid)();
	ShadowValue* currentVal = threadRegisters[tid][offset];

	if (clo_analyze && tmp >= 0) {
		/* check if a shadow value exits */
		ShadowValue* av = getTemp(tmp);
		if (av) {
			if (currentVal) {
				/* reuse allocated space if possible ... */
				res = currentVal;
				copyShadowValue(res, av);
			} else {
				/* ... else allocate new space */
				res = initShadowValue((UWord)offset);
				copyShadowValue(res, av);
				threadRegisters[tid][offset] = res;
			}
			res->active = True;
		}
	}

	if (currentVal && !res) {
		/* Invalidate existing shadow value (not free) because
           something was stored in this register. */
		currentVal->active = False;
	}
}

static void instrumentPutI(IRSB* sb, IRTypeEnv* env, IRStmt* st, Int argTmpInstead) {
	tl_assert(st->tag == Ist_PutI);
	IRExpr* data = st->Ist.PutI.details->data;
	IRExpr* ix = st->Ist.PutI.details->ix;
	IRRegArray* descr = st->Ist.PutI.details->descr;
	Int bias = st->Ist.PutI.details->bias;

	tl_assert(data->tag == Iex_RdTmp || data->tag == Iex_Const);
	tl_assert(ix->tag == Iex_RdTmp || ix->tag == Iex_Const);

	tl_assert(ix->tag == Iex_RdTmp ? typeOfIRTemp(env, ix->Iex.RdTmp.tmp) == Ity_I32 : True);
	tl_assert(ix->tag == Iex_Const ? ix->Iex.Const.con->tag == Ico_U32 : True);

	Int tmpNum = -1;
	if (data->tag == Iex_RdTmp) {
		if (argTmpInstead >= 0) {
			tmpNum = argTmpInstead;
		} else {
			tmpNum = data->Iex.RdTmp.tmp;
		}
	}

	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(circRegs->bias)), mkU64(bias));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(circRegs->ix)), ix);
	addStmtToIRSB(sb, store);

	IRExpr** argv = mkIRExprVec_3(mkU64(tmpNum), mkU64(descr->base), mkU64(descr->nElems));
	IRDirty* di = unsafeIRDirty_0_N(3, "processPutI", VG_(fnptr_to_fnentry)(&processPutI), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static VG_REGPARM(3) void processGetI(UWord tmp, UWord b, UWord n) {
	if (!clo_analyze) return;

	Int nElems = (Int)n;
	Int base = (Int)b;
	Int bias = (Int)(circRegs->bias);

	/* (ix + bias) % num-of-elems-in-the-array */
	Int offset = base + ((circRegs->ix + bias) % nElems);
	tl_assert(offset >= 0 && offset < MAX_REGISTERS);

	ThreadId tid = VG_(get_running_tid)();
	ShadowValue* av = threadRegisters[tid][offset];
	if (!av || !(av->active)) {
		return;
	}

	ShadowValue* res = setTemp((Int)tmp);
	copyShadowValue(res, av);
}

static void instrumentGetI(IRSB* sb, IRTypeEnv* env, IRStmt* st) {
	tl_assert(st->tag == Ist_WrTmp);
	tl_assert(st->Ist.WrTmp.data->tag == Iex_GetI);
	IRExpr* get = st->Ist.WrTmp.data;

	IRExpr* ix = get->Iex.GetI.ix;
	IRRegArray* descr = get->Iex.GetI.descr;
	Int bias = get->Iex.GetI.bias;

	tl_assert(ix->tag == Iex_RdTmp || ix->tag == Iex_Const);
	tl_assert(ix->tag == Iex_RdTmp ? typeOfIRTemp(env, ix->Iex.RdTmp.tmp) == Ity_I32 : True);
	tl_assert(ix->tag == Iex_Const ? ix->Iex.Const.con->tag == Ico_U32 : True);

	Int tmpNum = st->Ist.WrTmp.tmp;

	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&(circRegs->bias)), mkU64(bias));
	addStmtToIRSB(sb, store);
	store = IRStmt_Store(Iend_LE, mkU64(&(circRegs->ix)), ix);
	addStmtToIRSB(sb, store);

	IRExpr** argv = mkIRExprVec_3(mkU64(tmpNum), mkU64(descr->base), mkU64(descr->nElems));
	IRDirty* di = unsafeIRDirty_0_N(3, "processGetI", VG_(fnptr_to_fnentry)(&processGetI), argv);
	addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static void instrumentEnterSB(IRSB* sb) {
	/* inlining of sbExecuted++ */
	IRExpr* load = IRExpr_Load(Iend_LE, Ity_I64, mkU64(&sbExecuted));
	IRTemp t1 = newIRTemp(sb->tyenv, Ity_I64);
  	addStmtToIRSB(sb, IRStmt_WrTmp(t1, load));
	IRExpr* add = IRExpr_Binop(Iop_Add64, IRExpr_RdTmp(t1), mkU64(1));
  	IRTemp t2 = newIRTemp(sb->tyenv, Ity_I64);
  	addStmtToIRSB(sb, IRStmt_WrTmp(t2, add));
	IRStmt* store = IRStmt_Store(Iend_LE, mkU64(&sbExecuted), IRExpr_RdTmp(t2));
	addStmtToIRSB(sb, store);
}

static void reportUnsupportedOp(IROp op) {
	if (!VG_(OSetWord_Contains)(unsupportedOps, (UWord)op)) {
		VG_(OSetWord_Insert)(unsupportedOps, (UWord)op);
	}
}

static
IRSB* fd_instrument ( VgCallbackClosure* closure,
                      IRSB* sbIn,
                      const VexGuestLayout* layout,
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
	Int			i;
	IRSB*      	sbOut;
   	IRType     	type;
   	IRTypeEnv* 	tyenv = sbIn->tyenv;
	IRExpr* 	expr;
	/* address of current client instruction */
	Addr		cia = 0;

	if (gWordTy != hWordTy) {
		/* This case is not supported yet. */
		VG_(tool_panic)("host/guest word size mismatch");
	}

	sbCounter++;
	totalIns += sbIn->stmts_used;

	/* set up SB */
	sbOut = deepCopyIRSBExceptStmts(sbIn);

	/* copy verbatim any IR preamble preceding the first IMark */
	i = 0;
	while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
		addStmtToIRSB( sbOut, sbIn->stmts[i] );
		i++;
	}

	/* perform optimizations for each superblock */
	if (maxTemps < tyenv->types_used) {
		maxTemps = tyenv->types_used;
	}

	Int j;

	Int impTmp[tyenv->types_used];
	for (j = 0; j < tyenv->types_used; j++) {
		impTmp[j] = 0;
	}

	/* backward */
	for (j = sbIn->stmts_used - 1; j >= i; j--) {
		IRStmt* st = sbIn->stmts[j];
		if (!st || st->tag == Ist_NoOp) continue;

		switch (st->tag) {
			case Ist_Put:
				if (st->Ist.Put.data->tag == Iex_RdTmp) {
					impTmp[st->Ist.Put.data->Iex.RdTmp.tmp] = 1;
				}
				break;
			case Ist_Store:
				if (st->Ist.Store.data->tag == Iex_RdTmp) {
					impTmp[st->Ist.Store.data->Iex.RdTmp.tmp] = 1;
				}
				break;
			case Ist_WrTmp:
				expr = st->Ist.WrTmp.data;

		        switch (expr->tag) {
					case Iex_Get:
						break;
					case Iex_Unop:
						switch (expr->Iex.Unop.op) {
							case Iop_Sqrt32F0x4:
							case Iop_Sqrt64F0x2:
							case Iop_NegF32:
							case Iop_NegF64:
							case Iop_AbsF32:
							case Iop_AbsF64:
							case Iop_F32toF64:
							case Iop_ReinterpI64asF64:
							case Iop_32UtoV128:
							case Iop_V128to64:
							case Iop_V128HIto64:
							case Iop_64to32:
							case Iop_64HIto32:
							case Iop_64UtoV128:
							case Iop_32Uto64:
								if (expr->Iex.Unop.arg->tag == Iex_RdTmp) {
									impTmp[expr->Iex.Unop.arg->Iex.RdTmp.tmp] = 1;
								}
								break;
							default:
								/* backward -> args are important */
								if (expr->Iex.Unop.arg->tag == Iex_RdTmp && impTmp[expr->Iex.Unop.arg->Iex.RdTmp.tmp] == 0) {
									impTmp[expr->Iex.Unop.arg->Iex.RdTmp.tmp] = -1;
								}
								break;
						}
						break;
					case Iex_Binop:
						switch (expr->Iex.Binop.op) {
							case Iop_Add32F0x4:
							case Iop_Sub32F0x4:
							case Iop_Mul32F0x4:
							case Iop_Div32F0x4:
							case Iop_Add64F0x2:
							case Iop_Sub64F0x2:
							case Iop_Mul64F0x2:
							case Iop_Div64F0x2:
							case Iop_Min32F0x4:
							case Iop_Min64F0x2:
							case Iop_Max32F0x4:
							case Iop_Max64F0x2:
							/* case Iop_CmpF64: */
							case Iop_F64toF32:
							case Iop_64HLtoV128:
							case Iop_32HLto64:
								if (expr->Iex.Binop.arg1->tag == Iex_RdTmp) {
									impTmp[expr->Iex.Binop.arg1->Iex.RdTmp.tmp] = 1;
								}
								if (expr->Iex.Binop.arg2->tag == Iex_RdTmp) {
									impTmp[expr->Iex.Binop.arg2->Iex.RdTmp.tmp] = 1;
								}
								break;
							default:
								/* backward -> args are important */
								if (expr->Iex.Binop.arg1->tag == Iex_RdTmp && impTmp[expr->Iex.Binop.arg1->Iex.RdTmp.tmp] == 0) {
									impTmp[expr->Iex.Binop.arg1->Iex.RdTmp.tmp] = -1;
								}
								if (expr->Iex.Binop.arg2->tag == Iex_RdTmp && impTmp[expr->Iex.Binop.arg2->Iex.RdTmp.tmp] == 0) {
									impTmp[expr->Iex.Binop.arg2->Iex.RdTmp.tmp] = -1;
								}
								break;
						}
						break;
					case Iex_Triop:
						switch (expr->Iex.Triop.details->op) {
							case Iop_AddF64:
							case Iop_SubF64:
							case Iop_MulF64:
							case Iop_DivF64:
								if (expr->Iex.Triop.details->arg2->tag == Iex_RdTmp) {
									impTmp[expr->Iex.Triop.details->arg2->Iex.RdTmp.tmp] = 1;
								}
								if (expr->Iex.Triop.details->arg3->tag == Iex_RdTmp) {
									impTmp[expr->Iex.Triop.details->arg3->Iex.RdTmp.tmp] = 1;
								}
								break;
							default:
								/* backward -> args are important */
								if (expr->Iex.Triop.details->arg2->tag == Iex_RdTmp && impTmp[expr->Iex.Triop.details->arg2->Iex.RdTmp.tmp] == 0) {
									impTmp[expr->Iex.Triop.details->arg2->Iex.RdTmp.tmp] = -1;
								}
								if (expr->Iex.Triop.details->arg3->tag == Iex_RdTmp && impTmp[expr->Iex.Triop.details->arg3->Iex.RdTmp.tmp] == 0) {
									impTmp[expr->Iex.Triop.details->arg3->Iex.RdTmp.tmp] = -1;
								}
								break;
						}
						break;
					case Iex_ITE:
						/* nothing, impTmp is already true */
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}

	Int tmpInstead[tyenv->types_used];
	for (j = 0; j < tyenv->types_used; j++) {
		tmpInstead[j] = -1;
	}

	Int tmpInReg[MAX_REGISTERS];
	for (j = 0; j < MAX_REGISTERS; j++) {
		tmpInReg[j] = -1;
	}

	/* forward */
	for (j = i; j < sbIn->stmts_used; j++) {
		IRStmt* st = sbIn->stmts[j];
		if (!st || st->tag == Ist_NoOp) continue;

		switch (st->tag) {
			case Ist_Put:
				if (st->Ist.Put.data->tag == Iex_RdTmp) {
					tmpInReg[st->Ist.Put.offset] = st->Ist.Put.data->Iex.RdTmp.tmp;
				} else {
					tmpInReg[st->Ist.Put.offset] = -1;
				}
				break;
			case Ist_Store:
				break;
			case Ist_WrTmp:
				expr = st->Ist.WrTmp.data;
		        switch (expr->tag) {
					case Iex_Load:
						break;
					case Iex_Get:
						if (tmpInReg[expr->Iex.Get.offset] >= 0) {
							if (tmpInstead[tmpInReg[expr->Iex.Get.offset]] >= 0) {
								tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[tmpInReg[expr->Iex.Get.offset]];
							} else {
								tmpInstead[st->Ist.WrTmp.tmp] = tmpInReg[expr->Iex.Get.offset];
							}
						}
						break;
					case Iex_RdTmp:
						tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[expr->Iex.RdTmp.tmp];
						break;
					case Iex_Unop:
						switch (expr->Iex.Unop.op) {
							case Iop_F32toF64:
							case Iop_ReinterpI64asF64:
							case Iop_32UtoV128:
							case Iop_V128to64:
							case Iop_V128HIto64:
							case Iop_64to32:
							case Iop_64HIto32:
							case Iop_64UtoV128:
							case Iop_32Uto64:
								if (expr->Iex.Unop.arg->tag == Iex_RdTmp) {
									if (tmpInstead[expr->Iex.Unop.arg->Iex.RdTmp.tmp] >= 0) {
										tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[expr->Iex.Unop.arg->Iex.RdTmp.tmp];
									} else {
										tmpInstead[st->Ist.WrTmp.tmp] = expr->Iex.Unop.arg->Iex.RdTmp.tmp;
									}
								}
								break;
							default:
								break;
						}
						break;
					case Iex_Binop:
						switch (expr->Iex.Binop.op) {
							case Iop_F64toF32:
								if (expr->Iex.Binop.arg2->tag == Iex_RdTmp) {
									if (tmpInstead[expr->Iex.Binop.arg2->Iex.RdTmp.tmp] >= 0) {
										tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[expr->Iex.Binop.arg2->Iex.RdTmp.tmp];
									} else {
										tmpInstead[st->Ist.WrTmp.tmp] = expr->Iex.Binop.arg2->Iex.RdTmp.tmp;
									}
								}
								break;
							case Iop_64HLtoV128:
							case Iop_32HLto64:
								if (expr->Iex.Binop.arg1->tag == Iex_RdTmp) {
									if (tmpInstead[expr->Iex.Binop.arg1->Iex.RdTmp.tmp] >= 0) {
										tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[expr->Iex.Binop.arg1->Iex.RdTmp.tmp];
									} else {
										tmpInstead[st->Ist.WrTmp.tmp] = expr->Iex.Binop.arg1->Iex.RdTmp.tmp;
									}
								} else if (expr->Iex.Binop.arg2->tag == Iex_RdTmp) {
									if (tmpInstead[expr->Iex.Binop.arg2->Iex.RdTmp.tmp] >= 0) {
										tmpInstead[st->Ist.WrTmp.tmp] = tmpInstead[expr->Iex.Binop.arg2->Iex.RdTmp.tmp];
									} else {
										tmpInstead[st->Ist.WrTmp.tmp] = expr->Iex.Binop.arg2->Iex.RdTmp.tmp;
									}
								}
								break;
							default:
								break;
						}
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}

	instrumentEnterSB(sbOut);

	Int arg1tmpInstead = -1;
	Int arg2tmpInstead = -1;

	/* This is the main loop which hads instructions for the analysis (instrumentation).*/

	for (/*use current i*/; i < sbIn->stmts_used; i++) {
		IRStmt* st = sbIn->stmts[i];
		if (!st || st->tag == Ist_NoOp) continue;

		switch (st->tag) {
			case Ist_AbiHint:
				addStmtToIRSB(sbOut, st);
				break;
			case Ist_Put:
				addStmtToIRSB(sbOut, st);
				putCount++;

				if (st->Ist.Put.offset != 168) {
					arg1tmpInstead = -1;
					if (st->Ist.Put.data->tag == Iex_RdTmp) {
						arg1tmpInstead = tmpInstead[st->Ist.Put.data->Iex.RdTmp.tmp];
					}
					instrumentPut(sbOut, tyenv, st, arg1tmpInstead);
				} else {
					putsIgnored++;
				}
				break;
			case Ist_PutI:
				addStmtToIRSB(sbOut, st);
				arg1tmpInstead = -1;
				if (st->Ist.PutI.details->data->tag == Iex_RdTmp) {
					arg1tmpInstead = tmpInstead[st->Ist.PutI.details->data->Iex.RdTmp.tmp];
				}
				instrumentPutI(sbOut, tyenv, st, arg1tmpInstead);
				break;
			case Ist_IMark:
				/* address of current instruction */
				cia = st->Ist.IMark.addr;
				addStmtToIRSB(sbOut, st);
				break;
			case Ist_Exit:
				addStmtToIRSB(sbOut, st);
				break;
			case Ist_WrTmp:
				expr = st->Ist.WrTmp.data;
		        type = typeOfIRExpr(sbOut->tyenv, expr);
		        tl_assert(type != Ity_INVALID);

		        switch (expr->tag) {
					case Iex_Const:
						addStmtToIRSB(sbOut, st);
						break;
					case Iex_Load:
						addStmtToIRSB(sbOut, st);
						loadCount++;
						instrumentLoad(sbOut, tyenv, st);
						break;
					case Iex_Get:
						addStmtToIRSB(sbOut, st);
						getCount++;

						if (tmpInstead[st->Ist.WrTmp.tmp] < 0) {
							instrumentGet(sbOut, tyenv, st);
						} else {
							getsIgnored++;
						}
						break;
					case Iex_GetI:
						addStmtToIRSB(sbOut, st);

						if (tmpInstead[st->Ist.WrTmp.tmp] < 0) {
							instrumentGetI(sbOut, tyenv, st);
						}
						break;
					case Iex_Unop:
						switch (expr->Iex.Unop.op) {
							case Iop_Sqrt32F0x4:
							case Iop_Sqrt64F0x2:
							case Iop_NegF32:
							case Iop_NegF64:
							case Iop_AbsF32:
							case Iop_AbsF64:
								addStmtToIRSB(sbOut, st);

								arg1tmpInstead = -1;
								if (expr->Iex.Unop.arg->tag == Iex_RdTmp) {
									arg1tmpInstead = tmpInstead[expr->Iex.Unop.arg->Iex.RdTmp.tmp];
								}
								instrumentUnOp(sbOut, tyenv, cia, st->Ist.WrTmp.tmp, expr, arg1tmpInstead);
								break;
							case Iop_F32toF64:
							case Iop_ReinterpI64asF64:
							case Iop_32UtoV128:
							case Iop_V128to64:
							case Iop_V128HIto64:
							case Iop_64to32:
							case Iop_64HIto32:
							case Iop_64UtoV128:
							case Iop_32Uto64:
								/* ignored floating-point and related SSE operations */
								addStmtToIRSB(sbOut, st);
								break;
							case Iop_RoundF32x4_RM:
							case Iop_RoundF32x4_RP:
							case Iop_RoundF32x4_RN:
							case Iop_RoundF32x4_RZ:
							case Iop_SinF64:
							case Iop_CosF64:
							case Iop_TanF64:
							case Iop_2xm1F64:
							case Iop_RoundF64toF64_NEAREST:
							case Iop_RoundF64toF64_NegINF:
							case Iop_RoundF64toF64_PosINF:
							case Iop_RoundF64toF64_ZERO:
							case Iop_TruncF64asF32:
								addStmtToIRSB(sbOut, st);
								reportUnsupportedOp(expr->Iex.Unop.op);
								break;
							default:
								addStmtToIRSB(sbOut, st);
								break;
						}
						break;
					case Iex_Binop:
						switch (expr->Iex.Binop.op) {
							case Iop_Add32F0x4:
							case Iop_Sub32F0x4:
							case Iop_Mul32F0x4:
							case Iop_Div32F0x4:
							case Iop_Add64F0x2:
							case Iop_Sub64F0x2:
							case Iop_Mul64F0x2:
							case Iop_Div64F0x2:
							case Iop_Min32F0x4:
							case Iop_Min64F0x2:
							case Iop_Max32F0x4:
							case Iop_Max64F0x2:
								addStmtToIRSB(sbOut, st);

								arg1tmpInstead = -1;
								arg2tmpInstead = -1;
								if (expr->Iex.Binop.arg1->tag == Iex_RdTmp) {
									arg1tmpInstead = tmpInstead[expr->Iex.Binop.arg1->Iex.RdTmp.tmp];
								}
								if (expr->Iex.Binop.arg2->tag == Iex_RdTmp) {
									arg2tmpInstead = tmpInstead[expr->Iex.Binop.arg2->Iex.RdTmp.tmp];
								}
								instrumentBinOp(sbOut, tyenv, cia, st->Ist.WrTmp.tmp, expr, arg1tmpInstead, arg2tmpInstead);
								break;
							case Iop_CmpF64:
							case Iop_F64toF32:
							case Iop_64HLtoV128:
							case Iop_32HLto64:
								/* ignored floating-point and related SSE operations */
								addStmtToIRSB(sbOut, st);
								break;
							case Iop_Add32Fx4:
							case Iop_Sub32Fx4:
							case Iop_Mul32Fx4:
							case Iop_Div32Fx4:
							case Iop_Max32Fx4:
							case Iop_Min32Fx4:
							case Iop_Add64Fx2:
							case Iop_Sub64Fx2:
							case Iop_Mul64Fx2:
							case Iop_Div64Fx2:
							case Iop_Max64Fx2:
							case Iop_Min64Fx2:
							case Iop_SqrtF64:
							case Iop_SqrtF32:
							case Iop_AtanF64:
							case Iop_Yl2xF64:
							case Iop_Yl2xp1F64:
							case Iop_PRemF64:
							case Iop_PRemC3210F64:
							case Iop_PRem1F64:
							case Iop_PRem1C3210F64:
							case Iop_ScaleF64:
							case Iop_PwMax32Fx2:
							case Iop_PwMin32Fx2:
							case Iop_SinF64:
							case Iop_CosF64:
							case Iop_TanF64:
							case Iop_2xm1F64:
							case Iop_RoundF64toF32:
								addStmtToIRSB(sbOut, st);
								reportUnsupportedOp(expr->Iex.Binop.op);
								break;
							default:
								addStmtToIRSB(sbOut, st);
								break;
						}
						break;
					case Iex_Triop:
						switch (expr->Iex.Triop.details->op) {
							case Iop_AddF64:
							case Iop_SubF64:
							case Iop_MulF64:
							case Iop_DivF64:
								addStmtToIRSB(sbOut, st);

								arg1tmpInstead = -1;
								arg2tmpInstead = -1;
								if (expr->Iex.Triop.details->arg2->tag == Iex_RdTmp) {
									arg1tmpInstead = tmpInstead[expr->Iex.Triop.details->arg2->Iex.RdTmp.tmp];
								}
								if (expr->Iex.Triop.details->arg3->tag == Iex_RdTmp) {
									arg2tmpInstead = tmpInstead[expr->Iex.Triop.details->arg3->Iex.RdTmp.tmp];
								}
								instrumentTriOp(sbOut, tyenv, cia, st->Ist.WrTmp.tmp, expr, arg1tmpInstead, arg2tmpInstead);
								break;
							case Iop_AddF32:
							case Iop_SubF32:
							case Iop_MulF32:
							case Iop_DivF32:
							case Iop_AddF64r32:
							case Iop_SubF64r32:
							case Iop_MulF64r32:
							case Iop_DivF64r32:
							case Iop_AtanF64:
      						case Iop_Yl2xF64:
      						case Iop_Yl2xp1F64:
      						case Iop_PRemF64:
      						case Iop_PRemC3210F64:
      						case Iop_PRem1F64:
      						case Iop_PRem1C3210F64:
      						case Iop_ScaleF64:
								addStmtToIRSB(sbOut, st);
								reportUnsupportedOp(expr->Iex.Triop.details->op);
								break;
							default:
								addStmtToIRSB(sbOut, st);
								break;
						}
						break;
					case Iex_Qop:
						switch (expr->Iex.Qop.details->op) {
							case Iop_MAddF64r32:
							case Iop_MSubF64r32:
							case Iop_MAddF64:
							case Iop_MSubF64:
								addStmtToIRSB(sbOut, st);
								reportUnsupportedOp(expr->Iex.Qop.details->op);
								break;
							default:
								addStmtToIRSB(sbOut, st);
								break;
						}
						break;
					case Iex_ITE:
						addStmtToIRSB(sbOut, st);

						arg1tmpInstead = -1;
						arg2tmpInstead = -1;
						if (expr->Iex.ITE.iftrue->tag == Iex_RdTmp) {
							arg1tmpInstead = tmpInstead[expr->Iex.ITE.iftrue->Iex.RdTmp.tmp];
						}
						if (expr->Iex.ITE.iffalse->tag == Iex_RdTmp) {
							arg2tmpInstead = tmpInstead[expr->Iex.ITE.iffalse->Iex.RdTmp.tmp];
						}
						instrumentITE(sbOut, tyenv, st->Ist.WrTmp.tmp, expr, arg1tmpInstead, arg2tmpInstead);
						break;
					case Iex_CCall:
						addStmtToIRSB(sbOut, st);
						break;
					default:
						addStmtToIRSB(sbOut, st);
						break;
				}
				break;
			case Ist_Store:
				addStmtToIRSB(sbOut, st);

				arg1tmpInstead = -1;
				if (st->Ist.Store.data->tag == Iex_RdTmp) {
					arg1tmpInstead = tmpInstead[st->Ist.Store.data->Iex.RdTmp.tmp];
				}
				instrumentStore(sbOut, tyenv, st, arg1tmpInstead);
				storeCount++;
				break;
			default:
				addStmtToIRSB(sbOut, st);
				break;
		}
	}

    return sbOut;
}

static void getIntroducedError(mpfr_t* introducedError, MeanValue* mv) {
	mpfr_set_ui(*introducedError, 0, STD_RND);
	mpfr_abs(introMaxError, mv->max, STD_RND);
	if (mv->arg1 != 0 && mv->arg2 != 0) {
		MeanValue* mv1 = VG_(HT_lookup)(meanValues, mv->arg1);
		MeanValue* mv2 = VG_(HT_lookup)(meanValues, mv->arg2);
		tl_assert(mv1 && mv2);
		mpfr_abs(introErr1, mv1->max, STD_RND);
		mpfr_abs(introErr2, mv2->max, STD_RND);

		if (mv->arg1 == mv->key && mv->arg2 == mv->key) {
			mpfr_set(*introducedError, introMaxError, STD_RND);
		} else if (mpfr_cmp(introErr1, introErr2) > 0) {
			if (mpfr_cmp(introMaxError, introErr1) > 0 || mpfr_cmp(introMaxError, introErr2) > 0) {
				if (mv->arg1 == mv->key) {
					mpfr_set(*introducedError, introMaxError, STD_RND);
				} else {
					mpfr_sub(*introducedError, introMaxError, introErr1, STD_RND);
				}
			} else {
				/* introduced error gets negative */
				mpfr_sub(*introducedError, introMaxError, introErr2, STD_RND);
			}
		} else if (mpfr_cmp(introMaxError, introErr2) > 0 || mpfr_cmp(introMaxError, introErr1) > 0) {
			if (mv->arg2 == mv->key) {
				mpfr_set(*introducedError, introMaxError, STD_RND);
			} else {
				mpfr_sub(*introducedError, introMaxError, introErr2, STD_RND);
			}
		} else {
			/* introduced error gets negative */
			mpfr_sub(*introducedError, introMaxError, introErr1, STD_RND);
		}
	} else if (mv->arg1 != 0) {
		MeanValue* mv1 = VG_(HT_lookup)(meanValues, mv->arg1);
		tl_assert(mv1);
		mpfr_abs(introErr1, mv1->max, STD_RND);

		if (mv->arg1 == mv->key) {
			mpfr_set(*introducedError, introMaxError, STD_RND);
		} else {
			/* introduced error can get negative */
			mpfr_sub(*introducedError, introMaxError, introErr1, STD_RND);
		}
	} else if (mv->arg2 != 0) {
		MeanValue* mv2 = VG_(HT_lookup)(meanValues, mv->arg2);
		tl_assert(mv2);
		mpfr_abs(introErr2, mv2->max, STD_RND);

		if (mv->arg2 == mv->key) {
			mpfr_set(*introducedError, introMaxError, STD_RND);
		} else {
			/* introduced error can get negative */
			mpfr_sub(*introducedError, introMaxError, introErr2, STD_RND);
		}
	} else {
		mpfr_set(*introducedError, introMaxError, STD_RND);
	}
}

static void getFileName(HChar* name) {
	HChar tempName[256];
	struct vg_stat st;
	int i;
	for (i = 1; i < 100; ++i) {
		VG_(sprintf)(tempName, "%s_%d", name, i);
		SysRes res = VG_(stat)(tempName, &st);
		if (sr_isError(res)) {
			break;
		}
	}
	VG_(sprintf)(name, "%s_%d", name, i);
}

static __inline__
void fwrite_flush(void) {
    if ((fwrite_fd>=0) && (fwrite_pos>0)) {
		VG_(write)(fwrite_fd, (void*)fwrite_buf, fwrite_pos);
	}
    fwrite_pos = 0;
}

static void my_fwrite(Int fd, const HChar* buf, Int len) {
    if (fwrite_fd != fd) {
		fwrite_flush();
		fwrite_fd = fd;
    }
    if (len > FWRITE_THROUGH) {
		fwrite_flush();
		VG_(write)(fd, (void*)buf, len);
		return;
    }
    if (FWRITE_BUFSIZE - fwrite_pos <= len) {
		fwrite_flush();
	}
    VG_(strncpy)(fwrite_buf + fwrite_pos, buf, len);
    fwrite_pos += len;
}

static void writeOriginGraph(Int file, Addr oldAddr, Addr origin, Int arg, Int level, Int edgeColor, Bool careVisited) {
	if (level > MAX_LEVEL_OF_GRAPH) {
		if (careVisited) {
			MeanValue* mv = VG_(HT_lookup)(meanValues, oldAddr);
			mv->visited = True;
		}
		return;
	}

	if (level <= 1) {
		my_fwrite(file, "graph: {\n", 9);
		my_fwrite(file, "title: \"Created with FpDebug\"\n", 30);
		my_fwrite(file, "classname 1 : \"FpDebug\"\n", 24);

		Int i;
		for (i = 50; i < 150; i++) {
			VG_(sprintf)(formatBuf, "colorentry %d : 255 %d 0\n", i, (Int)((255.0 / 100.0) * (i - 50)));
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}
		for (i = 150; i < 250; i++) {
			VG_(sprintf)(formatBuf, "colorentry %d : %d 255 0\n", i, (Int)((255.0 / 100.0) * (i - 150)));
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}

		/* the set is used to avoid cycles */
		if (originAddrSet) {
			VG_(OSetWord_Destroy)(originAddrSet);
		}
		originAddrSet = VG_(OSetWord_Create)(VG_(malloc), "fd.writeOriginGraph.1", VG_(free));
	}
	tl_assert(originAddrSet);

	MeanValue* mv = VG_(HT_lookup)(meanValues, origin);
	tl_assert(mv);

	if (careVisited) {
		mv->visited = True;
	}

	Bool cycle = False;
	Bool inLibrary = False;
	if (VG_(OSetWord_Contains)(originAddrSet, origin)) {
		cycle = True;
	} else {
		/* create node */
		const HChar* originIp = VG_(describe_IP)(origin, NULL);
		if (ignoreFile(originIp)) {
			inLibrary = True;
		}

		if (clo_ignoreAccurate && mpfr_cmp_ui(mv->max, 0) == 0) {
			return;
		}

		Int color = 150; /* green */

		if (level > 1) {
			MeanValue* oldMv = VG_(HT_lookup)(meanValues, oldAddr);
			tl_assert(oldMv);

			getIntroducedError(&dumpGraphDiff, mv);

			if (mpfr_cmp_ui(dumpGraphDiff, 0) > 0) {
				mpfr_exp_t exp = mpfr_get_exp(dumpGraphDiff);
				if (exp > 1) exp = 1;
				if (exp < -8) exp = -8;
				exp = 9 + (exp - 1); /* range 0..9 */
				color = 149 - (exp * 10);
			}
		} else {
			color = 1; /* blue */
		}

		mpfr_div_ui(dumpGraphMeanError, mv->sum, mv->count, STD_RND);

		opToStr(mv->op);
		HChar meanErrorStr[MPFR_BUFSIZE];
		mpfrToStringShort(meanErrorStr, &dumpGraphMeanError);
		HChar maxErrorStr[MPFR_BUFSIZE];
		mpfrToStringShort(maxErrorStr, &(mv->max));

		HChar canceledAvg[10];
		if (mv->overflow) {
			VG_(sprintf)(canceledAvg, "overflow");
		} else {
			VG_(sprintf)(canceledAvg, "%ld", mv->canceledSum / mv->count);
		}

		const HChar *originFilename;
		VG_(get_filename)(origin, &originFilename);

		UInt linenum = -1;
		Bool gotLine = VG_(get_linenum)(origin, &linenum);
		HChar linenumber[10];
		linenumber[0] = '\0';
		if (gotLine) {
			VG_(sprintf)(linenumber, ":%u", linenum);
		}

		VG_(sprintf)(formatBuf, "node: { title: \"0x%lX\" label: \"%s (%s%s)\" color: %d info1: \"%s (%'u)\" info2: \"avg: %s, max: %s\" "
			"info3: \"canceled - avg: %s, max: %ld\" }\n",
			origin, opStr, originFilename, linenumber, color, originIp, mv->count, meanErrorStr, maxErrorStr, canceledAvg, mv->canceledMax);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}

	if (level > 1) {
		/* create edge */
		MeanValue* oldMv = VG_(HT_lookup)(meanValues, oldAddr);
		tl_assert(oldMv);

		getIntroducedError(&dumpGraphDiff, mv);

		HChar diffStr[30];
		mpfrToStringShort(&diffStr, &dumpGraphDiff);

		VG_(sprintf)(formatBuf, "edge: { sourcename: \"0x%lX\" targetname: \"0x%lX\" label: \"%s\" class: 1 color : %d }\n", origin, oldAddr, diffStr, edgeColor);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}

	if (cycle) {
		return;
	}

	if (mv != NULL) {
		VG_(OSetWord_Insert)(originAddrSet, origin);
		if (mv->arg1 != 0 && mv->arg2 != 0) {
			MeanValue* mv1 = VG_(HT_lookup)(meanValues, mv->arg1);
			MeanValue* mv2 = VG_(HT_lookup)(meanValues, mv->arg2);
			tl_assert(mv1 && mv2);

			Bool leftErrGreater = True;
			mpfr_abs(dumpGraphErr1, mv1->max, STD_RND);
			mpfr_abs(dumpGraphErr2, mv2->max, STD_RND);

			if (mpfr_cmp(dumpGraphErr1, dumpGraphErr2) < 0) {
				leftErrGreater = False;
			}
			mpfr_sub(dumpGraphDiff, dumpGraphErr1, dumpGraphErr2, STD_RND);
			mpfr_abs(dumpGraphDiff, dumpGraphDiff, STD_RND);
			mpfr_exp_t exp = mpfr_get_exp(dumpGraphDiff);
			if (exp > 1) exp = 1;
			if (exp < -8) exp = -8;
			exp = 9 + (exp - 1);
			Int red = 149 - (exp * 10);
			if (red > 120) red = 120;
			Int green = red + 100;

			const HChar* arg1Ip = VG_(describe_IP)(mv->arg1, NULL);
			if (!inLibrary || !ignoreFile(arg1Ip)) {
				writeOriginGraph(file, origin, mv->arg1, 1, ++level, (leftErrGreater ? red : green), careVisited);
			}
			const HChar* arg2Ip = VG_(describe_IP)(mv->arg2, NULL);
			if (!inLibrary || !ignoreFile(arg2Ip)) {
				writeOriginGraph(file, origin, mv->arg2, 2, level, (leftErrGreater ? green : red), careVisited);
			}
		} else if (mv->arg1 != 0) {
			const HChar* arg1Ip = VG_(describe_IP)(mv->arg1, NULL);
			if (!inLibrary || !ignoreFile(arg1Ip)) {
				writeOriginGraph(file, origin, mv->arg1, 1, ++level, 1, careVisited);
			}
		} else if (mv->arg2 != 0) {
			const HChar* arg2Ip = VG_(describe_IP)(mv->arg2, NULL);
			if (!inLibrary || !ignoreFile(arg2Ip)) {
				writeOriginGraph(file, origin, mv->arg2, 2, ++level, 1, careVisited);
			}
		}
	}
}

static Bool dumpGraph(HChar* fileName, ULong addr, Bool conditional, Bool careVisited) {
	if (!clo_computeMeanValue) {
		VG_(umsg)("DUMP GRAPH (%s): Mean error computation has to be active!\n", fileName);
		return False;
	}

	ShadowValue* svalue = VG_(HT_lookup)(globalMemory, addr);
	if (svalue) {
		if (careVisited) {
			MeanValue* mv = VG_(HT_lookup)(meanValues, svalue->origin);
			tl_assert(mv);
			if (mv->visited) {
				return False;
			}
		}

		const HChar* originIp = VG_(describe_IP)(svalue->origin, NULL);
		if (ignoreFile(originIp)) {
			return False;
		}

		if (svalue->orgType == Ot_FLOAT) {
			mpfr_set_flt(dumpGraphOrg, svalue->Org.fl, STD_RND);
		} else if (svalue->orgType == Ot_DOUBLE) {
			mpfr_set_d(dumpGraphOrg, svalue->Org.db, STD_RND);
		} else {
			tl_assert(False);
		}

		if (mpfr_cmp_ui(svalue->value, 0) != 0 || mpfr_cmp_ui(dumpGraphOrg, 0) != 0) {
			mpfr_reldiff(dumpGraphRel, svalue->value, dumpGraphOrg, STD_RND);
			mpfr_abs(dumpGraphRel, dumpGraphRel, STD_RND);
		} else {
			mpfr_set_ui(dumpGraphRel, 0, STD_RND);
		}

		if (conditional && mpfr_cmp_ui(dumpGraphRel, 0) == 0) {
			return False;
		}

		SysRes file = VG_(open)(fileName, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
		if (!sr_isError(file)) {
			writeOriginGraph(sr_Res(file), 0, svalue->origin, 0, 1, 1, careVisited);
			my_fwrite(sr_Res(file), "}\n", 2);
			fwrite_flush();
			VG_(close)(sr_Res(file));
			VG_(umsg)("DUMP GRAPH (%s): successful\n", fileName);
			return True;
		} else {
			VG_(umsg)("DUMP GRAPH (%s): Failed to create or open the file!\n", fileName);
			return False;
		}
	} else {
		VG_(umsg)("DUMP GRAPH (%s): Shadow variable was not found!\n", fileName);
		VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
		return False;
	}
}

static void printError(Char* varName, ULong addr, Bool conditional) {
	mpfr_t org, diff, rel;

	ShadowValue* svalue = VG_(HT_lookup)(globalMemory, addr);
	if (svalue) {
		mpfr_inits(diff, rel, NULL);

		Bool isFloat = svalue->orgType == Ot_FLOAT;
		if (svalue->orgType == Ot_FLOAT) {
			mpfr_init(org);
			mpfr_set_flt(org, svalue->Org.fl, STD_RND);
		} else if (svalue->orgType == Ot_DOUBLE) {
			mpfr_init_set_d(org, svalue->Org.db, STD_RND);
		} else {
			tl_assert(False);
		}

		if (mpfr_cmp_ui(svalue->value, 0) != 0 || mpfr_cmp_ui(org, 0) != 0) {
			mpfr_reldiff(rel, svalue->value, org, STD_RND);
			mpfr_abs(rel, rel, STD_RND);
		} else {
			mpfr_set_ui(rel, 0, STD_RND);
		}

		if (conditional && mpfr_cmp_ui(rel, 0) == 0) {
			mpfr_clears(org, diff, rel, NULL);
			return;
		}

		mpfr_sub(diff, svalue->value, org, STD_RND);

		HChar typeName[7];
		if (isFloat) {
			VG_(strcpy)(typeName, "float");
		} else {
			VG_(strcpy)(typeName, "double");
		}

		VG_(umsg)("(%s) %s PRINT ERROR OF: 0x%lX\n", typeName, varName, addr);
		HChar mpfrBuf[MPFR_BUFSIZE];
		mpfrToString(mpfrBuf, &org);
		VG_(umsg)("(%s) %s ORIGINAL:         %s\n", typeName, varName, mpfrBuf);
		mpfrToString(mpfrBuf, &(svalue->value));
		VG_(umsg)("(%s) %s SHADOW VALUE:     %s\n", typeName, varName, mpfrBuf);
		mpfrToString(mpfrBuf, &diff);
		VG_(umsg)("(%s) %s ABSOLUTE ERROR:   %s\n", typeName, varName, mpfrBuf);
		mpfrToString(mpfrBuf, &rel);
		VG_(umsg)("(%s) %s RELATIVE ERROR:   %s\n", typeName, varName, mpfrBuf);
		VG_(umsg)("(%s) %s CANCELED BITS:     %lld\n", typeName, varName, svalue->canceled);

		const HChar* lastOperation = VG_(describe_IP)(svalue->origin, NULL);
		VG_(umsg)("(%s) %s Last operation: %s\n", typeName, varName, lastOperation);

		if (svalue->canceled > 0 && svalue->cancelOrigin > 0) {
			const HChar* cancellationOrigin = VG_(describe_IP)(svalue->cancelOrigin, NULL);
			VG_(umsg)("(%s) %s Cancellation origin: %s\n", typeName, varName, cancellationOrigin);
		}

		VG_(umsg)("(%s) %s Operation count (max path): %'lu\n", typeName, varName, svalue->opCount);

		mpfr_clears(org, diff, rel, NULL);
	} else {
		VG_(umsg)("There exists no shadow value for %s!\n", varName);
		VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
	}
}

static Bool isErrorGreater(ULong addrFp, ULong addrErr) {
	mpfr_t org, rel;
	Double* errorBound = (Double*)addrErr;

	ShadowValue* svalue = VG_(HT_lookup)(globalMemory, addrFp);
	if (svalue) {
		mpfr_init(rel);

		Bool isFloat = svalue->orgType == Ot_FLOAT;
		if (svalue->orgType == Ot_FLOAT) {
			mpfr_init(org);
			mpfr_set_flt(org, svalue->Org.fl, STD_RND);
		} else if (svalue->orgType == Ot_DOUBLE) {
			mpfr_init_set_d(org, svalue->Org.db, STD_RND);
		} else {
			tl_assert(False);
		}

		if (mpfr_cmp_ui(svalue->value, 0) != 0 || mpfr_cmp_ui(org, 0) != 0) {
			mpfr_reldiff(rel, svalue->value, org, STD_RND);
			mpfr_abs(rel, rel, STD_RND);
		} else {
			mpfr_set_ui(rel, 0, STD_RND);
		}

		Bool isGreater = mpfr_cmp_d(rel, *errorBound) >= 0;

		mpfr_clears(org, rel, NULL);
		return isGreater;
	} else {
		VG_(umsg)("Error greater: there exists no shadow value!\n");
		VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
		return False;
	}
}

static void resetShadowValues(void) {
	Int i, j;
	for (i = 0; i < VG_N_THREADS; i++) {
		for (j = 0; j < MAX_REGISTERS; j++) {
			if (threadRegisters[i][j] != NULL) {
				threadRegisters[i][j]->active = False;
			}
		}
	}
	for (i = 0; i < MAX_TEMPS; i++) {
		if (localTemps[i] != NULL) {
			localTemps[i]->version = 0;
		}
	}
	ShadowValue* next;
	VG_(HT_ResetIter)(globalMemory);
	while (next = VG_(HT_Next)(globalMemory)) {
		next->active = False;
	}
}

static void insertShadow(ULong addrFp) {
	ShadowValue* svalue = VG_(HT_lookup)(globalMemory, addrFp);
	if (svalue) {
		if (svalue->orgType == Ot_FLOAT) {
			Float* orgFl = (Float*)addrFp;
			*orgFl = mpfr_get_flt(svalue->value, STD_RND);
		} else if (svalue->orgType == Ot_DOUBLE) {
			Double* orgDb = (Double*)addrFp;
			*orgDb = mpfr_get_d(svalue->value, STD_RND);
		} else {
			tl_assert(False);
		}
	}
}

static void beginAnalyzing(void) {
	clo_analyze = True;
}

static void endAnalyzing(void) {
    if (!clo_ignore_end) {
		clo_analyze = False;
    }
}

static void writeWarning(Int file) {
	if (VG_(OSetWord_Size)(unsupportedOps) == 0) {
		return;
	}
	VG_(sprintf)(formatBuf, "Unsupported operations detected: ");
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	VG_(OSetWord_ResetIter)(unsupportedOps);
	UWord next = 0;
	Bool first = True;
	while (VG_(OSetWord_Next)(unsupportedOps, &next)) {
		IROp op = (IROp)next;
		opToStr(op);
		if (first) {
			VG_(sprintf)(formatBuf, "%s", opStr);
		} else {
			VG_(sprintf)(formatBuf, ", %s", opStr);
		}
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		first = False;
	}
	VG_(sprintf)(formatBuf, "\n\n");
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
}

static void writeShadowValue(Int file, ShadowValue* svalue, Int num) {
	tl_assert(svalue);

	Bool isFloat = svalue->orgType == Ot_FLOAT;
	if (svalue->orgType == Ot_FLOAT) {
		mpfr_set_flt(writeSvOrg, svalue->Org.fl, STD_RND);
	} else if (svalue->orgType == Ot_DOUBLE) {
		mpfr_set_d(writeSvOrg, svalue->Org.db, STD_RND);
	} else {
		tl_assert(False);
	}

	if (mpfr_cmp_ui(svalue->value, 0) != 0 || mpfr_cmp_ui(writeSvOrg, 0) != 0) {
		mpfr_reldiff(writeSvRelError, svalue->value, writeSvOrg, STD_RND);
		mpfr_abs(writeSvRelError, writeSvRelError, STD_RND);
	} else {
		mpfr_set_ui(writeSvRelError, 0, STD_RND);
	}

	mpfr_sub(writeSvDiff, svalue->value, writeSvOrg, STD_RND);

	HChar mpfrBuf[MPFR_BUFSIZE];
	HChar typeName[7];
	if (isFloat) {
		VG_(strcpy)(typeName, "float");
	} else {
		VG_(strcpy)(typeName, "double");
	}

	VG_(sprintf)(formatBuf, "%d: 0x%lX of type %s\n", num, svalue->key, typeName);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	mpfrToString(mpfrBuf, &writeSvOrg);
	VG_(sprintf)(formatBuf, "    original:         %s\n", mpfrBuf);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	mpfrToString(mpfrBuf, &(svalue->value));
	VG_(sprintf)(formatBuf, "    shadow value:     %s\n", mpfrBuf);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	mpfrToString(mpfrBuf, &writeSvDiff);
	VG_(sprintf)(formatBuf, "    absolute error:   %s\n", mpfrBuf);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	mpfrToString(mpfrBuf, &writeSvRelError);
	VG_(sprintf)(formatBuf, "    relative error:   %s\n", mpfrBuf);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "    maximum number of canceled bits: %ld\n", svalue->canceled);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	if (svalue->canceled > 0 && svalue->cancelOrigin > 0) {
		const HChar* cancellationOrigin = VG_(describe_IP)(svalue->cancelOrigin, NULL);
		VG_(sprintf)(formatBuf, "    origin of maximum cancellation: %s\n", cancellationOrigin);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}

	const HChar* lastOperation = VG_(describe_IP)(svalue->origin, NULL);
	VG_(sprintf)(formatBuf, "    last operation: %s\n", lastOperation);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "    operation count (max path): %'lu\n", svalue->opCount);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
}

static Bool areSvsEqual(ShadowValue* sv1, ShadowValue* sv2) {
	if (sv1->opCount == sv2->opCount && sv1->origin == sv2->origin &&
		sv1->canceled == sv2->canceled && sv1->cancelOrigin == sv2->cancelOrigin &&
		sv1->orgType == sv2->orgType && mpfr_cmp(sv1->value, sv2->value) == 0)
	{
		return (sv1->orgType == Ot_FLOAT && sv1->Org.fl == sv2->Org.fl) ||
			   (sv1->orgType == Ot_DOUBLE && sv1->Org.db == sv2->Org.db);
	}
	return False;
}

static Int compareShadowValues(const void* n1, const void* n2) {
	ShadowValue* sv1 = *(ShadowValue**)n1;
	ShadowValue* sv2 = *(ShadowValue**)n2;
	if (sv1->opCount < sv2->opCount) return 1;
	if (sv1->opCount > sv2->opCount) return -1;
	if (sv1->key < sv2->key) return -1;
	if (sv1->key > sv2->key) return 1;
	return 0;
}

static void writeMemorySpecial(ShadowValue** memory, UInt n_memory) {
	HChar fname[256];
	const HChar* clientName = VG_(args_the_exename);
	VG_(sprintf)(fname, "%s_shadow_values_special", clientName);

	getFileName(fname);
	SysRes fileRes = VG_(open)(fname, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(fileRes)) {
		VG_(umsg)("SHADOW VALUES (%s): Failed to create or open the file!\n", fname);
		return;
	}
	Int file = sr_Res(fileRes);
	writeWarning(file);

	UInt specialFps = 0;
	UInt skippedLibrary = 0;
	UInt numWritten = 0;
	UInt total = 0;
	Int i;
	for (i = 0; i < n_memory; i++) {
		if (i > 0 && areSvsEqual(memory[i-1], memory[i])) {
			continue;
		}
		total++;

		if (memory[i]->orgType == Ot_FLOAT) {
			mpfr_set_flt(endAnalysisOrg, memory[i]->Org.fl, STD_RND);
		} else if (memory[i]->orgType == Ot_DOUBLE) {
			mpfr_set_d(endAnalysisOrg, memory[i]->Org.db, STD_RND);
		} else {
			tl_assert(False);
		}

		/* not a normal number => NaN, +Inf, or -Inf */
		if (mpfr_number_p(endAnalysisOrg) == 0) {
			specialFps++;

			if (clo_ignoreLibraries) {
				const HChar* originIp = VG_(describe_IP)(memory[i]->origin, NULL);
				if (ignoreFile(originIp)) {
					skippedLibrary++;
					continue;
				}
			}

			if (numWritten < MAX_ENTRIES_PER_FILE) {
				numWritten++;
				writeShadowValue(file, memory[i], total);
				VG_(sprintf)(formatBuf, "\n");
				my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
			}
		} else if (!clo_ignoreAccurate && numWritten < MAX_ENTRIES_PER_FILE) {
			numWritten++;
			writeShadowValue(file, memory[i], i);
			VG_(sprintf)(formatBuf, "\n");
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}
	}

	VG_(sprintf)(formatBuf, "%'u%s out of %'u shadow values are in this file\n", numWritten,
		numWritten == MAX_ENTRIES_PER_FILE ? " (maximum number written to file)" : "", total);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	if (skippedLibrary > 0) {
		VG_(sprintf)(formatBuf, "%'u are skipped because they are from a library\n", skippedLibrary);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}
	VG_(sprintf)(formatBuf, "%'u out of %'u shadow values are special (NaN, +Inf, or -Inf)\n", specialFps, n_memory);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "total number of floating-point operations: %'lu\n", fpOps);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "number of executed blocks: %'lu\n", sbExecuted);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	fwrite_flush();
	VG_(close)(file);
	VG_(umsg)("SHADOW VALUES (%s): successful\n", fname);
}

static void writeMemoryCanceled(ShadowValue** memory, UInt n_memory) {
	HChar fname[256];
	const HChar* clientName = VG_(args_the_exename);
	VG_(sprintf)(fname, "%s_shadow_values_canceled", clientName);

	getFileName(fname);
	SysRes fileRes = VG_(open)(fname, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(fileRes)) {
		VG_(umsg)("SHADOW VALUES (%s): Failed to create or open the file!\n", fname);
		return;
	}
	Int file = sr_Res(fileRes);
	writeWarning(file);

	UInt fpsWithError = 0;
	UInt skippedLibrary = 0;
	UInt numWritten = 0;
	UInt total = 0;
	Int i;
	for (i = 0; i < n_memory; i++) {
		if (i > 0 && areSvsEqual(memory[i-1], memory[i])) {
			continue;
		}
		total++;

		if (memory[i]->canceled > CANCEL_LIMIT) {
			fpsWithError++;

			if (clo_ignoreLibraries) {
				const HChar* originIp = VG_(describe_IP)(memory[i]->origin, NULL);
				if (ignoreFile(originIp)) {
					skippedLibrary++;
					continue;
				}
			}

			if (numWritten < MAX_ENTRIES_PER_FILE) {
				numWritten++;
				writeShadowValue(file, memory[i], i);
				VG_(sprintf)(formatBuf, "\n");
				my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
			}
		} else if (!clo_ignoreAccurate && numWritten < MAX_ENTRIES_PER_FILE) {
			numWritten++;
			writeShadowValue(file, memory[i], total);
			VG_(sprintf)(formatBuf, "\n");
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}
	}

	VG_(sprintf)(formatBuf, "%'u%s out of %'u shadow values are in this file\n", numWritten,
		numWritten == MAX_ENTRIES_PER_FILE ? " (maximum number written to file)" : "", total);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	if (skippedLibrary > 0) {
		VG_(sprintf)(formatBuf, "%'u are skipped because they are from a library\n", skippedLibrary);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}
	VG_(sprintf)(formatBuf, "%'u out of %'u shadow values have more than %'d canceled bits\n", fpsWithError, total, CANCEL_LIMIT);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "total number of floating-point operations: %'lu\n", fpOps);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "number of executed blocks: %'lu\n", sbExecuted);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	fwrite_flush();
	VG_(close)(file);
	VG_(umsg)("SHADOW VALUES (%s): successful\n", fname);
}

static void writeMemoryRelError(ShadowValue** memory, UInt n_memory) {
	HChar fname[256];
	const HChar* clientName = VG_(args_the_exename);
	VG_(sprintf)(fname, "%s_shadow_values_relative_error", clientName);

	getFileName(fname);
	SysRes fileRes = VG_(open)(fname, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(fileRes)) {
		VG_(umsg)("SHADOW VALUES (%s): Failed to create or open the file!\n", fname);
		return;
	}
	Int file = sr_Res(fileRes);
	writeWarning(file);

	UInt fpsWithError = 0;
	UInt skippedLibrary = 0;
	UInt numWritten = 0;
	UInt total = 0;
	Int j = 1;
	Int i;
	for (i = 0; i < n_memory; i++) {
		if (i > 0 && areSvsEqual(memory[i-1], memory[i])) {
			continue;
		}
		total++;

		if (memory[i]->orgType == Ot_FLOAT) {
			mpfr_set_flt(endAnalysisOrg, memory[i]->Org.fl, STD_RND);
		} else if (memory[i]->orgType == Ot_DOUBLE) {
			mpfr_set_d(endAnalysisOrg, memory[i]->Org.db, STD_RND);
		} else {
			tl_assert(False);
		}

		Bool hasError = True;
		if (mpfr_cmp_ui(memory[i]->value, 0) != 0 || mpfr_cmp_ui(endAnalysisOrg, 0) != 0) {
			mpfr_reldiff(endAnalysisRelError, memory[i]->value, endAnalysisOrg, STD_RND);

			if (mpfr_cmp_ui(endAnalysisRelError, 0) != 0) {
				fpsWithError++;

				if (clo_ignoreLibraries) {
					const HChar* originIp = VG_(describe_IP)(memory[i]->origin, NULL);
					if (ignoreFile(originIp)) {
						skippedLibrary++;
						continue;
					}
				}

				if (numWritten < MAX_ENTRIES_PER_FILE) {
					numWritten++;
					writeShadowValue(file, memory[i], total);

					if (j <= MAX_DUMPED_GRAPHS) {
						VG_(sprintf)(filename, "%s_%d_%d.vcg", clientName, j, i);
						if (dumpGraph(filename, memory[i]->key, True, True)) {
							VG_(sprintf)(formatBuf, "    graph dumped: %s\n", filename);
							my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

							j++;
						}
					}

					VG_(sprintf)(formatBuf, "\n");
					my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
				}
			} else {
				hasError = False;
			}
		} else {
			hasError = False;
		}
		if (!clo_ignoreAccurate && !hasError && numWritten < MAX_ENTRIES_PER_FILE) {
			numWritten++;
			writeShadowValue(file, memory[i], i);
			VG_(sprintf)(formatBuf, "\n");
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}
	}

	VG_(sprintf)(formatBuf, "%'u%s out of %'u shadow values are in this file\n", numWritten,
		numWritten == MAX_ENTRIES_PER_FILE ? " (maximum number written to file)" : "", total);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	if (skippedLibrary > 0) {
		VG_(sprintf)(formatBuf, "%'u are skipped because they are from a library\n", skippedLibrary);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}
	VG_(sprintf)(formatBuf, "%'u out of %'u shadow values have an error\n", fpsWithError, total);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "%'u graph(s) have been dumped\n", j - 1);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "total number of floating-point operations: %'lu\n", fpOps);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "number of executed blocks: %'lu\n", sbExecuted);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	fwrite_flush();
	VG_(close)(file);
	VG_(umsg)("SHADOW VALUES (%s): successful\n", fname);
}

static void endAnalysis(void) {
	UInt n_memory = 0;
	ShadowValue** memory = (ShadowValue**) VG_(HT_to_array)(globalMemory, &n_memory);
	VG_(ssort)(memory, n_memory, sizeof(VgHashNode*), compareShadowValues);

	writeMemoryRelError(memory, n_memory);
	writeMemoryCanceled(memory, n_memory);
	writeMemorySpecial(memory, n_memory);

	VG_(free)(memory);
}

static Int compareMVAddr(const void* n1, const void* n2) {
	MeanValue* mv1 = *(MeanValue**)n1;
	MeanValue* mv2 = *(MeanValue**)n2;
	if (mv1->key < mv2->key) return -1;
	if (mv1->key > mv2->key) return  1;
	return 0;
}

static Int compareMVCanceled(const void* n1, const void* n2) {
	MeanValue* mv1 = *(MeanValue**)n1;
	MeanValue* mv2 = *(MeanValue**)n2;
	if (mv1->cancellationBadnessMax < mv2->cancellationBadnessMax) return 1;
	if (mv1->cancellationBadnessMax > mv2->cancellationBadnessMax) return -1;
	if (mv1->canceledMax < mv2->canceledMax) return 1;
	if (mv1->canceledMax > mv2->canceledMax) return -1;
	return 0;
}

static Int compareMVIntroError(const void* n1, const void* n2) {
	MeanValue* mv1 = *(MeanValue**)n1;
	MeanValue* mv2 = *(MeanValue**)n2;

	getIntroducedError(&compareIntroErr1, mv1);
	getIntroducedError(&compareIntroErr2, mv2);
	Int cmp = mpfr_cmp(compareIntroErr1, compareIntroErr2);

	if (cmp < 0) return 1;
	if (cmp > 0) return -1;
	return 0;
}

static void writeMeanValues(HChar* fname, Int (*cmpFunc) (const void*, const void*), Bool forCanceled) {
	if (!clo_computeMeanValue) {
		return;
	}

	getFileName(fname);
	SysRes fileRes = VG_(open)(fname, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(fileRes)) {
		VG_(umsg)("MEAN ERRORS (%s): Failed to create or open the file!\n", fname);
		return;
	}
	Int file = sr_Res(fileRes);
	writeWarning(file);

	UInt n_values = 0;
	MeanValue** values = (MeanValue**) VG_(HT_to_array)(meanValues, &n_values);
	VG_(ssort)(values, n_values, sizeof(VgHashNode*), cmpFunc);

	mpfr_t meanError, maxError, introducedError, err1, err2;
	mpfr_inits(meanError, maxError, introducedError, err1, err2, NULL);
	Int fpsWritten = 0;
	Int skipped = 0;
	Int skippedLibrary = 0;
	Int i;
	for (i = 0; i < n_values; i++) {
		if (clo_ignoreAccurate && !forCanceled && mpfr_cmp_ui(values[i]->sum, 0) == 0) {
			skipped++;
			continue;
		}

		if (clo_ignoreAccurate && forCanceled && values[i]->canceledMax == 0) {
			skipped++;
			continue;
		}

		const HChar* originIp = VG_(describe_IP)(values[i]->key, NULL);
		if (ignoreFile(originIp)) {
			skippedLibrary++;
			continue;
		}

		if (i > MAX_ENTRIES_PER_FILE) {
			continue;
		}

		fpsWritten++;
		mpfr_div_ui(meanError, values[i]->sum, values[i]->count, STD_RND);

		opToStr(values[i]->op);
		HChar meanErrorStr[MPFR_BUFSIZE];
		mpfrToString(meanErrorStr, &meanError);
		HChar maxErrorStr[MPFR_BUFSIZE];
		mpfrToString(maxErrorStr, &(values[i]->max));

		VG_(sprintf)(formatBuf, "%s %s (%'u)\n", originIp, opStr, values[i]->count);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		VG_(sprintf)(formatBuf, "    avg error: %s\n", meanErrorStr);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		VG_(sprintf)(formatBuf, "    max error: %s\n", maxErrorStr);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

		if (values[i]->overflow) {
			VG_(sprintf)(formatBuf, "    canceled bits - max: %'ld, avg: overflow\n", values[i]->canceledMax);
		} else {
			mpfr_exp_t meanCanceledBits = values[i]->canceledSum / values[i]->count;
			VG_(sprintf)(formatBuf, "    canceled bits - max: %'ld, avg: %'ld\n", values[i]->canceledMax, meanCanceledBits);
		}
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

		if (clo_bad_cancellations) {
			Double avg = 0;
			if (values[i]->count != 0 && values[i]->cancellationBadnessMax != 0) {
				avg = values[i]->cancellationBadnessSum * 100.0 / (values[i]->count * values[i]->cancellationBadnessMax);
			}
			VG_(sprintf)(formatBuf, "    cancellation badness - max: %'ld, avg (sum/(count*max)): %.1f%%\n", values[i]->cancellationBadnessMax, avg);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}

		getIntroducedError(&introducedError, values[i]);
		if (mpfr_cmp_ui(introducedError, 0) > 0) {
			HChar introErrorStr[MPFR_BUFSIZE];
			mpfrToString(introErrorStr, &introducedError);
			VG_(sprintf)(formatBuf, "    introduced error (max path): %s\n", introErrorStr);
		} else {
			VG_(sprintf)(formatBuf, "    no error has been introduced (max path)\n");
		}
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		VG_(sprintf)(formatBuf, "    origin of the arguments (max path): 0x%lX, 0x%lX\n\n", values[i]->arg1, values[i]->arg2);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}

	VG_(sprintf)(formatBuf, "%'d%s out of %'d operations are listed in this file\n",
		fpsWritten, fpsWritten == MAX_ENTRIES_PER_FILE ? " (maximum number written to file)" : "", n_values);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	if (skipped > 0) {
		if (forCanceled) {
			VG_(sprintf)(formatBuf, "%'d operations have been skipped because no bits were canceled\n", skipped);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		} else {
			VG_(sprintf)(formatBuf, "%'d operations have been skipped because they are accurate\n", skipped);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}
	}
	if (skippedLibrary > 0) {
		VG_(sprintf)(formatBuf, "%'d operations have been skipped because they are in a library\n", skippedLibrary);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	}

	fwrite_flush();
	VG_(close)(file);
	VG_(umsg)("MEAN ERRORS (%s): successful\n", fname);

	mpfr_clears(meanError, maxError, introducedError, err1, err2, NULL);
	VG_(free)(values);
}

static Int compareStageReports(const void* n1, const void* n2) {
	StageReport* sr1 = *(StageReport**)n1;
	StageReport* sr2 = *(StageReport**)n2;
	if (sr1->count < sr2->count) return 1;
	if (sr1->count > sr2->count) return -1;

	if (sr1->iterMin < sr2->iterMin) return 1;
	if (sr1->iterMin > sr2->iterMin) return -1;

	if (sr1->iterMax < sr2->iterMax) return 1;
	if (sr1->iterMax > sr2->iterMax) return -1;

	if (sr1->origin < sr2->origin) return 1;
	if (sr1->origin > sr2->origin) return -1;
	return 0;
}

static void writeStageReports(HChar* fname) {
	Bool writeReports = False;
	Int i;
	for (i = 0; i < MAX_STAGES; i++) {
		if (stageReports[i]) writeReports = True;
	}
	if (!writeReports) {
		return;
	}

	getFileName(fname);
	SysRes fileRes = VG_(open)(fname, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(fileRes)) {
		VG_(umsg)("STAGE REPORTS (%s): Failed to create or open the file!\n", fname);
		return;
	}
	Int file = sr_Res(fileRes);
	writeWarning(file);

	Int reportsWritten = 0;
	Int totalReports = 0;
	Int numStages = 0;
	Int j;
	for (i = 0; i < MAX_STAGES; i++) {
		if (!stageReports[i]) {
			continue;
		}
		numStages++;

		UInt n_reports = 0;
		StageReport** reports = (StageReport**) VG_(HT_to_array)(stageReports[i], &n_reports);
		VG_(ssort)(reports, n_reports, sizeof(VgHashNode*), compareStageReports);
		totalReports += n_reports;

		VG_(sprintf)(formatBuf, "Stage %d:\n\n", i);
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

		for (j = 0; j < n_reports; j++) {
			if (reportsWritten > MAX_ENTRIES_PER_FILE) {
				break;
			}
			/* avoid output of duplicates */
			if (j > 0 && reports[j-1]->count == reports[j]->count &&
				reports[j-1]->iterMin == reports[j]->iterMin && reports[j-1]->iterMax == reports[j]->iterMax &&
				reports[j-1]->origin == reports[j]->origin)
			{
				totalReports--;
				continue;
			}
			reportsWritten++;

			VG_(sprintf)(formatBuf, "(%d) 0x%lX (%'u)\n", i, reports[j]->key, reports[j]->count);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
			VG_(sprintf)(formatBuf, "    executions: [%u, %u]\n", reports[j]->iterMin, reports[j]->iterMax);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
			VG_(sprintf)(formatBuf, "    origin: 0x%lX\n\n", reports[j]->origin);
			my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
		}

		VG_(sprintf)(formatBuf, "\n");
		my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

		VG_(free)(reports);
		if (reportsWritten > MAX_ENTRIES_PER_FILE) {
			break;
		}
	}

	VG_(sprintf)(formatBuf, "%'d%s out of %'d reports are listed in this file\n",
		reportsWritten, reportsWritten == MAX_ENTRIES_PER_FILE ? " (maximum number written to file)" : "", totalReports);
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));
	VG_(sprintf)(formatBuf, "%d stage%s produced reports\n", numStages, numStages > 1 ? "s" : "");
	my_fwrite(file, (void*)formatBuf, VG_(strlen)(formatBuf));

	fwrite_flush();
	VG_(close)(file);
	VG_(umsg)("STAGE REPORTS (%s): successful\n", fname);
}

static void fd_fini(Int exitcode) {
	endAnalysis();

	const HChar* clientName = VG_(args_the_exename);
	VG_(sprintf)(filename, "%s_mean_errors_addr", clientName);
	writeMeanValues(filename, &compareMVAddr, False);
	if (clo_bad_cancellations) {
		VG_(sprintf)(filename, "%s_mean_errors_canceled", clientName);
		writeMeanValues(filename, &compareMVCanceled, True);
	}
	VG_(sprintf)(filename, "%s_mean_errors_intro", clientName);
	writeMeanValues(filename, &compareMVIntroError, False);

	VG_(sprintf)(filename, "%s_stage_reports", clientName);
	writeStageReports(filename);

#ifndef NDEBUG
	VG_(umsg)("DEBUG - Client exited with code: %d\n", exitcode);
	VG_(dmsg)("DEBUG - SBs: %'lu, executed: %'lu, instr: %'lu\n", sbCounter, sbExecuted, totalIns);
	VG_(dmsg)("DEBUG - ShadowValues (frees/mallocs): %'lu/%'lu, diff: %'lu\n", avFrees, avMallocs, avMallocs - avFrees);
	VG_(dmsg)("DEBUG - Floating-point operations: %'lu\n", fpOps);
	VG_(dmsg)("DEBUG - Max temps: %'u\n", maxTemps);
	VG_(dmsg)("OPTIMIZATION - GET:   total %'u, ignored: %'u\n", getCount, getsIgnored);
	VG_(dmsg)("OPTIMIZATION - STORE: total %'u, ignored: %'u\n", storeCount, storesIgnored);
	VG_(dmsg)("OPTIMIZATION - PUT:   total %'u, ignored: %'u\n", putCount, putsIgnored);
	VG_(dmsg)("OPTIMIZATION - LOAD:  total %'u, ignored: %'u\n", loadCount, loadsIgnored);
#endif
}

/* Returns True if there is a return value. */
static Bool fd_handle_client_request(ThreadId tid, UWord* arg, UWord* ret) {
	switch (arg[0]) {
		case VG_USERREQ__PRINT_ERROR:
			printError((Char*)arg[1], arg[2], False);
			break;
		case VG_USERREQ__COND_PRINT_ERROR:
			printError((Char*)arg[1], arg[2], True);
			break;
		case VG_USERREQ__DUMP_ERROR_GRAPH:
			dumpGraph((HChar*)arg[1], arg[2], False, False);
			break;
		case VG_USERREQ__COND_DUMP_ERROR_GRAPH:
			dumpGraph((HChar*)arg[1], arg[2], True, False);
			break;
		case VG_USERREQ__BEGIN_STAGE:
			stageStart((Int)arg[1]);
			break;
		case VG_USERREQ__END_STAGE:
			stageEnd((Int)arg[1]);
			break;
		case VG_USERREQ__CLEAR_STAGE:
			stageClear((Int)arg[1]);
			break;
		case VG_USERREQ__ERROR_GREATER:
			*ret  = (UWord)isErrorGreater(arg[1], arg[2]);
			return True;
		case VG_USERREQ__RESET:
			resetShadowValues();
			break;
		case VG_USERREQ__INSERT_SHADOW:
			insertShadow(arg[1]);
			break;
		case VG_USERREQ__BEGIN:
			beginAnalyzing();
			break;
		case VG_USERREQ__END:
			endAnalyzing();
			break;
	}
	return False;
}

static void* gmp_alloc(size_t t) {
	return VG_(malloc)("fd.gmp_alloc.1", t);
}

static void* gmp_realloc(void* p, size_t t1, size_t t2) {
	return VG_(realloc)("fd.gmp_realloc.1", p, t1);
}

static void gmp_free(void* p, size_t t) {
	VG_(free)(p);
}

static void fd_post_clo_init(void) {
	VG_(umsg)("precision=%ld\n", clo_precision);
	VG_(umsg)("mean-error=%s\n", clo_computeMeanValue ? "yes" : "no");
	VG_(umsg)("ignore-libraries=%s\n", clo_ignoreLibraries ? "yes" : "no");
	VG_(umsg)("ignore-accurate=%s\n", clo_ignoreAccurate ? "yes" : "no");
	VG_(umsg)("sim-original=%s\n", clo_simulateOriginal ? "yes" : "no");
	VG_(umsg)("analyze-all=%s\n", clo_analyze ? "yes" : "no");
	VG_(umsg)("bad-cancellations=%s\n", clo_bad_cancellations ? "yes" : "no");
  VG_(umsg)("ignore-end=%s\n", clo_ignore_end ? "yes" : "no");

	mpfr_set_default_prec(clo_precision);

	globalMemory = VG_(HT_construct)("Global memory");
	meanValues = VG_(HT_construct)("Mean values");

	storeArgs = VG_(malloc)("fd.init.1", sizeof(Store));
	muxArgs = VG_(malloc)("fd.init.2", sizeof(ITE));
	unOpArgs = VG_(malloc)("fd.init.3", sizeof(UnOp));
	binOpArgs = VG_(malloc)("fd.init.4", sizeof(BinOp));
	triOpArgs = VG_(malloc)("fd.init.5", sizeof(TriOp));
	circRegs = VG_(malloc)("fd.init.6", sizeof(CircularRegs));
	threadRegisters = VG_(malloc)("fd.init.7", VG_N_THREADS * sizeof(ShadowValue**));

	mpfr_inits(meanOrg, meanRelError, NULL);
	mpfr_inits(stageOrg, stageDiff, stageRelError, NULL);
	mpfr_inits(dumpGraphOrg, dumpGraphRel, dumpGraphDiff, dumpGraphMeanError, dumpGraphErr1, dumpGraphErr2, NULL);
	mpfr_inits(endAnalysisOrg, endAnalysisRelError, NULL);

	mpfr_inits(introMaxError, introErr1, introErr2, NULL);
	mpfr_inits(compareIntroErr1, compareIntroErr2, NULL);
	mpfr_inits(writeSvOrg, writeSvDiff, writeSvRelError, NULL);
	mpfr_init(cancelTemp);
	mpfr_inits(arg1tmpX, arg2tmpX, arg3tmpX, NULL);

	Int i;
	for (i = 0; i < TMP_COUNT; i++) {
		sTmp[i] = VG_(malloc)("fd.init.7", sizeof(ShadowTmp));
		sTmp[i]->U128 = VG_(malloc)("fd.init.8", sizeof(UInt) * 4);	/* 128bit */
	}

	for (i = 0; i < CONST_COUNT; i++) {
		sConst[i] = VG_(malloc)("fd.init.9", sizeof(ShadowTmp));
	}

	Int j;
	for (i = 0; i < VG_N_THREADS; i++) {
		threadRegisters[i] = VG_(malloc)("fd.init.10", MAX_REGISTERS * sizeof(ShadowValue*));
		for (j = 0; j < MAX_REGISTERS; j++) {
			threadRegisters[i][j] = NULL;
		}
	}

	for (i = 0; i < MAX_TEMPS; i++) {
		localTemps[i] = NULL;
	}

	for (i = 0; i < MAX_STAGES; i++) {
		stages[i] = NULL;
		stageReports[i] = NULL;
	}

	unsupportedOps = VG_(OSetWord_Create)(VG_(malloc), "fd.init.10", VG_(free));
}

static void fd_pre_clo_init(void) {
   	VG_(details_name)            ("FpDebug");
   	VG_(details_version)         ("0.2");
   	VG_(details_description)     ("Floating-point arithmetic debugger");
   	VG_(details_copyright_author)("Copyright (C) 2010-2017 by Florian Benz.");
   	VG_(details_bug_reports_to)  ("florianbenz1@gmail.com");

   	VG_(basic_tool_funcs)        (fd_post_clo_init,
                                 fd_instrument,
                                 fd_fini);

	VG_(needs_command_line_options)(fd_process_cmd_line_options,
									fd_print_usage,
									fd_print_debug_usage);

	VG_(needs_client_requests)   (fd_handle_client_request);

	/* Calls to C library functions in GMP and MPFR have to be replaced with the Valgrind versions.
	   The function mp_set_memory_functions is part of GMP and thus MPFR, all others have been added
	   to MPFR. Therefore, this only works with pateched versions of GMP and MPFR. */
	mp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
	mpfr_set_strlen_function(VG_(strlen));
	mpfr_set_strcpy_function(VG_(strcpy));
	mpfr_set_memmove_function(VG_(memmove));
	mpfr_set_memcmp_function(VG_(memcmp));
	mpfr_set_memset_function(VG_(memset));
}

VG_DETERMINE_INTERFACE_VERSION(fd_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
