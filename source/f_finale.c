/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Game completion, final screen animation.
 *
 *-----------------------------------------------------------------------------
 */

#include "doomstat.h"
#include "d_event.h"
#include "v_video.h"
#include "w_wad.h"
#include "s_sound.h"
#include "sounds.h"
#include "f_finale.h" // CPhipps - hmm...
#include "dstrings.h"

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
static int finalestage; // cph -
static int finalecount; // made static
static const char*   finaletext; // cph -
static const char*   finaleflat; // made static const

// defines for the end mission display text                     // phares

#define TEXTSPEED    3     // original value                    // phares
#define TEXTWAIT     250   // original value                    // phares
#define NEWTEXTSPEED 0.01f  // new value                         // phares
#define NEWTEXTWAIT  1000  // new value                         // phares

// CPhipps - removed the old finale screen text message strings;
// they were commented out for ages already
// Ty 03/22/98 - ... the new s_WHATEVER extern variables are used
// in the code below instead.

void    F_StartCast (void);
void    F_CastTicker (void);
boolean F_CastResponder (event_t *ev);
void    F_CastDrawer (void);

void WI_checkForAccelerate(void);    // killough 3/28/98: used to
extern int acceleratestage;          // accelerate intermission screens
static int midstage;                 // whether we're in "mid-stage"

//
// F_StartFinale
//
void F_StartFinale (void)
{
  gameaction = ga_nothing;
  gamestate = GS_FINALE;
  automapmode &= ~am_active;

  // killough 3/28/98: clear accelerative text flags
  acceleratestage = midstage = 0;

  // Okay - IWAD dependend stuff.
  // This has been changed severly, and
  //  some stuff might have changed in the process.
  switch ( gamemode )
  {
    // DOOM 1 - E1, E3 or E4, but each nine missions
    case shareware:
    case registered:
  case retail:
  {
      S_ChangeMusic(mus_victor, true);

      switch (gameepisode)
      {
      case 1:
          finaleflat = "FLOOR4_8";
          finaletext = E1TEXT;
          break;
      case 2:
          finaleflat = "SFLR6_1";
          finaletext = E2TEXT;
          break;
      case 3:
          finaleflat = "MFLR8_4";
          finaletext = E3TEXT;
          break;
      case 4:
          finaleflat = "MFLR8_3";
          finaletext = E4TEXT;
          break;
      default:
          // Ouch.
          break;
      }
      break;
  }

    // DOOM II and missions packs with E1, M34
    case commercial:
    {
      S_ChangeMusic(mus_read_m, true);

      // Ty 08/27/98 - added the gamemission logic
      switch (gamemap)
      {
        case 6:
             finaleflat = "SLIME16";
             finaletext = (gamemission==pack_tnt)  ? T1TEXT :
                          (gamemission==pack_plut) ? P1TEXT : C1TEXT;
             break;
        case 11:
             finaleflat = "RROCK14";
             finaletext = (gamemission==pack_tnt)  ? T2TEXT :
                          (gamemission==pack_plut) ? P2TEXT : C2TEXT;
             break;
        case 20:
             finaleflat = "RROCK07";
             finaletext = (gamemission==pack_tnt)  ? T3TEXT :
                          (gamemission==pack_plut) ? P3TEXT : C3TEXT;
             break;
        case 30:
             finaleflat = "RROCK17";
             finaletext = (gamemission==pack_tnt)  ? T4TEXT :
                          (gamemission==pack_plut) ? P4TEXT : C4TEXT;
             break;
        case 15:
             finaleflat = "RROCK13";
             finaletext = (gamemission==pack_tnt)  ? T5TEXT :
                          (gamemission==pack_plut) ? P5TEXT : C5TEXT;
             break;
        case 31:
             finaleflat = "RROCK19";
             finaletext = (gamemission==pack_tnt)  ? T6TEXT :
                          (gamemission==pack_plut) ? P6TEXT : C6TEXT;
             break;
        default:
             // Ouch.
             break;
      }
      break;
      // Ty 08/27/98 - end gamemission logic
    }

    // Indeterminate.
    default:  // Ty 03/30/98 - not externalized
         S_ChangeMusic(mus_read_m, true);
         finaleflat = "F_SKY1"; // Not used anywhere else.
         finaletext = C1TEXT;  // FIXME - other text, music?
         break;
  }

  finalestage = 0;
  finalecount = 0;
}



boolean F_Responder (event_t *event)
{
  if (finalestage == 2)
    return F_CastResponder (event);

  return false;
}

// Get_TextSpeed() returns the value of the text display speed  // phares
// Rewritten to allow user-directed acceleration -- killough 3/28/98

static float Get_TextSpeed(void)
{
  return midstage ? NEWTEXTSPEED : (midstage=acceleratestage) ?
    acceleratestage=0, NEWTEXTSPEED : TEXTSPEED;
}


//
// F_Ticker
//
// killough 3/28/98: almost totally rewritten, to use
// player-directed acceleration instead of constant delays.
// Now the player can accelerate the text display by using
// the fire/use keys while it is being printed. The delay
// automatically responds to the user, and gives enough
// time to read.
//
// killough 5/10/98: add back v1.9 demo compatibility
//

void F_Ticker(void)
{

    WI_checkForAccelerate();  // killough 3/28/98: check for acceleration

  // advance animation
  finalecount++;

  if (finalestage == 2)
    F_CastTicker();

  if (!finalestage)
    {
      float speed = Get_TextSpeed();
      /* killough 2/28/98: changed to allow acceleration */
      if (finalecount > strlen(finaletext)*speed +
          (midstage ? NEWTEXTWAIT : TEXTWAIT) ||
          (midstage && acceleratestage)) {
        if (gamemode != commercial)       // Doom 1 / Ultimate Doom episode end
          {                               // with enough time, it's automatic
            finalecount = 0;
            finalestage = 1;
            wipegamestate = -1;         // force a wipe
            if (gameepisode == 3)
              S_StartMusic(mus_bunny);
          }
        else   // you must press a button to continue in Doom 2
          if (midstage)
            {
              if (gamemap == 30)
                F_StartCast();              // cast of Doom 2 characters
              else
                gameaction = ga_worlddone;  // next level, e.g. MAP07
            }
      }
    }
}

//
// F_TextWrite
//
// This program displays the background and text at end-mission     // phares
// text time. It draws both repeatedly so that other displays,      //   |
// like the main menu, can be drawn over it dynamically and         //   V
// erased dynamically. The TEXTSPEED constant is changed into
// the Get_TextSpeed function so that the speed of writing the      //   ^
// text can be increased, and there's still time to read what's     //   |
// written.                                                         // phares
// CPhipps - reformatted

#include "hu_stuff.h"
extern patchnum_t hu_font[HU_FONTSIZE];


static void F_TextWrite (void)
{
  V_DrawBackground(finaleflat, 0);
  { // draw some of the text onto the screen
    int         cx = 10;
    int         cy = 10;
    const char* ch = finaletext; // CPhipps - const
    int         count = (int)((float)(finalecount - 10)/Get_TextSpeed()); // phares
    int         w;

    if (count < 0)
      count = 0;

    for ( ; count ; count-- ) {
      int       c = *ch++;

      if (!c)
  break;
      if (c == '\n') {
  cx = 10;
  cy += 11;
  continue;
      }

      c = toupper(c) - HU_FONTSTART;
      if (c < 0 || c> HU_FONTSIZE) {
  cx += 4;
  continue;
      }

      w = hu_font[c].width;
      if (cx+w > SCREENWIDTH)
  break;
      // CPhipps - patch drawing updated
      V_DrawNumPatch(cx, cy, 0, hu_font[c].lumpnum, CR_DEFAULT, VPT_STRETCH);
      cx+=w;
    }
  }
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
  const char *name; // CPhipps - const**
  mobjtype_t   type;
} castinfo_t;

#define MAX_CASTORDER 18 /* Ty - hard coded for now */
static const castinfo_t castorder[] = { // CPhipps - static const, initialised here
  { CC_ZOMBIE,  MT_POSSESSED },
  { CC_SHOTGUN, MT_SHOTGUY },
  { CC_HEAVY,   MT_CHAINGUY },
  { CC_IMP,     MT_TROOP },
  { CC_DEMON,   MT_SERGEANT },
  { CC_LOST,    MT_SKULL },
  { CC_CACO,    MT_HEAD },
  { CC_HELL,    MT_KNIGHT },
  { CC_BARON,   MT_BRUISER },
  { CC_ARACH,   MT_BABY },
  { CC_PAIN,    MT_PAIN },
  { CC_REVEN,   MT_UNDEAD },
  { CC_MANCU,   MT_FATSO },
  { CC_ARCH,    MT_VILE },
  { CC_SPIDER,  MT_SPIDER },
  { CC_CYBER,   MT_CYBORG },
  { CC_HERO,    MT_PLAYER },
  { NULL,         0}
  };

int             castnum;
int             casttics;
state_t*        caststate;
boolean         castdeath;
int             castframes;
int             castonmelee;
boolean         castattacking;


//
// F_StartCast
//

void F_StartCast (void)
{
  wipegamestate = -1;         // force a screen wipe
  castnum = 0;
  caststate = &states[mobjinfo[castorder[castnum].type].seestate];
  casttics = caststate->tics;
  castdeath = false;
  finalestage = 2;
  castframes = 0;
  castonmelee = 0;
  castattacking = false;
  S_ChangeMusic(mus_evil, true);
}


//
// F_CastTicker
//
void F_CastTicker (void)
{
  int st;
  int sfx;

  if (--casttics > 0)
    return;                 // not time to change state yet

  if (caststate->tics == -1 || caststate->nextstate == S_NULL)
  {
    // switch from deathstate to next monster
    castnum++;
    castdeath = false;
    if (castorder[castnum].name == NULL)
      castnum = 0;
    if (mobjinfo[castorder[castnum].type].seesound)
      S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound);
    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    castframes = 0;
  }
  else
  {
    // just advance to next state in animation
    if (caststate == &states[S_PLAY_ATK1])
      goto stopattack;    // Oh, gross hack!
    st = caststate->nextstate;
    caststate = &states[st];
    castframes++;

    // sound hacks....
    switch (st)
    {
      case S_PLAY_ATK1:     sfx = sfx_dshtgn; break;
      case S_POSS_ATK2:     sfx = sfx_pistol; break;
      case S_SPOS_ATK2:     sfx = sfx_shotgn; break;
      case S_VILE_ATK2:     sfx = sfx_vilatk; break;
      case S_SKEL_FIST2:    sfx = sfx_skeswg; break;
      case S_SKEL_FIST4:    sfx = sfx_skepch; break;
      case S_SKEL_MISS2:    sfx = sfx_skeatk; break;
      case S_FATT_ATK8:
      case S_FATT_ATK5:
      case S_FATT_ATK2:     sfx = sfx_firsht; break;
      case S_CPOS_ATK2:
      case S_CPOS_ATK3:
      case S_CPOS_ATK4:     sfx = sfx_shotgn; break;
      case S_TROO_ATK3:     sfx = sfx_claw; break;
      case S_SARG_ATK2:     sfx = sfx_sgtatk; break;
      case S_BOSS_ATK2:
      case S_BOS2_ATK2:
      case S_HEAD_ATK2:     sfx = sfx_firsht; break;
      case S_SKULL_ATK2:    sfx = sfx_sklatk; break;
      case S_SPID_ATK2:
      case S_SPID_ATK3:     sfx = sfx_shotgn; break;
      case S_BSPI_ATK2:     sfx = sfx_plasma; break;
      case S_CYBER_ATK2:
      case S_CYBER_ATK4:
      case S_CYBER_ATK6:    sfx = sfx_rlaunc; break;
      case S_PAIN_ATK3:     sfx = sfx_sklatk; break;
      default: sfx = 0; break;
    }

    if (sfx)
      S_StartSound (NULL, sfx);
  }

  if (castframes == 12)
  {
    // go into attack frame
    castattacking = true;
    if (castonmelee)
      caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
    else
      caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
    castonmelee ^= 1;
    if (caststate == &states[S_NULL])
    {
      if (castonmelee)
        caststate=
          &states[mobjinfo[castorder[castnum].type].meleestate];
      else
        caststate=
          &states[mobjinfo[castorder[castnum].type].missilestate];
    }
  }

  if (castattacking)
  {
    if (castframes == 24
       ||  caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
    {
      stopattack:
      castattacking = false;
      castframes = 0;
      caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    }
  }

  casttics = caststate->tics;
  if (casttics == -1)
      casttics = 15;
}


//
// F_CastResponder
//

boolean F_CastResponder (event_t* ev)
{
  if (ev->type != ev_keydown)
    return false;

  if (castdeath)
    return true;                    // already in dying frames

  // go into death frame
  castdeath = true;
  caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
  casttics = caststate->tics;
  castframes = 0;
  castattacking = false;
  if (mobjinfo[castorder[castnum].type].deathsound)
    S_StartSound (NULL, mobjinfo[castorder[castnum].type].deathsound);

  return true;
}


static void F_CastPrint (const char* text) // CPhipps - static, const char*
{
  const char* ch; // CPhipps - const
  int         c;
  int         cx;
  int         w;
  int         width;

  // find width
  ch = text;
  width = 0;

  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c> HU_FONTSIZE)
    {
      width += 4;
      continue;
    }

    w = hu_font[c].width;
    width += w;
  }

  // draw it
  cx = 160-width/2;
  ch = text;
  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c> HU_FONTSIZE)
    {
      cx += 4;
      continue;
    }

    w = hu_font[c].width;
    // CPhipps - patch drawing updated
    V_DrawNumPatch(cx, 180, 0, hu_font[c].lumpnum, CR_DEFAULT, VPT_STRETCH);
    cx+=w;
  }
}


//
// F_CastDrawer
//

void F_CastDrawer (void)
{
  spritedef_t*        sprdef;
  spriteframe_t*      sprframe;
  int                 lump;
  boolean             flip;

  // erase the entire screen to a background
  // CPhipps - patch drawing updated
  V_DrawNamePatch(0,0,0, "BOSSBACK", CR_DEFAULT, VPT_STRETCH); // Ty 03/30/98 bg texture extern

  F_CastPrint ((castorder[castnum].name));

  // draw the current frame in the middle of the screen
  sprdef = &sprites[caststate->sprite];
  sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
  lump = sprframe->lump[0];
  flip = (boolean)sprframe->flip[0];

  // CPhipps - patch drawing updated
  V_DrawNumPatch(160, 170, 0, lump+firstspritelump, CR_DEFAULT,
     VPT_STRETCH | (flip ? VPT_FLIP : 0));
}

//
// F_BunnyScroll
//
static const char pfub2[] = { "PFUB2" };
static const char pfub1[] = { "PFUB1" };

static void F_BunnyScroll (void)
{
  char        name[10];
  int         stage;
  static int  laststage;

  {
    int scrolled = 320 - (finalecount-230)/2;
    if (scrolled <= 0) {
      V_DrawNamePatch(0, 0, 0, pfub2, CR_DEFAULT, VPT_STRETCH);
    } else if (scrolled >= 320) {
      V_DrawNamePatch(0, 0, 0, pfub1, CR_DEFAULT, VPT_STRETCH);
    } else {
      V_DrawNamePatch(320-scrolled, 0, 0, pfub1, CR_DEFAULT, VPT_STRETCH);
      V_DrawNamePatch(-scrolled, 0, 0, pfub2, CR_DEFAULT, VPT_STRETCH);
    }
  }

  if (finalecount < 1130)
    return;
  if (finalecount < 1180)
  {
    // CPhipps - patch drawing updated
    V_DrawNamePatch((320-13*8)/2, (200-8*8)/2,0, "END0", CR_DEFAULT, VPT_STRETCH);
    laststage = 0;
    return;
  }

  stage = (finalecount-1180) / 5;
  if (stage > 6)
    stage = 6;
  if (stage > laststage)
  {
    S_StartSound (NULL, sfx_pistol);
    laststage = stage;
  }

  sprintf (name,"END%i",stage);
  // CPhipps - patch drawing updated
  V_DrawNamePatch((320-13*8)/2, (200-8*8)/2, 0, name, CR_DEFAULT, VPT_STRETCH);
}


//
// F_Drawer
//
void F_Drawer (void)
{
  if (finalestage == 2)
  {
    F_CastDrawer ();
    return;
  }

  if (!finalestage)
    F_TextWrite ();
  else
  {
    switch (gameepisode)
    {
      // CPhipps - patch drawing updated
      case 1:
           if ( gamemode == retail )
             V_DrawNamePatch(0, 0, 0, "CREDIT", CR_DEFAULT, VPT_STRETCH);
           else
             V_DrawNamePatch(0, 0, 0, "HELP2", CR_DEFAULT, VPT_STRETCH);
           break;
      case 2:
           V_DrawNamePatch(0, 0, 0, "VICTORY2", CR_DEFAULT, VPT_STRETCH);
           break;
      case 3:
           F_BunnyScroll ();
           break;
      case 4:
           V_DrawNamePatch(0, 0, 0, "ENDPIC", CR_DEFAULT, VPT_STRETCH);
           break;
    }
  }
}