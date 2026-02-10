#include "lastbreach.h"

static void skip_block(Parser *ps){
  int depth=0;
  if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth=1; }
  while(depth>0 && !ps_is(ps,TK_EOF)){
    if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth++; continue; }
    if(ps_is(ps,TK_RBRACE)){ ps_expect(ps,TK_RBRACE,"}"); depth--; continue; }
    lx_next_token(&ps->lx);
  }
}
static void skip_until_semi(Parser *ps){
  while(!ps_is(ps,TK_SEMI) && !ps_is(ps,TK_EOF)) lx_next_token(&ps->lx);
}


void parse_world(World *w, const char *filename, char *src){
  Parser ps; ps_init(&ps, filename, src);
  while(!ps_is(&ps,TK_EOF) && !ps_is_ident(&ps,"world")) lx_next_token(&ps.lx);
  if(ps_is(&ps,TK_EOF)) return;
  lx_next_token(&ps.lx);
  if(ps_is(&ps,TK_STRING)){ char *tmp=ps_expect_string(&ps,"world name"); free(tmp); }
  ps_expect(&ps,TK_LBRACE,"{");
  while(!ps_is(&ps,TK_RBRACE) && !ps_is(&ps,TK_EOF)){
    if(ps_is_ident(&ps,"version")){ lx_next_token(&ps.lx); (void)ps_expect_number(&ps,"version"); ps_expect(&ps,TK_SEMI,";"); continue; }

    if(ps_is_ident(&ps,"shelter")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        char *k=ps_expect_ident(&ps,"shelter key");
        ps_expect(&ps,TK_COLON,":");
        double v=ps_expect_number(&ps,"number");
        ps_expect(&ps,TK_SEMI,";");
        if(strcmp(k,"temp_c")==0) w->shelter.temp_c=v;
        else if(strcmp(k,"signature")==0) w->shelter.signature=v;
        else if(strcmp(k,"power")==0) w->shelter.power=v;
        else if(strcmp(k,"water_safe")==0) w->shelter.water_safe=v;
        else if(strcmp(k,"water_raw")==0) w->shelter.water_raw=v;
        else if(strcmp(k,"structure")==0) w->shelter.structure=v;
        else if(strcmp(k,"contamination")==0) w->shelter.contamination=v;
        free(k);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"inventory")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        char *item=ps_expect_string(&ps,"item");
        ps_expect(&ps,TK_COLON,":");
        if(!ps_is_ident(&ps,"qty")) dief("%s:%d: expected qty", filename, ps.lx.cur.line);
        lx_next_token(&ps.lx);
        double qty=ps_expect_number(&ps,"qty");
        double cond=0.0;
        if(ps_is(&ps,TK_COMMA)){
          ps_expect(&ps,TK_COMMA,",");
          if(!ps_is_ident(&ps,"cond")) dief("%s:%d: expected cond", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          cond=ps_expect_number(&ps,"cond");
        }
        ps_expect(&ps,TK_SEMI,";");
        inv_add(&w->inv,item,qty,cond);
        free(item);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"events")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        if(ps_is_ident(&ps,"daily")){
          lx_next_token(&ps.lx);
          char *ename=ps_expect_string(&ps,"event name");
          if(!ps_is_ident(&ps,"chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          double ch=ps_expect_percent(&ps,"percent");
          if(ps_is_ident(&ps,"when")){ lx_next_token(&ps.lx); skip_until_semi(&ps); }
          ps_expect(&ps,TK_SEMI,";");
          if(strcmp(ename,"breach")==0) w->events.breach_chance=ch;
          free(ename);
          continue;
        }
        if(ps_is_ident(&ps,"overnight_threat_check")){
          lx_next_token(&ps.lx);
          if(!ps_is_ident(&ps,"chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          double ch=ps_expect_percent(&ps,"percent");
          if(ps_is_ident(&ps,"when")){ lx_next_token(&ps.lx); skip_until_semi(&ps); }
          ps_expect(&ps,TK_SEMI,";");
          w->events.overnight_chance=ch;
          continue;
        }
        dief("%s:%d: unknown events entry", filename, ps.lx.cur.line);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    /* ignore other blocks (constants/weather/...) */
    if(ps_is(&ps,TK_IDENT)){
      char *k=ps_expect_ident(&ps,"ident");
      if(ps_is(&ps,TK_LBRACE)) skip_block(&ps);
      else if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
      else {
        while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)) lx_next_token(&ps.lx);
        if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
      }
      free(k);
      continue;
    }
    lx_next_token(&ps.lx);
  }
  if(ps_is(&ps,TK_RBRACE)) ps_expect(&ps,TK_RBRACE,"}");
}

void parse_catalog(Catalog *cat, const char *filename, char *src){
  Parser ps; ps_init(&ps, filename, src);
  while(!ps_is(&ps,TK_EOF)){
    if(ps_is_ident(&ps,"taskdef")){
      lx_next_token(&ps.lx);
      char *tname=ps_expect_string(&ps,"task name");
      TaskDef *td=cat_get_or_add_task(cat,tname);
      free(tname);

      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        if(ps_is_ident(&ps,"time")){
          lx_next_token(&ps.lx); ps_expect(&ps,TK_COLON,":");
          int ticks=(int)(ps_expect_number(&ps,"ticks")+0.5);
          ps_expect(&ps,TK_SEMI,";");
          td->time_ticks = (ticks<=0)?1:ticks;
          continue;
        }
        if(ps_is_ident(&ps,"station")){
          lx_next_token(&ps.lx); ps_expect(&ps,TK_COLON,":");
          char *st=ps_expect_ident(&ps,"station");
          ps_expect(&ps,TK_SEMI,";");
          if(td->station) free(td->station);
          td->station=st;
          continue;
        }

        if(ps_is(&ps,TK_IDENT)){
          char *k=ps_expect_ident(&ps,"field");
          if(ps_is(&ps,TK_COLON)){
            ps_expect(&ps,TK_COLON,":");
            while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)){
              if(ps_is(&ps,TK_LBRACE)){ skip_block(&ps); break; }
              lx_next_token(&ps.lx);
            }
            if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
          } else if(ps_is(&ps,TK_LBRACE)){
            skip_block(&ps);
          } else if(ps_is(&ps,TK_SEMI)){
            ps_expect(&ps,TK_SEMI,";");
          } else {
            while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)) lx_next_token(&ps.lx);
            if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
          }
          free(k);
          continue;
        }
        lx_next_token(&ps.lx);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"itemdef")){
      lx_next_token(&ps.lx);
      char *nm=ps_expect_string(&ps,"item name");
      free(nm);
      if(ps_is(&ps,TK_LBRACE)) skip_block(&ps);
      continue;
    }

    lx_next_token(&ps.lx);
  }
}


