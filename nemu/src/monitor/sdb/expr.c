/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NUM

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"-", '-'},
  {"\\*",'*'},
  {"/",'/'},
  {"\\(",'('},
  {"\\)",')'},
  {"[0-9]+",TK_NUM},

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};   //save the regex after compiled

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {   //nmatch == 1, means matching one time  // rm_so == 0, relative to e+position
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE: break;
          case '+': tokens[nr_token].type = '+'; ++nr_token; break;
          case TK_EQ: tokens[nr_token].type = TK_EQ; ++nr_token; break;
          case '-': tokens[nr_token].type = '-'; ++nr_token; break;
          case '*': tokens[nr_token].type = '*'; ++nr_token; break;
          case '/': tokens[nr_token].type = '/'; ++nr_token; break;
          case '(': tokens[nr_token].type = '('; ++nr_token; break;
          case ')': tokens[nr_token].type = ')'; ++nr_token; break;
          case TK_NUM: tokens[nr_token].type = TK_NUM; 
            strncpy(tokens[nr_token].str,substr_start,substr_len); 
            tokens[nr_token].str[substr_len] = '\0'; 
            ++nr_token; break;

          default: panic("no such rule!");
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

int check_parenthese(int p){
  int position = 0;
  
  for(int find_right_parenthese = nr_token; find_right_parenthese > p; find_right_parenthese --){
    if(tokens[find_right_parenthese].type == ')'){
      position = find_right_parenthese;
      break;
    }
  }

  return position;
}

int rec_sub(int save_pos, int val2){
  int first_value = atoi(tokens[save_pos+1].str);
  val2 = first_value -(val2 - first_value);
  return val2;
}


int eval(int p, int q, bool *success){
  
  int position = 0;
  int save_pos = 0;
  char priority = 0;

  int val1,val2;
  
  if(p > q){
    *success = false;
    return 0;
  }
  else if (p == q){
    *success = true;
    return atoi(tokens[p].str);
  }
  else if(tokens[p].type == '(' && tokens[q].type == ')'){
    return eval(p+1,q-1,success);
  }
  else if(tokens[p].type == '('){
    save_pos = check_parenthese(p) + 1;
  }

  else{
    position = p;

    while(tokens[position].type != 0 && position <= q){
      if(tokens[position].type == '*'){
        if(priority == 0){
          priority = 1;
          save_pos = position;
        }
      }
      else if(tokens[position].type == '/'){
        if(priority == 0){
          priority = 1;
          save_pos = position;
        }
      }
      else if(tokens[position].type == '+'){
        if(priority == 0 || priority == 1){
          priority = 2;
          save_pos = position;
        }
      }
      else if(tokens[position].type == '-'){
        if(priority == 0 || priority == 1){
          priority = 2;
          save_pos = position;
        }
      }

      position ++;
    }

    if(priority == 0) panic();
  }

  val1 = eval(p,save_pos - 1,success);
  val2 = eval(save_pos + 1, q,success);

  
  

  switch (tokens[save_pos].type){
    case '+': *success = true; return val1 + val2; break;
    case '-': *success = true; 
      if(tokens[save_pos+1].type == TK_NUM)
        val2 = rec_sub(save_pos,val2);
    return val1 - val2; break;
    case '*': *success = true; return val1 * val2; break;
    case '/': *success = true; return val1 / val2; break;
    default : *success = false; break;
  }

  *success = false;
  return 0;
}


int expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  return eval(0,nr_token-1,success);
}
