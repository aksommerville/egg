/* mf_compile.c
 * High-level reading of text, converting to AST.
 * NOTE: All of the named statement-compile functions here, the keyword has already been popped off the reader.
 */

#include "mf_internal.h"

/* import
 */
 
static int mf_js_compile_import(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {

  /* Only allowed at root scope.
   */
  if (parent->type!=MF_NODE_TYPE_ROOT) return mf_jserr(ctx,&reader->prev,"'import' not allowed here.");
  
  /* Skip the block of symbols and "from", just validate their shape.
   * We don't do imports correctly, we implicitly import everything from every imported file (even the non-exported stuff).
   */
  struct mf_token token;
  int err;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!='{')) return mf_jserr(ctx,&token,"Expected '{'. We only support verbatim imports, no renaming or default.");
  for (;;) {
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (!err) return mf_jserr(ctx,&token,"Unexpected EOF reading 'import' statement.");
    if ((token.c==1)&&(token.v[0]=='}')) break;
    if (token.type==MF_TOKEN_TYPE_IDENTIFIER) continue;
    if ((token.c==1)&&(token.v[0]==',')) continue;
    return mf_jserr(ctx,&token,"Unexpected token '%.*s' in import symbols block.",token.c,token.v);
  }
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=4)||memcmp(token.v,"from",4)) return mf_jserr(ctx,&token,"Expected 'from' before '%.*s'",token.c,token.v);
  
  /* Now the part we care about: The file name.
   */
  struct mf_file *outerfile=mf_js_get_file_by_id(ctx,reader->fileid);
  if (!outerfile) return -1;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if (token.type!=MF_TOKEN_TYPE_STRING) return mf_jserr(ctx,&token,"Expected string before '%.*s'",token.c,token.v);
  char relpath[1024];
  int relpathc=sr_string_eval(relpath,sizeof(relpath),token.v,token.c);
  if ((relpathc<0)||(relpathc>sizeof(relpath))) return mf_jserr(ctx,&token,"Malformed string token.");
  char path[1024];
  int pathc=eggdev_relative_path(path,sizeof(path),outerfile->path,-1,relpath,relpathc);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct mf_file *innerfile=mf_js_get_file_by_path(ctx,path);
  if (!innerfile) { // Haven't imported this one yet.
    if (!(innerfile=mf_js_add_file(ctx,path,0,0))) return -1;
    if ((err=mf_js_gather_statements(parent,ctx,innerfile))<0) return err;
  }
  
  /* And finally a semicolon.
   */
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"';' required to complete 'import' statement.");
  return 0;
}

/* Block of statements in curly braces.
 */
 
static int mf_js_compile_block(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_BLOCK;
  node->token=reader->prev;
  for (;;) {
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]=='}')) return 0;
    mf_token_reader_unread(reader);
    if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  }
}

/* const,var,let
 */
 
static int mf_js_compile_decl(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_DECL;
  node->token=reader->prev;
  for (;;) {
    if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err;
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==';')) return 0;
    if ((token.c!=1)||(token.v[0]!=',')) return mf_jserr(ctx,&token,"Expected ',' or ';'.");
  }
}

/* if
 */
 
static int mf_js_compile_if(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_IF;
  node->token=reader->prev;
  if ((err=mf_js_compile_parenthesized_expression(node,ctx,reader))<0) return err;
  if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c==4)&&!memcmp(token.v,"else",4)) {
    if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  } else {
    mf_token_reader_unread(reader);
  }
  return 0;
}

/* switch
 */
 
static int mf_js_compile_switch(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_SWITCH;
  node->token=reader->prev;
  if ((err=mf_js_compile_parenthesized_expression(node,ctx,reader))<0) return err;
  
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!='{')) return mf_jserr(ctx,&token,"Expected '{' for switch body.");
  struct mf_token opentoken=token;
  for (;;) {
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]=='}')) return 0;
    if (!token.c) return mf_jserr(ctx,&opentoken,"Unclosed switch block.");
    
    if ((token.c==4)&&!memcmp(token.v,"case",4)) {
      struct mf_node *casenode=mf_node_spawn(node);
      if (!casenode) return -1;
      casenode->type=MF_NODE_TYPE_CASE;
      casenode->token=token;
      if ((err=mf_js_compile_expression(casenode,ctx,reader))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=1)||(token.v[0]!=':')) return mf_jserr(ctx,&token,"Expected ':'.");
      continue;
    }
    
    if ((token.c==7)&&!memcmp(token.v,"default",7)) {
      struct mf_node *casenode=mf_node_spawn(node);
      if (!casenode) return -1;
      casenode->type=MF_NODE_TYPE_CASE;
      casenode->token=token;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=1)||(token.v[0]!=':')) return mf_jserr(ctx,&token,"Expected ':'.");
      continue;
    }
    
    mf_token_reader_unread(reader);
    if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  }
}

/* try
 */
 
static int mf_js_compile_try(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_TRY;
  node->token=reader->prev;
  if ((err=mf_js_compile_bracketted_statement(node,ctx,reader))<0) return err;
  
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c==5)&&!memcmp(token.v,"catch",5)) {
    node->argv[0]=1;
    if ((err=mf_js_compile_parenthesized_expression(node,ctx,reader))<0) return err;
    if ((err=mf_js_compile_bracketted_statement(node,ctx,reader))<0) return err;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  }
  if ((token.c==7)&&!memcmp(token.v,"finally",7)) {
    node->argv[1]=1;
    if ((err=mf_js_compile_bracketted_statement(node,ctx,reader))<0) return err;
  } else {
    mf_token_reader_unread(reader);
  }
  return 0;
}

/* for
 */
 
static int mf_js_determine_for_mode(struct mf_token_reader *reader,struct eggdev_minify_js *ctx) {
  struct mf_token_reader save=*reader;
  int err,depth=1;
  struct mf_token opentoken,token;
  if ((err=mf_token_reader_next(&opentoken,reader,ctx))<0) return err;
  if ((opentoken.c!=1)||(opentoken.v[0]!='(')) return mf_jserr(ctx,&opentoken,"Expected '('.");
  for (;;) {
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (!err) return mf_jserr(ctx,&opentoken,"Unclosed 'for' control.");
    if ((token.c==1)&&(token.v[0]=='(')) {
      depth++;
      continue;
    }
    if ((token.c==1)&&(token.v[0]==')')) {
      if (!--depth) return mf_jserr(ctx,&opentoken,"Malformed 'for' control.");
      continue;
    }
    if ((token.c==1)&&(token.v[0]==';')) { *reader=save; return 3; } // in/of may contain a semicolon, but only after the keyword.
    if ((token.c==2)&&!memcmp(token.v,"of",2)) { *reader=save; return 2; }
    if ((token.c==2)&&!memcmp(token.v,"in",2)) { *reader=save; return 1; }
  }
}
 
static int mf_js_compile_for(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->token=reader->prev;
  int mode=mf_js_determine_for_mode(reader,ctx);
  if (mode<0) return mode;
  struct mf_token opentoken,token;
  if ((err=mf_token_reader_next(&opentoken,reader,ctx))<0) return err;
  switch (mode) {
  
    case 1: case 2: {
        node->type=MF_NODE_TYPE_FOR1;
        node->argv[0]=mode;
        
        /* The declaration keyword gets stored in the 'for' node, and then we read the rest of the declaration as a primary expression.
         * This allows for plain identifiers and destructured identifiers to compile transparently, and they'll stop before "in" or "of".
         */
        if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
             if ((token.c==5)&&!memcmp(token.v,"const",5)) node->argv[1]=1;
        else if ((token.c==3)&&!memcmp(token.v,"let",3)) node->argv[1]=2;
        else if ((token.c==3)&&!memcmp(token.v,"var",3)) node->argv[1]=3;
        else return mf_jserr(ctx,&token,"Expected 'const', 'let', or 'var'.");
        struct mf_node *lcv=mf_node_spawn(node);
        if (!lcv) return -1;
        if ((err=mf_js_compile_primary_expression(lcv,ctx,reader))<0) return err;
        
        if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
        if (mode==1) {
          if ((token.c!=2)||memcmp(token.v,"in",2)) return mf_jserr(ctx,&token,"Expected 'in'.");
        } else {
          if ((token.c!=2)||memcmp(token.v,"of",2)) return mf_jserr(ctx,&token,"Expected 'of'.");
        }
        if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err;
      } break;
      
    case 3: {
        node->type=MF_NODE_TYPE_FOR3;
        if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err; // init and the first semicolon
        if (!mf_token_reader_next_is(reader,";",1)) {
          if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err; // test, but not the second semicolon
        } else {
          if (!mf_node_spawn_token(node,"",0)) return err;
        }
        if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
        if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"Expected ';'.");
        if (!mf_token_reader_next_is(reader,")",1)) {
          if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err; // post, and should stop at the close paren
        } else {
          if (!mf_node_spawn_token(node,"",0)) return err;
        }
      } break;
      
    default: return -1;
  }
  if ((mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!=')')) return mf_jserr(ctx,&token,"Expected ')'.");
  if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  return 0;
}

/* while
 */
 
static int mf_js_compile_while(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_WHILE;
  node->token=reader->prev;
  if ((err=mf_js_compile_parenthesized_expression(node,ctx,reader))<0) return err;
  if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  return 0;
}

/* do
 */
 
static int mf_js_compile_do(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_DO;
  node->token=reader->prev;
  if ((err=mf_js_compile_statement(node,ctx,reader))<0) return err;
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=5)||memcmp(token.v,"while",5)) return mf_jserr(ctx,&token,"Expected 'while'.");
  if ((err=mf_js_compile_parenthesized_expression(node,ctx,reader))<0) return err;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"';' required to complete 'do' statement.");
  return 0;
}

/* throw
 */
 
static int mf_js_compile_throw(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_THROW;
  node->token=reader->prev;
  
  struct mf_token token={0};
  int err=mf_token_reader_next(&token,reader,ctx);
  if (err<0) return err;
  if ((token.c!=1)||(token.v[0]!=';')) {
    mf_token_reader_unread(reader);
    if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  }
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"';' required to complete 'throw' statement.");
  return 0;
}

/* return
 */
 
static int mf_js_compile_return(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_RETURN;
  node->token=reader->prev;
  
  struct mf_token token={0};
  int err=mf_token_reader_next(&token,reader,ctx);
  if (err<0) return err;
  if ((token.c!=1)||(token.v[0]!=';')) {
    mf_token_reader_unread(reader);
    if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  }
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"';' required to complete 'return' statement.");
  return 0;
}

/* break,continue
 */
 
static int mf_js_compile_loopctl(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_token token;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_LOOPCTL;
  node->token=reader->prev;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if (token.type==MF_TOKEN_TYPE_IDENTIFIER) return mf_jserr(ctx,&token,"Named loops not supported.");
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"Expected ';'.");
  return 0;
}

/* class
 */
 
static int mf_js_compile_class(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_CLASS;
  if ((err=mf_token_reader_next(&node->token,reader,ctx))<0) return err;
  if (node->token.type!=MF_TOKEN_TYPE_IDENTIFIER) return mf_jserr(ctx,&node->token,"Expected identifier for class name, found '%.*s'",node->token.c,node->token.v);
  
  struct mf_token opentoken;
  if ((err=mf_token_reader_next(&opentoken,reader,ctx))<0) return err;
  if ((opentoken.c!=1)||(opentoken.v[0]!='{')) return mf_jserr(ctx,&opentoken,"Expected class body.");
  
  for (;;) {
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]=='}')) break;
    
    int isstatic=0;
    if ((token.c==6)&&!memcmp(token.v,"static",6)) {
      isstatic=1;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    }
    if (!err) return mf_jserr(ctx,&opentoken,"Unclosed class body.");
    
    if (token.type!=MF_TOKEN_TYPE_IDENTIFIER) return mf_jserr(ctx,&token,"Expected identifier, 'static', or '}'");
    
    struct mf_node *method=mf_node_spawn(node);
    if (!method) return -1;
    method->token=token;
    method->type=MF_NODE_TYPE_METHOD;
    method->argv[0]=isstatic;
    
    if ((err=mf_js_compile_paramlist(method,ctx,reader))<0) return err;
    
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c!=1)||(token.v[0]!='{')) return mf_jserr(ctx,&token,"Expected method body.");
    if ((err=mf_js_compile_block(method,ctx,reader))<0) return err;
  }
  return 0;
}

/* function
 * These are expressions, but we make a special case for them: Trailing semicolon is not required.
 */
 
static int mf_js_compile_function(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  mf_token_reader_unread(reader);
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  if ((err=mf_js_compile_primary_expression(node,ctx,reader))<0) return err;
  return 0;
}

/* Compile one statement.
 */
 
int mf_js_compile_statement(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  struct mf_token token;
  int err=mf_token_reader_next(&token,reader,ctx);
  if (err<0) return err;
  if (!err) return mf_jserr(ctx,&token,"Expected statement before EOF.");
  
  /* "export" is meaningless to us, we implicitly export everything.
   * It must be followed by a statement, so just skip it.
   */
  if ((token.c==6)&&!memcmp(token.v,"export",6)) {
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (!err) return mf_jserr(ctx,&token,"Expected statement after 'export'.");
  }
  
  /* Mostly statements are introduced by a keyword.
   */
  if ((token.c==6)&&!memcmp(token.v,"import",6)) return mf_js_compile_import(parent,ctx,reader);
  if ((token.c==5)&&!memcmp(token.v,"const",5)) return mf_js_compile_decl(parent,ctx,reader);
  if ((token.c==3)&&!memcmp(token.v,"let",3)) return mf_js_compile_decl(parent,ctx,reader);
  if ((token.c==3)&&!memcmp(token.v,"var",3)) return mf_js_compile_decl(parent,ctx,reader);
  if ((token.c==2)&&!memcmp(token.v,"if",2)) return mf_js_compile_if(parent,ctx,reader);
  if ((token.c==6)&&!memcmp(token.v,"switch",6)) return mf_js_compile_switch(parent,ctx,reader);
  if ((token.c==3)&&!memcmp(token.v,"try",3)) return mf_js_compile_try(parent,ctx,reader);
  if ((token.c==3)&&!memcmp(token.v,"for",3)) return mf_js_compile_for(parent,ctx,reader);
  if ((token.c==5)&&!memcmp(token.v,"while",5)) return mf_js_compile_while(parent,ctx,reader);
  if ((token.c==2)&&!memcmp(token.v,"do",2)) return mf_js_compile_do(parent,ctx,reader);
  if ((token.c==5)&&!memcmp(token.v,"throw",5)) return mf_js_compile_throw(parent,ctx,reader);
  if ((token.c==6)&&!memcmp(token.v,"return",6)) return mf_js_compile_return(parent,ctx,reader);
  if ((token.c==5)&&!memcmp(token.v,"break",5)) return mf_js_compile_loopctl(parent,ctx,reader);
  if ((token.c==8)&&!memcmp(token.v,"continue",8)) return mf_js_compile_loopctl(parent,ctx,reader);
  if ((token.c==5)&&!memcmp(token.v,"class",5)) return mf_js_compile_class(parent,ctx,reader);
  if ((token.c==8)&&!memcmp(token.v,"function",8)) return mf_js_compile_function(parent,ctx,reader);
  if ((token.c==1)&&(token.v[0]=='{')) return mf_js_compile_block(parent,ctx,reader);
  
  /* Semicolon alone is a valid noop statement.
   * It's fair to pretend it was a pair of braces, and call it BLOCK.
   */
  if ((token.c==1)&&(token.v[0]==';')) {
    struct mf_node *node=mf_node_spawn(parent);
    if (!node) return -1;
    node->type=MF_NODE_TYPE_BLOCK;
    node->token=token;
    return 0;
  }
  
  /* Anything else must be an expression followed by a semicolon.
   * Per spec, semicolons are optional in certain cases, but we're sticklers for them.
   */
  struct mf_node *wrap=mf_node_spawn(parent);
  if (!wrap) return -1;
  wrap->type=MF_NODE_TYPE_EXPWRAP;
  wrap->token=token;
  mf_token_reader_unread(reader);
  if ((err=mf_js_compile_expression(wrap,ctx,reader))<0) return err;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!=';')) return mf_jserr(ctx,&token,"Semicolon required to complete statement.");
  
  return 0;
}

int mf_js_compile_bracketted_statement(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!='{')) return mf_jserr(ctx,&token,"Expected bracketted statement.");
  mf_token_reader_unread(reader);
  return mf_js_compile_statement(parent,ctx,reader);
}

/* Statement nodes from text.
 */
 
int mf_js_gather_statements(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_file *file) {
  struct mf_token_reader reader={.v=file->src,.c=file->srcc,.fileid=file->fileid};
  struct mf_token token;
  int err;
  for (;;) {
    mf_token_reader_skip_space(&reader);
    if (reader.p>=reader.c) break;
    if ((err=mf_js_compile_statement(parent,ctx,&reader))<0) {
      if (err!=-2) mf_jserr(ctx,&reader.prev,"Unspecified error.");
      return err;
    }
  }
  return err;
}
