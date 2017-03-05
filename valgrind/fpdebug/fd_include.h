
/*--------------------------------------------------------------------*/
/*--- A header file for all parts of the FpDebug tool.             ---*/
/*---                                                 fd_include.h ---*/
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
   The FpDebug tool uses MPFR, which relies on GMP, and thus has to be linked against adjusted
   versions of GMP and MPFR. The adjusted version are limited to the parts of the C library
   provided by Valgrind.

   All programs using GMP must link against the ‘libgmp’ library. On a typical Unix-like system
   this can be done with ‘-lgmp’, for example
   gcc myprogram.c -lgmp

   If GMP has been installed to a non-standard location then it may be necessary to use ‘-I’ and
   ‘-L’ compiler options to point to the right directories, and some sort of run-time path for a
   shared library.
*/

#ifndef __FD_INCLUDE_H
#define __FD_INCLUDE_H

#include <mpfr.h>

#include "pub_tool_xarray.h"
#include "pub_tool_hashtable.h"

typedef
   enum {
      Ot_INVALID,
      Ot_FLOAT,
      Ot_DOUBLE
   }
   OrgType;

/*
	Node with per-allocation information that will be stored in a hash map.
	As specified in <pub_tool_hashtable.h>, the first member must be a pointer
	and the second member must be an UWord.
 */
typedef struct _ShadowValue {
	struct _ShadowValue* 	next;
		UWord              	key;

		Bool				active;
		UInt				version;

		ULong				opCount;
		Addr				origin;

		mpfr_exp_t			canceled;
		Addr				cancelOrigin;

		OrgType				orgType;
		union {
			Float			fl;
			Double			db;
		} Org;

		mpfr_t				value;
	} ShadowValue;

typedef struct _MeanValue {
	struct _MeanValue* 	next;
		UWord              	key;

		IROp				op;
		UInt				count;
		mpfr_t				sum;
		mpfr_t				max;

		mpfr_exp_t			canceledMax;
		mpfr_exp_t			canceledSum;

		UInt				cancellationBadnessMax;
		UInt				cancellationBadnessSum;

		Addr				arg1;
		Addr				arg2;

		Bool				visited;

		Bool				overflow;
	} MeanValue;

typedef
	struct {
		Bool				active;
		UInt				count;

		VgHashTable* oldVals;
		VgHashTable* newVals;
		VgHashTable* limits;
	} Stage;

typedef struct _StageValue {
	struct _StageValue* 	next;
		UWord              	key;

		mpfr_t				val;
		mpfr_t				relError;
	} StageValue;

typedef struct _StageLimit {
	struct _StageLimit* 	next;
		UWord              	key;

		mpfr_t				limit;
	} StageLimit;

typedef struct _StageReport {
	struct _StageReport* 	next;
		UWord              	key;

		UInt				count;
		UInt				iterMin;
		UInt				iterMax;
		Addr				origin;
	} StageReport;

typedef
	struct {
		IROp	op;
		IRTemp  wrTmp;
		IRTemp	cond;
		IRTemp	expr0;
		IRTemp	exprX;

		UChar	condVal;
	} ITE;

typedef
	struct {
		Float	orgFloat;
		Double 	orgDouble;
	} Store;

typedef
	struct {
		UWord	bias;
		Int		ix;
	} CircularRegs;

typedef
	struct {
		IROp	op;
		IRTemp  wrTmp;
		IRTemp	arg;

		Float	orgFloat;
		Double 	orgDouble;
	} UnOp;

typedef
	struct {
		IROp	op;
		IRTemp  wrTmp;
		IRTemp	arg1;
		IRTemp	arg2;

		Float	orgFloat;
		Double 	orgDouble;
	} BinOp;

typedef
	struct {
		IROp	op;
		IRTemp  wrTmp;
		IRTemp	arg1;
		IRTemp	arg2;
		IRTemp	arg3;

		Double 	orgDouble;
	} TriOp;

typedef
	struct {
		IRType 	type;
		UInt*	U128;

		union {
			Int		I32;
			Long	I64;
			Float	F32;
			Double	F64;
		} Val;
	} ShadowTmp;

typedef
	struct {
		IRConstTag tag;

		union {
			Bool   U1;
         	UChar  U8;
         	UShort U16;
         	UInt   U32;
         	ULong  U64;
         	Double F64;
         	ULong  F64i;
         	UShort V128;
		} Val;
	} ShadowConst;

#endif /* ndef __FD_INCLUDE_H */
