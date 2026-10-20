#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"lang.h"
#include<stdbool.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef LANG_MODULE

char *plural_to_singular(const char *w)
{
    static const struct { const char *plural; const char *singular; } irregular[] =
    {
        { "children",  "child"     },
        { "people",    "person"    },
        { "men",       "man"       },
        { "women",     "woman"     },
        { "mice",      "mouse"     },
        { "geese",     "goose"     },
        { "oxen",      "ox"        },
        { "feet",      "foot"      },
        { "teeth",     "tooth"     },
        { "analyses",  "analysis"  },
        { "theses",    "thesis"    },
        { "criteria",  "criterion" },
        { "statuses",  "status"    },
        { "buses",     "bus"       },
        { "gases",     "gas"       },
        { "fish",      "fish"      },
        { "sheep",     "sheep"     },
        { "deer",      "deer"      },
        { "series",    "series"    },
        { "species",   "species"   }
    };
    for (size_t i = 0; i < sizeof irregular / sizeof irregular[0]; ++i)
    {
        if (!strcmp(w, irregular[i].plural))
        {
            char *nm = (char*)malloc(strlen(irregular[i].singular)+1);
            if (nm == NULL) return NULL;
            strcpy(nm, irregular[i].singular);
            return nm;
        }
    }

    const size_t len = strlen(w);
    if (len < 2 || w[len-1] != 's') return NULL; // Does not look plural.

    char *nm = (char*)malloc(len);
    if (nm == NULL) return NULL;

    // Consonant-y plurals: cities -> city, berries -> berry, flies -> fly.
    if (len >= 5 && !strncmp(w+len-3, "ies", 3))
    {
        memcpy(nm, w, len-3);
        nm[len-3] = 'y';
        nm[len-2] = 0;
        return nm;
    }

    // -es plurals:
    // boxes -> box, buzzes -> buzz, branches -> branch,
    // dishes -> dish, choices -> choice, potatoes -> potato,
    // "-sses" plurals: classes -> class, processes -> process.
    // Other trailing "ses" words are treated as regular +s plurals by the
    // generic rule below:
    // houses -> house, cases -> case, doses -> dose, licenses -> license
    // (the -es plurals -buses, -gases, statuses- are in the irregular table).
    if (len >= 4 && w[len-2] == 'e' &&
        (w[len-3] == 'x' || w[len-3] == 'z' ||
         w[len-3] == 'h' || w[len-3] == 'o' ||
         (w[len-3] == 's' && len >= 6 && w[len-4] == 's')))
    {
        memcpy(nm, w, len-2);
        nm[len-2] = 0;
        return nm;
    }

    // Regular plural: steps -> step, jobs -> job, names -> name.
    memcpy(nm, w, len-1);
    nm[len-1] = 0;
    return nm;
}

#endif // LANG_MODULE
