%{
#include <string>
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}

%%

[ \t\n]                 ;
^break					return TOKEN(TKBREAK);
^const					return TOKEN(TKCONST);
^continue				return TOKEN(TKCONTINUE);
^do						return TOKEN(TKDO);
^else					return TOKEN(TKELSE);
^enum					return TOKEN(TKENUM);
^extern					return TOKEN(TKEXTERN);
^false					return TOKEN(TKFALSE);
^final					return TOKEN(TKFINAL);
^fn 					return TOKEN(TKFUNC);
^for					return TOKEN(TKFOR);
^if						return TOKEN(TKIF);
^in 					return TOKEN(TKIN);
^let					return TOKEN(TKLET);
^loop					return TOKEN(TKLOOP);
^macro					return TOKEN(TKMACRO);
^match					return TOKEN(TKMATCH);
^mod					return TOKEN(TKMOD);
^move					return TOKEN(TKMOVE);
^mut					return TOKEN(TKMUT);
^return					return TOKEN(TKRETURN);
^sizeof					return TOKEN(TKSIZEOF);
^static					return TOKEN(TKSTATIC);
^struct					return TOKEN(TOSTRUCT);
^super					return TOKEN(TKSUPER);
^true					return TOKEN(TRUE);
^type 					return TOKEN(TKTYPE);
^typeof					return TOKEN(TKTYPEOF);
^unsafe					return TOKEN(TKUNSAFE);
^unsized				return TOKEN(TKUNSIZED);
^use 					return TOKEN(TKUSE);
^virtual				return TOKEN(TKVIRTUAL);
^where					return TOKEN(TKWHERE);
^while					return TOKEN(TKWHILE);
^yield					return TOKEN(YIELD);
[a-zA-Z_][a-zA-Z0-9_]*  SAVE_TOKEN; return TIDENTIFIER;
-?[0-9]+				SAVE_TOKEN; return TDINTEGER;
-?0x[0-9a-f]+			SAVE_TOKEN; return THINTEGER;
-?0o[0-7]+				SAVE_TOKEN; return TOINTEGER;
-?0b[0-1]+				SAVE_TOKEN; return TBINTEGER;
-?[0-9]+.[0-9]*         SAVE_TOKEN; return TDOUBLE;
"/*"					return TOKEN(TBLCOMMENT);
"*/"					return TOKEN(TBRCOMMENT);
"//"					return TOKEN(TLINECOMMENT);
"="                     return TOKEN(TEQUAL);
"=="                    return TOKEN(TCEQ);
"!="                    return TOKEN(TCNE);
"<"                     return TOKEN(TCLT);
"<="                    return TOKEN(TCLE);
">"                     return TOKEN(TCGT);
">="                    return TOKEN(TCGE);
"("                     return TOKEN(TLPAREN);
")"                     return TOKEN(TRPAREN);
"{"                     return TOKEN(TLBRACE);
"}"                     return TOKEN(TRBRACE);
"."                     return TOKEN(TDOT);
","                     return TOKEN(TCOMMA);
"+"                     return TOKEN(TPLUS);
"-"                     return TOKEN(TMINUS);
"*"                     return TOKEN(TMUL);
"/"                     return TOKEN(TDIV);
.                       printf("Unknown token!n"); yyterminate();

%%