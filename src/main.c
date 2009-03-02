
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_INCLUDES 8

#define BLOCK_SIZE   64
#define INVALID_OP   0xFF
#define BYTE_OP      0xFE
#define WORD_OP      0xFD

typedef unsigned char OperationType;
typedef unsigned int AddressType;

typedef struct MacroType {
   char *name;
   char *value;
   struct MacroType *next;
} MacroType;

typedef struct {
   char *arg;
   OperationType op;
} StatementType;

typedef struct {
   char *name;
   OperationType opcode;
   int arg_count;
} InstructionMapType;

typedef struct SymbolNode {
   char *name;
   size_t length;
   AddressType addr;
   struct SymbolNode *next;
} SymbolNode;

typedef enum {
   TOK_INVALID    = 'i',
   TOK_SYMBOL     = 's',
   TOK_VALUE      = 'v',
   TOK_ADD        = '+',
   TOK_SUBTRACT   = '-',
   TOK_MULTIPLY   = '*',
   TOK_DIVIDE     = '/',
   TOK_LPAREN     = '(',
   TOK_RPAREN     = ')'
} TokenType;

typedef struct TokenNode {
   TokenType type;
   union {
      unsigned int value;
      char *symbol;
   };
   struct TokenNode *next;
} TokenNode;

static InstructionMapType INSTRUCTION_MAP[] = {

   /* J-class */
   {  "j",        0x00,    1  },
   {  "jc",       0x01,    1  },
   {  "jz",       0x02,    1  },
   {  "jcz",      0x03,    1  },
   {  "jn",       0x04,    1  },
   {  "jcn",      0x05,    1  },
   {  "jzn",      0x06,    1  },
   {  "jczn",     0x07,    1  },
   {  "c",        0x08,    1  },
   {  "cc",       0x09,    1  },
   {  "cz",       0x0A,    1  },
   {  "ccz",      0x0B,    1  },
   {  "cn",       0x0C,    1  },
   {  "ccn",      0x0D,    1  },
   {  "czn",      0x0E,    1  },
   {  "cczn",     0x0F,    1  },

   /* LS-class */
   {  "ldb",      0x10,    1  },
   {  "ldc",      0x11,    1  },
   {  "lxh",      0x12,    1  },
   {  "lxl",      0x13,    1  },
   {  "stb",      0x14,    1  },
   {  "stc",      0x15,    1  },
   {  "sxh",      0x16,    1  },
   {  "sxl",      0x17,    1  },
   {  "sta",      0x18,    1  },

   /* A-class */
   {  "and",      0x20,    0  },
   {  "or",       0x21,    0  },
   {  "shl",      0x22,    0  },
   {  "shr",      0x23,    0  },
   {  "add",      0x24,    0  },
   {  "inc",      0x25,    0  },
   {  "dec",      0x26,    0  },
   {  "not",      0x27,    0  },
   {  "clr",      0x28,    0  },

   /* M-class */
   {  "mab",      0x30,    0  },
   {  "mac",      0x31,    0  },
   {  "sax",      0x32,    0  },
   {  "sbx",      0x33,    0  },
   {  "scx",      0x34,    0  },
   {  "lbx",      0x35,    0  },
   {  "lcx",      0x36,    0  },
   {  "ret",      0x37,    0  },
   {  "hlt",      0x38,    0  },

   /* Pseudo-instructions */
   {  "db",       BYTE_OP, 1  },
   {  "dw",       WORD_OP, 1  }

};

static enum { OUT_LISTING, OUT_RAW, OUT_HEX } output_format = OUT_LISTING;

static const size_t instruction_count
   = sizeof(INSTRUCTION_MAP) / sizeof(INSTRUCTION_MAP[0]);

static int error_count;
static SymbolNode *symbols;
static MacroType *macros;
static AddressType current_address;
static unsigned int byte_count;

static void DisplayUsage(const char *name);
static FILE *DoPreprocess(const char *filename);
static void DoPreprocessFile(const char *filename, int level, FILE *out_fd);
static void ProcessDefineStart(const char *line, char **name);
static void ProcessDefine(const char *line, const char *name);
static void ProcessDefineEnd(char **name);
static void DoFirstPass(FILE *fd);
static void DoSecondPass(FILE *input, FILE *output);
static int GetStatement(FILE *fd, StatementType *statement, int do_add,
                        char **raw);
static StatementType ParseStatement(char *line);
static void ParseLabel(char *line, int do_add);
static void ToLower(char *line);
static void TrimWhitespace(char *line);
static void StripComments(char *line);
static void StripWhitespace(char *line);
static int ReadLine(FILE *fd, char **line);
static int AddSymbol(const char *name, size_t len, unsigned int value);
static SymbolNode *FindSymbol(const char *name, size_t len);
static int AddMacro(const char *name);
static MacroType *FindMacro(const char *name);
static void AppendMacro(const char *name, const char *value);
static TokenNode *Tokenize(const char *expr);
static unsigned int Evaluate(const char *expr);
static unsigned int Eval1(TokenNode **tp);
static unsigned int Eval2(TokenNode **tp);
static unsigned int Eval3(TokenNode **tp);
static unsigned int Eval4(TokenNode **tp);

int main(int argc, char *argv[]) {

   const char *input_name;
   const char *output_name;
   FILE *input_fd;
   FILE *output_fd;
   int x;

   /* Parse arguments. */
   input_name = NULL;
   output_name = NULL;
   for(x = 1; x < argc; x++) {
      if(!strcmp(argv[x], "-o")) {
         if(output_name != NULL) {
            DisplayUsage(argv[0]);
            return -1;
         } else {
            output_name = argv[x + 1];
            ++x;
         }
      } else if(!strcmp(argv[x], "-raw")) {
         output_format = OUT_RAW;
      } else if(!strcmp(argv[x], "-list")) {
         output_format = OUT_LISTING;
      } else if(!strcmp(argv[x], "-hex")) {
         output_format = OUT_HEX;
      } else if(!strcmp(argv[x], "-h")) {
         DisplayUsage(argv[0]);
         return 0;
      } else {
         if(input_name != NULL) {
            DisplayUsage(argv[0]);
            return -1;
         } else {
            input_name = argv[x];
         }
      }
   }
   if(input_name == NULL) {
      DisplayUsage(argv[0]);
      return -1;
   }
   if(output_name == NULL) {
      switch(output_format) {
      case OUT_RAW:
         output_name = "out.raw";
         break;
      case OUT_HEX:
         output_name = "out.hex";
         break;
      default:
         output_name = "out.lst";
         break;
      }
   }

   byte_count = 0;
   error_count = 0;
   symbols = NULL;
   macros = NULL;
   input_fd = DoPreprocess(input_name);
   if(input_fd != NULL) {
      DoFirstPass(input_fd);
   }
   if(error_count == 0) {
      output_fd = fopen(output_name, output_format == OUT_RAW ? "wb" : "w");
      if(output_fd == NULL) {
         fclose(input_fd);
         fprintf(stderr, "ERROR: could not open %s for writing\n", output_name);
         return -1;
      }
      rewind(input_fd);
      DoSecondPass(input_fd, output_fd);
      fclose(output_fd);
   }
   if(input_fd) {
      fclose(input_fd);
   }

   printf("Errors:     %u\n", error_count);
   printf("Byte count: %u\n", byte_count);

   return error_count;

}

void DisplayUsage(const char *name) {
   fprintf(stderr, "usage: %s <options> filename\n", name);
   fprintf(stderr, "options:\n");
   fprintf(stderr, "\t-o <filename>   Output filename\n");
   fprintf(stderr, "\t-raw            Raw output\n");
   fprintf(stderr, "\t-list           Listing output\n");
   fprintf(stderr, "\t-hex            Hex output\n");
}

void DoFirstPass(FILE *fd) {

   StatementType statement;

   current_address = 0;
   while(GetStatement(fd, &statement, 1, NULL)) {
      ++current_address;
      ++byte_count;
      if(statement.arg) {
         switch(statement.op) {
         case BYTE_OP:
            break;
         case WORD_OP:
            ++current_address;
            ++byte_count;
            break;
         default:
            current_address += 2;
            byte_count += 2;
            break;
         }
      }
   }

}

void DoSecondPass(FILE *input, FILE *output) {

   StatementType statement;
   unsigned int temp;
   char *line;
   size_t len;
   char *start, *end;

   current_address = 0;
   line = NULL;
   while(GetStatement(input, &statement, 0, &line)) {

      start = line;
      for(;;) {
         end = strchr(start, '\n');
         if(!end) {
            break;
         }
         *end = 0;
         if(output_format == OUT_LISTING) {
            fprintf(output, "                    %s\n", start);
         }
         start = end + 1;
      }

      // Output the address.
      if(output_format == OUT_LISTING) {
         fprintf(output, "%04X ", current_address);
      }

      // Output the opcode.
      switch(statement.op) {
      case BYTE_OP:
      case WORD_OP:
         break;
      default:
         switch(output_format) {
         case OUT_RAW:
            fprintf(output, "%c", statement.op);
            break;
         case OUT_HEX:
            fprintf(output, "%02X\n", statement.op);
            break;
         default: // LISTING
            fprintf(output, "%02X", statement.op);
            break;
         }
         break;
      }

      // Output the argument (if there is one).
      if(statement.arg) {
         temp = Evaluate(statement.arg);
         switch(statement.op) {
         case BYTE_OP:
            switch(output_format) {
            case OUT_RAW:
               fprintf(output, "%c", temp);
               break;
            case OUT_HEX:
               fprintf(output, "%02X\n", temp);
               break;
            default: // LISTING
               fprintf(output, "%02X", temp);
               break;
            }
            break;
         default:
            switch(output_format) {
            case OUT_RAW:
               fprintf(output, "%c%c", temp >> 8, temp & 0xFF);
               break;
            case OUT_HEX:
               fprintf(output, "%02X\n", temp >> 8);
               fprintf(output, "%02X\n", temp & 0xFF);
               break;
            default: // LISTING
               fprintf(output, " %02X %02X", temp >> 8, temp & 0xFF);
               break;
            }
            break;
         }
      }

      if(output_format == OUT_LISTING) {
         len = 0;
         switch(statement.op) {
         case BYTE_OP:
            len = 3;
            break;
         case WORD_OP:
            len = 6;
            break;
         default:
            if(statement.arg) {
               len = 9;
            } else {
               len = 3;
            }
            break;
         }
         for(temp = 0; temp < (16 - len); temp++) {
            fprintf(output, " ");
         }
         fprintf(output, "%s\n", start);
      }

      ++current_address;
      if(statement.arg) {
         switch(statement.op) {
         case BYTE_OP:
            break;
         case WORD_OP:
            ++current_address;
            break;
         default:
            current_address += 2;
            break;
         }
      }

      free(line);
      line = NULL;

   }

}

int GetStatement(FILE *fd, StatementType *statement, int do_add, char **raw) {

   char *line;
   int rc;

   for(;;) {

      rc = ReadLine(fd, &line);
      if(raw) {
         if(*raw) {
            *raw = realloc(*raw, strlen(*raw) + strlen(line) + 2);
            strcat(*raw, "\n");
            strcat(*raw, line);
         } else {
            *raw = malloc(strlen(line) + 1);
            strcpy(*raw, line);
         }
      }
      if(rc) {
         StripWhitespace(line);
         StripComments(line);
         TrimWhitespace(line);
         ToLower(line);
         ParseLabel(line, do_add);
      }

      if(!rc) {
         free(line);
         return 0;
      } else if(line[0]) {
         *statement = ParseStatement(line);
         free(line);
         return 1;
      }

   }

}

void ParseLabel(char *line, int do_add) {

   const char *end;
   size_t len;
   int rc;

   end = strstr(line, ":");
   if(!end) {
      return;
   }

   len = end - line;
   if(do_add) {
      rc = AddSymbol(line, len, current_address);
      if(!rc) {
         ++error_count;
         line[len] = 0;
         fprintf(stderr, "ERROR: duplicate symbol: \"%s\"\n", line);
         line[len] = ':';
      }
   }

   memmove(line, end + 1, strlen(end + 1) + 1);
   TrimWhitespace(line);

}

StatementType ParseStatement(char *line) {

   StatementType result;
   InstructionMapType *instr;
   const char *op_name;
   const char *arg;
   size_t x, y;
   int found;

   result.op = INVALID_OP;
   result.arg = 0;

   /* Look up the instruction. */
   instr = NULL;
   arg = NULL;
   for(x = 0; x < instruction_count; x++) {
      op_name = INSTRUCTION_MAP[x].name;
      found = 1;
      for(y = 0; op_name[y]; y++) {
         if(line[y] != op_name[y]) {
            found = 0;
            break;
         }
      }
      if(found) {
         if(isspace(line[y]) || line[y] == 0) {
            instr = &INSTRUCTION_MAP[x];
            if(line[y]) {
               arg = &line[y + 1];
            }
            break;
         }
      }
   }

   /* If the instruction wasn't found log an error. */
   if(instr == NULL) {
      ++error_count;
      for(x = 0; line[x]; x++) {
         if(isspace(line[x])) {
            break;
         }
      }
      line[x] = 0;
      fprintf(stderr, "ERROR: invalid instruction: \"%s\"\n", line);
      return result;
   }

   /* Make sure the right number of arguments were given. */
   if(arg && instr->arg_count == 0) {
      ++error_count;
      fprintf(stderr, "ERROR: argument given for %s\n", instr->name);
      return result;
   }
   if(!arg && instr->arg_count > 0) {
      ++error_count;
      fprintf(stderr, "ERROR: no argument given for %s\n", instr->name);
      return result;
   }

   result.op = instr->opcode;
   if(arg) {
      result.arg = strdup(arg);
   }

   return result;

}

void ToLower(char *line) {

   size_t x;
   char ch;

   for(x = 0; line[x]; x++) {
      ch = line[x];
      if(ch >= 'A' && ch <= 'Z') {
         line[x] = ch - 'A' + 'a';
      }
   }

}

void TrimWhitespace(char *line) {

   size_t x;

   /* Leading whitespace */
   while(isspace(line[0])) {
      for(x = 0; line[x]; x++) {
         line[x] = line[x + 1];
      }
   }

   /* Trailing whitespace */
   x = strlen(line);
   while(isspace(line[x - 1])) {
      line[x - 1] = 0;
      --x;
   }

}

void StripComments(char *line) {

   int x;

   for(x = 0; line[x]; x++) {
      if(line[x] == ';') {
         line[x] = 0;
         break;
      }
   }

}

void StripWhitespace(char *line) {

   int x, y;

   for(x = 0; line[x]; x++) {
      while(isspace(line[x]) && isspace(line[x + 1])) {
         for(y = x + 1; line[y]; y++) {
            line[y] = line[y + 1];
         }
      }
   }

}

int ReadLine(FILE *fd, char **line) {

   char *temp;
   size_t len;
   size_t max_len;
   char ch;

   temp = malloc(BLOCK_SIZE + 1);
   max_len = BLOCK_SIZE;
   len = 0;
   for(;;) {

      ch = fgetc(fd);
      if(feof(fd)) {
         temp[len] = 0;
         *line = temp;
         return 0;
      }
      if(ferror(fd)) {
         temp[len] = 0;
         *line = temp;
         fprintf(stderr, "ERROR: read failed on input\n");
         ++error_count;
         return 0;
      }

      if(ch == '\n') {
         break;
      }

      if(len >= max_len) {
         max_len += BLOCK_SIZE;
         temp = realloc(temp, max_len + 1);
      }
      temp[len++] = ch;
   }

   temp[len] = 0;
   *line = temp;
   return 1;

}

int AddSymbol(const char *name, size_t len, unsigned int value) {

   SymbolNode *np;

   if(FindSymbol(name, len)) {
      return 0;
   }

   np = malloc(sizeof(SymbolNode));
   np->name = malloc(len + 1);
   memcpy(np->name, name, len);
   np->name[len] = 0;
   np->length = len;
   np->addr = value;
   np->next = symbols;
   symbols = np;

   return 1;

}

SymbolNode *FindSymbol(const char *name, size_t len) {

   SymbolNode *np;

   for(np = symbols; np; np = np->next) {
      if(np->length == len && !memcmp(np->name, name, len)) {
         return np;
      }
   }

   return NULL;

}

int AddMacro(const char *name) {

   MacroType *mp;

   if(FindMacro(name)) {
      return 0;
   }

   mp = malloc(sizeof(MacroType));
   mp->name = malloc(strlen(name) + 1);
   strcpy(mp->name, name);
   mp->value = NULL;
   mp->next = macros;
   macros = mp;

   return 1;

}

MacroType *FindMacro(const char *name) {

   MacroType *mp;
   for(mp = macros; mp; mp = mp->next) {
      if(!strcmp(name, mp->name)) {
         return mp;
      }
   }

   return NULL;

}

void AppendMacro(const char *name, const char *value) {

   MacroType *mp;
   size_t len;

   mp = FindMacro(name);

   len = strlen(value) + 2;
   if(mp->value) {
      len += strlen(mp->value);
      mp->value = realloc(mp->value, len);
   } else {
      mp->value = malloc(len);
      mp->value[0] = 0;
   }
   strcat(mp->value, value);
   strcat(mp->value, "\n");

}

TokenNode *Tokenize(const char *expr) {

   TokenNode *result;
   TokenNode *last;
   TokenNode *tp;
   char *endptr;
   size_t x;

   result = NULL;
   last = NULL;
   x = 0;
   while(expr[x]) {
      switch(expr[x]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         /* Decimal number. */
         tp = malloc(sizeof(TokenNode));
         tp->next = NULL;
         if(last) {
            last->next = tp;
         } else {
            result = tp;
         }
         last = tp;
         tp->type = TOK_VALUE;
         tp->value = strtoul(&expr[x], &endptr, 10);
         x = endptr - expr;
         break;
      case '$':
         /* Hex number. */
         tp = malloc(sizeof(TokenNode));
         tp->next = NULL;
         if(last) {
            last->next = tp;
         } else {
            result = tp;
         }
         last = tp;
         tp->type = TOK_VALUE;
         tp->value = strtoul(&expr[x + 1], &endptr, 16);
         x = endptr - expr;
         break;
      case '%':
         /* Binary number. */
         tp = malloc(sizeof(TokenNode));
         tp->next = NULL;
         if(last) {
            last->next = tp;
         } else {
            result = tp;
         }
         last = tp;
         tp->type = TOK_VALUE;
         tp->value = strtoul(&expr[x + 1], &endptr, 2);
         x = endptr - expr;
         break;
      case '+':
      case '-':
      case '*':
      case '/':
      case '(':
      case ')':
         /* Math. */
         tp = malloc(sizeof(TokenNode));
         tp->next = NULL;
         if(last) {
            last->next = tp;
         } else {
            result = tp;
         }
         last = tp;
         tp->type = expr[x];
         ++x;
         break;
      case ' ':
      case '\t':
      case '\r':
      case '\n':
         /* Whitespace. */
         ++x;
         break;
      default:
         /* Symbol. */
         tp = malloc(sizeof(TokenNode));
         tp->next = NULL;
         if(last) {
            last->next = tp;
         } else {
            result = tp;
         }
         last = tp;
         tp->type = TOK_SYMBOL;
         endptr = (char*)&expr[x] + 1;
         while((*endptr >= 'a' && *endptr <= 'z')
               || (*endptr >= '0' && *endptr <= '9')
               || *endptr == '_') {
            ++endptr;
         }
         tp->symbol = malloc(endptr - &expr[x] + 1);
         memcpy(tp->symbol, &expr[x], endptr - &expr[x]);
         tp->symbol[endptr - &expr[x]] = 0;
         x = endptr - expr;
         break;
      }
   }

   return result;

}

unsigned int Evaluate(const char *expr) {

   TokenNode *tokens;
   TokenNode *tp;
   unsigned int result;

   tokens = Tokenize(expr);

   if(tokens) {
      tp = tokens;
      result = Eval1(&tp);
      if(tp) {
         fprintf(stderr, "ERROR: invalid expression\n");
      }
   } else {
      result = 0;
   }

   while(tokens) {
      tp = tokens->next;
      if(tokens->type == TOK_SYMBOL) {
         free(tokens->symbol);
      }
      free(tokens);
      tokens = tp;
   }

   return result;

}

unsigned int Eval1(TokenNode **tp) {

   unsigned int result;
   unsigned int right;

   result = Eval2(tp);
   if(*tp) {
      switch((*tp)->type) {
      case TOK_ADD:
         *tp = (*tp)->next;
         right = Eval2(tp);
         result = result + right;
         break;
      case TOK_SUBTRACT:
         *tp = (*tp)->next;
         right = Eval2(tp);
         result = result - right;
         break;
      default:
         break;
      }
   }

   return result;

}

unsigned int Eval2(TokenNode **tp) {

   unsigned int result;
   unsigned int right;

   result = Eval3(tp);
   if(*tp) {
      switch((*tp)->type) {
      case TOK_MULTIPLY:
         *tp = (*tp)->next;
         right = Eval3(tp);
         result = result * right;
         break;
      case TOK_DIVIDE:
         *tp = (*tp)->next;
         right = Eval3(tp);
         if(right == 0) {
            ++error_count;
            fprintf(stderr, "ERROR: division by zero\n");
         } else {
            result = result / right;
         }
         break;
      default:
         break;
      }
   }

   return result;

}

unsigned int Eval3(TokenNode **tp) {
   return Eval4(tp);
}

unsigned int Eval4(TokenNode **tp) {

   SymbolNode *sp;
   unsigned int result;

   switch((*tp)->type) {
   case TOK_VALUE:
      result = (*tp)->value;
      *tp = (*tp)->next;
      break;
   case TOK_SYMBOL:
      sp = FindSymbol((*tp)->symbol, strlen((*tp)->symbol));
      if(sp) {
         result =  sp->addr;
      } else {
         fprintf(stderr, "ERROR: symbol not found: \"%s\"\n", (*tp)->symbol);
         result = 0;
      }
      *tp = (*tp)->next;
      break;
   case TOK_LPAREN:
      *tp = (*tp)->next;
      result = Eval1(tp);
      if(!*tp || (*tp)->type != TOK_RPAREN) {
         fprintf(stderr, "ERROR: expected ')'\n");
      }
      break;
   default:
      *tp = (*tp)->next;
      fprintf(stderr, "ERROR: expected value\n");
      result = 0;
      break;
   }

   return result;

}

FILE *DoPreprocess(const char *filename) {

   FILE *out_fd;

   out_fd = tmpfile();
   if(out_fd == NULL) {
      fprintf(stderr, "ERROR: could not open temporary file\n");
      ++error_count;
      return NULL;
   }

   DoPreprocessFile(filename, 0, out_fd);

   rewind(out_fd);
   return out_fd;

}

void DoPreprocessFile(const char *filename, int level, FILE *out_fd) {

   FILE *in_fd;
   char *line;
   char *current_define;

   if(level >= MAX_INCLUDES) {
      fprintf(stderr, "ERROR: exceeded %d levels\n", MAX_INCLUDES);
      ++error_count;
      return;
   }

   in_fd = fopen(filename, "r");
   if(in_fd == NULL) {
      fprintf(stderr, "ERROR: could not open %s for reading\n", filename);
      ++error_count;
      return;
   }

   current_define = NULL;
   while(ReadLine(in_fd, &line)) {
      if(line[0] == '#') {
         StripWhitespace(line);
         if(       !strncmp(line, "#include ", 9)) {
            DoPreprocessFile(&line[10], level + 1, out_fd);
         } else if(!strncmp(line, "#define ", 8)) {
            ProcessDefineStart(&line[9], &current_define);
         } else if(!strncmp(line, "#end", 4)) {
            ProcessDefineEnd(&current_define);
         } else {
            fprintf(stderr, "ERROR: preprocessor: \"%s\"\n",
                    line);
            ++error_count;
         }
      } else if(current_define) {
         ProcessDefine(line, current_define);
      } else {
         fprintf(out_fd, "%s\n", line);
      }
      free(line);
   }

   fclose(in_fd);

}

void ProcessDefineStart(const char *line, char **name) {

   size_t len;

   if(*name) {
      fprintf(stderr, "ERROR: \"#define\" without \"#end\"\n");
      ++error_count;
      return;
   }

   len = strlen(line);
   *name = malloc(len + 1);
   strcpy(*name, line);

   AddMacro(*name);

}

void ProcessDefine(const char *line, const char *name) {
   AppendMacro(name, line);
}

void ProcessDefineEnd(char **name) {

   if(!*name) {
      fprintf(stderr, "ERROR: \"#end\" not inside a \"#define\"\n");
      ++error_count;
      return;
   }

   free(*name);
   *name = NULL;

}

