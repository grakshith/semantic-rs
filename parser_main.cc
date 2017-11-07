#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

using namespace std;

extern int yylex();
extern int rsparse();

#define PUSHBACK_LEN 4

static char pushback[PUSHBACK_LEN];
static int verbose;

map<string,string> id_map = {
  {"i8","integer"},
  {"i16","integer"},
  {"i32","integer"},
  {"i64","integer"},
  {"u8","integer"},
  {"u16","integer"},
  {"u32","integer"},
  {"u64","integer"},
  {"f32","float"},
  {"f64","float"},
  {"LitStr","string"},
  {"LitBool","bool"},
  {"LitFloat","float"},
  {"LitInteger","integer"},
  {"bool", "bool"}
};

vector<string> semantic_errors;


void print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  if (verbose) {
    vprintf(format, args);
  }
  va_end(args);
}

// If there is a non-null char at the head of the pushback queue,
// dequeue it and shift the rest of the queue forwards. Otherwise,
// return the token from calling yylex.
int rslex() {
  if (pushback[0] == '\0') {
    return yylex();
  } else {
    char c = pushback[0];
    memmove(pushback, pushback + 1, PUSHBACK_LEN - 1);
    pushback[PUSHBACK_LEN - 1] = '\0';
    return c;
  }
}

// note: this does nothing if the pushback queue is full
void push_back(char c) {
  for (int i = 0; i < PUSHBACK_LEN; ++i) {
    if (pushback[i] == '\0') {
      pushback[i] = c;
      break;
    }
  }
}

extern int rsdebug;

struct node {
  struct node *next;
  struct node *prev;
  int own_string;
  char const *name;
  int n_elems;
  struct node *elems[];
};

struct node *nodes = NULL;
int n_nodes;

struct node *mk_node(char const *name, int n, ...) {
  va_list ap;
  int i = 0;
  unsigned sz = sizeof(struct node) + (n * sizeof(struct node *));
  struct node *nn, *nd = (struct node *)malloc(sz);

  print("# New %d-ary node: %s = %p\n", n, name, nd);

  nd->own_string = 0;
  nd->prev = NULL;
  nd->next = nodes;
  if (nodes) {
    nodes->prev = nd;
  }
  nodes = nd;

  nd->name = name;
  nd->n_elems = n;

  va_start(ap, n);
  while (i < n) {
    nn = va_arg(ap, struct node *);
    print("#   arg[%d]: %p\n", i, nn);
    print("#            (%s ...)\n", nn->name);
    nd->elems[i++] = nn;
  }
  va_end(ap);
  n_nodes++;
  return nd;
}

struct node *mk_atom(char *name) {
  struct node *nd = mk_node((char const *)strdup(name), 0);
  nd->own_string = 1;
  return nd;
}

struct node *mk_none() {
  return mk_atom("<none>");
}

struct node *ext_node(struct node *nd, int n, ...) {
  va_list ap;
  int i = 0, c = nd->n_elems + n;
  unsigned sz = sizeof(struct node) + (c * sizeof(struct node *));
  struct node *nn;

  print("# Extending %d-ary node by %d nodes: %s = %p",
        nd->n_elems, c, nd->name, nd);

  if (nd->next) {
    nd->next->prev = nd->prev;
  }
  if (nd->prev) {
    nd->prev->next = nd->next;
  }
  nd = (node*)realloc(nd, sz);
  nd->prev = NULL;
  nd->next = nodes;
  nodes->prev = nd;
  nodes = nd;

  print(" ==> %p\n", nd);

  va_start(ap, n);
  while (i < n) {
    nn = va_arg(ap, struct node *);
    print("#   arg[%d]: %p\n", i, nn);
    print("#            (%s ...)\n", nn->name);
    nd->elems[nd->n_elems++] = nn;
    ++i;
  }
  va_end(ap);
  return nd;
}

/* Symbol Table definition 
|--------|-------|--------|
|--name--|-scope-|--type--|
|--------|-------|--------| 
name - Name of the identifier
scope - pointer to the symbol table it is in (this or the child)
type - data type if applicable
*/
struct sym_table{
  struct sym_table* parent;
  map<string, pair<struct sym_table*,string> > symbols;
};

bool insert_symbol(struct sym_table *table, string name, string type, struct sym_table *child){
  // if(child){
  //   table = child;
  // }
  auto res_pair = table->symbols.insert(make_pair(name,make_pair(child,type)));
  return res_pair.second;
}

string lookup_table(struct sym_table *table, string name){
  while(table){
    for(auto it = table->symbols.begin(); it!=table->symbols.end(); it++){
      if(name==it->first){
        return it->second.second;
      }
    }
    //Static scoping
    table = table->parent;
  }
  return "id_not_found";
}

struct sym_table *global_sym_table;


int const indent_step = 4;

void print_indent(int depth) {
  while (depth) {
    if (depth-- % indent_step == 0) {
      print("|");
    } else {
      print(" ");
    }
  }
}

string find_in_ast(struct node *n, string key){
  string name;
  if(!n){
    return NULL;
  }
  if(strcmp(n->name, key.c_str())==0){
    string s (n->elems[0]->name);
    return s;
  }
  for(int i=0;i<n->n_elems;i++){
    name = find_in_ast(n->elems[i], key);
    if(name.size()>0)
      return name;
  }
  return name;

}

void build_sym_table(struct sym_table *table, struct node *n, struct sym_table *scope){
  struct sym_table *new_scope=NULL;
  bool status;
  if(strcmp(n->name, "ItemFn")==0){
    new_scope= new sym_table();
    new_scope->parent = table;
    status=insert_symbol(table, n->elems[0]->elems[0]->name,"func_decl",new_scope);
  }
  else if(strcmp(n->name, "DeclLocal")==0){
    string name = find_in_ast(n->elems[0],"ident");
    string type = find_in_ast(n->elems[1], "ident");
    if(n->elems[2]->name=="ExprLit")
      string infer = find_in_ast(n->elems[2], "ExprLit");
    else{
      
    }
    if(type.size()==0){
      type = infer;
    }
    else{
      if(id_map[type]!=id_map[infer]){
        stringstream ss;
        ss<<"Declaration of "<<name<<" invalid, types mismatch"<<endl;
        semantic_errors.push_back(ss.str());
      }
    }
    type = id_map[infer];
    status=insert_symbol(table, name, type, scope);
  }
  else if(strcmp(n->name, "ExprAssign")==0){
    string name = find_in_ast(n->elems[0],"ident");
    string type = find_in_ast(n->elems[1], "ExprLit");
    status=insert_symbol(table, name, id_map[type], scope);
  }
  if(new_scope){
    table = new_scope;
    scope = new_scope;
  }
  for(int i=0;i<n->n_elems;i++){
    build_sym_table(table, n->elems[i], scope);
  }
}

void print_symbol_table(struct sym_table *table, int depth){
  for(auto it=table->symbols.begin(); it!=table->symbols.end();it++){
    print_indent(depth);
    cout<<setw(15)<<it->first<<setw(15)<<it->second.first<<setw(15)<<it->second.second<<endl;
    if(it->second.first!=table){
      print_symbol_table(it->second.first, depth+indent_step);
    }
  }
}

void print_node(struct node *n, int depth) {
  int i = 0;
  print_indent(depth);
  if (n->n_elems == 0) {
    print("%s\n", n->name);
  } else {
    print("(%s\n", n->name);
    for (i = 0; i < n->n_elems; ++i) {
      print_node(n->elems[i], depth + indent_step);
    }
    print_indent(depth);
    print(")\n");
  }
}

void print_semantic_errors(){
  for(auto i: semantic_errors){
    cout<<i;
  }
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "-v") == 0) {
    verbose = 1;
  } else {
    verbose = 0;
  }
  int ret = 0;
  struct node *tmp;
  memset(pushback, '\0', PUSHBACK_LEN);
  /* rsdebug = 1; */
  global_sym_table = new sym_table();
  global_sym_table->parent = NULL;
  ret = rsparse();
  print("--- PARSE COMPLETE: ret:%d, n_nodes:%d ---\n", ret, n_nodes);
  if (nodes) {
    print_node(nodes, 0);
  }
  printf("Building symbol table with root %p\n",global_sym_table);
  build_sym_table(global_sym_table, nodes, global_sym_table);
  print_symbol_table(global_sym_table,0);
  printf("No. of semantic errors : %ld\n",semantic_errors.size());
  print_semantic_errors();
  while (nodes) {
    tmp = nodes;
    nodes = tmp->next;
    if (tmp->own_string) {
      free((void*)tmp->name);
    }
    free(tmp);
  }
  return ret;
}

void rserror(char const *s) {
  fprintf (stderr, "%s\n", s);
}

