
/*--------------------------------------------------------------------*/
/*--- A helper file for the FpDebug tool.                          ---*/
/*---                                                 opToString.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of FpDebug, a heavyweight Valgrind tool for
   detecting floating-point accuracy problems.

   Copyright (C) 2010-2011 Florian Benz 
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

static HChar* opStr = NULL;

static void storeOpStr(HChar* str) {
   opStr = VG_(realloc)("storeOpStr", opStr, sizeof(HChar) * (VG_(strlen)(str) + 1));
   VG_(strcpy)(opStr, str);
}

static void opToStr(IROp op) {
   HChar* str = NULL; 
   IROp   base;
   switch (op) {
      case Iop_Add8 ... Iop_Add64:
         str = "Add"; base = Iop_Add8; break;
      case Iop_Sub8 ... Iop_Sub64:
         str = "Sub"; base = Iop_Sub8; break;
      case Iop_Mul8 ... Iop_Mul64:
         str = "Mul"; base = Iop_Mul8; break;
      case Iop_Or8 ... Iop_Or64:
         str = "Or"; base = Iop_Or8; break;
      case Iop_And8 ... Iop_And64:
         str = "And"; base = Iop_And8; break;
      case Iop_Xor8 ... Iop_Xor64:
         str = "Xor"; base = Iop_Xor8; break;
      case Iop_Shl8 ... Iop_Shl64:
         str = "Shl"; base = Iop_Shl8; break;
      case Iop_Shr8 ... Iop_Shr64:
         str = "Shr"; base = Iop_Shr8; break;
      case Iop_Sar8 ... Iop_Sar64:
         str = "Sar"; base = Iop_Sar8; break;
      case Iop_CmpEQ8 ... Iop_CmpEQ64:
         str = "CmpEQ"; base = Iop_CmpEQ8; break;
      case Iop_CmpNE8 ... Iop_CmpNE64:
         str = "CmpNE"; base = Iop_CmpNE8; break;
      case Iop_CasCmpEQ8 ... Iop_CasCmpEQ64:
         str = "CasCmpEQ"; base = Iop_CasCmpEQ8; break;
      case Iop_CasCmpNE8 ... Iop_CasCmpNE64:
         str = "CasCmpNE"; base = Iop_CasCmpNE8; break;
      case Iop_Not8 ... Iop_Not64:
         str = "Not"; base = Iop_Not8; break;
      /* other cases must explicitly "return;" */
      case Iop_8Uto16:   storeOpStr("8Uto16");  return;
      case Iop_8Uto32:   storeOpStr("8Uto32");  return;
      case Iop_16Uto32:  storeOpStr("16Uto32"); return;
      case Iop_8Sto16:   storeOpStr("8Sto16");  return;
      case Iop_8Sto32:   storeOpStr("8Sto32");  return;
      case Iop_16Sto32:  storeOpStr("16Sto32"); return;
      case Iop_32Sto64:  storeOpStr("32Sto64"); return;
      case Iop_32Uto64:  storeOpStr("32Uto64"); return;
      case Iop_32to8:    storeOpStr("32to8");   return;
      case Iop_16Uto64:  storeOpStr("16Uto64"); return;
      case Iop_16Sto64:  storeOpStr("16Sto64"); return;
      case Iop_8Uto64:   storeOpStr("8Uto64"); return;
      case Iop_8Sto64:   storeOpStr("8Sto64"); return;
      case Iop_64to16:   storeOpStr("64to16"); return;
      case Iop_64to8:    storeOpStr("64to8");  return;

      case Iop_Not1:     storeOpStr("Not1");    return;
      case Iop_32to1:    storeOpStr("32to1");   return;
      case Iop_64to1:    storeOpStr("64to1");   return;
      case Iop_1Uto8:    storeOpStr("1Uto8");   return;
      case Iop_1Uto32:   storeOpStr("1Uto32");  return;
      case Iop_1Uto64:   storeOpStr("1Uto64");  return;
      case Iop_1Sto8:    storeOpStr("1Sto8");  return;
      case Iop_1Sto16:   storeOpStr("1Sto16");  return;
      case Iop_1Sto32:   storeOpStr("1Sto32");  return;
      case Iop_1Sto64:   storeOpStr("1Sto64");  return;

      case Iop_MullS8:   storeOpStr("MullS8");  return;
      case Iop_MullS16:  storeOpStr("MullS16"); return;
      case Iop_MullS32:  storeOpStr("MullS32"); return;
      case Iop_MullS64:  storeOpStr("MullS64"); return;
      case Iop_MullU8:   storeOpStr("MullU8");  return;
      case Iop_MullU16:  storeOpStr("MullU16"); return;
      case Iop_MullU32:  storeOpStr("MullU32"); return;
      case Iop_MullU64:  storeOpStr("MullU64"); return;

      case Iop_Clz64:    storeOpStr("Clz64"); return;
      case Iop_Clz32:    storeOpStr("Clz32"); return;
      case Iop_Ctz64:    storeOpStr("Ctz64"); return;
      case Iop_Ctz32:    storeOpStr("Ctz32"); return;

      case Iop_CmpLT32S: storeOpStr("CmpLT32S"); return;
      case Iop_CmpLE32S: storeOpStr("CmpLE32S"); return;
      case Iop_CmpLT32U: storeOpStr("CmpLT32U"); return;
      case Iop_CmpLE32U: storeOpStr("CmpLE32U"); return;

      case Iop_CmpLT64S: storeOpStr("CmpLT64S"); return;
      case Iop_CmpLE64S: storeOpStr("CmpLE64S"); return;
      case Iop_CmpLT64U: storeOpStr("CmpLT64U"); return;
      case Iop_CmpLE64U: storeOpStr("CmpLE64U"); return;

      case Iop_CmpNEZ8:  storeOpStr("CmpNEZ8"); return;
      case Iop_CmpNEZ16: storeOpStr("CmpNEZ16"); return;
      case Iop_CmpNEZ32: storeOpStr("CmpNEZ32"); return;
      case Iop_CmpNEZ64: storeOpStr("CmpNEZ64"); return;

      case Iop_CmpwNEZ32: storeOpStr("CmpwNEZ32"); return;
      case Iop_CmpwNEZ64: storeOpStr("CmpwNEZ64"); return;

      case Iop_Left8:  storeOpStr("Left8"); return;
      case Iop_Left16: storeOpStr("Left16"); return;
      case Iop_Left32: storeOpStr("Left32"); return;
      case Iop_Left64: storeOpStr("Left64"); return;
      case Iop_Max32U: storeOpStr("Max32U"); return;

      case Iop_CmpORD32U: storeOpStr("CmpORD32U"); return;
      case Iop_CmpORD32S: storeOpStr("CmpORD32S"); return;

      case Iop_CmpORD64U: storeOpStr("CmpORD64U"); return;
      case Iop_CmpORD64S: storeOpStr("CmpORD64S"); return;

      case Iop_DivU32: storeOpStr("DivU32"); return;
      case Iop_DivS32: storeOpStr("DivS32"); return;
      case Iop_DivU64: storeOpStr("DivU64"); return;
      case Iop_DivS64: storeOpStr("DivS64"); return;

      case Iop_DivModU64to32: storeOpStr("DivModU64to32"); return;
      case Iop_DivModS64to32: storeOpStr("DivModS64to32"); return;

      case Iop_DivModU128to64: storeOpStr("DivModU128to64"); return;
      case Iop_DivModS128to64: storeOpStr("DivModS128to64"); return;

      case Iop_16HIto8:  storeOpStr("16HIto8"); return;
      case Iop_16to8:    storeOpStr("16to8");   return;
      case Iop_8HLto16:  storeOpStr("8HLto16"); return;

      case Iop_32HIto16: storeOpStr("32HIto16"); return;
      case Iop_32to16:   storeOpStr("32to16");   return;
      case Iop_16HLto32: storeOpStr("16HLto32"); return;

      case Iop_64HIto32: storeOpStr("64HIto32"); return;
      case Iop_64to32:   storeOpStr("64to32");   return;
      case Iop_32HLto64: storeOpStr("32HLto64"); return;

      case Iop_128HIto64: storeOpStr("128HIto64"); return;
      case Iop_128to64:   storeOpStr("128to64");   return;
      case Iop_64HLto128: storeOpStr("64HLto128"); return;

      case Iop_AddF64:    storeOpStr("AddF64"); return;
      case Iop_SubF64:    storeOpStr("SubF64"); return;
      case Iop_MulF64:    storeOpStr("MulF64"); return;
      case Iop_DivF64:    storeOpStr("DivF64"); return;
      case Iop_AddF64r32: storeOpStr("AddF64r32"); return;
      case Iop_SubF64r32: storeOpStr("SubF64r32"); return;
      case Iop_MulF64r32: storeOpStr("MulF64r32"); return;
      case Iop_DivF64r32: storeOpStr("DivF64r32"); return;
      case Iop_AddF32:    storeOpStr("AddF32"); return;
      case Iop_SubF32:    storeOpStr("SubF32"); return;
      case Iop_MulF32:    storeOpStr("MulF32"); return;
      case Iop_DivF32:    storeOpStr("DivF32"); return;

      case Iop_ScaleF64:      storeOpStr("ScaleF64"); return;
      case Iop_AtanF64:       storeOpStr("AtanF64"); return;
      case Iop_Yl2xF64:       storeOpStr("Yl2xF64"); return;
      case Iop_Yl2xp1F64:     storeOpStr("Yl2xp1F64"); return;
      case Iop_PRemF64:       storeOpStr("PRemF64"); return;
      case Iop_PRemC3210F64:  storeOpStr("PRemC3210F64"); return;
      case Iop_PRem1F64:      storeOpStr("PRem1F64"); return;
      case Iop_PRem1C3210F64: storeOpStr("PRem1C3210F64"); return;
      case Iop_NegF64:        storeOpStr("NegF64"); return;
      case Iop_AbsF64:        storeOpStr("AbsF64"); return;
      case Iop_NegF32:        storeOpStr("NegF32"); return;
      case Iop_AbsF32:        storeOpStr("AbsF32"); return;
      case Iop_SqrtF64:       storeOpStr("SqrtF64"); return;
      case Iop_SqrtF32:       storeOpStr("SqrtF32"); return;
      case Iop_SinF64:    storeOpStr("SinF64"); return;
      case Iop_CosF64:    storeOpStr("CosF64"); return;
      case Iop_TanF64:    storeOpStr("TanF64"); return;
      case Iop_2xm1F64:   storeOpStr("2xm1F64"); return;

      case Iop_MAddF64:    storeOpStr("MAddF64"); return;
      case Iop_MSubF64:    storeOpStr("MSubF64"); return;
      case Iop_MAddF64r32: storeOpStr("MAddF64r32"); return;
      case Iop_MSubF64r32: storeOpStr("MSubF64r32"); return;

      case Iop_Est5FRSqrt:    storeOpStr("Est5FRSqrt"); return;
      case Iop_RoundF64toF64_NEAREST: storeOpStr("RoundF64toF64_NEAREST"); return;
      case Iop_RoundF64toF64_NegINF: storeOpStr("RoundF64toF64_NegINF"); return;
      case Iop_RoundF64toF64_PosINF: storeOpStr("RoundF64toF64_PosINF"); return;
      case Iop_RoundF64toF64_ZERO: storeOpStr("RoundF64toF64_ZERO"); return;

      case Iop_TruncF64asF32: storeOpStr("TruncF64asF32"); return;
      case Iop_CalcFPRF:      storeOpStr("CalcFPRF"); return;

      case Iop_Add16x2:   storeOpStr("Add16x2"); return;
      case Iop_Sub16x2:   storeOpStr("Sub16x2"); return;
      case Iop_QAdd16Sx2: storeOpStr("QAdd16Sx2"); return;
      case Iop_QAdd16Ux2: storeOpStr("QAdd16Ux2"); return;
      case Iop_QSub16Sx2: storeOpStr("QSub16Sx2"); return;
      case Iop_QSub16Ux2: storeOpStr("QSub16Ux2"); return;
      case Iop_HAdd16Ux2: storeOpStr("HAdd16Ux2"); return;
      case Iop_HAdd16Sx2: storeOpStr("HAdd16Sx2"); return;
      case Iop_HSub16Ux2: storeOpStr("HSub16Ux2"); return;
      case Iop_HSub16Sx2: storeOpStr("HSub16Sx2"); return;

      case Iop_Add8x4:   storeOpStr("Add8x4"); return;
      case Iop_Sub8x4:   storeOpStr("Sub8x4"); return;
      case Iop_QAdd8Sx4: storeOpStr("QAdd8Sx4"); return;
      case Iop_QAdd8Ux4: storeOpStr("QAdd8Ux4"); return;
      case Iop_QSub8Sx4: storeOpStr("QSub8Sx4"); return;
      case Iop_QSub8Ux4: storeOpStr("QSub8Ux4"); return;
      case Iop_HAdd8Ux4: storeOpStr("HAdd8Ux4"); return;
      case Iop_HAdd8Sx4: storeOpStr("HAdd8Sx4"); return;
      case Iop_HSub8Ux4: storeOpStr("HSub8Ux4"); return;
      case Iop_HSub8Sx4: storeOpStr("HSub8Sx4"); return;
      case Iop_Sad8Ux4:  storeOpStr("Sad8Ux4"); return;

      case Iop_CmpNEZ16x2: storeOpStr("CmpNEZ16x2"); return;
      case Iop_CmpNEZ8x4:  storeOpStr("CmpNEZ8x4"); return;

      case Iop_CmpF64:    storeOpStr("CmpF64"); return;

      case Iop_F64toI16S: storeOpStr("F64toI16S"); return;
      case Iop_F64toI32S: storeOpStr("F64toI32S"); return;
      case Iop_F64toI64S: storeOpStr("F64toI64S"); return;

      case Iop_F64toI32U: storeOpStr("F64toI32U"); return;

      case Iop_I16StoF64: storeOpStr("I16StoF64"); return;
      case Iop_I32StoF64: storeOpStr("I32StoF64"); return;
      case Iop_I64StoF64: storeOpStr("I64StoF64"); return;

      case Iop_I32UtoF64: storeOpStr("I32UtoF64"); return;

      case Iop_F32toF64: storeOpStr("F32toF64"); return;
      case Iop_F64toF32: storeOpStr("F64toF32"); return;

      case Iop_RoundF64toInt: storeOpStr("RoundF64toInt"); return;
      case Iop_RoundF32toInt: storeOpStr("RoundF32toInt"); return;
      case Iop_RoundF64toF32: storeOpStr("RoundF64toF32"); return;

      case Iop_ReinterpF64asI64: storeOpStr("ReinterpF64asI64"); return;
      case Iop_ReinterpI64asF64: storeOpStr("ReinterpI64asF64"); return;
      case Iop_ReinterpF32asI32: storeOpStr("ReinterpF32asI32"); return;
      case Iop_ReinterpI32asF32: storeOpStr("ReinterpI32asF32"); return;

      case Iop_I32UtoFx4: storeOpStr("I32UtoFx4"); return;
      case Iop_I32StoFx4: storeOpStr("I32StoFx4"); return;

      case Iop_F32toF16x4: storeOpStr("F32toF16x4"); return;
      case Iop_F16toF32x4: storeOpStr("F16toF32x4"); return;

      case Iop_Rsqrte32Fx4: storeOpStr("VRsqrte32Fx4"); return;
      case Iop_Rsqrte32x4:  storeOpStr("VRsqrte32x4"); return;
      case Iop_Rsqrte32Fx2: storeOpStr("VRsqrte32Fx2"); return;
      case Iop_Rsqrte32x2:  storeOpStr("VRsqrte32x2"); return;

      case Iop_QFtoI32Ux4_RZ: storeOpStr("QFtoI32Ux4_RZ"); return;
      case Iop_QFtoI32Sx4_RZ: storeOpStr("QFtoI32Sx4_RZ"); return;

      case Iop_FtoI32Ux4_RZ: storeOpStr("FtoI32Ux4_RZ"); return;
      case Iop_FtoI32Sx4_RZ: storeOpStr("FtoI32Sx4_RZ"); return;

      case Iop_I32UtoFx2: storeOpStr("I32UtoFx2"); return;
      case Iop_I32StoFx2: storeOpStr("I32StoFx2"); return;

      case Iop_FtoI32Ux2_RZ: storeOpStr("FtoI32Ux2_RZ"); return;
      case Iop_FtoI32Sx2_RZ: storeOpStr("FtoI32Sx2_RZ"); return;

      case Iop_RoundF32x4_RM: storeOpStr("RoundF32x4_RM"); return;
      case Iop_RoundF32x4_RP: storeOpStr("RoundF32x4_RP"); return;
      case Iop_RoundF32x4_RN: storeOpStr("RoundF32x4_RN"); return;
      case Iop_RoundF32x4_RZ: storeOpStr("RoundF32x4_RZ"); return;

      case Iop_Abs8x8: storeOpStr("Abs8x8"); return;
      case Iop_Abs16x4: storeOpStr("Abs16x4"); return;
      case Iop_Abs32x2: storeOpStr("Abs32x2"); return;
      case Iop_Add8x8: storeOpStr("Add8x8"); return;
      case Iop_Add16x4: storeOpStr("Add16x4"); return;
      case Iop_Add32x2: storeOpStr("Add32x2"); return;
      case Iop_QAdd8Ux8: storeOpStr("QAdd8Ux8"); return;
      case Iop_QAdd16Ux4: storeOpStr("QAdd16Ux4"); return;
      case Iop_QAdd32Ux2: storeOpStr("QAdd32Ux2"); return;
      case Iop_QAdd64Ux1: storeOpStr("QAdd64Ux1"); return;
      case Iop_QAdd8Sx8: storeOpStr("QAdd8Sx8"); return;
      case Iop_QAdd16Sx4: storeOpStr("QAdd16Sx4"); return;
      case Iop_QAdd32Sx2: storeOpStr("QAdd32Sx2"); return;
      case Iop_QAdd64Sx1: storeOpStr("QAdd64Sx1"); return;
      case Iop_PwAdd8x8: storeOpStr("PwAdd8x8"); return;
      case Iop_PwAdd16x4: storeOpStr("PwAdd16x4"); return;
      case Iop_PwAdd32x2: storeOpStr("PwAdd32x2"); return;
      case Iop_PwAdd32Fx2: storeOpStr("PwAdd32Fx2"); return;
      case Iop_PwAddL8Ux8: storeOpStr("PwAddL8Ux8"); return;
      case Iop_PwAddL16Ux4: storeOpStr("PwAddL16Ux4"); return;
      case Iop_PwAddL32Ux2: storeOpStr("PwAddL32Ux2"); return;
      case Iop_PwAddL8Sx8: storeOpStr("PwAddL8Sx8"); return;
      case Iop_PwAddL16Sx4: storeOpStr("PwAddL16Sx4"); return;
      case Iop_PwAddL32Sx2: storeOpStr("PwAddL32Sx2"); return;
      case Iop_Sub8x8: storeOpStr("Sub8x8"); return;
      case Iop_Sub16x4: storeOpStr("Sub16x4"); return;
      case Iop_Sub32x2: storeOpStr("Sub32x2"); return;
      case Iop_QSub8Ux8: storeOpStr("QSub8Ux8"); return;
      case Iop_QSub16Ux4: storeOpStr("QSub16Ux4"); return;
      case Iop_QSub32Ux2: storeOpStr("QSub32Ux2"); return;
      case Iop_QSub64Ux1: storeOpStr("QSub64Ux1"); return;
      case Iop_QSub8Sx8: storeOpStr("QSub8Sx8"); return;
      case Iop_QSub16Sx4: storeOpStr("QSub16Sx4"); return;
      case Iop_QSub32Sx2: storeOpStr("QSub32Sx2"); return;
      case Iop_QSub64Sx1: storeOpStr("QSub64Sx1"); return;
      case Iop_Mul8x8: storeOpStr("Mul8x8"); return;
      case Iop_Mul16x4: storeOpStr("Mul16x4"); return;
      case Iop_Mul32x2: storeOpStr("Mul32x2"); return;
      case Iop_Mul32Fx2: storeOpStr("Mul32Fx2"); return;
      case Iop_PolynomialMul8x8: storeOpStr("PolynomialMul8x8"); return;
      case Iop_MulHi16Ux4: storeOpStr("MulHi16Ux4"); return;
      case Iop_MulHi16Sx4: storeOpStr("MulHi16Sx4"); return;
      case Iop_QDMulHi16Sx4: storeOpStr("QDMulHi16Sx4"); return;
      case Iop_QDMulHi32Sx2: storeOpStr("QDMulHi32Sx2"); return;
      case Iop_QRDMulHi16Sx4: storeOpStr("QRDMulHi16Sx4"); return;
      case Iop_QRDMulHi32Sx2: storeOpStr("QRDMulHi32Sx2"); return;
      case Iop_QDMulLong16Sx4: storeOpStr("QDMulLong16Sx4"); return;
      case Iop_QDMulLong32Sx2: storeOpStr("QDMulLong32Sx2"); return;
      case Iop_Avg8Ux8: storeOpStr("Avg8Ux8"); return;
      case Iop_Avg16Ux4: storeOpStr("Avg16Ux4"); return;
      case Iop_Max8Sx8: storeOpStr("Max8Sx8"); return;
      case Iop_Max16Sx4: storeOpStr("Max16Sx4"); return;
      case Iop_Max32Sx2: storeOpStr("Max32Sx2"); return;
      case Iop_Max8Ux8: storeOpStr("Max8Ux8"); return;
      case Iop_Max16Ux4: storeOpStr("Max16Ux4"); return;
      case Iop_Max32Ux2: storeOpStr("Max32Ux2"); return;
      case Iop_Min8Sx8: storeOpStr("Min8Sx8"); return;
      case Iop_Min16Sx4: storeOpStr("Min16Sx4"); return;
      case Iop_Min32Sx2: storeOpStr("Min32Sx2"); return;
      case Iop_Min8Ux8: storeOpStr("Min8Ux8"); return;
      case Iop_Min16Ux4: storeOpStr("Min16Ux4"); return;
      case Iop_Min32Ux2: storeOpStr("Min32Ux2"); return;
      case Iop_PwMax8Sx8: storeOpStr("PwMax8Sx8"); return;
      case Iop_PwMax16Sx4: storeOpStr("PwMax16Sx4"); return;
      case Iop_PwMax32Sx2: storeOpStr("PwMax32Sx2"); return;
      case Iop_PwMax8Ux8: storeOpStr("PwMax8Ux8"); return;
      case Iop_PwMax16Ux4: storeOpStr("PwMax16Ux4"); return;
      case Iop_PwMax32Ux2: storeOpStr("PwMax32Ux2"); return;
      case Iop_PwMin8Sx8: storeOpStr("PwMin8Sx8"); return;
      case Iop_PwMin16Sx4: storeOpStr("PwMin16Sx4"); return;
      case Iop_PwMin32Sx2: storeOpStr("PwMin32Sx2"); return;
      case Iop_PwMin8Ux8: storeOpStr("PwMin8Ux8"); return;
      case Iop_PwMin16Ux4: storeOpStr("PwMin16Ux4"); return;
      case Iop_PwMin32Ux2: storeOpStr("PwMin32Ux2"); return;
      case Iop_CmpEQ8x8: storeOpStr("CmpEQ8x8"); return;
      case Iop_CmpEQ16x4: storeOpStr("CmpEQ16x4"); return;
      case Iop_CmpEQ32x2: storeOpStr("CmpEQ32x2"); return;
      case Iop_CmpGT8Ux8: storeOpStr("CmpGT8Ux8"); return;
      case Iop_CmpGT16Ux4: storeOpStr("CmpGT16Ux4"); return;
      case Iop_CmpGT32Ux2: storeOpStr("CmpGT32Ux2"); return;
      case Iop_CmpGT8Sx8: storeOpStr("CmpGT8Sx8"); return;
      case Iop_CmpGT16Sx4: storeOpStr("CmpGT16Sx4"); return;
      case Iop_CmpGT32Sx2: storeOpStr("CmpGT32Sx2"); return;
      case Iop_Cnt8x8: storeOpStr("Cnt8x8"); return;
      case Iop_Clz8Sx8: storeOpStr("Clz8Sx8"); return;
      case Iop_Clz16Sx4: storeOpStr("Clz16Sx4"); return;
      case Iop_Clz32Sx2: storeOpStr("Clz32Sx2"); return;
      case Iop_Cls8Sx8: storeOpStr("Cls8Sx8"); return;
      case Iop_Cls16Sx4: storeOpStr("Cls16Sx4"); return;
      case Iop_Cls32Sx2: storeOpStr("Cls32Sx2"); return;
      case Iop_ShlN8x8: storeOpStr("ShlN8x8"); return;
      case Iop_ShlN16x4: storeOpStr("ShlN16x4"); return;
      case Iop_ShlN32x2: storeOpStr("ShlN32x2"); return;
      case Iop_ShrN8x8: storeOpStr("ShrN8x8"); return;
      case Iop_ShrN16x4: storeOpStr("ShrN16x4"); return;
      case Iop_ShrN32x2: storeOpStr("ShrN32x2"); return;
      case Iop_SarN8x8: storeOpStr("SarN8x8"); return;
      case Iop_SarN16x4: storeOpStr("SarN16x4"); return;
      case Iop_SarN32x2: storeOpStr("SarN32x2"); return;
      case Iop_QNarrow16Ux4: storeOpStr("QNarrow16Ux4"); return;
      case Iop_QNarrow16Sx4: storeOpStr("QNarrow16Sx4"); return;
      case Iop_QNarrow32Sx2: storeOpStr("QNarrow32Sx2"); return;
      case Iop_InterleaveHI8x8: storeOpStr("InterleaveHI8x8"); return;
      case Iop_InterleaveHI16x4: storeOpStr("InterleaveHI16x4"); return;
      case Iop_InterleaveHI32x2: storeOpStr("InterleaveHI32x2"); return;
      case Iop_InterleaveLO8x8: storeOpStr("InterleaveLO8x8"); return;
      case Iop_InterleaveLO16x4: storeOpStr("InterleaveLO16x4"); return;
      case Iop_InterleaveLO32x2: storeOpStr("InterleaveLO32x2"); return;
      case Iop_CatOddLanes8x8: storeOpStr("CatOddLanes8x8"); return;
      case Iop_CatOddLanes16x4: storeOpStr("CatOddLanes16x4"); return;
      case Iop_CatEvenLanes8x8: storeOpStr("CatEvenLanes8x8"); return;
      case Iop_CatEvenLanes16x4: storeOpStr("CatEvenLanes16x4"); return;
      case Iop_InterleaveOddLanes8x8: storeOpStr("InterleaveOddLanes8x8"); return;
      case Iop_InterleaveOddLanes16x4: storeOpStr("InterleaveOddLanes16x4"); return;
      case Iop_InterleaveEvenLanes8x8: storeOpStr("InterleaveEvenLanes8x8"); return;
      case Iop_InterleaveEvenLanes16x4: storeOpStr("InterleaveEvenLanes16x4"); return;
      case Iop_Shl8x8: storeOpStr("Shl8x8"); return;
      case Iop_Shl16x4: storeOpStr("Shl16x4"); return;
      case Iop_Shl32x2: storeOpStr("Shl32x2"); return;
      case Iop_Shr8x8: storeOpStr("Shr8x8"); return;
      case Iop_Shr16x4: storeOpStr("Shr16x4"); return;
      case Iop_Shr32x2: storeOpStr("Shr32x2"); return;
      case Iop_QShl8x8: storeOpStr("QShl8x8"); return;
      case Iop_QShl16x4: storeOpStr("QShl16x4"); return;
      case Iop_QShl32x2: storeOpStr("QShl32x2"); return;
      case Iop_QShl64x1: storeOpStr("QShl64x1"); return;
      case Iop_QSal8x8: storeOpStr("QSal8x8"); return;
      case Iop_QSal16x4: storeOpStr("QSal16x4"); return;
      case Iop_QSal32x2: storeOpStr("QSal32x2"); return;
      case Iop_QSal64x1: storeOpStr("QSal64x1"); return;
      case Iop_QShlN8x8: storeOpStr("QShlN8x8"); return;
      case Iop_QShlN16x4: storeOpStr("QShlN16x4"); return;
      case Iop_QShlN32x2: storeOpStr("QShlN32x2"); return;
      case Iop_QShlN64x1: storeOpStr("QShlN64x1"); return;
      case Iop_QShlN8Sx8: storeOpStr("QShlN8Sx8"); return;
      case Iop_QShlN16Sx4: storeOpStr("QShlN16Sx4"); return;
      case Iop_QShlN32Sx2: storeOpStr("QShlN32Sx2"); return;
      case Iop_QShlN64Sx1: storeOpStr("QShlN64Sx1"); return;
      case Iop_QSalN8x8: storeOpStr("QSalN8x8"); return;
      case Iop_QSalN16x4: storeOpStr("QSalN16x4"); return;
      case Iop_QSalN32x2: storeOpStr("QSalN32x2"); return;
      case Iop_QSalN64x1: storeOpStr("QSalN64x1"); return;
      case Iop_Sar8x8: storeOpStr("Sar8x8"); return;
      case Iop_Sar16x4: storeOpStr("Sar16x4"); return;
      case Iop_Sar32x2: storeOpStr("Sar32x2"); return;
      case Iop_Sal8x8: storeOpStr("Sal8x8"); return;
      case Iop_Sal16x4: storeOpStr("Sal16x4"); return;
      case Iop_Sal32x2: storeOpStr("Sal32x2"); return;
      case Iop_Sal64x1: storeOpStr("Sal64x1"); return;
      case Iop_Perm8x8: storeOpStr("Perm8x8"); return;
      case Iop_Reverse16_8x8: storeOpStr("Reverse16_8x8"); return;
      case Iop_Reverse32_8x8: storeOpStr("Reverse32_8x8"); return;
      case Iop_Reverse32_16x4: storeOpStr("Reverse32_16x4"); return;
      case Iop_Reverse64_8x8: storeOpStr("Reverse64_8x8"); return;
      case Iop_Reverse64_16x4: storeOpStr("Reverse64_16x4"); return;
      case Iop_Reverse64_32x2: storeOpStr("Reverse64_32x2"); return;
      case Iop_Abs32Fx2: storeOpStr("Abs32Fx2"); return;

      case Iop_CmpNEZ32x2: storeOpStr("CmpNEZ32x2"); return;
      case Iop_CmpNEZ16x4: storeOpStr("CmpNEZ16x4"); return;
      case Iop_CmpNEZ8x8:  storeOpStr("CmpNEZ8x8"); return;

      case Iop_Add32Fx4:  storeOpStr("Add32Fx4"); return;
      case Iop_Add32Fx2:  storeOpStr("Add32Fx2"); return;
      case Iop_Add32F0x4: storeOpStr("Add32F0x4"); return;
      case Iop_Add64Fx2:  storeOpStr("Add64Fx2"); return;
      case Iop_Add64F0x2: storeOpStr("Add64F0x2"); return;

      case Iop_Div32Fx4:  storeOpStr("Div32Fx4"); return;
      case Iop_Div32F0x4: storeOpStr("Div32F0x4"); return;
      case Iop_Div64Fx2:  storeOpStr("Div64Fx2"); return;
      case Iop_Div64F0x2: storeOpStr("Div64F0x2"); return;

      case Iop_Max32Fx4:  storeOpStr("Max32Fx4"); return;
      case Iop_Max32Fx2:  storeOpStr("Max32Fx2"); return;
      case Iop_PwMax32Fx4:  storeOpStr("PwMax32Fx4"); return;
      case Iop_PwMax32Fx2:  storeOpStr("PwMax32Fx2"); return;
      case Iop_Max32F0x4: storeOpStr("Max32F0x4"); return;
      case Iop_Max64Fx2:  storeOpStr("Max64Fx2"); return;
      case Iop_Max64F0x2: storeOpStr("Max64F0x2"); return;

      case Iop_Min32Fx4:  storeOpStr("Min32Fx4"); return;
      case Iop_Min32Fx2:  storeOpStr("Min32Fx2"); return;
      case Iop_PwMin32Fx4:  storeOpStr("PwMin32Fx4"); return;
      case Iop_PwMin32Fx2:  storeOpStr("PwMin32Fx2"); return;
      case Iop_Min32F0x4: storeOpStr("Min32F0x4"); return;
      case Iop_Min64Fx2:  storeOpStr("Min64Fx2"); return;
      case Iop_Min64F0x2: storeOpStr("Min64F0x2"); return;

      case Iop_Mul32Fx4:  storeOpStr("Mul32Fx4"); return;
      case Iop_Mul32F0x4: storeOpStr("Mul32F0x4"); return;
      case Iop_Mul64Fx2:  storeOpStr("Mul64Fx2"); return;
      case Iop_Mul64F0x2: storeOpStr("Mul64F0x2"); return;

      case Iop_Recip32x2: storeOpStr("Recip32x2"); return;
      case Iop_Recip32Fx2:  storeOpStr("Recip32Fx2"); return;
      case Iop_Recip32Fx4:  storeOpStr("Recip32Fx4"); return;
      case Iop_Recip32x4:  storeOpStr("Recip32x4"); return;
      case Iop_Recip32F0x4: storeOpStr("Recip32F0x4"); return;
      case Iop_Recip64Fx2:  storeOpStr("Recip64Fx2"); return;
      case Iop_Recip64F0x2: storeOpStr("Recip64F0x2"); return;
      case Iop_Recps32Fx2:  storeOpStr("VRecps32Fx2"); return;
      case Iop_Recps32Fx4:  storeOpStr("VRecps32Fx4"); return;
      case Iop_Abs32Fx4:  storeOpStr("Abs32Fx4"); return;
      case Iop_Rsqrts32Fx4:  storeOpStr("VRsqrts32Fx4"); return;
      case Iop_Rsqrts32Fx2:  storeOpStr("VRsqrts32Fx2"); return;

      case Iop_RSqrt32Fx4:  storeOpStr("RSqrt32Fx4"); return;
      case Iop_RSqrt32F0x4: storeOpStr("RSqrt32F0x4"); return;
      case Iop_RSqrt64Fx2:  storeOpStr("RSqrt64Fx2"); return;
      case Iop_RSqrt64F0x2: storeOpStr("RSqrt64F0x2"); return;

      case Iop_Sqrt32Fx4:  storeOpStr("Sqrt32Fx4"); return;
      case Iop_Sqrt32F0x4: storeOpStr("Sqrt32F0x4"); return;
      case Iop_Sqrt64Fx2:  storeOpStr("Sqrt64Fx2"); return;
      case Iop_Sqrt64F0x2: storeOpStr("Sqrt64F0x2"); return;

      case Iop_Sub32Fx4:  storeOpStr("Sub32Fx4"); return;
      case Iop_Sub32Fx2:  storeOpStr("Sub32Fx2"); return;
      case Iop_Sub32F0x4: storeOpStr("Sub32F0x4"); return;
      case Iop_Sub64Fx2:  storeOpStr("Sub64Fx2"); return;
      case Iop_Sub64F0x2: storeOpStr("Sub64F0x2"); return;

      case Iop_CmpEQ32Fx4: storeOpStr("CmpEQ32Fx4"); return;
      case Iop_CmpLT32Fx4: storeOpStr("CmpLT32Fx4"); return;
      case Iop_CmpLE32Fx4: storeOpStr("CmpLE32Fx4"); return;
      case Iop_CmpGT32Fx4: storeOpStr("CmpGT32Fx4"); return;
      case Iop_CmpGE32Fx4: storeOpStr("CmpGE32Fx4"); return;
      case Iop_CmpUN32Fx4: storeOpStr("CmpUN32Fx4"); return;
      case Iop_CmpEQ64Fx2: storeOpStr("CmpEQ64Fx2"); return;
      case Iop_CmpLT64Fx2: storeOpStr("CmpLT64Fx2"); return;
      case Iop_CmpLE64Fx2: storeOpStr("CmpLE64Fx2"); return;
      case Iop_CmpUN64Fx2: storeOpStr("CmpUN64Fx2"); return;
      case Iop_CmpGT32Fx2: storeOpStr("CmpGT32Fx2"); return;
      case Iop_CmpEQ32Fx2: storeOpStr("CmpEQ32Fx2"); return;
      case Iop_CmpGE32Fx2: storeOpStr("CmpGE32Fx2"); return;

      case Iop_CmpEQ32F0x4: storeOpStr("CmpEQ32F0x4"); return;
      case Iop_CmpLT32F0x4: storeOpStr("CmpLT32F0x4"); return;
      case Iop_CmpLE32F0x4: storeOpStr("CmpLE32F0x4"); return;
      case Iop_CmpUN32F0x4: storeOpStr("CmpUN32F0x4"); return;
      case Iop_CmpEQ64F0x2: storeOpStr("CmpEQ64F0x2"); return;
      case Iop_CmpLT64F0x2: storeOpStr("CmpLT64F0x2"); return;
      case Iop_CmpLE64F0x2: storeOpStr("CmpLE64F0x2"); return;
      case Iop_CmpUN64F0x2: storeOpStr("CmpUN64F0x2"); return;

      case Iop_Neg32Fx4: storeOpStr("Neg32Fx4"); return;
      case Iop_Neg32Fx2: storeOpStr("Neg32Fx2"); return;

      case Iop_V128to64:   storeOpStr("V128to64");   return;
      case Iop_V128HIto64: storeOpStr("V128HIto64"); return;
      case Iop_64HLtoV128: storeOpStr("64HLtoV128"); return;

      case Iop_64UtoV128:   storeOpStr("64UtoV128"); return;
      case Iop_SetV128lo64: storeOpStr("SetV128lo64"); return;

      case Iop_32UtoV128:   storeOpStr("32UtoV128"); return;
      case Iop_V128to32:    storeOpStr("V128to32"); return;
      case Iop_SetV128lo32: storeOpStr("SetV128lo32"); return;

      case Iop_Dup8x16: storeOpStr("Dup8x16"); return;
      case Iop_Dup16x8: storeOpStr("Dup16x8"); return;
      case Iop_Dup32x4: storeOpStr("Dup32x4"); return;
      case Iop_Dup8x8: storeOpStr("Dup8x8"); return;
      case Iop_Dup16x4: storeOpStr("Dup16x4"); return;
      case Iop_Dup32x2: storeOpStr("Dup32x2"); return;

      case Iop_NotV128:    storeOpStr("NotV128"); return;
      case Iop_AndV128:    storeOpStr("AndV128"); return;
      case Iop_OrV128:     storeOpStr("OrV128");  return;
      case Iop_XorV128:    storeOpStr("XorV128"); return;

      case Iop_CmpNEZ8x16: storeOpStr("CmpNEZ8x16"); return;
      case Iop_CmpNEZ16x8: storeOpStr("CmpNEZ16x8"); return;
      case Iop_CmpNEZ32x4: storeOpStr("CmpNEZ32x4"); return;
      case Iop_CmpNEZ64x2: storeOpStr("CmpNEZ64x2"); return;

      case Iop_Abs8x16: storeOpStr("Abs8x16"); return;
      case Iop_Abs16x8: storeOpStr("Abs16x8"); return;
      case Iop_Abs32x4: storeOpStr("Abs32x4"); return;

      case Iop_Add8x16:   storeOpStr("Add8x16"); return;
      case Iop_Add16x8:   storeOpStr("Add16x8"); return;
      case Iop_Add32x4:   storeOpStr("Add32x4"); return;
      case Iop_Add64x2:   storeOpStr("Add64x2"); return;
      case Iop_QAdd8Ux16: storeOpStr("QAdd8Ux16"); return;
      case Iop_QAdd16Ux8: storeOpStr("QAdd16Ux8"); return;
      case Iop_QAdd32Ux4: storeOpStr("QAdd32Ux4"); return;
      case Iop_QAdd8Sx16: storeOpStr("QAdd8Sx16"); return;
      case Iop_QAdd16Sx8: storeOpStr("QAdd16Sx8"); return;
      case Iop_QAdd32Sx4: storeOpStr("QAdd32Sx4"); return;
      case Iop_QAdd64Ux2: storeOpStr("QAdd64Ux2"); return;
      case Iop_QAdd64Sx2: storeOpStr("QAdd64Sx2"); return;
      case Iop_PwAdd8x16: storeOpStr("PwAdd8x16"); return;
      case Iop_PwAdd16x8: storeOpStr("PwAdd16x8"); return;
      case Iop_PwAdd32x4: storeOpStr("PwAdd32x4"); return;
      case Iop_PwAddL8Ux16: storeOpStr("PwAddL8Ux16"); return;
      case Iop_PwAddL16Ux8: storeOpStr("PwAddL16Ux8"); return;
      case Iop_PwAddL32Ux4: storeOpStr("PwAddL32Ux4"); return;
      case Iop_PwAddL8Sx16: storeOpStr("PwAddL8Sx16"); return;
      case Iop_PwAddL16Sx8: storeOpStr("PwAddL16Sx8"); return;
      case Iop_PwAddL32Sx4: storeOpStr("PwAddL32Sx4"); return;

      case Iop_Sub8x16:   storeOpStr("Sub8x16"); return;
      case Iop_Sub16x8:   storeOpStr("Sub16x8"); return;
      case Iop_Sub32x4:   storeOpStr("Sub32x4"); return;
      case Iop_Sub64x2:   storeOpStr("Sub64x2"); return;
      case Iop_QSub8Ux16: storeOpStr("QSub8Ux16"); return;
      case Iop_QSub16Ux8: storeOpStr("QSub16Ux8"); return;
      case Iop_QSub32Ux4: storeOpStr("QSub32Ux4"); return;
      case Iop_QSub8Sx16: storeOpStr("QSub8Sx16"); return;
      case Iop_QSub16Sx8: storeOpStr("QSub16Sx8"); return;
      case Iop_QSub32Sx4: storeOpStr("QSub32Sx4"); return;
      case Iop_QSub64Ux2: storeOpStr("QSub64Ux2"); return;
      case Iop_QSub64Sx2: storeOpStr("QSub64Sx2"); return;

      case Iop_Mul8x16:    storeOpStr("Mul8x16"); return;
      case Iop_Mul16x8:    storeOpStr("Mul16x8"); return;
      case Iop_Mul32x4:    storeOpStr("Mul32x4"); return;
      case Iop_Mull8Ux8:    storeOpStr("Mull8Ux8"); return;
      case Iop_Mull8Sx8:    storeOpStr("Mull8Sx8"); return;
      case Iop_Mull16Ux4:    storeOpStr("Mull16Ux4"); return;
      case Iop_Mull16Sx4:    storeOpStr("Mull16Sx4"); return;
      case Iop_Mull32Ux2:    storeOpStr("Mull32Ux2"); return;
      case Iop_Mull32Sx2:    storeOpStr("Mull32Sx2"); return;
      case Iop_PolynomialMul8x16: storeOpStr("PolynomialMul8x16"); return;
      case Iop_PolynomialMull8x8: storeOpStr("PolynomialMull8x8"); return;
      case Iop_MulHi16Ux8: storeOpStr("MulHi16Ux8"); return;
      case Iop_MulHi32Ux4: storeOpStr("MulHi32Ux4"); return;
      case Iop_MulHi16Sx8: storeOpStr("MulHi16Sx8"); return;
      case Iop_MulHi32Sx4: storeOpStr("MulHi32Sx4"); return;
      case Iop_QDMulHi16Sx8: storeOpStr("QDMulHi16Sx8"); return;
      case Iop_QDMulHi32Sx4: storeOpStr("QDMulHi32Sx4"); return;
      case Iop_QRDMulHi16Sx8: storeOpStr("QRDMulHi16Sx8"); return;
      case Iop_QRDMulHi32Sx4: storeOpStr("QRDMulHi32Sx4"); return;

      case Iop_MullEven8Ux16: storeOpStr("MullEven8Ux16"); return;
      case Iop_MullEven16Ux8: storeOpStr("MullEven16Ux8"); return;
      case Iop_MullEven8Sx16: storeOpStr("MullEven8Sx16"); return;
      case Iop_MullEven16Sx8: storeOpStr("MullEven16Sx8"); return;

      case Iop_Avg8Ux16: storeOpStr("Avg8Ux16"); return;
      case Iop_Avg16Ux8: storeOpStr("Avg16Ux8"); return;
      case Iop_Avg32Ux4: storeOpStr("Avg32Ux4"); return;
      case Iop_Avg8Sx16: storeOpStr("Avg8Sx16"); return;
      case Iop_Avg16Sx8: storeOpStr("Avg16Sx8"); return;
      case Iop_Avg32Sx4: storeOpStr("Avg32Sx4"); return;

      case Iop_Max8Sx16: storeOpStr("Max8Sx16"); return;
      case Iop_Max16Sx8: storeOpStr("Max16Sx8"); return;
      case Iop_Max32Sx4: storeOpStr("Max32Sx4"); return;
      case Iop_Max8Ux16: storeOpStr("Max8Ux16"); return;
      case Iop_Max16Ux8: storeOpStr("Max16Ux8"); return;
      case Iop_Max32Ux4: storeOpStr("Max32Ux4"); return;

      case Iop_Min8Sx16: storeOpStr("Min8Sx16"); return;
      case Iop_Min16Sx8: storeOpStr("Min16Sx8"); return;
      case Iop_Min32Sx4: storeOpStr("Min32Sx4"); return;
      case Iop_Min8Ux16: storeOpStr("Min8Ux16"); return;
      case Iop_Min16Ux8: storeOpStr("Min16Ux8"); return;
      case Iop_Min32Ux4: storeOpStr("Min32Ux4"); return;

      case Iop_CmpEQ8x16:  storeOpStr("CmpEQ8x16"); return;
      case Iop_CmpEQ16x8:  storeOpStr("CmpEQ16x8"); return;
      case Iop_CmpEQ32x4:  storeOpStr("CmpEQ32x4"); return;
      case Iop_CmpGT8Sx16: storeOpStr("CmpGT8Sx16"); return;
      case Iop_CmpGT16Sx8: storeOpStr("CmpGT16Sx8"); return;
      case Iop_CmpGT32Sx4: storeOpStr("CmpGT32Sx4"); return;
      case Iop_CmpGT64Sx2: storeOpStr("CmpGT64Sx2"); return;
      case Iop_CmpGT8Ux16: storeOpStr("CmpGT8Ux16"); return;
      case Iop_CmpGT16Ux8: storeOpStr("CmpGT16Ux8"); return;
      case Iop_CmpGT32Ux4: storeOpStr("CmpGT32Ux4"); return;

      case Iop_Cnt8x16: storeOpStr("Cnt8x16"); return;
      case Iop_Clz8Sx16: storeOpStr("Clz8Sx16"); return;
      case Iop_Clz16Sx8: storeOpStr("Clz16Sx8"); return;
      case Iop_Clz32Sx4: storeOpStr("Clz32Sx4"); return;
      case Iop_Cls8Sx16: storeOpStr("Cls8Sx16"); return;
      case Iop_Cls16Sx8: storeOpStr("Cls16Sx8"); return;
      case Iop_Cls32Sx4: storeOpStr("Cls32Sx4"); return;

      case Iop_ShlV128: storeOpStr("ShlV128"); return;
      case Iop_ShrV128: storeOpStr("ShrV128"); return;

      case Iop_ShlN8x16: storeOpStr("ShlN8x16"); return;
      case Iop_ShlN16x8: storeOpStr("ShlN16x8"); return;
      case Iop_ShlN32x4: storeOpStr("ShlN32x4"); return;
      case Iop_ShlN64x2: storeOpStr("ShlN64x2"); return;
      case Iop_ShrN8x16: storeOpStr("ShrN8x16"); return;
      case Iop_ShrN16x8: storeOpStr("ShrN16x8"); return;
      case Iop_ShrN32x4: storeOpStr("ShrN32x4"); return;
      case Iop_ShrN64x2: storeOpStr("ShrN64x2"); return;
      case Iop_SarN8x16: storeOpStr("SarN8x16"); return;
      case Iop_SarN16x8: storeOpStr("SarN16x8"); return;
      case Iop_SarN32x4: storeOpStr("SarN32x4"); return;
      case Iop_SarN64x2: storeOpStr("SarN64x2"); return;

      case Iop_Shl8x16: storeOpStr("Shl8x16"); return;
      case Iop_Shl16x8: storeOpStr("Shl16x8"); return;
      case Iop_Shl32x4: storeOpStr("Shl32x4"); return;
      case Iop_Shl64x2: storeOpStr("Shl64x2"); return;
      case Iop_QSal8x16: storeOpStr("QSal8x16"); return;
      case Iop_QSal16x8: storeOpStr("QSal16x8"); return;
      case Iop_QSal32x4: storeOpStr("QSal32x4"); return;
      case Iop_QSal64x2: storeOpStr("QSal64x2"); return;
      case Iop_QShl8x16: storeOpStr("QShl8x16"); return;
      case Iop_QShl16x8: storeOpStr("QShl16x8"); return;
      case Iop_QShl32x4: storeOpStr("QShl32x4"); return;
      case Iop_QShl64x2: storeOpStr("QShl64x2"); return;
      case Iop_QSalN8x16: storeOpStr("QSalN8x16"); return;
      case Iop_QSalN16x8: storeOpStr("QSalN16x8"); return;
      case Iop_QSalN32x4: storeOpStr("QSalN32x4"); return;
      case Iop_QSalN64x2: storeOpStr("QSalN64x2"); return;
      case Iop_QShlN8x16: storeOpStr("QShlN8x16"); return;
      case Iop_QShlN16x8: storeOpStr("QShlN16x8"); return;
      case Iop_QShlN32x4: storeOpStr("QShlN32x4"); return;
      case Iop_QShlN64x2: storeOpStr("QShlN64x2"); return;
      case Iop_QShlN8Sx16: storeOpStr("QShlN8Sx16"); return;
      case Iop_QShlN16Sx8: storeOpStr("QShlN16Sx8"); return;
      case Iop_QShlN32Sx4: storeOpStr("QShlN32Sx4"); return;
      case Iop_QShlN64Sx2: storeOpStr("QShlN64Sx2"); return;
      case Iop_Shr8x16: storeOpStr("Shr8x16"); return;
      case Iop_Shr16x8: storeOpStr("Shr16x8"); return;
      case Iop_Shr32x4: storeOpStr("Shr32x4"); return;
      case Iop_Shr64x2: storeOpStr("Shr64x2"); return;
      case Iop_Sar8x16: storeOpStr("Sar8x16"); return;
      case Iop_Sar16x8: storeOpStr("Sar16x8"); return;
      case Iop_Sar32x4: storeOpStr("Sar32x4"); return;
      case Iop_Sar64x2: storeOpStr("Sar64x2"); return;
      case Iop_Sal8x16: storeOpStr("Sal8x16"); return;
      case Iop_Sal16x8: storeOpStr("Sal16x8"); return;
      case Iop_Sal32x4: storeOpStr("Sal32x4"); return;
      case Iop_Sal64x2: storeOpStr("Sal64x2"); return;
      case Iop_Rol8x16: storeOpStr("Rol8x16"); return;
      case Iop_Rol16x8: storeOpStr("Rol16x8"); return;
      case Iop_Rol32x4: storeOpStr("Rol32x4"); return;

      case Iop_Narrow16x8:   storeOpStr("Narrow16x8"); return;
      case Iop_Narrow32x4:   storeOpStr("Narrow32x4"); return;
      case Iop_QNarrow16Ux8: storeOpStr("QNarrow16Ux8"); return;
      case Iop_QNarrow32Ux4: storeOpStr("QNarrow32Ux4"); return;
      case Iop_QNarrow16Sx8: storeOpStr("QNarrow16Sx8"); return;
      case Iop_QNarrow32Sx4: storeOpStr("QNarrow32Sx4"); return;
      case Iop_Shorten16x8: storeOpStr("Shorten16x8"); return;
      case Iop_Shorten32x4: storeOpStr("Shorten32x4"); return;
      case Iop_Shorten64x2: storeOpStr("Shorten64x2"); return;
      case Iop_QShortenU16Ux8: storeOpStr("QShortenU16Ux8"); return;
      case Iop_QShortenU32Ux4: storeOpStr("QShortenU32Ux4"); return;
      case Iop_QShortenU64Ux2: storeOpStr("QShortenU64Ux2"); return;
      case Iop_QShortenS16Sx8: storeOpStr("QShortenS16Sx8"); return;
      case Iop_QShortenS32Sx4: storeOpStr("QShortenS32Sx4"); return;
      case Iop_QShortenS64Sx2: storeOpStr("QShortenS64Sx2"); return;
      case Iop_QShortenU16Sx8: storeOpStr("QShortenU16Sx8"); return;
      case Iop_QShortenU32Sx4: storeOpStr("QShortenU32Sx4"); return;
      case Iop_QShortenU64Sx2: storeOpStr("QShortenU64Sx2"); return;
      case Iop_Longen8Ux8: storeOpStr("Longen8Ux8"); return;
      case Iop_Longen16Ux4: storeOpStr("Longen16Ux4"); return;
      case Iop_Longen32Ux2: storeOpStr("Longen32Ux2"); return;
      case Iop_Longen8Sx8: storeOpStr("Longen8Sx8"); return;
      case Iop_Longen16Sx4: storeOpStr("Longen16Sx4"); return;
      case Iop_Longen32Sx2: storeOpStr("Longen32Sx2"); return;

      case Iop_InterleaveHI8x16: storeOpStr("InterleaveHI8x16"); return;
      case Iop_InterleaveHI16x8: storeOpStr("InterleaveHI16x8"); return;
      case Iop_InterleaveHI32x4: storeOpStr("InterleaveHI32x4"); return;
      case Iop_InterleaveHI64x2: storeOpStr("InterleaveHI64x2"); return;
      case Iop_InterleaveLO8x16: storeOpStr("InterleaveLO8x16"); return;
      case Iop_InterleaveLO16x8: storeOpStr("InterleaveLO16x8"); return;
      case Iop_InterleaveLO32x4: storeOpStr("InterleaveLO32x4"); return;
      case Iop_InterleaveLO64x2: storeOpStr("InterleaveLO64x2"); return;

      case Iop_CatOddLanes8x16: storeOpStr("CatOddLanes8x16"); return;
      case Iop_CatOddLanes16x8: storeOpStr("CatOddLanes16x8"); return;
      case Iop_CatOddLanes32x4: storeOpStr("CatOddLanes32x4"); return;
      case Iop_CatEvenLanes8x16: storeOpStr("CatEvenLanes8x16"); return;
      case Iop_CatEvenLanes16x8: storeOpStr("CatEvenLanes16x8"); return;
      case Iop_CatEvenLanes32x4: storeOpStr("CatEvenLanes32x4"); return;

      case Iop_InterleaveOddLanes8x16: storeOpStr("InterleaveOddLanes8x16"); return;
      case Iop_InterleaveOddLanes16x8: storeOpStr("InterleaveOddLanes16x8"); return;
      case Iop_InterleaveOddLanes32x4: storeOpStr("InterleaveOddLanes32x4"); return;
      case Iop_InterleaveEvenLanes8x16: storeOpStr("InterleaveEvenLanes8x16"); return;
      case Iop_InterleaveEvenLanes16x8: storeOpStr("InterleaveEvenLanes16x8"); return;
      case Iop_InterleaveEvenLanes32x4: storeOpStr("InterleaveEvenLanes32x4"); return;

      case Iop_GetElem8x16: storeOpStr("GetElem8x16"); return;
      case Iop_GetElem16x8: storeOpStr("GetElem16x8"); return;
      case Iop_GetElem32x4: storeOpStr("GetElem32x4"); return;
      case Iop_GetElem64x2: storeOpStr("GetElem64x2"); return;

      case Iop_GetElem8x8: storeOpStr("GetElem8x8"); return;
      case Iop_GetElem16x4: storeOpStr("GetElem16x4"); return;
      case Iop_GetElem32x2: storeOpStr("GetElem32x2"); return;
      case Iop_SetElem8x8: storeOpStr("SetElem8x8"); return;
      case Iop_SetElem16x4: storeOpStr("SetElem16x4"); return;
      case Iop_SetElem32x2: storeOpStr("SetElem32x2"); return;

      case Iop_Extract64: storeOpStr("Extract64"); return;
      case Iop_ExtractV128: storeOpStr("ExtractV128"); return;

      case Iop_Perm8x16: storeOpStr("Perm8x16"); return;
      case Iop_Reverse16_8x16: storeOpStr("Reverse16_8x16"); return;
      case Iop_Reverse32_8x16: storeOpStr("Reverse32_8x16"); return;
      case Iop_Reverse32_16x8: storeOpStr("Reverse32_16x8"); return;
      case Iop_Reverse64_8x16: storeOpStr("Reverse64_8x16"); return;
      case Iop_Reverse64_16x8: storeOpStr("Reverse64_16x8"); return;
      case Iop_Reverse64_32x4: storeOpStr("Reverse64_32x4"); return;

      case Iop_F32ToFixed32Ux4_RZ: storeOpStr("F32ToFixed32Ux4_RZ"); return;
      case Iop_F32ToFixed32Sx4_RZ: storeOpStr("F32ToFixed32Sx4_RZ"); return;
      case Iop_Fixed32UToF32x4_RN: storeOpStr("Fixed32UToF32x4_RN"); return;
      case Iop_Fixed32SToF32x4_RN: storeOpStr("Fixed32SToF32x4_RN"); return;
      case Iop_F32ToFixed32Ux2_RZ: storeOpStr("F32ToFixed32Ux2_RZ"); return;
      case Iop_F32ToFixed32Sx2_RZ: storeOpStr("F32ToFixed32Sx2_RZ"); return;
      case Iop_Fixed32UToF32x2_RN: storeOpStr("Fixed32UToF32x2_RN"); return;
      case Iop_Fixed32SToF32x2_RN: storeOpStr("Fixed32SToF32x2_RN"); return;

      default: vpanic("ppIROp(1)");
   }

   HChar* str2 = VG_(malloc)("opToStr", sizeof(HChar) * (VG_(strlen)(str) + 2 + 1));
   switch (op - base) {
      case 0: VG_(strcpy)(str2, str); VG_(strcat)(str2, "8"); storeOpStr(str2); break;
      case 1: VG_(strcpy)(str2, str); VG_(strcat)(str2, "16"); storeOpStr(str2); break;
      case 2: VG_(strcpy)(str2, str); VG_(strcat)(str2, "32"); storeOpStr(str2); break;
      case 3: VG_(strcpy)(str2, str); VG_(strcat)(str2, "64"); storeOpStr(str2); break;
      default: storeOpStr("unknwon");
   }
   VG_(free)(str2);
}

