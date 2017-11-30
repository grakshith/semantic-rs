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
  {"bool", "bool"},
  {"integer","integer"},
  {"string","string"},
  {"float","float"},
  {"bool","bool"}
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
  // int line_no;
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
  // cout<<table<<"   "<<child<<"   "<<name<<endl;
  // if(table!=child){
  //   child=table;
  // }

  auto res_pair = table->symbols.insert(make_pair(name,make_pair(child,type)));
  return res_pair.second;
}

string lookup_table(struct sym_table *table, string name){
  while(table && name.size()){
    for(auto it = table->symbols.begin(); it!=table->symbols.end(); it++){
      if(name==it->first){
        return it->second.second;
      }
    }
    //Static scoping
    table = table->parent;
  }
  string empty;
  stringstream ss;
  ss<<"Identifier "<<name<<" not found"<<endl;
  semantic_errors.push_back(ss.str());
  return empty;
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

int  expr_bin_type_check(struct sym_table *table, struct node *root, vector<string> &types){
  //returns zero in case of error, 1 otherwise

  for(int i=0;i<root->n_elems;i++){
    if(strcmp(root->elems[i]->name, "ExprBinary")==0){
      int ret = expr_bin_type_check(table, root->elems[i], types);
      if(!ret)
        return ret;
    }
    else if(strcmp(root->elems[i]->name, "ExprPath")==0){
      // ident 1
      // cout<<root->elems[1]->name<<" "<<root->elems[2]->name<<endl;
      
      string ident = find_in_ast(root->elems[i], "ident");
      string type = lookup_table(table, ident);
      if(!type.size()){
        return 0;
      }
      types.push_back(type);
      // ident 2
      // string ident2,type2;
      // if(strcmp(root->elems[2]->name,"ExprLit")==0){
      //   ident2 = find_in_ast(root->elems[2], "ExprLit");
      //   type2 = id_map[ident2];
      // }
      // else if(strcmp(root->elems[2]->name,"ExprPath")==0){
      //   ident2 = find_in_ast(root->elems[2], "ident");
      //   //string lit = find_in_ast(root->elems[2],)
      //   type2 = lookup_table(table, ident2);
      // }
      // if(!type2.size()){
      //   return 0;
      // }
      // types.push_back(type2);
    }
    else if(strcmp(root->elems[i]->name, "ExprLit")==0){
      string type = id_map[find_in_ast(root->elems[i],"ExprLit")];
      // string type2 = id_map[find_in_ast(root->elems[i+1], "ExprLit")];
      if(!type.size()){
        return 0;
      }
      types.push_back(type);
    }

  }
  return 1;
}

int expr_flow_type_check(struct sym_table *table, struct node *n, vector<string> &types){
  int flag=1;
  if(strcmp(n->elems[1]->name, "ExprBinary")==0){
    
      int ret = expr_bin_type_check(table, n->elems[1],types);
      if(!ret){
        flag=0;
        stringstream ss;
        ss<<"Invalid types for binary operation in the flow control predicate"<<endl;
        semantic_errors.push_back(ss.str());
      }
      else if(ret==1)
      {
        for(int i=0;i<types.size()-1;i++){
          if(id_map[types[i]]!=id_map[types[i+1]]){
            flag=0;
            stringstream ss;
            ss<<"Invalid types for binary operation in the flow control predicate"<<endl;
            semantic_errors.push_back(ss.str());
          }
        }
      }
    }

    return flag;
}
int global_flag=1;
int build_sym_table(struct sym_table *table, struct node *n, struct sym_table *scope){
  struct sym_table *new_scope=NULL;
  
  bool status;
  if(strcmp(n->name, "ItemFn")==0){
    new_scope= new sym_table();
    new_scope->parent = table;
    status=insert_symbol(table, n->elems[0]->elems[0]->name,"func_decl",new_scope);
  }
  else if(strcmp(n->name, "DeclLocal")==0){
        int flag=1;
        string name = find_in_ast(n->elems[0],"ident");
        string type = find_in_ast(n->elems[1], "ident");

        if(table->symbols.find(name)!=table->symbols.end())
        {
          flag=0;

          stringstream ss;
          ss<<"Redeclaration of "<<name<<endl;
          semantic_errors.push_back(ss.str());

        }
        
        else if(type.size()!=0&&id_map.find(type)==id_map.end())
        {
          flag=0;
          stringstream ss;
          ss<<"Invalid type "<<type<<" in declaration of "<<name<<endl;
          semantic_errors.push_back(ss.str());

        }

        else
        {
          type = id_map[type];
        if(strcmp(n->elems[2]->name,"ExprLit")==0){
              string infer = find_in_ast(n->elems[2], "ExprLit");//inferred type
              if(type.size()==0 && infer.size()!=0){
                type = infer;
              }
              else if(type.size()==0&&infer.size()==0)
                flag=0; //dont insert into symbol table
              else{
                if(id_map[type]!=id_map[infer]){
                  flag=0;
                  stringstream ss;
                  ss<<"Declaration of "<<name<<" invalid, types mismatch"<<endl;
                  semantic_errors.push_back(ss.str());
                }
              }
        }
        if(strcmp(n->elems[2]->name,"ExprPath")==0){
          string infer = find_in_ast(n->elems[2],"ident");
          infer = lookup_table(table,infer);
          if(type.size()==0 && infer.size()!=0){
                type = infer;
              }
              else if(type.size()==0&&infer.size()==0)
                flag=0; //dont insert into symbol table
              else{
                if(id_map[type]!=id_map[infer]){
                  flag=0;
                  stringstream ss;
                  ss<<"Declaration of "<<name<<" invalid, types mismatch"<<endl;
                  semantic_errors.push_back(ss.str());
                }
              }
        }
        if(strcmp(n->elems[2]->name,"ExprBinary")==0){
          vector<string> types;
          int ret = expr_bin_type_check(table, n->elems[2], types);

          int flag_no_right_type=1;
          if(types.size()==0)
          {
            flag=0;
            flag_no_right_type=0;
            stringstream ss;
            ss<<"Invalid declaration of "<<name<<endl;
            semantic_errors.push_back(ss.str());

            
          }
          
          else
          {
          //fills the type vector with all the types of the 
          // variables in the expression
          //set flag according to ret
            flag = flag && ret;
            int rhs_type_flag=1;
            if(ret==1)
            {
              for(int i=0;i<types.size()-1;i++){
                if(id_map[types[i]]!=id_map[types[i+1]]){
                  flag=0;
                  rhs_type_flag=0;
                  stringstream ss;
                  ss<<"Expression involving declaration of "<<name<<" is invalid"<<endl;
                  semantic_errors.push_back(ss.str());
                }
              }
            }

            // &&(type.size()!=0)&&(!(id_map[type]==types[0]))

            if (rhs_type_flag)
            {
              if(type.size()==0)
              {
                type = types[0];
              }

              else if (id_map[type] != types[0])
              {
              flag=0;
              stringstream ss;
              ss<<"Type mis match in declaration of "<<name<<" LHS TYPE "<<type<<" RHS TYPE "<<types[0]<<endl;
              semantic_errors.push_back(ss.str());
              }
            
            }
           }
          }

        }
        
        if(flag)
        {
          
          type = id_map[type];
          status=insert_symbol(table, name, type, scope);
        
        }

        else
        {
          global_flag=0;
        }
  }

  else if(strcmp(n->name,"ExprIf")==0){
    vector<string> types;
    int flag =expr_flow_type_check(table, n->elems[0],types);
    if(flag==0)
    {
      global_flag=0;
    }
  }

  else if(strcmp(n->name,"ExprWhile")==0){
    vector<string> types;
    int flag = expr_flow_type_check(table, n->elems[1],types); 

    if(flag==0)
    {
      global_flag=0;
    }
  }



  else if(strcmp(n->name, "ExprAssign")==0){
    int flag = 1;
    string name = find_in_ast(n->elems[0],"ident");
    string status = lookup_table(table, name);
    if(!status.size())
      flag=0;
    else
    {

    string type, ident;
    if(strcmp(n->elems[1]->name, "ExprLit")==0)
      {
        type = find_in_ast(n->elems[1], "ExprLit");
        type = id_map[type];

        if(status!=type && flag)
        {
          flag=0;
          stringstream ss;
          ss<<"Type mis match in assignement of "<<name<<" LHS TYPE "<<status<<" RHS TYPE "<<type<<endl;
          semantic_errors.push_back(ss.str());

        }

      }
    if(strcmp(n->elems[1]->name, "ExprPath")==0){
      ident = find_in_ast(n->elems[1]->elems[0], "ident");
      string type = lookup_table(table, ident);
      if(!type.size())
        flag=0;
    }
    if(strcmp(n->elems[1]->name, "ExprBinary")==0){
      vector<string> types;
      int ret = expr_bin_type_check(table, n->elems[1],types);
      if(!ret){
        flag=0;
        stringstream ss;
        ss<<"Invalid types for binary operation during assignment of "<<name<<endl;
        semantic_errors.push_back(ss.str());
      }
      else if(ret==1)
      {
        int rhs_type_flag=1; //rhs types are assumed to be valid
        for(int i=0;i<types.size()-1;i++){
          if(id_map[types[i]]!=id_map[types[i+1]]){
            flag=0;
            rhs_type_flag=0;
            stringstream ss;
            ss<<"Expression involving assignment of "<<name<<" is invalid"<<endl;
            semantic_errors.push_back(ss.str());
          }
        }

        if (rhs_type_flag&&!(status==types[0]))
        {
          flag=0;
          stringstream ss;
          ss<<"Type mismatch in assignement of "<<name<<" LHS TYPE "<<status<<" RHS TYPE "<<types[0]<<endl;
          semantic_errors.push_back(ss.str());


        }


      }
    }
  }
    if(flag){
      
      string type = id_map[status];
      insert_symbol(table, name, id_map[type], scope);
    }

    else
    {
     
      global_flag=0;
    }
  }
  if(new_scope){
    table = new_scope;
    scope = new_scope;
  }
  for(int i=0;i<n->n_elems;i++){
    build_sym_table(table, n->elems[i], scope);
  }

  return global_flag;
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

void print_ast(struct node *n, int depth){
  int i=0;
 // print_indent(depth);
  if (strcmp(n->name,"ident") == 0) {
    // cout<<"Printing depth-"<<depth<<endl;
    print_indent(depth);
    print("%s\n", n->elems[0]->name);

  }
  else if (strcmp(n->name,"ExprLit") == 0)
  {
    print_indent(depth);
    print("%s\n", n->elems[0]->elems[0]->name);

  }
  else{
    int step=0;
    if(strcmp(n->name,"ExprBinary")==0){
      print_indent(depth);
      print("(%s\n",n->elems[0]->name);
      step=indent_step;
      // cout<<"Traversing step - "<<depth+step<<endl;
      }
      for (i = 0; i < n->n_elems; ++i) {
      print_ast(n->elems[i], depth + step);
      }
      if(strcmp(n->name,"ExprBinary")==0){
        // cout<<"Closing depth- "<<depth<<endl;
      print_indent(depth);
      print(")\n");
    }
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
  if(ret==0)
  {
  printf("Building symbol table with root %p\n",global_sym_table);
  int status = build_sym_table(global_sym_table, nodes, global_sym_table);
  print_symbol_table(global_sym_table,0);
  printf("No. of semantic errors : %ld\n",semantic_errors.size());
  
  print_semantic_errors();
  

  
  if(status==1)
  {
    cout<<"Abstract Syntax Tree\n";
    print_ast(nodes,0);
  }
  }
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

