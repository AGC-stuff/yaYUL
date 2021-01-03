/*
  Copyright 2003,2016 Ronald S. Burkey <info@sandroid.org>

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

  Filename:     PseudoToSegmented.c
  Purpose:      Converts a pseudo-address (i.e., a number in the linear
                range 0-0117777) to a fully parsed address with
                bank numbers, S-register contents, etc.
  Mod History:  04/17/03 RSB.   Began.
                04/27/03 RSB.   Split off from ParseSETLOC.c
                06/22/03 RSB.   Added PseudoToEBanked.
                08/18/16 RSB.   Tweaks related to --block1.
                10/12/16 RSB.   Cosmetic change which should not affect
                                the output in any way.
 */

#include "yaYUL.h"
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------
// The difference between PseudoToEBanked and PseudoToSegmented is simply
// that in the case of the former E-banks are used in preference to 
// unbanked erasable, whereas for the latter unbanked erasable is used 
// whenever possible.
void PseudoToEBanked(int Value, ParseOutput_t *OutRecord)
{
    OutRecord->ProgramCounter = (const Address_t) { 0 };

    if (Value < 0 || Value > 0117777) {
        strcpy(OutRecord->ErrorMessage, "Addresses must be between 0 and 0117777.");
        OutRecord->Fatal = 1;
        OutRecord->ProgramCounter.Invalid = 1;
        return;
    }

    if (Value <= 03777) {
        OutRecord->ProgramCounter.Address = 1;
        OutRecord->ProgramCounter.Erasable = 1;
        OutRecord->ProgramCounter.Banked = 1;
        OutRecord->ProgramCounter.SReg = 01400 + (Value & 0377);
        OutRecord->ProgramCounter.EB = Value / 0400;
    } else if (Value <= 07777) {
        OutRecord->ProgramCounter.Address = 1;
        OutRecord->ProgramCounter.Fixed = 1;
        OutRecord->ProgramCounter.Unbanked = 1;
        OutRecord->ProgramCounter.SReg = Value;
    } else {
        OutRecord->ProgramCounter.Address = 1;
        OutRecord->ProgramCounter.Fixed = 1;
        OutRecord->ProgramCounter.Banked = 1;
        OutRecord->ProgramCounter.SReg = 02000 + (Value & 01777);

        if (Value >= 04000 && Value <= 05777)
            OutRecord->ProgramCounter.FB = 2;
        else if (Value >= 06000 && Value <= 07777)
            OutRecord->ProgramCounter.FB = 3;
        else if (Value < 0110000)
            OutRecord->ProgramCounter.FB = (Value - 010000) / 02000;
        else  {
            OutRecord->ProgramCounter.Super = 1;
            OutRecord->ProgramCounter.FB = (Value - 030000) / 02000;
        } 
    }

    OutRecord->ProgramCounter.Value = Value;
}

//------------------------------------------------------------------------

void PseudoToSegmented(int Value, ParseOutput_t *OutRecord)
{
    if (PseudoToStruct(Value, &OutRecord->ProgramCounter)) {
        strcpy(OutRecord->ErrorMessage, "Addresses must be between 0 and 0117777.");
        OutRecord->Fatal = 1;
    }
}

int PseudoToStruct(int Value, Address_t *Address)
{
    *Address = VALID_ADDRESS;

    if (Value < 0 || Value > 0117777) {
        Address->Invalid = 1;
        return (1);
    }

    if ((Block1 && Value <= 01777) || Value <= 01377) {
        Address->Address = 1;
        Address->Erasable = 1;
        Address->Unbanked = 1;
        Address->SReg = Value;
    } else if (!Block1 && Value <= 03777) {
        Address->Address = 1;
        Address->Erasable = 1;
        Address->Banked = 1;
        Address->SReg = 01400 + (Value & 0377);
        Address->EB = Value / 0400;
    } else if ((!Block1 && Value <= 07777) || (Block1 && Value <= 05777)) {
        Address->Address = 1;
        Address->Fixed = 1;
        Address->Unbanked = 1;
        Address->SReg = Value;
    } else {
        Address->Address = 1;
        Address->Fixed = 1;
        Address->Banked = 1;
        Address->SReg = Block1 ? (06000 + (Value & 01777)) : (02000 + (Value & 01777));

        if (Block1)
            Address->FB = 3 + (Value - 06000) / 02000;
        else if (Value >= 04000 && Value <= 05777)
            Address->FB = 2;
        else if (Value >= 06000 && Value <= 07777)
            Address->FB = 3;
        else if (Value < 0110000)
            Address->FB = (Value - 010000) / 02000;
        else {
            Address->Super = 1;
            Address->FB = (Value - 030000) / 02000;
        }
    }

    Address->Value = Value;

    return (0);
}
