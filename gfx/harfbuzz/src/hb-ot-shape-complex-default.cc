/*
 * Copyright © 2010,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-ot-shape-complex-private.hh"


/* TODO Add kana, and other small shapers here */


/* The default shaper *only* adds additional per-script features.*/

static const hb_tag_t hangul_features[] =
{
  HB_TAG('l','j','m','o'),
  HB_TAG('v','j','m','o'),
  HB_TAG('t','j','m','o'),
  HB_TAG_NONE
};

static const hb_tag_t tibetan_features[] =
{
  HB_TAG('a','b','v','s'),
  HB_TAG('b','l','w','s'),
  HB_TAG('a','b','v','m'),
  HB_TAG('b','l','w','m'),
  HB_TAG_NONE
};

static void
collect_features_default (hb_ot_shape_planner_t *plan)
{
  const hb_tag_t *script_features = NULL;

  switch ((hb_tag_t) plan->props.script)
  {
    /* Unicode-1.1 additions */
    case HB_SCRIPT_HANGUL:
      script_features = hangul_features;
      break;

    /* Unicode-2.0 additions */
    case HB_SCRIPT_TIBETAN:
      script_features = tibetan_features;
      break;
  }

  for (; script_features && *script_features; script_features++)
    plan->map.add_bool_feature (*script_features);
}

static hb_ot_shape_normalization_mode_t
normalization_preference_default (const hb_segment_properties_t *props)
{
  switch ((hb_tag_t) props->script)
  {
    /* Unicode-1.1 additions */
    case HB_SCRIPT_HANGUL:
      return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_FULL;
  }
  return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_DIACRITICS;
}

static bool
compose_default (const hb_ot_shape_normalize_context_t *c,
		 hb_codepoint_t  a,
		 hb_codepoint_t  b,
		 hb_codepoint_t *ab)
{
  /* Hebrew presentation-form shaping.
   * https://bugzilla.mozilla.org/show_bug.cgi?id=728866
   * Hebrew presentation forms with dagesh, for characters 0x05D0..0x05EA;
   * Note that some letters do not have a dagesh presForm encoded.
   */
  static const hb_codepoint_t sDageshForms[0x05EA - 0x05D0 + 1] = {
    0xFB30, /* ALEF */
    0xFB31, /* BET */
    0xFB32, /* GIMEL */
    0xFB33, /* DALET */
    0xFB34, /* HE */
    0xFB35, /* VAV */
    0xFB36, /* ZAYIN */
    0x0000, /* HET */
    0xFB38, /* TET */
    0xFB39, /* YOD */
    0xFB3A, /* FINAL KAF */
    0xFB3B, /* KAF */
    0xFB3C, /* LAMED */
    0x0000, /* FINAL MEM */
    0xFB3E, /* MEM */
    0x0000, /* FINAL NUN */
    0xFB40, /* NUN */
    0xFB41, /* SAMEKH */
    0x0000, /* AYIN */
    0xFB43, /* FINAL PE */
    0xFB44, /* PE */
    0x0000, /* FINAL TSADI */
    0xFB46, /* TSADI */
    0xFB47, /* QOF */
    0xFB48, /* RESH */
    0xFB49, /* SHIN */
    0xFB4A /* TAV */
  };

  bool found = c->unicode->compose (a, b, ab);

  if (!found && (b & ~0x7F) == 0x0580) {
      /* Special-case Hebrew presentation forms that are excluded from
       * standard normalization, but wanted for old fonts. */
      switch (b) {
      case 0x05B4: /* HIRIQ */
	  if (a == 0x05D9) { /* YOD */
	      *ab = 0xFB1D;
	      found = true;
	  }
	  break;
      case 0x05B7: /* patah */
	  if (a == 0x05F2) { /* YIDDISH YOD YOD */
	      *ab = 0xFB1F;
	      found = true;
	  } else if (a == 0x05D0) { /* ALEF */
	      *ab = 0xFB2E;
	      found = true;
	  }
	  break;
      case 0x05B8: /* QAMATS */
	  if (a == 0x05D0) { /* ALEF */
	      *ab = 0xFB2F;
	      found = true;
	  }
	  break;
      case 0x05B9: /* HOLAM */
	  if (a == 0x05D5) { /* VAV */
	      *ab = 0xFB4B;
	      found = true;
	  }
	  break;
      case 0x05BC: /* DAGESH */
	  if (a >= 0x05D0 && a <= 0x05EA) {
	      *ab = sDageshForms[a - 0x05D0];
	      found = (*ab != 0);
	  } else if (a == 0xFB2A) { /* SHIN WITH SHIN DOT */
	      *ab = 0xFB2C;
	      found = true;
	  } else if (a == 0xFB2B) { /* SHIN WITH SIN DOT */
	      *ab = 0xFB2D;
	      found = true;
	  }
	  break;
      case 0x05BF: /* RAFE */
	  switch (a) {
	  case 0x05D1: /* BET */
	      *ab = 0xFB4C;
	      found = true;
	      break;
	  case 0x05DB: /* KAF */
	      *ab = 0xFB4D;
	      found = true;
	      break;
	  case 0x05E4: /* PE */
	      *ab = 0xFB4E;
	      found = true;
	      break;
	  }
	  break;
      case 0x05C1: /* SHIN DOT */
	  if (a == 0x05E9) { /* SHIN */
	      *ab = 0xFB2A;
	      found = true;
	  } else if (a == 0xFB49) { /* SHIN WITH DAGESH */
	      *ab = 0xFB2C;
	      found = true;
	  }
	  break;
      case 0x05C2: /* SIN DOT */
	  if (a == 0x05E9) { /* SHIN */
	      *ab = 0xFB2B;
	      found = true;
	  } else if (a == 0xFB49) { /* SHIN WITH DAGESH */
	      *ab = 0xFB2D;
	      found = true;
	  }
	  break;
      }
  }

  return found;
}

const hb_ot_complex_shaper_t _hb_ot_complex_shaper_default =
{
  "default",
  collect_features_default,
  NULL, /* override_features */
  NULL, /* data_create */
  NULL, /* data_destroy */
  NULL, /* preprocess_text */
  normalization_preference_default,
  NULL, /* decompose */
  compose_default,
  NULL, /* setup_masks */
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_UNICODE,
  true, /* fallback_position */
};
