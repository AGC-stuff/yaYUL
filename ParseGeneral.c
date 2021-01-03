/*
  Copyright 2003-2004,2016 Ronald S. Burkey <info@sandroid.org>

  This file is part of yaAGC. 

  yaAGC is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  yaAGC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with yaAGC; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Filename:	ParseGeneral.c
  Purpose:	Assembles most opcodes opcode.
  Mode:		04/18/03 RSB	Began.
  		04/26/03 RSB	Distilled from the essences of 
				ParseTC.c et al.
		07/05/04 RSB	Added KeepExtend.
		07/21/04 RSB	The parse-behavior modification-flag definitions
				were poor, in that the overlap between QC and PC
				flags causes some address-range tests to fail.
				Added CAE, CAF.  
		07/22/04 RSB	Oops!  Broke every instruction with QC1, QC2, or 
				QC3.
		07/27/04 RSB	Moved STCALL, STODL, STORE, and STOVL over to
				ParseST.c.
		2012-09-25 JL   Handle arguments like "DUMMYJOB + 2", i.e.
		                Mod1=+, Mod2=2.
                2016-08-18 RSB  Tweaks related to Block1.
                2016-12-18 MAS  Weakened erroneous EXTEND checks when dealing
                                with explicit TC 6's, which can show up as
                                targets for an INDEX and so don't actually extend.
                2017-01-29 MAS  Added some special logic for handling Raython-
                                style literal operands.
                2017-06-28 MAS  Added BLK2's special  handling of EDRUPT.
 */

#include "yaYUL.h"
#include <stdlib.h>
#include <string.h>

int KeepExtend = 0;

//------------------------------------------------------------------------
// A sort of generalized parser that works for most instruction types.
// "Normally", Flags==0. However, various fancier alternatives can be
// set by ORing together various of the constants defined above.
// The Opcode is always something of the form 0n0000 (in octal), where
// n=0,1,...,7.
int ParseGeneral(ParseInput_t *InRecord, ParseOutput_t *OutRecord, int Opcode, int Flags)
{
    ParseOutput_t dummy;
    int Value, i;
    Address_t K;
    char args[32];

    args[0] = '\0';

    if (InRecord->Mod1)
        strcpy(args, InRecord->Mod1);

    // Handle arguments like "DUMMYJOB + 2", i.e. Mod1=+, Mod2=2.
    if (InRecord->Mod2)
        strcat(args, InRecord->Mod2);

    IncPc(&InRecord->ProgramCounter, 1, &OutRecord->ProgramCounter);

    if (!OutRecord->ProgramCounter.Invalid && OutRecord->ProgramCounter.Overflow) {
        strcpy(OutRecord->ErrorMessage, "Next code may overflow storage.");
        OutRecord->Warning = 1;
    }

    OutRecord->EBank = InRecord->EBank;
    OutRecord->SBank = InRecord->SBank;

    // Set the default binary word.
    OutRecord->NumWords = 1;

    if (Block1)
      Flags = Flags & ~(PC1|PC2|PC3|PC4|PC5|PC6|PC7|QC1|QC2|QC3|QCNOT0);

    if (Flags & PC1)
        Opcode |= 01000;
    else if (Flags & PC2)
        Opcode |= 02000;
    else if (Flags & PC3)
        Opcode |= 03000;
    else if (Flags & PC4)
        Opcode |= 04000;
    else if (Flags & PC5)
        Opcode |= 05000;
    else if (Flags & PC6)
        Opcode |= 06000;
    else if (Flags & PC7)
        Opcode |= 07000;
    else if (Flags & QC1)
        Opcode |= 02000;
    else if (Flags & QC2)
        Opcode |= 04000;
    else if (Flags & QC3)
        Opcode |= 06000;

    OutRecord->Words[0] = Opcode;

    if (Flags & QCNOT0)
        OutRecord->Words[0] |= 06000;

    // Do some sanity checking.
    if (!Block1) {
      if (InRecord->Extend &&  !(Flags & EXTENDED) && !InRecord->IndexValid) {
          if (InRecord->Extend != 2) {
             // Bomb out if the extend came from an EXTEND, but not from an explicit TC 6
             strcpy(OutRecord->ErrorMessage, "Illegally preceded by EXTEND.");
             OutRecord->Fatal = 1;
          }
          OutRecord->Extend = 0;
      } else if (!InRecord->Extend && (Flags & EXTENDED)) {
          strcpy(OutRecord->ErrorMessage, "Required EXTEND is missing.");
          OutRecord->Fatal = 1;
          OutRecord->Extend = 0;
      }
    }

    if (InRecord->IndexValid) {
        OutRecord->IndexValid = 0;
    }

    i = GetOctOrDec(InRecord->Operand, &Value);
    if (!i && (Flags & ENUMBER) && *InRecord->Operand != '+' && *InRecord->Operand != '-') {
        int Offset;

        if (Raytheon && Value >= 02000 && Value < 04000) {
            // For Raytheon builds, shift common-fixed literals into the expected range
            Value += 010000 + InRecord->ProgramCounter.FB * 02000;
        }
        PseudoToStruct(Value, &K);

        if (!GetOctOrDec(args, &Offset))
            OpcodeOffset = Offset;

        goto DoIt;
    }

    if (!i) {
        if (*InRecord->Operand == '+' || *InRecord->Operand == '-')
            IncPc(&InRecord->ProgramCounter, Value, &K);
        else
            PseudoToStruct(Value, &K);

        if (*args) {
            i = GetOctOrDec(args, &Value);
            if (!i)
                OpcodeOffset = Value;
        }
        DoIt:
        if (K.Invalid) {
            strcpy(OutRecord->ErrorMessage, "Destination address not resolved.");
            OutRecord->Fatal = 1;
        } else if (K.Overflow) {
            strcpy(OutRecord->ErrorMessage, "Destination address out of range.");
            OutRecord->Fatal = 1;
        } else if (!K.Address) {
            // There are a lot of cases in which an actual numerical constant
            // is used where an address would be expected.  As a first cut at
            // addressing this, we suppose that numbers in the range 0-03777
            // may be converted straightforwardly to unswitched erasable.
            if (K.Constant)
                goto AddressFound;
            strcpy(OutRecord->ErrorMessage, "Destination not an address.");
            OutRecord->Fatal = 1;
        } else {
            AddressFound:
            // Here I had originally intended to add a check that the bank
            // numbers were compatible.  This turns out not to be generally
            // possible at assembly time, unless a bunch of analysis is
            // added to understand program flow.  However, certain checks,
            // particularly having to do with quarter-codes, can be done.
            // ... Later:  Not true for erasable banks, since the EBANK=
            // pseudo-op tells the assembler what bank is expected.  Also,
            // some constants need to be translated to erasable.
            if (K.Constant) {
                PseudoToSegmented(K.Value, &dummy);
                K = dummy.ProgramCounter;
            }

            if (Flags & KPLUS1)
                IncPc(&K, 1, &K);

            i = K.SReg;
            if (!InRecord->IndexValid) {
                // If we've been preceded by an INDEX instruction, none of the following
                // checks are valid, since the instruction may be changed to essentially
                // anything else prior to execution.
                if ((Flags & FIXED) && !K.Fixed) {
                    i &= ~07000;
                    strcpy(OutRecord->ErrorMessage, "The address is not in fixed memory.");
                    OutRecord->Fatal = 1;
                } else if ((Flags & ERASABLE) && !K.Erasable) {
                    i &= ~07600;
                    strcpy(OutRecord->ErrorMessage, "The address is not in erasable memory.");
                    OutRecord->Fatal = 1;
                }
                if (Flags & (PC0 | PC1 | PC2 | PC3 | PC4 | PC5 | PC6 | PC7)) {
                    if ((i & 07000) != 0 && (i & 07000) != (Opcode & 07000)) {
                        i &= ~07000;
                        strcpy(OutRecord->ErrorMessage, "Operand out of range.");
                        OutRecord->Fatal = 1;
                    }
                } else if (Flags & (QC0 | QC1 | QC2 | QC3)) {
                    if ((i & 06000) != 0 && (i & 06000) != (Opcode & 06000)) {
                        i &= ~06000;
                        sprintf(OutRecord->ErrorMessage, "Operand (0%o) out of range.", i);
                        OutRecord->Fatal = 1;
                    }
                } else if (Flags & QCNOT0) {
                    if (0 == (K.SReg & 06000)) {
                        i |= 06000;
                        strcpy(OutRecord->ErrorMessage, "Operand out of range.");
                        OutRecord->Fatal = 1;
                    }
                }
            }
            OutRecord->Words[0] = Opcode | i;
        }
    } else {
        // The operand is NOT a number.  Presumably, it's a symbol.
        i = FetchSymbolPlusOffset(&InRecord->ProgramCounter, InRecord->Operand, args, &K);
        if (!i)
            goto DoIt;

        sprintf(OutRecord->ErrorMessage, "Symbol \"%s\" undefined or offset bad", InRecord->Operand);
        OutRecord->Fatal = 1;
    }

    // Handle the special case `TC 6' setting the Extend bit.
    if (KeepExtend) {
        KeepExtend = 0;
        OutRecord->Extend = InRecord->Extend;
    } else {
        OutRecord->Extend = 0;
        if (Opcode == 000000 && !(Flags & EXTENDED) && !K.Invalid && (K.Constant || (K.Erasable && K.Unbanked)) && K.SReg == 06) {
            if (strlen(InRecord->Alias) == 0) {
                // This was an explicit TC 6 rather than an EXTEND. This can show up (as in Retread, for example),
                // as something that gets indexed into something other than an EXTEND, and so should be allowed through.
                // Setting Extend = 2 makes the erroneous Extend check non-fatal.
                OutRecord->Extend = 2;
            } else {
                OutRecord->Extend = 1;
            }
        } else {
            OutRecord->Extend = 0;
        }
    }
    OutRecord->IndexValid = 0;
    return (0);
}

//------------------------------------------------------------------------
// Various little parsers based on ParseGeneral.

int ParseAD(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 060000, ENUMBER));
}

int ParseADS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, QC3));
}

int ParseAUG(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, EXTENDED | QC2));
}

int ParseBZMF(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 060000, EXTENDED | QCNOT0));
}

int ParseBZF(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 010000, EXTENDED | QCNOT0));
}

int ParseCA(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 030000, ENUMBER));
}

int ParseCAE(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 030000, ERASABLE | ENUMBER));
}

int ParseCAF(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 030000, FIXED | ENUMBER));
}

int ParseCCS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 010000, QC0 | ENUMBER));
}

int ParseCS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 040000, ENUMBER));
}

int ParseDAS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, QC0 | KPLUS1));
}

int ParseDCA(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 030000, EXTENDED | KPLUS1 | ENUMBER));
}

int ParseDCS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 040000, EXTENDED | KPLUS1 | ENUMBER));
}

int ParseDIM(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, EXTENDED | QC3));
}

int ParseDNCHAN(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 034000, PC0 | ENUMBER));
}

int ParseDV(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, (Block1 ? 050000 : 010000), EXTENDED | QC0));
}

int ParseDXCH(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 050000, QC1 | KPLUS1 | ENUMBER));
}

int ParseEDRUPT(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    int result;

    if (blk2)
    {
        // For BLK2, EDRUPT was implied-address and always assembled
        // to 07000.
        if (*InRecord->Operand != 0)
        {
            strcpy(OutRecord->ErrorMessage,
                "Extra fields are present.");
            OutRecord->Warning = 1;
        }
        InRecord->Operand = "0";
    }

    result = (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC7));

    if (blk2)
    {
        InRecord->Operand = "";
    }

    return result;
}

int ParseLXCH(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, QC1 | ENUMBER));
}

int ParseINCR(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, QC2));
}

int ParseMASK(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 070000, 0));
}

int ParseMP(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, (Block1 ? 040000 : 070000), EXTENDED));
}

int ParseMSU(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, EXTENDED | QC0 | ENUMBER));
}

int ParseQXCH(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 020000, EXTENDED | QC1 | ENUMBER));
}

int ParseRAND(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC2 | ENUMBER));
}

int ParseREAD(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC0 | ENUMBER));
}

int ParseROR(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC4 | ENUMBER));
}

int ParseRXOR(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC6 | ENUMBER));
}

int ParseSU(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 060000, EXTENDED | QC0 | ENUMBER));
}

int ParseTC(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, ENUMBER));
}

int ParseTCF(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    if (*InRecord->Operand == 0) {
        /* Handle a TCF with no operand, which translates to TCF +0, i.e. NOOP. */
        InRecord->Operand = "+0";
    }
    return (ParseGeneral(InRecord, OutRecord, 010000, QCNOT0 | ENUMBER));
}

int ParseTS(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 050000, QC2 | ENUMBER));
}

int ParseWAND(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC3 | ENUMBER));
}

int ParseWOR(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC5 | ENUMBER));
}

int ParseWRITE(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, 000000, EXTENDED | PC1 | ENUMBER));
}

int ParseXCH(ParseInput_t *InRecord, ParseOutput_t *OutRecord)
{
    return (ParseGeneral(InRecord, OutRecord, (Block1 ? 030000 : 050000), QC3 | ENUMBER));
}

