#include "lastbreach.h"

void seed_default_catalog(Catalog *cat){
  struct { const char *name; int t; const char *station; } defs[] = {
    {"Eating",1,"kitchen"},
    {"Resting",2,"cot"},
    {"Sleeping",4,"cot"},
    {"Water filtration",2,"wash"},
    {"Meal prep",2,"kitchen"},
    {"Cooking",2,"kitchen"},
    {"Cleaning",2,"wash"},
    {"General shelter chores",2,"chores"},
    {"Maintenance chores",2,"workshop"},
    {"Gun smithing",2,"workshop"},
    {"Power management",2,"power"},
    {"Electrical diagnostics",2,"power"},
    {"Electronics repair",2,"workshop"},
    {"Watering plants",1,"hydroponics"},
    {"Hydroponics maintenance",2,"hydroponics"},
    {"Reading",1,"lounge"},
    {"Talking",1,"lounge"},
    {"Socializing",1,"lounge"},
    {"Food preservation",2,"kitchen"},
    {"Heating",2,"heat"},
    {"Tending a fire",2,"heat"},
    {"Scouting outside",3,"outside"},
    {"Fishing",3,"outside"},
    {"Fish cleaning",1,"kitchen"},
    {"Defensive shooting",3,"defense"},
    {"Defensive combat",3,"defense"},
    {"First aid",1,"med"},
    {"Medical treatment",2,"med"},
  };
  int n=(int)(sizeof(defs)/sizeof(defs[0]));
  for(int i=0;i<n;i++){
    TaskDef *t=cat_get_or_add_task(cat, defs[i].name);
    t->time_ticks=defs[i].t;
    if(defs[i].station){
      if(t->station) free(t->station);
      t->station=xstrdup(defs[i].station);
    }
  }
}


